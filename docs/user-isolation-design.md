# Bounded ring3 processes and contained faults

Status: proposed for issue #32, based on #31. Independent review precedes runtime
implementation. This gate implements architectural page/privilege separation and
process-local failure for two bounded embedded Assembly programs. It does not yet
load ELF, expose capabilities/AI, provide general scheduling, or complete M2/M4.

## Roots, storage and mapping authority

Create two private four-level roots before owned-root activation, using exact
paging-owned chunks and the existing whole-chunk ownership/cache proof. Copy the
completed kernel PML4 into each root, retaining supervisor-only kernel subtrees.
Reserve PML4 slot128 (VA0000400000000000h) for user mappings; require it absent in
the kernel root before construction. All other entries match the kernel root and
remain supervisor-only. Controller/lifecycle subtrees stay shared supervisor-only;
no user table builder can modify those subtrees. The kernel root's slot128 remains
absent, and each user root has a distinct PDPT/PD/PT path. No table allocation or
private-root construction occurs after activation. PCIDE/PGE remain disabled.

Within the private2-MiB window, code is one RX/U page at base, data is one RW/NX/U
page at base+1000h, base+2000h is absent, and four RW/NX/U stack pages cover
[base+3000h,base+7000h). All other leaves are absent. Each process owns six distinct
physical pages (code,data,four stack), exact frame-allocator records and a separate
root/table path. No writable backing is shared. CPU-visible parents use P/RW/U with
NX clear; leaves enforce RX versus RW/NX and no global/pkey bits. Walk proofs check
effective U/RW/NX at every level, exact backing records and table ownership.

Allocate and zero backing before the kernel identity-map construction, copy a
bounded <=4096-byte embedded position-independent Assembly payload into each code
page, and initialize only explicit public test inputs in data. Cut the kernel RAM
mapping policy at these physical code-page boundaries: their identity aliases are
RO/NX, while private user aliases are RX. Data/stack identity aliases remain RW/NX
supervisor-only. This avoids a writable alias to executable code. Preserve each
inherited leaf's normalized PAT selector in every alias; retain MTRRs/PAT and the
no-promotion-across-inherited-boundaries policy. Require backing coverage by the
validated allocatable RAM/ownership contract; never expose arbitrary physical pages.

Verify all roots before first user entry, including every kernel PML4 entry and
user leaf, absence of the other process's backing from every U-accessible mapping,
and unchanged cache selectors. Root/table/backing allocations stay reserved for
this bounded stage even after exit/fault; no claim of reusable process teardown.
Construction failure poisons the whole user subsystem and stops; do not publish a
partial runnable process. Metadata is fixed owned image BSS, not caller text.

## GDT, kernel entry stacks and machine state

Append DPL3 long-code and data descriptors in addition to the existing ring0/TSS
layout; increase the original-GDT capacity bound accordingly and test exact limits.
Use explicit selectors with RPL3 on user entry, owned ring0 selectors on kernel
continuations. Descriptor proof verifies present/type/DPL/L/D bits. Boot-info v0/v1
layouts remain unchanged. Keep original selector numbers for transition compatibility,
but audit inherited descriptor authority before any user launch. A bounded descriptor
walker handles16-byte IA32e system descriptors without treating their upper halves
as independent entries and rejects truncation/unsupported system formats. Reject
all inherited call/task gates and present DPL3 system descriptors; permit only
explicitly supported ring0 system descriptor types and ordinary data segments.
Reject every present inherited code descriptor callable from CPL3: nonconforming
DPL3 code and conforming code at any DPL. Inherited present nonconforming ring0
code is permitted only as supervisor authority. Non-present descriptors grant no
entry authority but may produce a contained #NP if selected. The owned
DPL3 code/data descriptors are the only newly granted user authority. Clear LDTR
with LLDT0 after owned descriptors/IDT are active and verify SLDT0 before launch;
no inherited LDT may provide an alternative call gate. Reject unsupported firmware
layouts rather than copying unreviewed user-callable authority. Synthetic fixtures
must include a present DPL3 call gate, malformed16-byte descriptor and an inherited
active LDT path. Exception IST assignments remain unchanged.

Add two image-owned16-KiB kernel-entry stacks with absent4-KiB lower guards, extending
the kernel-stack proof from seven to nine. TSS.RSP0 is set to the selected process's
kernel-entry top before user IRETQ. Restore bootstrap RSP0 after termination. User
stacks are unrelated private backing, with their own absent user VA guard. No use of
a user RSP for kernel helper calls. Existing NMI/MC/DF and ordinary exception ISTs
remain dedicated; nested/early-transition/hardware resilience is not established.

User entry uses a constructed five-qword IRETQ frame and initializes all fifteen
GPRs to zero except documented public user-virtual arguments. No kernel pointer,
physical backing address or prior kernel register value is passed accidentally.
Test-only kernel-probe addresses may be deliberate fixture inputs, not a public API.
Initial user RIP/RSP are canonical and in the proved RX/stack mappings; CS/SS exact
DPL3 selectors, RFLAGS202h (IF1, IOPL0, DF/TF/NT/AC/ID0). No SYSRET or unvalidated
user-supplied kernel return address is used. Kernel continuations are separately
captured trusted ring0 frames and validated before selection.

Set CR0.TS before any user entry so unsupported x87/SIMD execution causes contained
#NM; the kernel uses none. Preserve other CR0 and mitigation bits. Reject CR4.CET,
LA57 and CR4.PKE (PKRU context is not implemented in this stage), rather than
ignoring hidden context state. Existing enabled protections are never silently
disabled to make launch work; unsupported modes stop before user execution.
Before user launch, clear only EFER.SCE and verify it remains clear, preserving
LME/NXE and other implemented EFER bits. No inherited STAR/LSTAR/FMASK path is
therefore callable by SYSCALL. If CPUID1.EDX.SEP is advertised, write and read back
IA32_SYSENTER_CS=0, and zero SYSENTER_ESP/EIP to canonical0 as well; a null CS prevents
Intel SYSENTER privilege entry. On CPUs without SEP the instruction is unsupported.
Record the supported Intel/AMD profile: AMD long-mode SYSENTER is #UD; Intel with
zero SYSENTER_CS is #GP(0). SYSCALL with SCE0 is #UD. Target fixtures require contained
rejection and successful processB afterward, not merely MSR readback. No fast syscall
entry is re-enabled until an owned audited entry/stack protocol exists.

No FPU context or floating-point execution is supported yet. TSS I/O bitmap offset
remains beyond limit and user IOPL0, denying port access. Privileged instructions
must fault. SMEP/SMAP are retained if already enabled; do not bypass them for copies.
Access user backing only through proved supervisor aliases, never arbitrary user VAs.

This stage uses no FS/GS-relative kernel accesses or user TLS. Define both user
FS/GS bases as zero before launch and preserve no user TLS contract. Kernel entry
must not rely on incoming segment/base/register/DF/AC state. Before helper calls,
normalize live flags to IF0/DF0/AC0 with a kernel-stack flags sequence and load
owned kernel DS/ES. Capture any required fault fields first. Do not trust user
changes to FS/GS bases; entry restores the kernel's fixed zero bases before use.
Do not use SWAPGS in this version. After saving integer state, write zero to
IA32_FS_BASE and IA32_GS_BASE before calling helpers; all pre-normalization accesses
are RIP-relative or use the proved kernel RSP, never FS/GS. Even if FSGSBASE is enabled,
user changes are discarded at entry and remain zero on a returning unknown call.
This explicit no-TLS ABI is not a future application context contract. No speculative attack
or side-channel protection claim follows from architectural U/S separation.

## Launch, gate ABI and trusted kernel return

A DPL0 IST0 launch gate43h captures a parked bootstrap frame under the current kernel
root with IF0 and normalized flags, analogous to the reviewed kernel-context launch.
It accepts only a prepared record, no active process and a proved selected root.
Set TSS.RSP0 and process-active state before CR3/IRETQ; no external work occurs in
this IF0 transition. All required kernel instructions, descriptors, metadata and
entry stacks are mapped supervisor-only under every root. Reject an unexpected
CR3/current identity before processing any claimed user event.

Reserve DPL3 interrupt gate80h, IST0, for an internal version0 integer-only ABI:
RAX0=exit, RDI=status; other call IDs return RAX=-38 (unsupported) and resume the
same process. Every other GPR is preserved for a returning unknown call. No user
pointer argument is dereferenced. This is a documented seed ABI, not the proposal's
public handle/syscall namespace. User software INTs to DPL0 gates must fail #GP.
Interrupt gate entry clears IF; entry saves integer state on the TSS.RSP0 stack,
normalizes live flags and validates active identity/root, CPL3 CS/SS, user RIP/RSP,
allowed return flags and complete frame bounds before any returning IRETQ. Malformed
return state terminates the process rather than fabricating a kernel return frame.

Exit never returns to user. Switch to the owned kernel root, record termination
reason/status in DRAINING state and restore kernel segment/base state. Quiesce the
owned timer through the trusted continuation described below, then restore RSP0
and select the separately parked bootstrap frame. Terminal RSP restore/IRETQ resumes
the actual kernel launch caller with IF0; never RET across stacks and never derive
this continuation from user memory.
The kernel orchestrator then runs the second process and checks its data/progress.
All normal handler paths avoid allocator, firmware, arbitrary callbacks and logging;
termination diagnostics occur only after switching to the kernel root with IF0.

## Fault containment and bounded CPU time

Extend the normalized exception entry with a user-origin dispatch before the existing
stop handler destroys registers/fields. Frame vector,error,RIP,CS,RFLAGS,oldRSP,SS
is on the configured IST. For supported synchronous user exceptions (#DE,#DB,#BP,#OF,#BR,
#UD,#NM,#TS,#NP,#SS,#GP,#PF,#MF,#AC,#XF), require saved CS RPL3, active process,
expected current root and an intact kernel IST frame. This discarded-state path
uses hardware-saved CPL3 origin, not exact CS equality: an unexpected user selector
still terminates only the active process. Exact CS/SS checks remain mandatory for
any returning user frame; mismatch there becomes process termination, not a kernel
stop. A saved CPL0 frame never qualifies as a user fault. Capture vector/error/RIP/CR2
without reading user RSP or RIP. A faulting/noncanonical user RSP or instruction
pointer must still be containable: do not dereference or require resumability of
state that will be discarded. Kill only the active process; switch root and restore
the trusted bootstrap continuation exactly as for exit. Kernel-origin exceptions,
NMI/MC/DF, inconsistent ownership and unsupported events retain permanent stop.
User-origin #SS contains noncanonical user stack accesses even when oldRSP cannot
be resumed. User #AC is also contained when CR0.AM and user AC cause alignment faults;
live AC is normalized on the kernel stack as above. A separate target #AC fixture
explicitly enables CR0.AM if needed, preserves all other CR0 bits, sets user AC and
performs a misaligned access, then requires processB progress and restores the prior
AM setting after the campaign. Noncanonical-RSP #SS and #AC frames are discarded,
not forced through ordinary resumable-user-frame checks.
No user instruction is retried after a fatal fault. Do not resume a kernel fault as
if it belonged to a user process, even if process-active metadata is set.

Arm a one-shot BSP timer during each user run, with IF1 in user mode. The timer entry
recognizes active user context independently from the completed kernel-context smoke.
Validate saved CS/root and frame ownership. At most eight deliveries are allowed;
EOI before rearm, then mask/stop before final EOI and terminate the spinning process
as timeout. Preserve/revalidate a valid user frame on ordinary timer return. A user
CLI attempt cannot disable the watchdog at IOPL0. Entry code/gates never STI; interrupts
are only enabled by validated user IRETQ. Keep kernel memory APIs IF-clear and outside
user running intervals. On exit/fault, mask/stop the timer before another process starts. A timer may
already be pending in IRR; masking is not assumed to cancel it. After completing
any currently owned timer EOI, restore to a dedicated trusted kernel termination-
drain continuation under kernel CR3, not directly to bootstrap. Its entry stack is
proved, process state is DRAINING and no user frame can be resumed. This continuation
uses bounded STI/NOP/CLI windows (no HLT) to deliver an already pending owned timer.
The timer adapter accepts CPL0 only in the exact drain text/stack/state and sends
EOI without rearm; every other unexpected kernel-origin timer remains fatal. Check
ISR/IRR with IF0 after each window, at most1024 windows; inability to quiesce stops,
never falsely exits or starts the next process. Once empty, restore the separately
parked bootstrap frame with IF0 and publish terminal process state. No allocator or
other kernel work runs during drain windows. This is the only new kernel IF-enabled
window and is separate from ordinary handlers, which never STI. No arbitrary device interrupts, user scheduling policy
or AP/SMP is introduced.

## Evidence gates and limits

Host fixtures execute exact Assembly table construction/proof, descriptor extension,
user/kernel frame validation and state/termination policy. Independent oracles walk
all roots, check ancestor permission mutations, ownership aliases, table capacities,
range/overflow/canonical/selector/flags/root/state failures and failure atomicity.
Assert firmware integer nonvolatile ABI and integer preservation across returning
unknown calls. Separate user versus kernel frame formats; the old kernel-only
align_down(oldRSP)-192 relation cannot validate a CPL3 frame on RSP0.

Actual QEMU must demonstrate CPL3 (saved CS evidence from a real gate), distinct roots,
private data sentinels at identical user VAs and successful processB after processA
exits or faults. Fixtures include unknown-call return, normal exit, kernel read/write,
code write, NX data execution, user stack guard, privileged I/O/CLI, unsupported FPU
a spinning timeout, disabled SYSCALL/SYSENTER, noncanonical-stack #SS and enabled
alignment-check #AC. Require applicable exact vector/error/RIP/CR2 and continued
kernel/processB progress, with unchanged B storage, not merely a process-dead counter.
Add an invalid user-return-frame fixture rejected before IRETQ and a kernel-origin
fault fixture that still permanently stops. QMP walks confirm every U-accessible
leaf/ancestor/backing and no user access to kernel/controller/other private pages.

Run normal ISO/FAT/x2 profiles, all existing nine fault fixtures, kernel-context and
controller negatives, all host suites, Build and reproducibility. Independent code
review receives original brief and all findings/responses; publish a linked stacked
PR, no merge. The source of truth is actual test evidence, not this design.
Shared supervisor kernel mappings are not KPTI; speculative CPU mitigation auditing,
DMA/SMM, hostile firmware and physical compatibility remain open release gates.
No untrusted AI/agent execution is authorized by this bounded bring-up. ELF loading,
handles, richer syscalls, user preemption/scheduling, FPU contexts, keyboard/shell and
full process lifecycle/reclamation remain subsequent roadmap work.

Architectural references: Intel SDM Vol3A chapters5/6/7 (page/privilege permissions,
interrupt gates and64-bit TSS) and Vol2 IRETQ/CR0.TS behavior:
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf


## Initial design review responses

- P1 fast-entry authority: EFER.SCE is disabled/read back; advertised SYSENTER MSRs
  are zeroed/read back, with Intel #GP versus AMD long-mode #UD target expectations.
  Unsupported paths must fault locally before any inherited privileged entry.
- P1 stack/alignment containment: #SS and #AC join the synchronous user-fault list;
  discarded-state containment never dereferences or requires a valid user oldRSP.
  Explicit target fixtures require successful processB after each fault.
- P2 firmware descriptor authority: reject inherited gates/user-callable system
  descriptors with a bounded16-byte-aware walker; clear/verify LDTR0. Test synthetic
  DPL3 call gates and malformed layouts before user launch.
- P1 follow-up #NP: user-origin non-present-segment faults are contained without
  requiring resumable selectors/RSP. A fixture installs an explicitly non-present
  DPL3 data descriptor in bounded owned test storage, selects it from ring3 and
  requires exact #NP plus processB progress. It grants no present authority.
- P1 follow-up alternate CS: reject inherited present user-callable code descriptors
  (DPL3 nonconforming and all conforming code), and make discarded-fault origin
  classification robust to any saved CPL3 selector. Strict returning selector checks
  terminate only that process on mismatch. Synthetic authority fixtures cover each
  code class; a target far-transfer to prohibited ring0 nonconforming code must #GP
  locally and processB must continue. A separate injected-inherited-code audit fixture
  must reject before any user launch; no unchecked conforming path is executed.

Also force a real owned timer into IRR while IF0 at exit and fault fixture entries
(by arming a short timer and boundedly polling the actual pending bit), confirm the
pending bit before normal mask/drain, then require one EOI, empty state and processB.
A drain-exhaustion fixture suppresses its STI windows with that pending timer and
must stop at the bound without terminal success/B launch. A kernel INT40h outside
the exact drain text/stack while DRAINING must stop before EOI/rearm. These are
separate explicit negative fixtures, not inferred from normal early exits.

Additional architectural sources:
Intel SDM Vol2B SYSENTER (null IA32_SYSENTER_CS rejects entry):
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2b-manual.pdf
AMD APM Vol2 section6.1.2 (SYSENTER illegal in long mode):
https://docs.amd.com/api/khub/documents/sD1_QL~h4Afq2_tvzxqqSQ/content


## Implementation progress (partial evidence)

The GDT audit/extension passes2,310 host authority/bounds/ABI checks and a focused
source-only review with no findings; target descriptor/MSR integration is pending.
Storage/private-root/proof helpers pass2,578 cumulative host checks (including setup
checks), covering exact ownership, mixed per-page PAT decoding, table/permission
mutations, zeroing, capacity quarantine and eight ABI sentinels. Review found two
P2 test gaps (independent mixed-PAT oracle and ABI assertions); both were addressed
and the follow-up found no remaining findings within that component scope.

Memory preparation is now boot-integrated: two roots are constructed before existing
whole-chunk/cache proof, private code identity aliases are RO/NX, and the full user
root/ownership proof runs before kernel CR3 activation. The ISO boot reached its
normal kernel checkpoint after emitting USER_ROOTS_PROVED with distinct roots, and
QMP checked nine absent kernel guards and all supervisor RW/NX kernel stack pages.
Build passed20,766 media assertions. User roots have not yet been activated, and no
CPL3/LDTR/MSR/fault-containment behavior is claimed from this preparation checkpoint.
FAT also reached the normal checkpoint with the nine-stack QMP proof. Two-build
EFI/FAT/ISO/manifest reproducibility, relative NASM invocation and corrupt-PE rejection
passed. Final whole-issue regressions remain pending the privilege adapter.

Frame-policy helpers now have2,159 host cases, each repeated with eight integer
nonvolatile register sentinels, including both active identities and wrong-stack
rejection. Focused independent source review found no actionable finding; privileged
adapters and real CPL3 remain pending. `machine.inc` now contains the planned
stop-only fast-entry/LDTR/TS preflight. It is boot-integrated after trusted-worker
smoke, passes Build/20,766 media assertions and the normal QEMU ISO checkpoint,
and normal FAT boot,
and has a focused independent source review with no findings. QMP records TS set,
SCE clear and zero LDTR/FS/GS bases. SYSENTER readbacks occur inside the helper.
This is machine preparation evidence; no real CPL3 fast-entry/fault claim follows.
