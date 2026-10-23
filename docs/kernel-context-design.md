# BSP preemptive kernel contexts

Status: proposed for issue #30, based on #29. Review before runtime implementation.
The first usable release still requires ring3 isolation, validated syscalls, program
loading, keyboard and shell. This stage supplies actual timer-driven context switches
between two trusted Assembly workers sharing the owned address space. It does not
complete the proposal's SMP scheduler or introduce untrusted execution.

## Ownership and interrupt contract

Add two distinct image-reserved16-KiB RW/NX stacks, each with a lower absent4-KiB
guard. Extend the existing guard array/count, mapping cuts, whole-image permission
proof and QEMU assertions to cover all seven guards. Original five guards, TSS/RSP0,
IST assignments and boot-info layouts remain unchanged. All stack storage stays
reserved for image lifetime; no frame reclamation or postactivation table allocation.
Worker stacks cannot overlap one another, bootstrap, emergency stacks or metadata.

Three fixed records identify bootstrap(slot0) and workers(slot1/2). Each stores its
stack bounds, saved frame pointer, per-context timer selections and progress. Context/record metadata occupies a dedicated image BSS page to avoid overflowing
the existing one-page mutable data section. No
run queue allocation, sleep, priority, migration, callbacks or arbitrary task API.
The bootstrap record is parked during the64-delivery switching interval. Workers
alternate on every tick, receiving32 timer interrupts each; tick64 selects bootstrap.
Do not infer worker completion from scheduling counts: each must publish actual
progress and pass its own integer/RSP/DF checks before final success.

Only the BSP runs; IF clear serializes entry/selection. Normal handlers never STI or call
allocation, mapping, firmware or logging. Fatal context rejection may emit bounded
serial diagnostics before the existing permanent stop, with IF clear. No nesting/reentrant scheduler use. NMI/MC
keep their stop-only IST paths. Memory APIs retain their IF-clear contract and are
not callable by the smoke workers. CR3, segment bases, descriptor tables, TSS and
FS/GS state are shared and unchanged. No SIMD/x87/AMX is used or preserved; this is
not an application/FPU context ABI. Runtime rejects active CET shadow stacks for
this conventional IRETQ context mechanism rather than silently ignoring SSP.

## Saved frame and proof before dereference

Reuse the current returning IRQ integer save/restore order. Record the pointer at
the bottom of its32-byte call shadow area. The192-byte record-on-stack consists of:

- offsets0..31: volatile shadow space, never interpreted as task state;
- offsets32..144: R15,R14,R13,R12,R11,R10,R9,R8,RDI,RSI,RBP,RBX,RDX,RCX,RAX;
- offsets152,160,168,176,184: RIP,CS,RFLAGS,old RSP,SS.

Intel64 interrupt delivery aligns the incoming RSP before pushing its five qwords;
IRETQ restores SS:RSP even at unchanged CPL. Thus the saved pointer is16-byte aligned
for existing calls. Synthetic initial frames use this same layout and restore an
aligned worker stack top with IF1, IOPL0 and the existing owned code/data selectors.
Source: Intel SDM Vol3A sections6.14.2/6.14.3 (64-bit stack frame/IRET).

Before reading any frame field, validate context/record indexes, fixed trusted
record storage, ordered canonical stack bounds, disjoint configured stack ranges,
16-byte saved-frame alignment, checked frame+192<=stack_high and
frame>=stack_low+4096. The lower4-KiB usable reserve below frames provides adapter/
policy call-stack headroom; the separate absent guard is outside stack_low. Trusted
record initialization/proof establishes readable RW/NX supervisor stack pages.
No host-supplied pointer is dereferenced merely because arithmetic is in range.

Then validate RIP within the owned PE RX text interval, CS/SS equal the configured
owned ring0 selectors (all high bits zero), old RSP within that record's stack and
with sufficient headroom, and RFLAGS from an explicit whitelist: bit1, arithmetic
CF/PF/AF/ZF/SF/OF, DF, IF and (for workers only) architectural RF. Bit1 must be set; workers require IF1; the parked
bootstrap continuation requires IF0/RF0. Reject TF,NT,VM,IOPL,AC,ID and reserved bits.
Require frame pointer consistent with align_down(old_RSP,16)-192 for hardware and
synthetic frames. This bounds the restore pointer as well as the interrupted stack.
Canonical lower-half image bounds and root/selector invariants are checked once
before activation and rechecked where privileged adapters need them.

Pure validation/selection routines accept a fixed context and frame; the privileged
adapter checks IF0, owned CR3 and expected active state. A malformed frame/state
returns failure without publishing current/next saved pointers or accounting. Fatal
adapter failure halts via the existing fault path; no invalid IRETQ is attempted.
All returning helpers preserve the firmware integer nonvolatile ABI and shadow-space
alignment; the frame switch itself restores all fifteen saved GPRs via irq_restore.

## Launch, preemption and bounded return

Keep the existing16-tick APIC/return self-test first. Its evidence remains independent
of scheduling. Add a DPL0 IST0 software gate42h for launch only, accepted when the
prepared scheduler is inactive and current slot0. Immediately before INT42h, bootstrap executes CLI; PUSH 2; POPFQ so its saved
RFLAGS meet the whitelist regardless of firmware-originated ID/AC/IOPL state. No
intervening instruction changes flags before INT42h. The target InheritedId fixture
sets ID first, confirms it was set, then executes the same normalization and must
complete successful switching. This is distinct from malformed saved-frame fixtures.
The gate saves its continuation in the same192-byte layout, validates it plus the
first worker's synthetic frame, atomically parks bootstrap/selects worker1, arms a
fresh one-shot timer and IRETQs to worker1. Unexpected launch calls fail closed.

Timer entry branches on an explicit scheduler-active state. The old APIC smoke path
is unchanged when inactive. Active path saves the interrupted worker frame, validates
current and next frames before publication, increments per-worker/total accounting
and chooses the other worker. Send one EOI and rearm only for ticks<64. On tick64,
mask timer/zero initial count before EOI, select the parked bootstrap frame, mark the
interval complete, then restore that frame. No stack is freed. EOI and controller
access finish on the outgoing stack before RSP changes. The adapter changes RSP only
at the terminal restore jump, after all helper calls have returned; no RET crosses
stacks. Increment return-selection evidence before that jump and verify the actual
bootstrap continuation afterward; counters alone do not prove successful IRETQ.

Each worker has a distinct15-GPR sentinel set and expected fixed RSP. Its Assembly
loop checks all GPRs, stack pointer and DF (worker1 DF1, worker2 DF0), increments its
own progress and waits with HLT while IF1. Flag snapshots use owned stack scratch;
CLD in handlers must not leak through IRETQ. Workers never call arbitrary helpers,
return or invoke memory APIs. A sentinel failure executes a controlled stop, not a
task-local recovery claim. Timer may interrupt any instruction in those loops.
The bootstrap final check requires64 ticks/EOIs/restores,32 interrupted selections
per worker, nonzero per-worker progress, complete state, matching owned CR3, masked/
stopped timer and empty ISR/IRR. It emits CONTEXT_SWITCH_OK before normal/fault paths.

## Validation and limitations

Host-only ELF wrappers execute exact frame validation/selection Assembly against a
reference alternating schedule. Include predereference invalid-pointer rejection,
range wrap, overlap, alignment/headroom, bad RIP/selectors/RFLAGS/old RSP, state/index
errors, exhausted interval, unchanged-state snapshots on failure, and eight integer
ABI sentinels. Randomized valid interruptions vary old RSP alignment and arithmetic/
DF flags within the contract; invalid mutations must never publish a next context.
Decode synthetic frames independently and inspect all seven guards/stack permissions.

Normal ISO/FAT and X2 ISO/FAT must execute both workers and resume bootstrap with the
exact count/progress/root/final-state assertions. Controlled context-corruption and
worker-stack guard fixtures must be rejected before invalid return or report the
exact expected #PF/IST evidence. Retain MissingEoi rejection and all nine current
fault fixtures. Run Build, reproducibility and all existing host suites. Independent
code review gets this brief plus every prior finding/response. Link a stacked PR;
no merge or milestone completion. Physical qualification, arbitrary kernel-task
preemption, scheduler fairness, AP/SMP, user protection and FPU contexts remain open.


## Design review response

P2 (bootstrap whitelist mismatch) is addressed by explicit CLI/PUSH2/POPFQ immediately
before INT42h and an InheritedId target fixture. The saved-frame validator remains
strict; malformed frames do not gain normalization. Implementation must document
maximum nested adapter/helper stack consumption below the4-KiB reserve, and permit
old_RSP==stack_high for synthetic launch while keeping frame accesses bounded.
No other design objections were reported; source-only review does not prove runtime.


## Initial target-frame evidence

The first switching run rejected a worker interrupt frame with RFLAGS10602h: RF
was present in the hardware-saved image. The worker whitelist now permits RF while
the normalized software-launch bootstrap still requires RF0. RF is preserved, not
silently cleared; it controls instruction-breakpoint restart, not privilege. This
remains a trusted-kernel-only context contract without debug/user isolation claims.
Intel SDM Vol3B section18.3.1.1 describes RF transfer by IRETQ and its preservation
in hardware interrupt images:
https://cdrdv2-public.intel.com/671427/253669-sdm-vol-3b.pdf
Host fixtures exercise worker RF acceptance and bootstrap RF rejection separately.


## Final evidence review response

P2: the initial QMP walker checked RW only at the leaf. It now rejects RW0 at every
present traversed level, alongside supervisor checks. Fifteen fixtures execute the
actual PowerShell function with modeled physical entries, mutating each level for
read-only/user/presence failures. Leaf NX plus every-level RW/supervisor checks prove
the documented permission property; this is separate from the runtime ownership proof.
