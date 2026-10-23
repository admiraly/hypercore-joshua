# Early boot and Assembly contract v0

Status: proposed internal contract, not a frozen public syscall or application ABI.

## Runtime provenance and executable format

All shipped instructions originate in `boot/uefi.asm`,
`kernel/arch/x86_64/entry.inc` and `kernel/mm/*.inc`. NASM emits PE32+ directly, with separate RX code,
RW data/BSS, and read-only constant/relocation sections. No imported libraries, third-party
bootloader, generated C startup, or host-language interpreter is shipped.
Position-independent RIP-relative code needs no pointer fixups; an ABSOLUTE-only
base relocation block keeps the image relocatable. Its preferred base is zero.
The image is an EFI application, not yet a separate ELF64 kernel loader.

## Firmware ABI

UEFI x64/Microsoft ABI: RCX, RDX, R8, R9 arguments; RAX status; 32 bytes of caller
shadow space; RSP aligned to 16 immediately before CALL. RAX, RCX, RDX, R8-R11 and
XMM0-XMM5 are volatile. RBX, RBP, RSI, RDI, R12-R15 and XMM6-XMM15 are preserved
when returning to firmware. No red zone. Direction flag is clear. No SIMD is used.
UEFI offsets and boot-info offsets are centralized in `sdk/boot.inc`.

`print` takes a UTF-16 string in RDX and follows this ABI. `print_hex` takes a
64-bit value in RCX. `debug_write` takes an ASCII string in RCX. Other helpers
are internal and use this ABI unless their entry explicitly switches to the
non-returning kernel path. No undocumented cross-module register clobbers are allowed.

## Boot-info v0

RDI points to the versioned record defined in `sdk/boot.inc` at kernel entry.
Fields are fixed-width little endian, naturally aligned. The record includes
magic/version/size, framebuffer base/size/width/height/stride/pixel format,
memory-map base/used size/descriptor size/version, and ACPI RSDP when present.
The map is variable stride; never assume an array of a compiler-defined struct.
Its final copy is captured immediately before ExitBootServices. No allocations,
console calls, or logging occur between that call and ExitBootServices.
An invalid map key is retried at most eight times, reacquiring the map each time.
After an attempted ExitBootServices, failures are handled without console or
other non-memory boot-service calls: execution stops rather than returning into
possibly partially shut-down firmware. This avoids unsafe firmware reuse.

Kernel entry never returns. It disables maskable interrupts, selects an owned
64 KiB stack, clones the active firmware GDT into owned storage, appends its own
code/data/TSS descriptors, loads the owned task register and a 256-entry stop-only
IDT with guarded emergency stacks, and retains the
firmware's existing page tables during reservation/allocation bring-up, then
constructs and activates an owned root. It does not free/reuse firmware memory. Exceptions
stop the CPU and emit a bounded serial report only on the recognized QEMU TCG
serial path. Interrupt routing and recoverable exception handling remain absent;
stop-only IST diagnostics do not constitute production fault containment.
Retaining all original GDT selectors during the transition keeps the still-active
firmware IDT's selectors valid until the replacement IDT is loaded. Firmware GDTs
without room for 32 appended descriptor bytes, or with a partial entry, stop safely.

Before interactive diagnostics, the firmware watchdog is disabled. Failure to
disable it aborts the application rather than silently allowing a timed reset.

## Memory and ownership invariants

- Physical frame allocation starts only after all reservation stages; no firmware
  memory is reclaimed.
- Loader, stack, boot-info, IDT, and map remain within the loaded PE image.
- Framebuffer writes are bounded by validated width, height, stride and byte size.
- The loader does not open Block I/O, Disk I/O, Simple File System, or network protocols.
- No firmware calls occur after a successful ExitBootServices.
- Raw COM1 is enabled only for the explicitly recognized QEMU TCG vendor. Physical
  machines do not probe or program legacy UART ports. Early QEMU markers use the
  firmware-initialized UART; configuration is changed only after firmware exit.
- The stub executes on the bootstrap CPU only and returns to the final checkpoint with maskable interrupts off.

## Deferred contracts

The public handle/syscall ABI, interrupt lock ordering, PMM/VMM ownership,
DMA/IOMMU policy, and AI capability broker remain design tasks. They must be
reviewed before their implementations; illustrative syscall names in the spec
are not assigned stable numeric IDs here.

## Primary references

- UEFI Specification 2.10: x64 calling conventions, EFI System Table, Boot
  Services, GetMemoryMap, ExitBootServices, Graphics Output Protocol, ResetSystem.
- Microsoft PE/COFF specification: PE32+, section flags, base relocation blocks.
- Intel 64 and IA-32 Software Developer's Manual, Volume 3: GDT, IDT, long mode.

## Stop-only fault diagnostics

All 256 IDT gates target fixed-stride Assembly stubs. CPU exception vectors
8, 10–14, 17, 21, 29 and 30 already push an error code; other entries synthesize
zero. Stubs push the vector, yielding vector/error/RIP/CS/RFLAGS/oldRSP/oldSS at RSP. The common
non-returning path captures vector, error, RIP, CR2 and stack evidence while staying
on the selected IST stack,
then emits `JOSHUA:KERNEL_FAULT` and a `JOSHUA:FAULT` line with fixed-width hex
fields and halts with maskable interrupts disabled. It never attempts IRET.
Software invariant failures still use the generic marker, without a CPU frame.
CR2 is meaningful as a fault address only for #PF. No firmware calls are used.

This is not a safe arbitrary software-INT ABI: do not issue INT to error-code
exception vectors. Four distinct IST stacks have lower unmapped guards. Same-IST
reentry, bad emergency stacks and faults before replacement-IDT installation may
prevent reporting; arbitrary nesting/NMI/machine-check resilience is not claimed. Physical machines still do not touch
legacy UART ports.

`Build.ps1 -FaultTest Ud2` and `-FaultTest Gp` produce explicitly identified test
images under `out/fault-*` by default. They fault immediately after the EBS marker.
Their manifest records the fixture; they are not normal boot artifacts. The tests
compare reported RIP with the injected instruction address and exercise both CPU
frame shapes. See `tests/Test-FaultDiagnostics.ps1`.

## Final map validation

The normal kernel path validates boot-info version 1/size 104, descriptor version
1 and the exact image-owned map-buffer pointer, then calls `memory_map_validate`
with RCX=map, RDX=length, R8=stride and R9D=physical address width. The leaf routine
preserves firmware-ABI nonvolatile registers and returns EAX=0 valid or 1 invalid.
It reads only, performs no allocations and rejects ambiguous overlapping ranges.
A successful normal boot emits `JOSHUA:MEMORY_MAP_VALID` before the halt marker.
See the memory contract for accepted layouts and limits. Fault fixtures still
stop before this normal-path validation.

## Boot-info v1 and reservation candidates

The v0 structure stays 88 bytes. v1 preserves that entire prefix and appends
`image_base` at offset 88 and `image_size` at offset 96, total 104 bytes. The loader
and embedded kernel now explicitly exchange version 1; version 0 is rejected by
this kernel rather than guessed. RDI and the firmware helper ABI are unchanged.
Before final map capture, HandleProtocol obtains the Loaded Image protocol. Its
base must match the actual PE start, size must cover SizeOfImage, and both must
be page aligned without wrap. The kernel independently requires complete coverage
by validated loader-code/data descriptors and CPU-addressable physical bounds.

The image's code reservation is now 16 KiB; image-owned BSS includes an 8,192-entry
candidate array. Headers and exact PE tests cover the expanded layout. Candidate
ranges include only conventional memory advertising WB and no attributes beyond
UC/WC/WT/WB capabilities. Subtract page zero, the full loaded image and the
page-rounded framebuffer range. `MEMORY_CANDIDATES_READY COUNT=` reports completion
of this candidate stage, not permission to allocate. Nonconventional and unfamiliar
attributes/types remain unavailable. The subsequent paging inventory excludes
active table frames before the normal halt checkpoint.

`range_reserve` takes RCX=RANGE_SET, RDX=start, R8=exclusive end; EAX=0 succeeds,
1 invalidates the whole set. Caller supplies owned capacity and validated disjoint
page ranges. It preserves nonvolatile registers and uses no firmware or SIMD. A
failed split can leave prior edits; the builder clears count and aborts on any
failure. Candidates are not consumable until all reservation stages succeed.

## Active paging inventory ABI

`paging_inventory_build` receives RCX pointing to PAGING_INVENTORY from
`sdk/boot.inc`: owned queue/count/capacity, validated map/size/stride, physical
width, NXE/1-GiB flags and root frame. Entries are two qwords: physical frame and
level 4–1. EAX=0 success; EAX=1 clears count. All firmware-ABI nonvolatile registers
are preserved. No firmware calls, SIMD, CR3 writes or explicit page-entry writes
occur. The caller requires four-level IA-32e mode, captures raw CR3 and derives
the root by masking low control bits. It retains EFER.NXE and CPUID 1-GiB support
for leaf interpretation. Unsupported modes and classifications stop boot.

`paging_inventory_reserve` takes RCX=successful inventory and RDX=candidate
RANGE_SET. It subtracts each table page. EAX=1 clears candidate count and invalidates
the ledger; EAX=0 completes exclusion. It requires a successful inventory rather
than treating an empty failed queue as permission to use candidates. The runtime
checks the return at each stage. `JOSHUA:PAGING_RESERVED CR3=... TABLES=...` is
emitted only after exclusion and a comparison against captured CR3. The runtime checks this inherited value before construction. The QEMU test
subsequently requires final CR3 to match the distinct OWNED_PAGING root, not the
inherited root recorded here.

Tables are bounded to 16,384 entries in 256 KiB of BSS inside the image reservation.
Only known readable RAM below the low canonical identity boundary is inspected.
Recursive mappings and cross-level sharing are deliberately unsupported. This
implements inventory and reservation, not page-table ownership or fault recovery.

## Physical allocation ABI

`frame_allocator_init`: RCX=unready FRAME_ALLOCATOR, RDX=completed RANGE_SET,
R8=owned records, R9=capacity (1–4,096). EAX=0 success, 1 invalid with no writes.
The caller owns/excludes metadata and guarantees writable retained identity RAM.
After initialization, only the allocator may mutate its range set; reservation
helpers must not be called on that live set.

`frame_alloc`: RCX=ready allocator, RDX=pages, R8=nonzero owner, R9=power-of-two
page alignment. EAX=0 returns base in RDX; EAX=1 invalid, 2 physical exhaustion,
3 metadata capacity, all with RDX=0 and unchanged state/RAM. Successful allocation
zeroes the entire span. `frame_free`: RCX=allocator, RDX=exact base, R8=exact pages,
R9=exact owner. EAX=0 succeeds, 1 rejects without mutation. Every helper preserves
firmware-ABI nonvolatile registers; no firmware or SIMD is used. BSP-only and
interrupts-disabled are caller preconditions. Internal owner tags grant no user
authority. No generation-based public handle ABI is defined here.

`frame_allocator_check` runs after active paging exclusions: allocate two owners
and alignments, check every allocated qword is zero, write/read patterns, reject
a wrong owner, free, reject a repeated free, and verify counters restored. Only
then emit `JOSHUA:FRAME_ALLOCATOR_OK FREE_PAGES=...` and continue to the checkpoint.
The allocator remains initialized for owned-paging construction. A fault during
clearing or failed boot self-check halts; it does not recover into usable state.

## Owned root construction and activation

OWNED_PAGING describes the allocator/root/table count, poison flag, physical limit,
tracked-chunk cursor, inherited root, image/guard/framebuffer bounds, PAT snapshot
and readiness. `owned_paging_init` takes RCX=zeroed builder, RDX=ready allocator,
R8=physical limit (power of two, 2^32 through 2^47). EAX=0 succeeds; 1 poisons it.
Private chunk allocation returns frames from exact paging-owner records, using
64-page chunks or one-page fallback only for allocator physical-exhaustion status2.

`owned_map_preserved`: RCX=builder, RDX=page start, R8=end, R9=permission flags
(P/RW/NX, supervisor). It proves inherited physical identity and submits each
subrange separately, retaining its cache selectors and inherited leaf boundary.
The private mapper uses 2-MiB/4-KiB leaves without writable execution or replacing
existing leaves. Any failure poisons the incomplete root. `paging_lookup` accepts
RCX=root/RDX=address and returns RAX=raw leaf, RDX=physical translation, R8=leaf bytes
(4KiB/2MiB/1GiB), or zeros when absent. All helpers preserve nonvolatile registers.

`owned_paging_start` verifies transition mappings and whole-chunk cache compatibility,
then enables NXE/WP, flushes PCID/global translations and loads the root. PAT/MTRRs
are never programmed; no leaf promotion across old boundaries is allowed. The
OWNED_PAGING marker is emitted only on successful return under the new root.
Old table frames remain reserved. Memory operations require IF clear. The timer smoke check described below temporarily
enables interrupts only inside its wait sequence. The guard
and four emergency-stack guards are deliberately absent pages; stop-only reporting
is not stack-overflow recovery.

ReadOnly/Nx/Guard fixture images fault after this marker: #PF errors 3/0x11/0,
with exact expected instruction RIP and CR2 target. The NX fixture allocates and
writes its target under the owned root. Ud2/Gp fixtures remain before memory
bring-up. Fixture images are isolated test artifacts, not normal boot images.

## Active kernel virtual arena ABI

The initial arena is `0xffff800000000000` through the next 2 MiB (exclusive).
`lifecycle_prepare(RCX=context,RDX=inactive owned builder,R8=zero record array)`
creates three table pages in existing paging-owned chunks before activation and
whole-chunk cache/ownership proof. It does not activate the root. All 512 leaves
start absent. MAPPING_CONTEXT and 40-byte MAPPING_RECORD are internal kernel
structures, not boot-info extensions. Record states: 0 unused, 1 staged, 2 live,
3 quarantined, 4 retiring. Capacity is 64 records; empty tables remain reserved.

`lifecycle_alloc` takes RCX=context, RDX=1..512 pages and R8=nonzero caller owner.
EAX=0/RDX=base VA succeeds. EAX=1 rejects invalid arguments, 2 reports arena/record/
physical exhaustion, 3 reports physical allocator metadata exhaustion and 4 disables
the context on invariant failure. Every failure returns RDX=0. Private physical
owner FRAME_OWNER_MAPPING separates the backing frames from paging chunks.
`lifecycle_free` takes RCX=context, RDX=exact base VA, R8=exact pages, R9=exact caller
owner. EAX=0 succeeds, 1 rejects unchanged and 4 quarantines/disables. Physical
ownership is retained on fatal failure; callers stop. All eight firmware integer
nonvolatile registers are preserved; no firmware calls, SIMD or user callbacks.

Both active operations require the owned root current, IF clear and PCIDE/PGE off.
The privileged adapter checks these conditions and executes INVLPG. Allocation
preflights identity/cache/permissions, publishes RW/NX supervisor 4-KiB aliases,
invalidates locally, then returns. Release preflights the exact record/leaves,
clears the entire run, invalidates every VA, then frees exact physical ownership.
A/D updates are accepted. No frame or VA is reusable before those invalidations.
This contract is BSP-only and does not permit calls from the returning timer handler; enabling SMP requires a new contract.
`JOSHUA:MAPPING_LIFECYCLE_OK` follows actual high-half write/read/release/reallocation
and zeroing checks, restores allocator accounting, and leaves every arena leaf absent.


## Owned TSS and guarded exception stacks

The GDT constructor validates a nonzero multiple-of-eight original size no greater
than 65,504 before copying; 32 appended bytes contain owned code/data and a 16-byte
available 64-bit TSS descriptor. Limit103 and base identify the aligned 104-byte
TSS wholly inside writable data. After LTR, STR must match the owned selector and
memory descriptor type must be busy (0x8b). Only then is the owned IDT loaded.
QEMU TR evidence confirms selector/base/limit; its cached attributes may display
available type, so the busy check uses descriptor memory in the runtime.

RSP0 points to kernel_stack_top. IST1/2/3/4 point to separate 16-KiB double-fault,
NMI, machine-check and ordinary exception stacks. All other TSS fields are zero
except I/O bitmap offset104 (beyond limit). Future user entry must also force
IOPL0; user I/O enforcement is not tested here. Vectors8/2/18 use IST1/2/3;
all remaining stop gates use IST4. Each stack has a distinct lower 4-KiB guard;
the owned mapping proof checks all five guards including the normal stack guard.

The common handler captures frame RSP and interrupted RSP before aligning below
that intact IST frame and reserving shadow space. FAULT_STACK reports FRAME,
OLD_RSP and IST after the unchanged fault line. StackFault sets normal RSP to the
main guard upper boundary and reads zero; #PF/error0 must use IST4 and retain the
expected old RSP. DoubleFault makes IST4 point into its guard, then reads zero;
failed #PF delivery must produce #DF/error0 on IST1. Saved #DF RIP is not required
to identify a restart instruction; guard CR2 is profile evidence only.

No handler returns. Same-IST nesting can overwrite earlier frames; independent
stacks do not establish general reentrancy, NMI/MC recovery, early-transition
containment or physical qualification. See exception-stacks-design.md.

## Validated ACPI topology discovery

After owned paging/lifecycle checks, `acpi_topology_start` identifies the BSP via
CPUID0Bh EDX when available/nonempty, otherwise CPUID1 EBX[31:24]. Leaf1 APIC support
is required. No APIC mode/base MSR, MMIO, PIC ports or interrupt enable is changed.
`acpi_topology_build(RCX=trusted ACPI_CONTEXT)` consumes boot-info/map/root pointers,
a caller-owned 256-record array/capacity and 32-bit BSP ID. No boot-info ABI changes.
Each internal 16-byte CPU record stores APIC ID, ACPI UID, original flags and kind.

`acpi_readable(RCX=context,RDX=address,R8=length)` accepts 1..65,536 bytes only when
nonzero, no overflow, inside physical/low-canonical bounds, fully covered by type9/
10 map descriptors without RP, and present supervisor identity translations at every
page. Headers are proved before reading lengths; full declared ranges are proved
before checksumming/parsing. The map/root/context are trusted earlier-stage inputs.

The parser requires ACPI2+ RSDP length36..4096 with both checksums and an XSDT.
XSDT entries are eight-byte/divisible and capped at256; each child header is proved
before signature inspection. Exactly one checksummed MADT length44..65536 is consumed.
Unrelated child payloads/checksums are not consumed or certified. MADT records must
fit enclosing bounds with length>=2; known minimum lengths apply, extensions and
unknown records are skipped. Active type0/type9 CPU IDs/UIDs must be unique and
one enabled record must match BSP. Online-capable-only records do not authorize AP
startup. One optional type5 address override replaces the header local-APIC base;
its final page must be aligned/nonzero/in-range. PCAT_COMPAT is recorded only.

EAX=0 publishes ACPI_READY_MAGIC. EAX=1 unreadable, 2 root/signature, 3 checksum,
4 layout, 5 topology, 6 capacity; start also returns7 for missing CPUID/APIC support.
Failure clears ready/count/enabled; staged output is not consumable. Helpers preserve
all eight firmware integer nonvolatile registers and perform no firmware calls/SIMD.
Entry prints ACPI_FAILURE CODE before stop, or ACPI_TOPOLOGY_READY with BSP, CPU and
enabled counts and local APIC base before the normal checkpoint. No controller
programming, IOAPIC/ISO/NMI routing, AML or physical qualification is implied.


## BSP APIC timer foundation (issue #28)

ACPI discovery additionally requires one through eight unique type1 IOAPIC records,
with checked ID, page base and GSI base. This extends internal ACPI_CONTEXT to120
bytes, not boot-info. Controller authority and cache rules are specified in
`docs/apic-timer-design.md`: a dedicated preallocated owned window accepts only
identified controllers, scans every overlapping memory descriptor, rejects RAM and
owned storage, preserves inherited identity/cache selectors and proves effective
strict UC with a bounded read-only PAT/MTRR snapshot. Firmware map gaps grant no
RAM ownership or general device authority. No cache control register is changed.

With IF clear, preflight validates LAPIC ID/version/LVT capacity and IOAPIC ID,
supported version11h or20h, redirection capacity and nonoverlapping GSI ranges
before route programming. Unsupported IOAPIC versions stop before redirection
reads. Maskable inherited routes and legacy PIC lines are masked; SMI/NMI routes
are preserved. Unmasked unsupported delivery modes and pending ISR/IRR state stop.
No ICR/IPI or AP startup is used. Normal boot retains inherited APIC mode; a
separate X2 fixture enables x2APIC on a CPU advertising support.

Vectors40h(timer),41h(software preservation check) andFFh(spurious) use returning
IST0 gates. Timer entries save all fifteen integer registers and restore with
IRETQ; they allocate nothing and call no firmware. The software check covers all
integer registers, RSP and arithmetic/direction flags. The real timer check covers
sixteen one-shot deliveries, matching ticks/EOIs/returns and integer state. IF is
enabled only in the STI/HLT/flags-snapshot wait sequence, then cleared. At completion
the timer is masked/stopped, ISR/IRR are empty, and normal/fault checkpoints have
IF clear. Memory APIs retain their IF-clear precondition. No scheduler, SIMD
preservation contract, calibrated timing, SMP or hardware qualification is claimed.


## Guarded BSP context switching (issue #30)

After the independent16-delivery timer self-test, three fixed trusted context
records identify parked bootstrap and two Assembly workers. Two additional16-KiB
RW/NX stacks have absent lower guards; the guard policy now covers seven stacks.
TSS/RSP0, exception ISTs and versioned boot-info remain unchanged. The PE text
capacity grows to0x7000 bytes; data/rodata/BSS start at8000h/9000h/A000h, BSS is
DB000h, relocations E5000h and image size E6000h. Scheduler metadata has its own
BSS page; all stack/metadata pages stay reserved for image lifetime.

`kernel/sched/context.inc` validates the fixed context configuration and incoming/
selected192-byte frames before publication. The frame has32 bytes of shadow space,
fifteen saved integer registers, then RIP/CS/RFLAGS/oldRSP/SS. It checks stack
arithmetic/headroom/alignment before frame reads, disjoint proved stack allocations,
text RIP, exact ring0 selectors and an explicit flags whitelist. Workers permit
arithmetic/DF/IF/architectural RF with IF1; bootstrap requires IF0/RF0. TF/NT/VM/IOPL/
AC/ID/reserved flags are rejected. The frame must equal align_down(oldRSP,16)-192;
oldRSP==stack_high is valid. Caller-owned context/records and proved readable stack
ranges are preconditions; this is not validation of arbitrary hostile pointers.

INT42h launches only from prepared bootstrap state. CLI/PUSH2/POPFQ immediately
before it normalizes inherited flags. The saved bootstrap continuation is parked;
workers alternate on each of64 timer interrupts,32 interrupts each. Selection is
failure-atomic; EOI/accounting/rearm finish before the terminal RSP change and
irq_restore/IRETQ. Final delivery masks/stops the timer before EOI and restores the
bootstrap frame. No RET crosses stacks, and no allocation/firmware/logging occurs
on normal handler paths. Fatal rejection may emit bounded diagnostics with IF0.
Maximum nested policy/adapter usage is below256 bytes, within a4-KiB lower reserve.
CET is rejected; FS/GS, CR3 and descriptors are shared/unchanged, with no SIMD/x87
context ABI. The trusted workers are not isolated processes.

Success requires actual bootstrap resumption,64 ticks/EOIs/restore selections,
32 per-worker interruptions, nonzero progress and distinct fifteen-register/RSP/DF
sentinels, stopped timer, empty ISR/IRR and IF0. CONTEXT_SWITCH_OK logs bounded counts
and progress; CONTEXT_STACK records expose bounds/counts for independent QEMU walks
of all seven absent guards and every supervisor RW/NX stack page. A successful
selection counter alone is not proof of IRETQ execution. The fixed smoke is not a
fair scheduler, arbitrary kernel preemption API, user isolation or SMP support.

## User memory preparation under development (issue #32)

The preceding issue #30 seven-stack/layout values describe that revision. Current
preparation extends the kernel guard proof to nine stacks: two additional16-KiB
user-entry stacks each have an absent lower4-KiB guard. The current PE text capacity
is9000h bytes, data/rodata/BSS start atA000h/B000h/D000h, rodata spans2000h, BSS is
E5000h, relocations F2000h and image size F3000h. Versioned boot-info is unchanged.

Two exact six-page backing allocations and distinct user table paths are prepared
before activation. Code identity aliases are RO/NX with preserved inherited PAT;
user leaves are RX code and RW/NX private data/stack with an absent user guard.
The root/ownership proof follows whole-kernel/chunk proof before kernel CR3 activation.
USER_ROOTS_PROVED is preparation evidence only: privilege entry/fault containment
remain under implementation and this marker establishes no CPL3 execution.


## Bounded user transition bring-up (issue #32, draft)

The reviewed descriptor-retirement refinement now replaces the copied firmware
prefix with null entries only after proving every live selector/TR/IDT target uses
the unchanged owned suffix; LDTR/FS/GS selectors must be zero. IF0, exact GDTR/IDTR,
owned code/data/busy-TSS encoding and all256 gates are checked before the first
store. Hidden-cache provenance comes from the earlier owned segment reload/LTR.
NMI/MC entries continue to use the owned suffix throughout. Strict authority audit
then appends the only present DPL3 code/data descriptors. Firmware tables and owned
selector indices remain unchanged. There is no later firmware return.

INT43h parks an exact validated bootstrap IRQ frame and constructs a scrubbed
192-byte private user frame. INT80h supports exit0 and unknown-call -38, preserving
all other integer registers/RSP/DF; no user pointer is read by the gate. Per-process
RSP0 and owned roots are selected under IF0 before IRETQ. Live IF/DF/AC and FS/GS
bases are normalized before helpers; no TLS, SWAPGS or FPU context is supported.
Hardware CPL3 exceptions are classified from a bounded ordinary IST frame without
requiring a resumable user RSP. Kernel-origin faults still stop permanently.

The one-shot user watchdog allows eight deliveries. Termination switches to kernel
CR3, masks/stops the timer and enters an exact owned stack/text DRAINING window;
only a timer at that window may return to CPL0, without rearm. The drain stack
reserves64 bytes below its top; its saved IRQ frame is exactly high-256. Up to1024
STI/NOP/CLI windows may clear pending ISR/IRR, then the separately parked kernel
frame is revalidated and restored with IF0. Forced PendingExit/PendingFault fixtures require an observed IRR bit under IF0,
one drain EOI beyond delivered ticks, and independent B continuation. DrainExhaust
and KernelTimer must stop before EOI or B launch; they test the 1024-window bound
and rejection outside the exact kernel return window.

USER_RESULT logs saved user CS/private root/state/unknown-call count/ticks/EOIs and
fault vector/error/RIP/CR2. USER_PAIR_RETURNED is emitted only after actual bootstrap
resumption and both independent private data/progress checks. The host listing/PE
oracle checks exact applicable fault identity and timeout8/8; this does not replace
the independent QMP private-root walk described in docs/testing.md. No full issue/M2/M4, speculative
isolation, FPU, SMP, DMA or physical qualification is established by this draft.


### Independent private-paging evidence

After QMP stop, physical page captures include both private paths, metadata, exact
allocation records, private code/data and identity walk paths for every whole
paging allocation. The Python oracle independently decodes permissions, reserved
bits, per-page PAT selectors, ownership, separate backing, guard absence, payload
bytes, zero padding and terminal metadata. Private parent PWT/PCD must be zero;
large kernel leaves retain PAT bit12 while rejecting level-specific reserved bits.
The oracle checks structural invariants of captured trusted metadata; it is not an
independent proof of all runtime code, arbitrary-process security or hardware.

InheritedCode and LiveSelector fixtures require rejection before user launch.
InheritedLdt installs an owned test LDT before machine preflight and requires the
subsequent user far call to fault #GP(4) after LLDT0, followed by B continuation.
KernelFault injects kernel-origin UD2 inside INT80: exact #UD/RIP on IST4, no user
result or B continuation, and permanent stop under A's still-active private root.

## Embedded ELF profile v1 (issue #36)

The boot path now validates an immutable ELF64 ET_EXEC image before any private
backing allocation. Its two sorted PT_LOAD entries describe one RX code page and
one RW/NX data/BSS page at the existing user VAs. Four stack pages and the guard
remain trusted launch policy. The profile rejects sections/dynamic linking and all
unsupported headers, flags, addresses, sizes, alignment and entry states; see
elf-loader-design.md. This is not general ELF or a public application ABI.

The separate64-byte plan is revalidated before copy and launch. Exact whole backing
ownership precedes every copy. Copy clears all six pages after complete preflight,
then installs file bytes; any failure poisons the context before roots/readiness.
USER_CONTEXT's18-qword prefix/layout and USER_PROCESS remain unchanged. The raw
storage helper remains a trusted internal component; actual boot uses the ELF adapter.

Launch RIP is USER_BASE+16, past an INT3 prefix that makes a page-start launch fail.
The program verifies initialized data at128/136 and zero BSS at256. Data[0,128) is
reserved for documented bootstrap/probe/scratch mutations. Independent QMP captures
check the actual load plan against ELF headers, code/data bytes, BSS/padding and
prior ownership/cache/permission invariants. Malformed magic/entry fixtures must
stop before root publication/CPL3 and preserve the physically read allocation count.

Current PE bounds supersede prior checkpoint layouts: text RVA1000h/sizeA000h,
data B000h/1000h, constants C000h/4000h, BSS10000h/E5000h, relocation F5000h and
image F6000h. The expanded constant section holds the authored ELF envelope; no
external loader or interpreter runs in HyperCore.

## Keyboard preparation checkpoint (issue #38, incomplete driver)

After the normal private ELF pair has returned and before the halted status display,
IF0 preparation requires the owned root, completed trusted contexts and the validated
APIC state. A separate authority record checks FADT8042 and MADT ISA/GSI ownership
before port access. The selected IOAPIC entry must already be masked and use an
eligible maskable delivery mode; its low/high words are unchanged by this checkpoint.
No new interrupt gate, unmask or STI is introduced here.

Bounded real60h/64h commands prepare translation/set2 and verify final configuration
while keyboard scanning is disabled. KEYBOARD_PREPARED reports that checkpoint;
it is not KEYBOARD_READY or evidence of delivered input. Failure stops under IF0
with a controller stage diagnostic. Normal image sizes and stack guards are unchanged.
The controller/authority/decoder occupy previously reserved context-metadata storage.

Future live input must implement the reviewed vector44h frame/EOI contract and
joint40h/44h deadline/drain ownership before using the queue. No shell input or
physical keyboard compatibility is established by preparation alone.


### Experimental explicit keyboard window

Explicit KeyboardTest images open the bounded owned input window after
trusted/private contexts finish. IRQ44 and deadline40 use the shared192-byte saved
frame and Microsoft integer ABI, with exact frame location, owned root/CS/SS/oldRSP,
phase-specific RIP and4-KiB helper reserve checked before frame use/EOI/IRETQ.
Metadata flags are202h (or602h for active Df); drain sets/checks202h. Hardware RF may
additionally be set and remains unchanged for IRETQ; no other saved flag is accepted.
All15 GPRs and post-CLI2h/402h flags are checked around each active wait. Handler
helpers consume at most200 bytes below the frame. Partial failures stop IF0.

The active consumer stores at most64 events. Text/Df have a512-delivery deadline;
Missing requires16 deliveries without input. Cleanup stops/masks40, masks44,
disables controller IRQ generation/first interface, flushes at most32 bytes and
retains joint drain ownership until actual ISR/IRR are empty (maximum1024 windows).
A terminal overflow failure suppresses repeated collection/disable and deadline
failure overwrite if another owned interrupt was already pending. Window phase3 rejects
subsequent keyboard-window timer/keyboard entries. The controlled lifecycle campaign below exercises overflow, forced-pending,
drain-exhaustion and invalid-frame gates; docs/validation.md records results and
qualification limits.


Target-only lifecycle fixtures now exercise actual pending40/44/both, suppressed-STI
drain exhaustion, single-field/proposed-address/root rejection, both out-of-window
vectors, full-queue drop-newest and controlled-producer32-read exhaustion/reentry.
The144-byte evidence record does not grant hardware authority. Refill waits exist
only in OverflowPending after reads16/32 under armed stage2 of the fixture record;
the driver window remains active phase1. Sticky timeout, real-OBF acknowledgments
and actual zero-budget OBF proof are mandatory. Normal returning ISR work remains
bounded without producer waits. WINDOW_DONE follows positive checks, while terminal
failure2 remains a permanent stop after physical retirement. These fixtures establish
neither arbitrary hardware timing nor physical compatibility.


## Current console-profile integration (issue #40)

This current layout supersedes earlier checkpoint layout values above: text RVA
0x1000/size0xd000, data0xe000/0x1000, rodata0xf000/0x5000, BSS0x14000/0xe8000,
relocation RVA0xfc000/raw0x14000, total image0xfd000/file0x15000. Three BSS pages
hold console records after existing user-entry stacks; nine guards and boot/user
ABI sizes are unchanged. Whole-image ownership/identity/cache/permission proofs
still cover every byte; console state is RW/NX, code RX and constants RO/NX.

`-Console` selects separate outputs and never enables fixture inputs. Its real
keyboard consumer uses the shared fixed-RSP activation/retirement macros and
existing exact192-byte IRQ frames. Only the STI/HLT/CLI prompt wait and bounded
joint drain enable interrupts. Active vector40 is unowned and fatal before EOI;
phase2 accepts only exactly validated real pending vectors. IRQ44 collects/EOIs;
foreground IF0 performs editing, source-checked facts capture, bounded formatting,
model updates and rendering. All indefinite counters saturate. Halt masks/stops
hardware, disables and flushes the controller and drains before the visible HALTED
row and permanent IF0 marker. See docs/console-design.md for exact helper records,
source provenance and failure policies. QEMU smoke is narrower than the full
interactive/negative/regression exit gates and does not qualify physical machines.

## Isolated console rejection and counter fixtures

Explicit ConsoleTest profiles are compile-time instrumentation in separate media.
BadEditor writes length128 immediately before checked editor_event, retaining the
actual EDX token. BadFacts writes owned enabled-CPU count0 after parse and before
facts-source validation. Both fail permanently before output publication; phase1
and IF0 are stop-only evidence, not successful hardware retirement. Saturation seeds
selected foreground and keyboard counters at MAX-1 before READY, with normal queue,
root and permissions. Its separate finite actual-ISR44 trace is updated only after
exact frame and hardware ISR ownership proof. Neither injection nor trace exists
in normal Console images. No versioned ABI or image bounds change. See the exact
fixture/source evidence expectations in console-design.md and testing.md.

Console timer fixtures4–6 add only compile-time controlled pending injection and
64-byte evidence in owned data. Phase1 UnexpectedTimer proves a real ISR40 then
stops without EOI. PendingTimer proves the exact phase2 drain return and one EOI
without rearm before successful phase3 retirement. DrainExhaust leaves IF0 in the
fixture drain windows and stops at1025 with timer still in IRR, not ISR. The existing
normal wait/drain/frame policy and versioned layouts are unchanged. Saved-field
copying follows frame and ISR proof, uses only volatile RAX, adds no call/stack
depth, and does not modify the returning frame. The forcing helper consumes48
bytes below its entry RSP through a leaf lapic_read/write, or96 through nested
user_timer_stop/console_timer_check (including helper calls), within the bootstrap
reserve. Normal Console contains neither forcing nor evidence-copy instructions.

## Process-owned handle policy foundation checkpoint

At PR #45, issue #44's handle-design.md specified pure bounded Assembly helpers
and host fixtures without a boot include or changes to the exit/unknown INT80 gate.
Issue #46 now wires the adapter below; USER_CONTEXT/USER_PROCESS layouts remain
unchanged. This is not
a public handle/syscall ABI freeze. Trusted table backing, per-incarnation tag
nonreuse and active-process provenance remain caller proof obligations; privileged
integration, termination revocation and target tests remain separate gates.

## Experimental diagnostic-handle gate

handle-gate-design.md is the exact issue #46 contract. Two single-use process
incarnations have distinct fixed nonreused tags and kernel-derived frame owners.
Two1056-byte tables, two72-byte counter records and readiness occupy2264 bytes of
the existing metadata padding; stack/guard/section boundaries do not move. Trusted
boot preparation grants only immutable diagnostic object1, never a user-selected
object or pointer. No dynamic incarnation/tag allocator is implied.

After exact frame/root/active-record proof, INT80 IDs1/2/3 query, attenuate duplicate
and close. RAX is status and RDX is the nonpointer result; every other saved GPR
and permitted user flags are unchanged. Exit0 and unknown-call behavior retain
their prior contracts. All user termination paths revoke/check the exact derived
table under IF0/kernel CR3 before opening drain windows or resuming bootstrap.
Normal timer EOI/stop/retirement ownership is unchanged. Malformed privileged
metadata stops permanently. Returning failures preserve table authority.

This bounded diagnostic interface is not a public ABI freeze, arbitrary resource
capability system, lifetime/refcount mechanism or general hostile-program claim.
Target evidence and remaining emulator/Windows/physical limits are in validation.md.
