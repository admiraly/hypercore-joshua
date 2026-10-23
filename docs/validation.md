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
