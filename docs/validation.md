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


## Keyboard exact-frame policy checkpoint (issue #38)

The actual Assembly frame checker passes4,694 cumulative host assertions with
integer ABI sentinels, exact active/drain continuations, root/selector/flags/old-RSP
mutations, checked helper-reserve bounds and a protected unreadable proposed frame.
It is assembled into the production image and registered in the manifest; Linux CI
runs the host suite. There is still no live adapter call site or IRQ44 gate.

Current-source Linux PowerShell7.4.6/NASM2.16.03 build passes20,790 media assertions;
two builds reproduce EFI/FAT/ISO/manifest hashes and reject corrupt PE. Normal ISO
and FAT QEMU10.2.2/OVMF boots both pass prepared/scanning-off keyboard capture and
existing private ELF/root/ownership checks. Independent source reviews cleared the
helper and its build/CI/documentation integration without running tests. Windows CI,
actual IRQ/IRETQ behavior, nested helper-stack accounting, input-window timer/drain
retirement and complete live-stage regressions remain open. No real key delivery,
complete driver, shell, physical compatibility or milestone completion is claimed.


## Experimental live keyboard window (issue #38, incomplete qualification)

Linux PowerShell7.4.6/NASM2.16.03/QEMU10.2.2/OVMF: current-source Text, Df and
Missing each pass ISO/FAT; Text additionally passes forced x2APIC ISO/FAT with
actual mode2 evidence. Text/Df snapshots contain ordered events97,65,66,3,256,8,13
(`a`, `A`, `B`, Ctrl-C, Left, Backspace, Enter),23 actual keyboard IRQs with23 EOIs,
and60–61 timer deliveries/EOIs. Both Missing snapshots have no input, exactly16
timer deliveries/EOIs and terminal deadline status1. All eight windows finish phase3
with controller stage11/scanning off, valid empty queue indices, the physical
IOAPIC low entry masked/vector44 and actual LAPIC masked/vector40, zero timer counts
and empty ISR/IRR. Active DF fixtures capture402h after CLI; all15 GPR sentinels are
checked immediately after each active IRETQ. These passes exercise real emulated
PS2/IOAPIC/IRQ input, not direct host calls into the decoder. None exercised a
nonempty drain window; forced pending-delivery tests remain mandatory.

Two early strict-frame Missing invocations stopped before any timer EOI because
TCG saved RFLAGS10202h (architectural RF), despite matching root/location/selectors/
oldRSP/RIP. They remain failed invocations. Reviewed refinement permits only RF in
addition to the exact hardware flags, preserves it for IRETQ, prohibits RF in
metadata and normalizes/checks metadata202h before drain. New host frame fixtures
pass5,996 assertions across RF absent/present and all other flag-bit corruptions.

Decoder180,744, authority1,890 and modeled-controller503 host assertions pass again.
Shared actual-target LAPIC/queue predicates pass8,338 cumulative mutation checks.
Normal images still stop at masked preparation: current normal ISO/FAT private-root,
ELF and keyboard-prepared snapshot checks pass. Build/media20,790 assertions,
normal two-build EFI/FAT/ISO/manifest reproducibility and corrupt-PE rejection pass.

Independent source review found no additional runtime defect, with maximum200-byte
helper depth below the saved frame. Findings were addressed: terminal pendingIRQ44
must not recollect/disable twice or allow deadline overwrite; strict drain metadata;
independent actual LAPIC retirement; queue-index invariants; immediate Windows
native-Python CI exit checking; normal-versus-fixture documentation. Reviewers ran
no tests. Windows CI and complete current-source regressions remain pending.
Overflow/reentry, forced pending40/44/both, drain exhaustion and invalid/out-of-window
frame target gates remain open. No complete driver, shell, physical compatibility,
issue/milestone completion or merge is claimed. Speech work remains separate.


## Keyboard lifecycle and rejection campaign (issue #38)

The final frozen-source `Test-KeyboardCampaign.ps1 -IncludeX2` run passes40 actual
QEMU10.2.2/OVMF target invocations:17 profiles each on ISO/FAT, plus Text,
PendingBoth and OverflowPending on x2APIC ISO/FAT. This is18 successful-window
checks and22 exact permanent-stop checks, not40 independent hardware platforms.
Pending40/44/both prove actual IRR1h/10h/11h before normal source retirement and
nonempty drain returns with exact EOIs/no active ticks. DrainExhaust stops at1025
attempts before any EOI with IRR64 retained. Six frame/address/root defects and
both out-of-window vectors stop with exact saved state/current CR3 and actual ISR
ownership, without EOI or normal continuation. An arbitrary fault cannot pass.

Overflow snapshots all64 old sentinel events and valid metadata before real input,
then requires one dropped make with unchanged old storage/indices/count and no
consumer. Controlled OverflowPending requires exactly32 actual data-port reads,
16 decoded `a` events, post-baseline actual IRQ44 reentry/EOIs, one emergency disable,
a timer EOI observing failure2 with zero ticks, then quiet output/masked route/
stopped timer/empty ISR+IRR and phase3/failure2 permanent stop. All four xAPIC/x2APIC
ISO/FAT cases pass with sticky timeout0, both actual-OBF refill acknowledgments and
actual OBF recorded at zero budget. WINDOW_DONE/normal HALTED are forbidden there.

The earlier62-event single-batch attempt failed: only16 collector bytes/eight make
events arrived, no emergency disable occurred, and the512-tick deadline fired.
It remains failure evidence. Mode17 alone now uses two finite producer-refill waits
after reads16/32, with host injection only after physically observing each threshold.
The normal32-read budget is unchanged, and no data/OBF/decoder outcome is synthesized.
This qualifies controlled exhaustion/reentry branches, not natural input rates or
production ISR latency. A source-review P2 second-refill-timeout false pass is closed
by sticky timeout, real-OBF acknowledgments and zero-budget status. The actual port
response must match the full byte-port64 response, rejecting unsupported-command
errors that contain the address. Shared actual-oracle mutations pass8,373 checks.

Regression runs pass24 user execution/fault/watchdog profiles, nine fault fixtures,
x2APIC ISO/FAT plus missing-EOI rejection, context launch/rejection/guard fixtures,
and both malformed-ELF targets. Twenty host suites pass, including keyboard decoder
180,744, authority1,890, controller503, exact-frame5,996 and15 host stack-walker
fixtures. The existing physical snapshot oracle rejects54 mutations plus49 synthetic
reserved/cache-policy cases. Normal ISO/FAT and stable normal EFI/FAT/ISO/manifest
reproducibility/corrupt-PE rejection pass. All61 tested boot variants were rebuilt
against final runtime source with identical EFI/FAT/ISO hashes; a subsequent frozen
40-target run uses the final checker and final source throughout.

Independent read-only reviews cleared pending/rejection code and overflow/refill
logic after the recorded findings were addressed. Normal helper depth remains200
bytes maximum; the instrumented refill branch consumes120 (decoder136). Reviewers
ran no tests. Documentation now distinguishes window phase3 from fixture stage3,
armed fixture stage2 from window phase1, and the final8,373 mutation count from
historical counts. Windows CI and physical Latitude/input qualification remain open.
No issue/milestone completion, full input-stack/shell claim or merge follows. Issue
#40 owns the next native WOPR/Joshua console; speech branches remain unchanged.

## Console editor checkpoint (23 October 2026)

Issue #40, `feature/console-shell`, based on `32a0bbe`: the reviewed pure
Assembly editor is host-tested but has no boot call site. The independent model
and actual Assembly ABI harness passed **344,659 cumulative checks**, including
all 65,536 input tokens, 20,000 seeded mixed events, fixed-buffer guard pages,
corruption rejection, history, submitted-buffer lifetime and counter saturation.
This count includes repeated state/result assertions, not independent scenarios.
Independent read-only source review found no actionable findings and did not
reproduce the test run. The prior three editor design findings were resolved
before implementation (layout/invariants, no-op/browsing semantics, result ABI).

`tools/Build.ps1` passed all 20,790 static media assertions;
`tests/Test-Reproducibility.ps1` produced identical EFI/FAT/ISO/manifest hashes
and rejected the corrupt PE. Normal `tools/Test-Qemu.ps1` ISO and `-BootMedia Fat`
both passed with private-root/ownership evidence and the intended halted ring0
checkpoint. These image runs exercise the existing boot path, not the unused
editor. Linux CI includes the new editor suite but has not yet reported results.
No interactive console, renderer, parser, IRQ lifecycle, Windows or physical
qualification is established by this checkpoint. No merge or milestone completion.

## Checked console renderer checkpoint (23 October 2026)

Issue #40/PR #41 extends the host-only console component validation with five
Assembly grid helpers and an original95-character font. No boot call site or
interactive console is added at this checkpoint. `tests/Test-ConsoleGrid.ps1`
passed **5,055 cumulative checks**, including complete actual-framebuffer byte
comparisons for RGB/BGR surfaces, margins and stride padding, dirty rows/cursor,
all printable glyph indices, min/clamped geometry, invalid state/anchor values,
overflow/direct-overlap rejection and inaccessible guard pages around framebuffer,
grid and anchor. Pixel addressing/colors are independently modeled; font rasters
reuse the authored font asset with two literal checks, not an independent font
oracle. The actual Assembly-rendered host preview was visually inspected at
`out/console-grid-tests/console-grid-preview.png`; it is not a boot screenshot.
The editor regression passed344,659 cumulative checks again.

Independent renderer design review found P2 gaps in alias exclusion and success
returns; both were corrected and re-reviewed before implementation. Independent
read-only implementation and final guard/preview/CI reviews found no actionable
findings. Reviewers did not reproduce tests or inspect output artifacts.

`tests/Test-Reproducibility.ps1` invoked two successful `tools/Build.ps1` builds,
each passing20,790 static assertions; EFI/FAT/ISO/manifest hashes matched and the
corrupt PE was rejected. Normal QEMU ISO and FAT runs both passed private-root,
permissions, ownership and actual CPL3/continuation assertions, then halted as
expected. Those images still exercise the existing checkpoint, not the unused
console helpers. Linux CI now includes both component suites but has not reported
results here. Prompt/model/parser/IRQ integration, interactive target campaigns,
Windows and physical qualification remain open. No merge/milestone completion.

## Console command and response checkpoint (23 October 2026)

Issue #40/PR #41 now has pure Assembly command parsing and diagnostic response
components, with no boot call sites yet. `Test-ConsoleCommand.ps1` passed95,317
cumulative checks against an independent grammar model: case permutations,
argument precedence,127-byte boundaries,15,000 seeded mixed inputs, whole-editor
corruption, overlap, guards and eight-register sentinels. A design P2 requiring
an exact result readiness value was fixed and re-reviewed before coding.
Independent read-only parser implementation review found no actionable findings.

`Test-ConsoleResponse.ps1` passed28,687 cumulative checks: independent complete
text/schema/JSON expectations, command-combination validation,1,000 seeded facts
snapshots, unsigned64 extremes, input preservation/zero tails, overlap/corruption,
protected extents and actual internal append capacity/overflow checks. Response
design and implementation reviews found no actionable findings. Static audit
bounds the deepest formatter stack at288 bytes below entry (296 including its
incoming return address); all calls align and provide shadow space. Reviewers
made no edits and did not independently reproduce tests or inspect artifacts.
The editor and grid regressions passed344,659 and5,055 checks respectively.
Counts are cumulative assertions, not independent scenarios.

Current-source reproducibility invoked two builds passing20,790 static assertions
each, with identical EFI/FAT/ISO/manifest hashes and corrupt-PE rejection. Normal
QEMU ISO and FAT runs passed private paging/ownership, actual CPL3/continuation
and the expected ring0 halt. They exercise the existing boot checkpoint, not the
unused console components. Linux CI includes all four console suites; no CI result
is asserted here. No snapshot boot adapter, prompt/transcript model, real keyboard
console lifecycle or interactive ISO is delivered by these host component tests.
Windows, physical qualification, full console target/regression campaigns and
issue/milestone completion remain open. No merge.

## Console screen-model checkpoint (23 October 2026)

Issue #40/PR #41 now composes prompt, transcript and result rows through pure
Assembly helpers, without a boot call site. `Test-ConsoleModel.ps1` passed17,279
cumulative checks against an independent complete-grid/model oracle. It invokes
actual Assembly parse/build/compose helpers, covers viewport positions through127,
three geometries,100 seeded completions each, exact-width/LF/wrap/scroll, clear
ordering, saturation, fresh-parse mismatch rejection, all record overlaps and
inaccessible boundaries around six records. Synthetic4095-byte printable/LF and
empty bodies test defensive composition bounds beyond current response templates;
they do not prove production response provenance. No pixels change in composition.

The actual Assembly renderer produced the visually inspected host preview at
`out/console-model-tests/console-model-preview.png`. Its facts are synthetic and it
is not a boot screenshot or actual keyboard session. Shared `command_check`
extraction leaves response behavior unchanged; editor/grid/parser/response suites
passed344,659/5,055/95,317/28,687 cumulative checks again. Independent design, code
and maximum-body follow-up reviews found no actionable findings. Static maximum
model depth is616 bytes below entry RSP (624 with incoming return address).
Reviewers did not reproduce tests or inspect artifacts. Counts are assertions,
not independent scenarios.

Current-source reproducibility ran two builds passing20,790 static assertions
each, with identical EFI/FAT/ISO/manifest hashes and corrupt-PE rejection. Normal
QEMU ISO and FAT passed private paging/ownership, actual CPL3/continuation and the
intended ring0 halt. Those images do not exercise unused console helpers. Linux
CI now includes the model suite; no CI result is claimed. Boot facts/provenance,
console profile/layout integration, actual IRQ idle/halt, interactive media and
full console target/regression campaigns remain open, as do Windows and physical
qualification. No merge, issue closure or milestone completion.

## First native console test media (23 October 2026)

Issue #40/PR #41 now integrates the owned facts adapter, checked console components
and real keyboard foreground loop in the separate `Build.ps1 -Console` profile.
The current non-installing test image is `out/console/JOSHUA-Console-Test.iso`:
SHA256 `3dcdaed9167b8c94a50c8938c06ae1ccb5e488c18f5887ad85fc4995f48f8369`.
This supersedes the earlier checkpoint's statement that profile/layout integration
and interactive media do not exist. It does not close the wider qualification gates.

Final hardened `tools/Test-Console.ps1` passed xAPIC ISO, xAPIC FAT and x2APIC ISO.
Each uses real QMP device key events for editing, status JSON, history, Ctrl-C and
halt, with30 seconds of timer-free idle. Physical captures check actual CPU root/
RSP/RIP and IF1/HLT wait, records and IRQ/EOI/queue state, then actual stopped LAPIC,
selected keyboard IOAPIC mask and controller output quietness, and IF0/ring0 halt.
Actual QMP screenshot pixels match the constant HALTED footer; the entire prompt
underline area is background and terminal cursor metadata is independently fixed.
Target PNG screenshots were visually inspected. Artifact hashes and fixture flags
are checked before boot; success summaries are published only after clean exit.

The first ad-hoc native boot reached input then faulted on halt-row drawing: two
indexed symbol LEAs encoded absolute image RVAs. Both were corrected to RIP-relative
base plus row offset, with a focused independent review. That failed log was
preserved privately. A subsequent ad-hoc invocation reached native HALTED but its
QMP quit reader failed; neither invocation is counted as a passing smoke command.
Earlier weaker ISO/FAT smoke passes predate the strengthened oracle and are not
substituted for the three final hardened passes above.

Normal and Console reproducibility passed two builds each,20,822 static assertions
per build, identical EFI/FAT/ISO/manifest hashes (plus named Console ISO), and corrupt
PE rejection. Updated reservation fixtures passed6,425 checks. All24 Linux host
suites passed, plus15 stack-walker cases. Normal ISO/FAT passed private-root and
CPL3/continuation checks. Original Text keyboard ISO passed after the shared-macro
refactor. Nine fault fixtures, context launch/rejection/guard, two ELF rejections,
x2APIC ISO/FAT and expected missing-EOI stall regressions passed. Full40-target
keyboard and24-profile TCG user campaigns are still running and are not claimed
as complete here; final batch results will be recorded separately.

Independent design, integration, addressing-fix and oracle follow-ups found no
unresolved runtime findings. Four initial oracle evidence gaps (manifest/media
identity, IOREGSEL, stale/premature summaries and unchecked screenshot) and a cursor
coordinate gap were corrected and re-reviewed. Reviewers inspected source only;
they did not reproduce runs or inspect artifacts. The new Windows CI job uploads
both tested media plus manifest and captures, but Windows execution is pending.
Broader native command breadth, negative console fixtures, saturation targets,
full pixel/snapshot mutation campaigns and physical qualification remain open.
No merge, issue closure or milestone completion is claimed.


### Completed first-integration regression batches

After the preceding checkpoint, the frozen runtime source in23a66e3 completed the
full40-target keyboard campaign:17 xAPIC profiles on ISO/FAT plus Text/PendingBoth/
OverflowPending x2APIC on both media,18 positives and22 expected stops. The script
exited0 and all40 per-target result lines were checked. It includes8,373 shared
oracle mutations. The24-profile TCG user campaign also exited0 with its final exact
oracle summary. Nine faults and the timer/context/ELF batches had already passed.
These preserve the inherited collection/fixture qualifications; controlled refill
is not natural production timing evidence. No new console negative/saturation or
full-feature campaign is inferred from the older keyboard fixtures.

A frozen copy for testing is `out/JOSHUA-Console-Test-23a66e3.iso`, with the same
SHA256 recorded above. All native evidence remains QEMU-only. Native command breadth,
negative console/mutation coverage, Windows CI and physical qualification remain
open. Independent reviews were source-only. No issue/milestone closure or merge.

### Native console command and visible-pixel expansion

Host-only validation expands the unchanged23a66e3 console runtime: `Test-Console.ps1
-Features` submits20 commands through actual QMP PS2 key events. Eighteen added
submissions cover help/about, human status/mem/ps/devices and JSON variants, padded
mixed-case names, unavailable/unknown/usage results, case-sensitive options, rejected
halt arguments, empty input and clear. Each completion checks the exact command,
model result and full bounded response bytes. Dynamic numbers are checked against
the captured facts record; independent source-record provenance remains open.
Capacity checks use127 characters with distinct ends, a rejected insertion, exact
intermediate Home/Delete/End/Backspace states, cancellation and prompt viewport.

The pixel oracle independently rasterizes all active cells and the cursor and
compares the entire visible PPM, including margins. It uses captured grid text and
the authored font: this establishes display/grid consistency, not independent live
transcript provenance, font correctness or GOP padding coverage. Halt/footer and
capacity-prompt expectations additionally constrain visible content. Twenty-four
baseline/mutation checks reject altered grid metadata/text/cursor and actual margin,
transcript, prompt/footer pixels, malformed headers and incomplete/extra pixels.
The mutation baseline must pass first. Screenshots and physical records are retained
under ignored output paths. Windows CI now requests Features on its three profiles
and explicitly propagates mutation failure; Windows execution remains unverified.

Independent source review caught a navigation false pass when identical characters
could conceal incorrect Home behavior. Distinct ends and intermediate assertions
resolved it; a follow-up found no actionable findings. Reviewers did not run tests
or inspect artifacts. There are no runtime, ABI, memory or image changes in this
slice. Console-specific negative/saturation targets, independent live transcript
and facts-source oracles, broader snapshot mutations and physical qualification
remain open. No issue/milestone closure or merge is claimed.

Final strengthened runs passed on xAPIC ISO/FAT and x2APIC ISO: each completed all20
submissions, capacity/navigation assertions,30-second idle, complete visible pixel
comparisons and physical IF0 halt retirement, with QEMU exit0. The24 baseline/pixel
mutations passed against both xAPIC and x2APIC ISO captures. The final xAPIC screenshot
was visually inspected. Earlier successful invocations before the intermediate
navigation assertions are not substituted for these reruns. The frozen Console ISO
and current named image retain SHA256
`3dcdaed9167b8c94a50c8938c06ae1ccb5e488c18f5887ad85fc4995f48f8369`.
Build/reproducibility and inherited regressions remain the previously recorded
23a66e3 results; they were not rerun for this host-only validation change.

### Isolated console rejection/saturation implementation

Three separate ConsoleTest profiles add controlled BadEditor/BadFacts rejections
and near-MAX counters. Each build passed20,822 static assertions. Design review
cleared the slice; code review found two oracle gaps (early rejection-counter
saturation and nonzero unpublished baseline outputs), both corrected and re-reviewed
without remaining findings. Reviews were source-only, without reproducing results.
First ISO invocations passed, but final stronger-oracle ISO/FAT reruns are pending
at this checkpoint. Normal and interactive Console two-build reproducibility passed,
including20,822 assertions per build and corrupt-PE rejection. No final target batch
or Windows result is inferred here. Active console timer/drain negative fixtures,
independent facts/transcript provenance, broader mutations and hardware gates remain
open. No milestone/issue closure or merge is claimed.

The final stronger-oracle batch subsequently passed all six xAPIC targets:
BadEditor and BadFacts exact expected stops on ISO/FAT, and Saturation full20-command
Features on ISO/FAT with near-limit transitions, independent ISR trace growth,
visible pixels and physical retirement. All six commands exited0. Normal Test-Qemu
ISO/FAT passed private paging/ownership/CPL3/continuation checks. Ordinary Console
Features ISO/FAT passed again, and24 pixel mutations passed on its ISO capture.
Normal, Console and all three fixture reproducibility profiles each passed two
builds with20,822 assertions per build, identical artifacts and corrupted-PE
rejection. Fixture reproducibility ran in separate output directories; their ISOs
were byte-compared with the actual tested fixture media. The ordinary interactive
ISO remains byte-identical to the frozen23a66e3 image. No fixture instrumentation
was added to the user-facing image.

Final source/CI/evidence review found no actionable findings; reviewers did not
reproduce these tests. Windows execution and physical qualification remain open.
The wider inherited regression results remain the earlier frozen-runtime evidence;
they were not all rerun for these compile-time-only hooks. Active console timer/
drain rejection and independent live facts/transcript oracles remain next gates.

### Console timer ownership and bounded drain fixtures

Design/source reviews cleared the controlled UnexpectedTimer/PendingTimer/
DrainExhaust profiles. Initial xAPIC ISO/FAT runs passed all six targets: actual
phase1 ISR40 rejection before EOI, one phase2 timer EOI followed by complete halt
retirement, and exact1025-window rejection with IRR40 still pending. The215 captured
baseline/mutation checks subsequently passed. The first mutation invocation failed
its own single-field assertion because LVTT also matched LVTTHMR; a word boundary
corrected the driver selector, without weakening the actual timer oracle. That
failed invocation is not counted as a pass. Source follow-up found no findings;
reviewers did not run tests or inspect artifacts.

Normal/Console reproducibility passed two builds and corrupt-PE rejection; all
builds passed20,822 static assertions. Frame5,996, editor344,659 and model17,279
host checks passed. Final regression/reproducibility and optional x2 fixture results
are still pending here. Controlled injection does not qualify arbitrary timer or
NMI/MC timing, independent live facts/transcript provenance, hardware or Windows.
No merge, issue/milestone closure or exhaustive corruption claim is made.

Subsequent checks confirmed normal Test-Qemu and ordinary Console Features ISO/FAT,
normal/Console/three timer-fixture reproducibility and byte equality between rebuilt
and tested fixture ISOs. These passed; optional x2 fixture targets remain pending.
One x2 DrainExhaust ISO invocation failed in the host driver while parsing a partial
READY address line. Its serial log is preserved separately. The driver now waits
for the complete strictly formatted line under the existing deadline;255 checks
extract the actual regex and test every interrupted prefix plus malformed fields.
No address assertion was relaxed and no target success is inferred from that failed
invocation. A final x2 rerun is required. The older fixture paragraph's pending
regression/reproducibility wording describes its earlier checkpoint only.

Final x2APIC reruns under the complete-READY reader passed all six profiles on
ISO/FAT, including the previously failed DrainExhaust ISO invocation's profile.
Together with six xAPIC passes this establishes12 timer/drain target profiles;
the earlier partial-line failure is retained as a failed driver invocation, not
substituted by another medium. All final commands exited0. The old KeyboardTest
DrainExhaust ISO also passed after the shared compile-time STI condition change.
The215 timer mutations and255 readiness cases passed. Source-only follow-up cleared
the readiness/CI/evidence changes and resolved the earlier stale-status finding.

Five reproducibility profiles passed: normal, Console and the three xAPIC timer
fixtures. x2 fixture media were built/hashed/tested, but separate x2 reproducibility
is not claimed. Current ordinary Console ISO remains SHA256
`3dcdaed9167b8c94a50c8938c06ae1ccb5e488c18f5887ad85fc4995f48f8369`.
No general asynchronous interrupt resilience or complete OS qualification follows.
Independent live facts/transcript provenance, broader corruption, Windows and
physical qualification remain open. No issue/milestone closure or merge.

### Independent live facts and transcript checkpoint

Host-only validation now derives source/console locations from a hashed build
listing/EFI and actual TR base, captures read-only owned records and independently
predicts complete editor/model/grid/command/facts/response state from real qcodes.
Enter make/break separation permits an exact expected IRQ/EOI increment; source,
CPU/LAPIC/queue/port quiet evidence is frozen before submission. Pixels use the
reference grid. This addresses live reporting/composition against owned records;
shared font content, underlying subsystem correctness, trusted build/host provenance
and physical/Windows qualification remain separate assumptions/gates.

An initial invocation matched independent status state but then failed when QMP
rejected key-up while the VM was stopped. Its failed serial evidence is retained;
key-up now follows the completed comparison and resume. That invocation is not a
pass. Subsequent20-command ISO/FAT sessions passed before the final coherent port-
capture/replay wiring. The final20-command ISO/FAT/x2APIC ISO sessions then passed
with that wiring, each including full independent state/source-facts/transcript and
pixel comparisons,30-second idle and physical retirement. Replay and mutation tests
passed488 cumulative checks on each profile, including a valid coordinated forged
facts/JSON/grid/pixel case. This count includes baseline/replayed events, not488
independent corruption scenarios. Twenty-four pixel mutations and255 READY cases
also passed. Console listing-inclusive reproducibility passed two builds,20,822
assertions each and corrupt-PE rejection. Tested EFI/FAT/both ISO names/listing/
manifest were byte-compared with the separate reproducibility output. Ordinary ISO
remains byte-identical to frozen23a66e3. Source reviews found no findings, with
assumptions retained; one stale pending-status documentation P3 was corrected.
Reviewers did not run tests or inspect artifacts. No milestone/issue closure or merge.

### Real-key navigation and development-readiness audit

The issue40 audit identified a live-test gap despite host coverage: wrapped history,
draft cursor restoration and Left/Right extended-key behavior. Features now traverses
all eight slots in exact order, verifies history/live boundary no-ops against complete
editor bytes, moves a cursor without detaching browsing, restores `draft` at cursor3,
and checks every subsequent insertion/navigation/deletion step separately with distinct
bytes. The final navigation prompt also receives independent visible-pixel comparison.
There are still20 command submissions.

Final expanded ordinary ISO/FAT/x2APIC ISO and Saturation ISO runs passed, including
30-second idle and physical halt retirement. Ordinary profiles passed561 cumulative
replay/source/state/mutation checks each; the count grew through events/checkpoints,
not new independent corruption scenarios. Pixel24 and readiness255 checks passed.
An earlier expanded run passed before the final per-key assertions; it is not used
instead of the final four reruns. Source review found no blocking findings, noted
possible grouped-key compensation, then cleared the intermediate assertions.
Reviewers inspected source only and did not reproduce results/artifacts. A minor
stale README qualification sentence was corrected to distinguish completed local
campaigns from the remaining Windows/physical gates.

README/roadmap now link development-readiness.md, which maps console acceptance to
local evidence and keeps Windows, exact reference inventory, repeated physical boots,
failure-path qualification, milestone exits and maintainer decisions open. No source
or image runtime changes occurred; the ordinary ISO still matches frozen23a66e3.
Earlier runtime regression/reproducibility records remain inherited, not newly run
results for this host-only slice. No release/milestone/issue completion or merge.
