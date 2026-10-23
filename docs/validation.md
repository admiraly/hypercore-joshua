# Initial validation record - 22 October 2026

Host: Windows PowerShell 5.1 on the development HP EliteBook 1040 G11.
Tools: NASM 2.16.03; pinned QEMU Windows 2025-12-24, reporting 10.2.0; bundled EDK II.
Artifact identities are in `out/manifest.json`; this document does not embed stale hashes.

Completed locally:

- Assembled all shipped runtime code with warnings treated as errors, except
  documented flat-binary section-crossing relocation diagnostics.
- PE32+ AMD64 format, EFI subsystem, deterministic header, executable/data separation,
  relocation directory, entry-point range, section alignment and virtual/file bounds.
- 20,694 static FAT/ISO assertions per build, including exact EFI extraction,
  directory/cluster integrity and UEFI El Torito embedded-image equivalence.
- Two independent builds produced identical EFI, FAT, ISO and manifest hashes.
- Build from the parent directory with a relative NASM executable path passed.
- Corrupt MZ signature fixture was rejected.
- All host PowerShell scripts parsed under Windows PowerShell 5.1.
- Benchmark record schema parsed as JSON; no benchmark result or performance claim.
- QEMU ISO and raw-FAT UEFI boots passed on the conservative `qemu64` TCG profile.
- Ordered UEFI entry, GOP success, ExitBootServices success and final kernel-halt
  serial markers passed without boot-failure or kernel-fault markers.
- Native framebuffer PNGs passed background, title and final-status pixel checks.
  The ISO screenshot was also visually inspected.
- QEMU register inspection verified ring 0, the halted CPU state, and maskable
  interrupts disabled after the final screen was drawn.

Review identified and implementation addressed firmware watchdog handling,
GDT/IDT selector compatibility during transition, physical-machine UART side
effects, and relative host tool-path resolution. Contracts describe the limits
of stop-only exception handling and inherited paging.

## Owner-reported physical result

On 22 October 2026 the owner confirmed a flawless bare-metal boot through Ventoy
on the reference Dell Latitude 7430, reaching the final checkpoint. The owner
also selected MIT licensing for publication. The test was not observed remotely;
photo, Ventoy version/mode, exact display resolution, verified artifact hash and
repeated cold-boot evidence remain outstanding.

Not completed: repeated Latitude qualification, Secure Boot signing, remote USB debugging,
fault/retry/NMI injection, firmware-menu keyboard automation, source-level crash
debugging, native memory isolation, or any workload benchmark. The CI workflow
is added but has not run on a remote runner. M0/M1 are not declared fully qualified.

## M2 preparation: Linux host evidence, 22 October 2026

This change adds diagnostics and proposed contracts, not owned paging or an
allocator. On Nobara Linux x86-64, PowerShell 7.4.6, locally built NASM 2.16.03
and QEMU 10.2.2 (TCG qemu64):

- `tools/Build.ps1` passed PE validation and all 20,694 media assertions.
- `tests/Test-Reproducibility.ps1` passed two identical builds, relative tool-path
  invocation from outside the checkout, and rejection of a corrupted PE fixture.
- `tools/Test-Qemu.ps1` passed normal ISO and `-BootMedia Fat` boots, ordered
  markers, framebuffer pixel checks, halted ring 0 and IF clear. The normal ISO
  PNG was also visually inspected.
- `tests/Test-FaultDiagnostics.ps1` passed separate ISO fixtures: #UD vector 6
  with synthesized zero error; #GP vector 13 with CPU error 0xFFFC. Both reported
  exact injected instruction RIP, a formatted CR2 field, and halted ring 0/IF clear.
- Independent read-only agent review reported no actionable findings after
  inspecting source, emitted stub encoding and logs. It did not independently
  rerun builds/QEMU.

The local non-Secure-Boot OVMF pair was used through ignored filename symlinks:
code SHA-256 `904bfa3e0d966372b43b804c4fe323ae63751566687c2bfdf52ca947f47eb13a`;
vars-template SHA-256 `6ed987af3a3c155be71665f510eae3e007eda9b8b94afd59d45e91c4a11565cc`.
These are host-specific results, distinct from the earlier pinned Windows setup.
Current Windows execution and remote CI are not claimed here. No new physical
boot evidence was collected. #PF address capture, nested faults, NMI, double
faults and damaged-stack behavior were not exercised. M0/M1/M2 remain open.

## M2a first slice: final map validation

On the same Linux host/toolchain and OVMF pair recorded above:

- `tests/Test-MemoryMap.ps1` passed 24 fixtures calling the shipped Assembly
  validator through a native Linux ELF harness.
- `tools/Build.ps1` passed PE/media checks; `tests/Test-Reproducibility.ps1`
  passed identical artifacts, relative invocation and corrupted-PE rejection.
- Normal ISO and FAT QEMU boots passed, including the new ordered
  `MEMORY_MAP_VALID` marker, framebuffer checks and halted ring-0/IF-clear state.
- Both Ud2/Gp exception fixtures passed again.
- `git diff --check` passed. Independent read-only review found no actionable
  issues; the reviewer did not independently execute the tests.

Linux fixture CI and Windows boot CI are added/configured, not claimed passing
for this slice here. There is no allocator, reservation ledger, active paging
inventory or new physical-machine evidence. This does not complete M2a or M2.

## M2a image reservations and candidate ranges

On the previously recorded Linux host, native NASM 2.16.03, PowerShell 7.4.6,
QEMU 10.2.2 and OVMF pair:

- Build and reproducibility passed with the expanded PE: 20,710 media assertions,
  exact PE section/image bounds, identical rebuilds and corrupted-PE rejection.
- Normal ISO/FAT boots passed; both observed ten candidate ranges after image,
  page-zero and framebuffer exclusions, framebuffer checks and halted ring 0.
- Both exception fixtures and all 24 map-validation fixtures passed again.
- Reservation host suite passed 6,425 oracle checks (6,400 seeded randomized cuts),
  covering range subtraction, image coverage, candidate attributes and failures.
- Design review before runtime edits and subsequent read-only code review found
  no actionable issues. Reviewers did not independently rerun tests.

An integration run using attributes exactly 8 produced zero candidates. The
selection policy was corrected to require WB and accept UC/WC/WT/WB capability
bits only, rejecting protection/runtime/unknown bits; normal QEMU now requires
a nonzero count. No memory is allocated. Active page-table inventory/reservation
is still mandatory before using candidates; M2a remains incomplete. No new
physical test or remote CI result is claimed for this slice in this record.

## M2a active paging inventory and exclusions

On the same recorded Linux/PowerShell 7.4.6/NASM 2.16.03/QEMU 10.2.2/OVMF setup:

- Build and reproducibility passed PE checks and 20,710 media assertions,
  identical artifacts, relative tool invocation and corrupted-PE rejection.
- Normal ISO and FAT boots each inventoried 1,033 table frames, excluded them
  from candidates and reached the intended checkpoint. Captured and final CR3
  both equalled 0x1F401000; framebuffer and halted ring-0/IF-clear checks passed.
- Paging host suite passed 250 checks, including 100 seeded shared-table graphs
  and exclusion comparisons. Snapshots verified no writes to synthetic tables.
- All 6,425 reservation-oracle checks, 24 map fixtures and Ud2/Gp boot fixtures
  passed again. `git diff --check` passed.
- Independent design and source reviews found no actionable issues. Reviewers
  did not rerun tests; source review covered ABI, masks, traversal and exclusions.

The initial 4,096-frame proposal was expanded to 16,384 after observing the OVMF
count and sizing for a possible 16-GiB/4-KiB mapping. This sizing is not hardware
qualification. Full 16,384-frame exhaustion and explicit register-sentinel tests
were not run; small-capacity exhaustion and source ABI review were performed.
LA57, recursive mappings and unreadable firmware tables remain unsupported. No
allocator, CR3 switch, paging ownership, process isolation or new physical-machine
evidence is claimed. CI results for this slice are pending publication.

## M2b ownership-tracked frame allocation

On the same Linux/PowerShell 7.4.6/NASM 2.16.03/QEMU 10.2.2/OVMF setup:

- Frame host suite passed 6,297 ownership/oracle checks including 6,000 seeded
  mixed operations, physical and metadata exhaustion, malformed initialization,
  immutable failures, zeroing, coalescing and reserved-gap checks. Distinct
  register sentinels verified eight integer nonvolatile registers around alloc/free.
- Build and reproducibility passed exact PE/image checks, 20,710 media assertions,
  identical artifacts, relative invocation and corrupted-PE rejection.
- Normal ISO and FAT boots passed the allocator checkpoint after paging exclusions:
  two owners, one- and three-page allocations, 16-KiB alignment, all-qword zero
  checks, pattern writes/reads, wrong-owner/double-free rejection and restored
  free/total counters. Captured/final CR3 remained unchanged.
- All 6,425 reservation, 250 paging and 24 map checks plus Ud2/Gp boots passed again.
- Independent design review found no actionable issues. Source review found no
  runtime issues and identified two stale testing-document statements about no
  allocation; both were corrected to describe the new smoke check. Reviewers did
  not independently execute tests.

The initial randomized fixture count varied with host ASLR alignment; mapping
pools at a consistent 64-KiB alignment made seeded outcomes repeatable. This was
a host-test determinism adjustment, not a runtime workaround. Full 4,096-live-record
load, SIMD sentinels, SMP, fault recovery and new physical qualification are not
claimed. This is a bounded BSP allocator under inherited paging; M2 and process
isolation remain incomplete. Remote CI for this slice is pending publication.

## M2c initial owned paging transition

On the same recorded Linux host/toolchain/QEMU 10.2.2/OVMF pair:

- `Test-OwnedMap.ps1` passed 1,061 checks, independently decoding every owned
  table/leaf for tracked-frame ownership, identity, supervisor/W^X flags, PAT
  encoding, chunks/fallback and poisoned failures. Constant and mixed inherited
  4-KiB selectors did not promote across old leaf boundaries; large leaves were
  retained/subdivided only. The host suite does not execute privileged activation.
- Normal ISO and FAT boots passed with captured CR3 0x1F401000 and distinct owned
  CR3 0x1000. CPU inspection verified WP/NXE, disabled PCID/global policy and the
  halted ring-0/IF-clear state. Framebuffer pixel checks passed; the normal ISO
  PNG was visually inspected with the owned-paging status line.
- Five isolated exception images passed: Ud2/Gp normalization before memory
  bring-up and post-switch #PF text write (error3), NX execution (error0x11),
  guard read (error0). Each #PF matched exact expected RIP and CR2; the NX target
  was allocated/written under the owned root.
- Build/reproducibility passed exact PE checks, 20,710 media assertions, identical
  artifacts, relative invocation and corrupted-PE rejection. All prior frame
  (6,297), reservation (6,425), paging (250) and map (24) suites passed again.
- Independent design reviews initially blocked forced cache-type conversion and
  unproven leaf promotion. Responses preserve inherited PAT selectors and unchanged
  MTRR effects, prohibit promotion across old boundaries, and verify whole-chunk
  data/walk cache compatibility. Follow-up design review resolved the P1.
- Implementation review found no actionable runtime issues and a stale CR3 evidence
  contract. That contract was corrected; follow-up review resolved it without new
  findings. Reviews were read-only and did not independently rerun tests.

No effective-WB claim follows from PAT selection alone. No physical qualification,
initial-PCID-enabled firmware, unusual PAT/MTRR layouts, SMP, TSS/IST, damaged-stack
recovery or process isolation is established. Immutable assets still share RW/NX
data storage; dedicated read-only constants and mapping lifecycle remain open.
Old firmware memory/tables remain reserved. CI for this revision is pending.

## Dedicated immutable storage — 23 October 2026

Issue #20 adds a dedicated `.rodata` PE page with exact RO/non-executable flags,
separate from writable boot buffers, descriptor state and memory metadata. The
owned policy and whole-image proof enforce RO/NX for constants. Boot-info layouts,
inherited cache selectors and inherited-leaf boundaries remain unchanged.

On Linux PowerShell 7.4.6/NASM 2.16.03/QEMU 10.2.2 with the same OVMF profile:

- Build and two-build reproducibility passed, including all 20,718 media assertions
  and identical EFI/FAT/ISO/manifest hashes; corrupted PE rejection passed.
- Normal ISO and FAT boots reached the intended halt with owned CR3, WP/NXE,
  guarded stack and the existing framebuffer checks.
- Host suites passed: 6,425 reservation, 1,061 owned-map, 6,297 allocator,
  250 paging-inventory and 24 memory-map checks.

All six controlled faults passed: Ud2, Gp, code write, NX execution, stack guard
and constant write. ConstWrite reported #PF/error 3 with exact RIP and CR2.
Independent design and implementation reviews found no actionable findings.
Reviews were read-only and did not independently reproduce test results.
This slice does not establish mapping lifecycle, process isolation,
fault recovery, SMP or physical qualification. Windows execution remains CI work.

## Active kernel mapping lifecycle — 23 October 2026

Issue #22 adds a bounded 2-MiB high-half arena with 64 exact virtual records and
allocator-owned physical backing. Tables are prepared before owned-root activation
and included in whole-chunk proof. RW/NX aliases preserve physical cache selectors.
Release clears the full run and invalidates all former VAs before physical free;
internal inconsistency permanently quarantines ownership and disables the context.

Tests actually run with Linux PowerShell 7.4.6, native NASM 2.16.03 and QEMU 10.2.2,
using the same OVMF firmware profile as the owned paging evidence above:

- Build and two independent reproducibility builds passed all 20,718 media
  assertions, identical EFI/FAT/ISO/manifest hashes and corrupted-PE rejection.
- Normal ISO/FAT boots passed the real high-half write/read, invalid-release,
  retirement, reallocation/zeroing and accounting-restoration smoke. All arena
  leaves were absent at its completion; both logs emitted MAPPING_LIFECYCLE_OK.
- All seven controlled faults passed: Ud2, Gp, code write, NX, guard, constant write
  and released-arena read. The last reported #PF vector 14/error 0, exact instruction
  RIP and CR2 FFFF800000000000, then halted ring 0 with IF clear.
- The lifecycle host oracle passed 6,249 operations including 6,000 seeded mixed
  operations, ownership/zeroing, ordinary failure atomicity, capacity/exhaustion,
  all PAT selectors, A/D updates, unchanged boot tables, quarantine and eight
  integer-register ABI sentinels. The host logs invalidation ordering; target
  privileged checks and INVLPG are exercised by QEMU.
- Existing suites passed: 24 map, 6,425 reservation, 250 inventory, 6,297 allocator
  and 1,061 owned-map checks.
- Independent design and implementation reviews found no actionable findings.
  Reviews were read-only and did not independently run the reported tests.

No table reclamation, general virtual manager, cross-core shootdown, user isolation,
fault recovery, physical qualification or performance claim follows. Low identity
aliases and old firmware reservations remain. Windows execution is left to CI for
this revision. M2 remains incomplete; this is the initial bounded lifecycle stage.

## Owned TSS and guarded emergency stacks — 23 October 2026

Issue #24 appends an owned 64-bit TSS descriptor to the preserved firmware GDT,
loads/verifies TR before the owned IDT, and assigns separate DF/NMI/MC/ordinary IST
stacks. Four new lower guards join the normal stack guard. The owned mapping proof
checks all five guards and image permissions; boot-info layouts are unchanged.
Stop handlers stay below the hardware IST frame and log frame/interrupted RSP.

Actually run on Linux PowerShell 7.4.6, native NASM 2.16.03, QEMU 10.2.2 and the
same OVMF profile as prior memory validation:

- Build and two-build reproducibility passed all 20,718 media assertions, identical
  EFI/FAT/ISO/manifest hashes and corrupted-PE rejection.
- Normal ISO/FAT boots passed TR selector/base/limit and four distinct stack-bound
  checks, memory/lifecycle checkpoints and framebuffer checks. The runtime checks
  the descriptor's busy type after LTR; QEMU's cached TR attributes display avl.
- All nine fault fixtures passed. Ordinary normalized frames landed at IST4 top-56.
  StackFault reported exact #PF/error0/RIP/CR2 and interrupted RSP despite a normal
  stack at its guard boundary. DoubleFault reported vector8/error0 on IST1, with
  CR2 in the deliberately invalid ordinary-stack guard. Saved #DF RIP was logged
  without a restart assertion. Every run halted ring0 with IF clear.
- 1,470 host descriptor/TSS/gate/guard checks passed, including GDT rejection before
  writes, maximum size, full-width TSS base, reserved bytes, all gates, randomized
  guard boundaries and eight integer nonvolatile sentinels across all four helpers.
- Regression suites passed: 24 map, 6,425 reservation, 250 inventory, 6,297 allocator,
  1,061 owned-map and 6,249 lifecycle checks.
- Independent design and implementation reviews found no actionable findings.
  Reviews were source-only and did not independently reproduce reported tests.

These are controlled QEMU/host results. No NMI or machine-check injection, arbitrary
nesting/reentrancy, early-transition fault containment, recovery, process isolation,
SMP or physical qualification is established. IF remains clear. The memory exit
audit keeps M2 open, and keyboard/shell functionality remains future work. Windows
execution for this revision is left to CI; no merge was performed.

## Bounded ACPI topology discovery — 23 October 2026

Issue #26 validates ACPI2+ RSDP/XSDT/MADT through an explicit retained-ACPI-memory
and owned-identity translation boundary before any firmware byte reads. It publishes
bounded normalized active CPU records and requires an enabled BSP matching CPUID,
with a unique optional local-APIC base override. No controller is programmed.
RX capacity and static CPU-record storage grow with exact PE/reservation fixtures;
boot-info layouts, cache policy and firmware reservations remain unchanged.

Actually run on Linux PowerShell7.4.6, native NASM2.16.03, QEMU10.2.2 and the same
OVMF profile used above:

- Build/two-build reproducibility passed 20,734 media assertions, identical EFI/
  FAT/ISO/manifest hashes, relative assembler invocation and corrupted-PE rejection.
- Normal ISO/FAT boots passed real OVMF topology discovery: BSP0, two enabled CPU
  records, local APIC base FEE00000, plus existing TR/IST/memory/lifecycle/pixel checks.
- 1,131 ACPI host cases passed, including 500 seeded normalized CPU-list oracles,
  predereference nonexistent-pointer rejection, adjacent coverage, RP/type/identity/
  U/S failures, both RSDP and description-table checksum failures, malformed lengths,
  capacities/duplicates/BSP/overrides, a 64-KiB MADT and eight integer ABI sentinels.
  Snapshots asserted no firmware source writes and invalidated failed publication.
- Existing suites passed: 24 map, 6,425 reservation, 250 inventory, 6,297 allocator,
  1,061 owned-map, 6,249 lifecycle and 1,470 descriptor/TSS/guard cases.
- Independent design/code reviews found no actionable findings. Reviews were source-
  only and did not independently reproduce parent test results. CPUID startup is
  exercised by QEMU, not the parser/read-predicate host harness.

All nine final fault regressions passed with selected IST/frame evidence, exact
ordinary fault fields and controlled double-fault guard evidence. Unknown child
payloads/checksums are neither interpreted nor certified. IOAPIC/ISO/NMI routing,
APIC mode/MSR/MMIO/cache setup, AP startup, interrupts/timer delivery, physical ACPI
compatibility, isolation and performance remain unqualified. IF stays clear; all
ACPI memory remains reserved. Windows execution is left to CI; no merge performed.

## BSP APIC timer foundation — 23 October 2026

Issue #28 extends bounded discovery to IOAPIC records and preallocates a dedicated
owned controller window. Read-only PAT/MTRR capture proves effective strict UC;
explicit ACPI/CPU authority covers controller pages omitted by the tested OVMF map.
Controller preflight masks supported external sources while preserving SMI/NMI,
then sixteen one-shot BSP timer deliveries return with EOI/rearm/IRETQ. No scheduler,
AP startup or process isolation is introduced.

Actually run on Linux PowerShell7.4.6, native NASM2.16.03, QEMU10.2.2 and the same
OVMF code/vars pair documented above:

- Build passed20,734 media assertions. Two-build reproducibility passed with identical
  EFI/FAT/ISO/manifest hashes, relative NASM invocation and corrupt-PE rejection.
- Normal xAPIC ISO/FAT passed with two UC controller aliases, one IOAPIC and exactly
  sixteen ticks/EOIs/returns; owned CR3, permissions, TR/IST/guards, framebuffer and
  final halted ring0/IF-clear assertions passed. The screenshot was inspected.
- X2 fixtures passed ISO/FAT using `qemu64,+x2apic`, one UC IOAPIC alias and exactly
  sixteen ticks/EOIs/returns. This executes the x2APIC MSR adapter and IRETQ path.
- MissingEoi was rejected at the bounded15-second timer checkpoint timeout, after
  APIC_READY and without TIMER_OK or a fault marker. A negative rejection is not
  a successful boot or an independent runtime timeout mechanism.
- Host suites passed:24 map,6,425 reservations,250 inventory,6,297 frames,1,061
  owned-map,6,249 lifecycle,1,470 descriptor/TSS/guards and1,145 ACPI cases.
- New effective-UC oracle passed6,271 cases including6,000 seeded combinations,
  maximum255 pairs, malformed active masks, overlap permutations and PAT WC rejection.
  APIC policy/staging passed1,364 cases with a modeled controller backend and real
  host page tables. All254 unsupported IOAPIC version bytes fail before route reads
  or configuration writes; supported11h/20h, masking, SMI/NMI preservation, failures,
  authority/descriptor/ownership/cache rejection and failure atomicity are covered.
- Independent design/cache/runtime reviews were read-only. Runtime review found a
  missing IOAPIC version check; it was fixed and re-reviewed with no remaining
  actionable findings. Reviewers did not independently reproduce parent test runs.

Hardware compatibility, NMI/MC resilience, unusual firmware/cache layouts, calibrated
frequency, SIMD context preservation, SMP and isolation remain unqualified. Windows
execution is left to CI; no merge performed. Full milestone exits remain open.

All nine final fault regressions passed after the version fix: Ud2, Gp, ReadOnly,
Nx, Guard, ConstWrite, Unmapped, StackFault and DoubleFault. Each image passed media
checks and selected IST/frame assertions; ordinary fault vector/error/RIP/CR2 and
controlled double-fault guard evidence matched, with final ring0 and IF clear.

## Timer-driven guarded kernel contexts — 23 October 2026

Issue #30 parks the bootstrap continuation and alternates two trusted Assembly
workers on separate16-KiB stacks for64 real BSP timer deliveries, then restores the
bootstrap frame. Each worker observes32 interruptions and publishes actual progress
while checking distinct15-GPR/RSP/DF sentinels. This is a bounded switching mechanism,
not a general scheduler or isolated process implementation. All seven stack guards
are absent; metadata/stacks remain image-owned and boot-info layouts are unchanged.

Actually run on Linux PowerShell7.4.6, native NASM2.16.03, QEMU10.2.2 and the same
OVMF pair documented above:

- Build passed20,742 static media assertions. Two-build EFI/FAT/ISO/manifest hashes
  matched, including relative assembler invocation and corrupted-PE rejection.
- Normal xAPIC ISO/FAT and x2APIC ISO/FAT completed64 switching ticks/EOIs/restores,
 32 per-worker interrupts and32 progress increments each, then resumed bootstrap,
  stopped the timer, checked empty ISR/IRR and halted ring0/IF0 with owned CR3.
  The existing16-delivery APIC self-test remains independently checked first.
- QMP reads of actual owned page tables checked all seven absent guards and every
  stack page's supervisor RW/NX permissions. A review found that the first checker
  ignored read-only ancestors; it now requires RW at every traversed level, with
 15 fixtures extracting the actual PowerShell function and mutating RW/US/presence
  at all four levels, plus large leaves. Four boot profiles reran with the fix.
- InheritedId confirmed ID was set before explicit bootstrap RFLAGS normalization
  and then completed switching. BadFrame stopped with state/ticks/EOIs/current all
  zero before invalid return/publication. WorkerGuard reported exact #PF/error0/
  RIP/guard CR2 on IST4. These are separate fixture images, not normal artifacts.
- All nine existing fault fixtures passed after context switching, including their
  applicable exact vector/error/RIP/CR2 and IST/frame assertions. MissingEoi remained
  rejected at the bounded timer checkpoint, rather than being counted as a boot pass.
- Host suites passed:24 map,6,425 reservation,250 inventory,6,297 frames,1,061
  owned-map,6,249 lifecycle,1,470 descriptor/guard,1,145 ACPI,6,271 cache and1,364
  APIC policy/staging checks. The new exact Assembly context suite passed8,914
  frame/schedule/ABI checks, including2,000 seeded interrupted-frame variations,
 100 full64-tick reference schedules, failure-state snapshots and eight integer
  nonvolatile sentinels. Host tests do not execute the privileged adapter or IRETQ.

Independent design review required bootstrap flag normalization; it was addressed
and re-reviewed. Target bring-up also exposed an indexed absolute-RVA store, fixed
with a runtime LEA base, and a worker interrupt frame with RF set. Worker frames now
preserve architectural RF while bootstrap RF remains forbidden, consistent with the
cited SDM contract. Full runtime review found no actionable findings; final evidence
review's ancestor-RW finding was fixed and the follow-up found no remaining issues.
Reviews were source-only and did not independently reproduce parent test results.

Windows execution awaits CI. FPU/SIMD contexts, arbitrary kernel task APIs, fairness,
AP/SMP, user isolation, physical qualification and complete milestone exits remain
open. CET is rejected for this conventional context path; no security/performance
parity claims are made. No merge performed.


## Draft private-context transition evidence (issue #32)

23 October2026, Linux PowerShell7.4.6/NASM2.16.03/QEMU10.2.2/OVMF: Build passes20,766
static media assertions. Sixteen TCG/qemu64 ISO profiles pass the independent exact
serial/PE/listing oracle for real CPL3 roots/CS, fault identity or bounded timeout,
and actual trusted bootstrap/processB continuation. Payloads check zeroed entry
registers and preserved non-result GPRs/RSP/DF across unknown INT80 return.
Normal FAT and actual x2APIC ISO pass; two-build EFI/FAT/ISO/manifest reproducibility
passes after the current fixture/harness wiring. Whole-issue requalification remains
due after the remaining runtime changes.

On this host's Intel i7-14700K via explicit KVM/host profiles, actual #AC17/error0,
#SS12/error0, SCE-disabled SYSCALL #UD6 and null-CS SYSENTER #GP13/error0 all pass the
same precise fault and independent B oracle. This is hardware-assisted virtualized
evidence, not physical HyperCore/Dell qualification. TCG Alignment did not deliver
#AC and TCG noncanonical Stack delivered #GP, so those exact profiles are failures
and excluded from TCG passing counts. They are not relabeled successful containment.

Descriptor retirement adds host bounds/suffix/ABI cases (2,778 cumulative total).
Independent design/code review cleared retirement after proof of every live owned
selector/IDT authority. Transition source review found P2 fixture output collision,
fixed by isolated default UserTest directories; follow-up review found no actionable
findings. Reviews did not execute tests. Full pending-timer/drain negatives,
independent QMP private-root/ownership walks, full host/fault regressions and final
review remain open; neither issue #32 nor a milestone is complete.

### Issue #32 final qualification delta

Forced pending exit/fault now pass local exact oracles with one drain EOI and B
continuation. Drain exhaustion and an unexpected CPL0 timer stop before EOI/B.
Target authority fixtures reject injected callable code/live DS, clear an inherited
owned LDT before a user #GP(4), and preserve permanent stop for a kernel-origin #UD
inside INT80. All four authority targets pass locally. Independent source reviews
found no remaining actionable findings; they did not execute tests.

Physical QMP snapshots pass independent private-root/backing/permission/cache and
whole paging-allocation alias checks. The actual decoder rejects47 corrupted
snapshots and49 synthetic reserved/cache-policy mutations. Review findings about
parent PWT/PCD and large-leaf reserved bits were explicitly fixed and re-reviewed.

Windows CI at bd5ecb5 exposed a DoubleFault parser collision with earlier user-result
CR2 fields. The parser now selects the standalone vector8/error0 report; the local
DoubleFault target passes with CR2 inside the intended ordinary-stack guard. Fault
logs are now uploaded by CI. The final28-profile campaign, complete regressions and
new-head Windows CI are still running/pending at this checkpoint. No planned test
is counted as passing, and no issue or milestone completion is claimed here.

### Final local campaign on runtime commit 859225e

The final separate24-profile TCG campaign passes, including19 successful contained/
exit paths and five intentional kernel-stop cases. All four Intel KVM profiles
pass exact fault and private-paging oracles. An initial KVM launch exited before
JOSHUA entry; its #AC retry passed. This failed launch is retained as a reliability
limit, not counted as successful execution. TCG #SS/#AC exact-vector failures from
the earlier checkpoint remain excluded; KVM evidence does not erase them.

All15 host suites, nine fault fixtures, x2APIC ISO/FAT plus missing-EOI rejection,
context launch/bad-frame/worker-guard regressions, normal ISO/FAT, and two-build
EFI/FAT/ISO/manifest reproducibility pass on the final runtime source. The corrupted
PE rejection check passes. One extra ad-hoc x2 expectation against the normal
xAPIC image was correctly rejected; actual x2 evidence uses the X2 fixture in
Test-ApicTimer.ps1. No product change was made for that invocation mistake.

Independent final source review found no actionable findings, including the #DF
parser correction, campaign wiring, private-paging oracle and documentation. Reviews
were read-only and did not independently reproduce the parent-run evidence. Windows
CI on the final PR head remains pending. PR #35 stays draft; issue #32 and broader
milestone/physical/speculative/SMP qualification are not declared complete.

## Embedded ELF loading bring-up (issue #36)

Linux PowerShell7.4.6/NASM2.16.03/QEMU10.2.2/OVMF: parser/copy host suite passes6,198
cumulative checks. Reviewed design fixed the bootstrap reservation at128 bytes;
reviewed alias fixtures now reach actual overlap checks with page-aligned mappings.
No runtime defect remained in the component reviews, which were source-only.

The current ELF-backed ISO/FAT boot pair passes actual two-process entry beyond an
INT3 prefix, initialized-data/BSS checks and the private-paging oracle. Build passes
20,790 media assertions. BadMagic/BadEntry targets reject before private allocation,
root publication or CPL3; QMP allocation count stays unchanged and CPU halts CPL0/IF0.
The count check is not a full ledger snapshot comparison. A prior reservation test
failed because its synthetic loader coverage retained the old image size; C6 pages
now cover the larger image and the corrected suite passes6,425 checks.

Full current-source user/KVM, host/fault/APIC/context, snapshot-mutation and
reproducibility regressions are running or pending. No CI, issue or milestone
completion is claimed by this checkpoint. Reviews do not independently reproduce
parent-run tests. Physical/speculative/general-ELF/SMP qualification stays open.

### Issue #36 final local runtime b5f5dbd

The complete24-profile TCG campaign passes. All15 preexisting host suites plus the
6,198-check ELF suite pass; nine kernel faults, x2APIC ISO/FAT and missing-EOI,
context launch/rejection/guard, normal ISO/FAT, two-build reproducibility and corrupt
PE rejection pass. The paging oracle rejects50 snapshot mutations and49 synthetic
level-policy mutations. No earlier planned test is counted as a result here.

All four Intel KVM instruction cases now have current ELF-backed execution evidence:
#AC, #SS and disabled SYSCALL pass ISO; null-CS SYSENTER passes FAT with #GP(0) and
B continuation. KVM ISO attempts can stall before JOSHUA entry: the first #AC attempt
timed out, and SYSENTER ISO still has a failed180-second firmware attempt. The FAT
result does not convert that failed ISO launch into a pass; the complete default
IncludeKvm ISO command remains unqualified at this checkpoint. TCG's earlier exact
SS/AC vector failures remain explicit. This is virtualized evidence, not physical
HyperCore compatibility or a general ELF/security qualification.

Final independent source review found no new actionable findings across loader,
integration, negative targets, contracts, independent oracles and CI. Source-only
reviewers did not independently reproduce parent-run evidence. PR #37 remains draft
for final-head CI and the stated KVM ISO reliability limit. No issue/milestone exit
or merge is claimed.

A subsequent isolated SYSENTER ISO retry also passes exact #GP(0), B continuation
and the independent physical oracle. Thus all four KVM cases have current ISO
execution evidence, while the failed first launches and unqualified one-shot full
IncludeKvm command remain reliability limits. No source change was used to obtain
that retry result; neither firmware timeout is reclassified as success.

## Keyboard prepared checkpoint (issue #38)

Linux PowerShell7.4.6/NASM2.16.03/QEMU10.2.2/OVMF: actual controller preparation
reaches GSI1/config65h with scanning disabled and the IOAPIC entry unchanged/masked.
Normal ISO and FAT pass existing private ELF continuation/protection checks plus
independent capture of fresh controller/authority/decoder metadata. Build passes
20,790 media assertions; EFI/FAT/ISO/manifest reproducibility and corrupt-PE rejection
pass. The actual snapshot oracle rejects54 corruptions and49 synthetic level-policy
mutations. There is no real keyboard IRQ/delivery evidence at this checkpoint.

Host policy suites pass180,744 decoder assertions (40,000 mixed operations),1,890
ACPI/ISA/GSI assertions (500 generated maps) and503 modeled-port assertions. Independent
reviews found no remaining actionable findings in design, decoder, authority,
controller policy, masked preparation or target evidence checks. Reviews were source
only and did not independently reproduce parent-run tests. Authority review's coverage
fixture now places a readable FADT header before an unproved PROT_NONE payload page.
Design findings about command/input races, deadline retirement and exact IRQ frames
are resolved in the contract; the live implementation and its required tests remain open.

No issue/milestone, complete keyboard driver, interactive shell, USB or physical
compatibility claim follows. Speech branches/artifacts remain separate and unchanged.
