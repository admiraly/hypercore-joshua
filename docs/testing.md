# Boot qualification

## Automated checks

```powershell
.\tools\Build.ps1
.\tests\Test-Reproducibility.ps1
.\tools\Test-Qemu.ps1
.\tools\Test-Qemu.ps1 -BootMedia Fat
.\tests\Test-FaultDiagnostics.ps1
```

Build runs PE format/permission/range checks and more than 20,000 FAT/ISO assertions,
including an exact byte comparison of the EFI payload and embedded boot image.
QEMU uses project-local EDK II firmware, fresh writable NVRAM, TCG, a conservative
CPU, two configured CPUs (only the BSP runs JOSHUA), no NIC or internal disk, and
read-only boot media. It fails on timeout, emulator exit, boot failure, kernel fault,
or missing ordered milestone markers. PNG screenshots and serial logs stay in
`out/qemu-iso-*` or `out/qemu-fat-*`; QMP binds loopback only. The test terminates
only its own emulator process. Default timeout is 90 seconds.

## Latitude / Ventoy test

1. Rebuild and pass automated tests. Copy **`out/joshua-m0.iso`** as a regular file
   onto the existing Ventoy data partition. Do not raw-write the image over Ventoy.
2. On the designated Latitude 7430 only, record BIOS 1.35.0 and disable Secure Boot.
   Keep UEFI enabled. Do not change disk-controller settings or internal boot entries.
3. Open Dell's one-time boot menu (F12) and select the UEFI Ventoy USB device.
4. Select `joshua-m0.iso`. Initially try Ventoy normal mode. Ventoy chainloading
   bring-up on the reference Dell was owner-confirmed on 22 October 2026; the exact
   Ventoy version/mode and repeated cold-boot qualification remain outstanding.
5. The firmware screen identifies JOSHUA, CPU, framebuffer base/resolution, and ACPI.
   After five seconds it exits boot services automatically. Press any key during
   that interval to stay in the firmware diagnostics menu. Menu: B boot, I info,
   M memory map, R reboot, S shutdown. This menu is not the native OS shell.
6. Success is the native dark status screen headed `JOSHUA OS / HYPERCORE`, with
   `UEFI BOOT SERVICES EXITED` and `BOOT CHECKPOINT REACHED. POWER OFF TO EXIT.`
   The CPU intentionally halts here; there is no post-exit keyboard handler yet.
7. Photograph the final screen. Use the laptop power button to power off. Report
   exact Ventoy version, whether normal mode worked, displayed resolution, and
   any error or last visible line. Repeat three cold boots before claiming reliability.

If the firmware rejects the image, shows a black screen, or Ventoy refuses to load
it, record that failure. Do not try BIOS flashing, random storage-mode changes,
Secure Boot key enrollment, or disk installation as a workaround. A separate FAT32
USB with `EFI/BOOT/BOOTX64.EFI` can isolate Ventoy from loader problems, but preparing
that additional USB must be an explicit separate action, never an automatic disk format.

## Qualification limits

Static checks prove structure, not firmware behavior. QEMU success does not prove
Ventoy or Dell compatibility, DMA confinement, memory isolation, SMP correctness,
or useful workload performance. Bare-metal testing must be performed by the owner.
The checkpoint now performs a bounded physical allocate/write/free smoke check
then switches to verified owned page tables and halts.
The full M0 and M1 exit criteria remain open until review and real-machine results.

Firmware-menu keyboard commands, watchdog failure, invalid map-key retry, oversized
maps, unsupported GOP formats, and NMI injection are not yet automated.
Those paths require additional fixtures; do not infer their dynamic correctness
from the successful default boot path.

## Linux host execution

PowerShell 7 can run the host scripts with native NASM 2.16.03. Supply native QEMU
with `-QemuPath` and a `-FirmwareDirectory` containing the code/vars pair named
`edk2-x86_64-code.fd` and `edk2-i386-vars.fd` (local symlinks are sufficient). Use
a matching, non-Secure-Boot OVMF pair. Record actual versions and hashes; this is
separate host evidence from the pinned Windows toolchain. The test checks P6
framebuffer bytes without System.Drawing and retains PNGs for visual inspection.

The fault suite builds nine separate fixture images. Normal exceptions require
exact vector/error/RIP/CR2 and selected IST4 frame evidence. StackFault additionally
requires the interrupted unusable RSP. DoubleFault requires vector8/error0 and
IST1 delivery after an ordinary-stack guard fault; saved RIP is logged without a
restart assertion. No NMI/MC injection or general nesting/recovery is qualified.
Firmware-menu and map-retry failures remain unqualified. Fixture images must not be distributed
as normal boot images.

`tests/Test-MemoryMap.ps1` is a Linux host-only native ELF harness using NASM
2.16.03 and `ld`. Its 24 fixtures call the exact shipped Assembly validator: normal
and extended stride, unsorted adjacency, unknown types, physical boundary, empty
and oversized buffers, invalid stride/width, pointer wrap, zero pages, misalignment,
physical overflow and overlapping/contained/duplicate descriptors. ELF relocation
warnings are disabled because the host linker resolves those relocations; other
warnings remain errors. Linux CI builds a checksum-verified NASM source archive
and runs this suite; Windows CI exercises the validator through normal QEMU boot.

`tests/Test-Reservations.ps1` builds Linux SysV wrappers around the exact shipped
firmware-ABI range/candidate routines, then uses a Python ctypes oracle. It checks
6,425 cases including 6,400 seeded random cuts, plus image coverage and candidate
selection/exclusion. Python and the ELF wrappers are host-only. The fixed QEMU
profile additionally requires a nonempty candidate count before the subsequent
allocator smoke check allocates and releases test frames.

`tests/Test-PagingInventory.ps1` runs the exact Assembly paging walker and table
exclusion routines through Linux host wrappers. Its 250 checks include 100 seeded
shared-table graphs; fixtures cover 4-KiB/large-page leaves, PAT/NX interpretation,
ignored non-present entries, bad classifications/addresses, cross-level cycles,
capacity failure and range exclusion. Host snapshots assert the walker does not
write synthetic tables. This does not emulate privileged CR3/CR4/EFER reads: normal
ISO/FAT QEMU boots exercise those and compare captured/final CR3. No LA57 or
malformed real-firmware injection is claimed; unsupported modes stop.

`tests/Test-Frames.ps1` exercises the exact Assembly frame allocator via Linux
wrappers and a Python oracle. It verifies ownership, arbitrary contiguous counts,
alignment, whole-allocation zeroing, reservation gaps, accounting, failure without
mutation, exhaustion, metadata capacity, coalescing and 6,000 seeded mixed
operations. Host pools are aligned to 64 KiB so ASLR does not change allocation
outcomes. Distinct integer-register sentinels check all eight firmware nonvolatile
registers around allocation/free. This does not test SMP or recovery from faults.
Normal QEMU ISO/FAT boots also allocate, inspect and write RAM, reject wrong/double
frees and restore counters before the FRAME_ALLOCATOR_OK marker.

`tests/Test-OwnedMap.ps1` independently decodes all parents/leaves of the exact
Assembly builder. Its 1,061 checks include identity, W^X/supervisor flags, PAT-bit
conversion, allocation/chunk ownership, poisoned failures, physical single-page
fallback and metadata exhaustion. Cache fixtures require no promotion across
inherited 4-KiB leaves, plus retained/subdivided large leaves and selector equality.
It does not execute privileged activation or the policy orchestration helper.
Normal ISO/FAT boots and four #PF fixtures exercise those paths in QEMU, checking
new/final CR3, CR0.WP, EFER.NXE and PCID/global-page policy. Physical compatibility,
initial-PCID-enabled firmware and unusual PAT/MTRR layouts remain unqualified.

The ConstWrite fixture writes the immutable screen title after owned activation.
It requires #PF vector 14/error 3, the exact write instruction RIP and constant
address CR2. Exact PE tests check all five section names, bounds and flags,
including RO/non-executable `.rodata`; the boot proof checks every image page.

`tests/Test-Lifecycle.ps1` runs the exact lifecycle core and real frame allocator
through Linux wrappers. The host adapter records invalidation VA/PTE, physical free
count and total present arena leaves. Assertions require all leaves published before
allocation invalidation, all removed before retirement invalidation, and physical
free only afterward. The 6,249 operations include 6,000 seeded mixed allocations/
releases, 64-record and 512-page capacity, physical/metadata exhaustion, unchanged
ordinary rejection, exact ownership, whole-run zeroing, corrupted-leaf/ownership
quarantine, all eight PAT indices, A/D tolerance, unchanged boot tables and eight
integer ABI sentinels. Privileged checks/INVLPG are exercised by QEMU, not the host.
Normal ISO/FAT smoke writes real high-half memory, rejects invalid frees, releases,
reallocates zeroed memory and restores accounting. Unmapped reads a released VA and
requires #PF/error 0 with exact RIP and high-half CR2. This is local BSP evidence;
no SMP shootdowns, hardware qualification or user isolation are established.

`tests/Test-Descriptors.ps1` runs the exact Assembly GDT/TSS/IDT constructors and
five-guard boundary helper through host wrappers. Its 1,470 cases check malformed
GDT sizes before destination writes, maximum size, full-width TSS base/limit/type,
reserved TSS bytes/I/O offset, all 256 gate targets/selectors/IST indices, randomized
guard decisions and eight integer nonvolatile sentinels across all four helpers.
It does not execute LTR or exception delivery: normal ISO/FAT and nine fault QEMU
runs cover target evidence. TR cached attributes are not used as the memory busy-bit
check; the kernel checks the descriptor after LTR before announcing TSS_READY.

`tests/Test-Acpi.ps1` calls the exact Assembly range reader and RSDP/XSDT/MADT parser
through native host wrappers and owned synthetic identity tables. Its 1,131 cases
include 500 seeded CPU-list oracles, extension/unknown records, all checksums, full
64-KiB MADT, adjacent coverage, RP/wrong types/unmapped or wrong/user translations,
nonexistent pointers, duplicate/missing topology, capacities and eight integer ABI
sentinels. Snapshots require no source-table writes and cleared failure readiness.
It does not simulate away production read checks. Normal QEMU ISO/FAT additionally
requires real OVMF BSP0/two enabled records and a nonzero local-APIC base; controller
access, AP startup, physical ACPI compatibility and interrupt delivery remain untested.


## BSP APIC validation

`tests/Test-CacheType.ps1` checks the exact Assembly effective-UC classifier against
host expectations, including255 MTRR pairs and PAT WC rejection.
`tests/Test-ApicPolicy.ps1` runs the exact quiescence and controller staging code
with a modeled register backend and real host page tables. It checks supported
versions, rejects every other IOAPIC version before route reads/writes, preserves
SMI/NMI, checks masking/readback failures and rejects unauthorized/cache-unsafe
controller pages. These host tests do not execute RDMSR/WRMSR, MMIO or IRETQ.

Normal ISO/FAT QEMU requires APIC_READY and TIMER_OK, sixteen ticks/EOIs/returns,
matching mode and UC page count, followed by the existing owned-memory/final-state
checks. `tests/Test-ApicTimer.ps1` additionally boots X2 fixtures from ISO and FAT
with `qemu64,+x2apic`, and requires MissingEoi to fail the normal timer checkpoint.
A timeout in that controlled negative fixture is expected rejection, not a passed
boot. Physical compatibility, calibration, AP startup and scheduling remain open.


## Guarded kernel context validation

`tests/Test-KernelContext.ps1` executes the exact Assembly frame/scheduling policy
against independently constructed host frames and a reference alternating schedule.
It covers invalid pointers before frame reads, range/selector/flags/RSP rejection,
worker RF acceptance/bootstrap RF rejection, failed-publication snapshots and eight
integer ABI sentinels. Host pointer storage is caller-owned; no privileged adapter
or IRETQ is executed by this suite.

Normal ISO/FAT and existing X2 ISO/FAT fixtures now require64 switching ticks/EOIs/
restore selections, nonzero progress from both workers and successful bootstrap
resumption. QMP reads the actual owned page tables to check all seven absent guards
and every stack page's supervisor RW/NX permissions. `tests/Test-ContextSwitch.ps1`
checks InheritedId launch normalization, BadFrame rejection before publication or
invalid IRETQ, and WorkerGuard exact #PF/error0/RIP/CR2 on IST4. These fixture images
are separate from the normal distribution image. No recovery, arbitrary task API,
FPU context, AP/SMP, user isolation or physical qualification is established.

`tests/Test-QemuStackWalk.ps1` extracts the actual QEMU walk function by its
PowerShell AST and supplies a read-only physical-memory model. Fifteen fixtures
reject read-only/user ancestors at every level, handle absent entries and large
leaves. Effective stack writability requires RW at every traversed level.


## Draft ring3 transition campaign (issue #32)

`Test-Qemu.ps1` now requires host Python3 for the exact user-result oracle on normal
boots. `tests/Test-UserRuntime.ps1` builds24 separate TCG profiles (including five expected-stop cases) with immutable
fixture identity in each manifest, then verifies actual CPL3/roots, precise fault
vector/error/RIP/applicable CR2, watchdog8/8 and independent B continuation.
It uses isolated `out/user-*` folders, including when Build's UserTest is used alone.

On the explicitly tested Intel Linux KVM host, add `-IncludeKvm` (plus native QEMU
and firmware paths as needed) for AlignmentKvm, StackKvm, SyscallKvm and SysenterKvm.
These select `-Acceleration Kvm -CpuModel host`; they are not Windows CI or
bare-metal evidence, and the SYSENTER expectation is Intel-specific. Normal images
recognize only the TCG emulator UART profile; these explicit KVM fixture builds
add exact KVM CPUID recognition for serial evidence. No physical UART probe is
added to normal images. TCG's Alignment and Stack fixtures deliberately fail their
exact expected-vector oracles; they must not be counted as passes just because B
continued. Intel KVM supplies the actual #AC/#SS vector evidence.

PendingExit/PendingFault force actual IRR pending under IF0 and require drain EOI
plus B progress. DrainExhaust/KernelTimer require bounded stop before EOI. The
InheritedCode/LiveSelector rejection, InheritedLdt #GP(4) and KernelFault #UD stop
fixtures exercise descriptor and saved-CS authority. With four Intel KVM profiles,
the full campaign has28 profiles; TCG's two mismatching SS/AC cases are excluded.

Normal boots capture physical memory after QMP stop into
`qemu-<media>-user-paging.json`. `tests/test_user_paging.py` independently checks
private roots/permissions/cache selectors, complete paging-allocation aliases,
exact backing ownership, code bytes, padding and terminal metadata. Run
`python3 tests/test_user_paging_mutations.py out/user-none` after the campaign;
Windows uses `python`. It requires the baseline to pass, then rejects54 corrupted
snapshots and49 synthetic reserved/cache-policy mutations through actual oracle
code. These counts describe mutations, not hardware qualification. CI runs the
same mutation suite. Source review closed parent-cache/reserved-bit findings.
Final local campaigns and regressions pass as recorded in validation.md. Windows
CI remains pending; PR #35 remains draft.

## Embedded ELF loader campaign (issue #36)

`tests/Test-Elf.ps1` executes the actual Assembly parser/copy helper on Linux against
an independent host profile oracle. It currently passes6,198 cumulative checks,
including guarded truncation, field/size/entry rejection, plan revalidation,
page-aligned complete/partial aliases, unchanged rejection buffers, zero-fill and
eight integer ABI sentinels. It does not execute privileged entry.

`tests/Test-ElfRuntime.ps1` builds isolated `out/elf-badmagic`/`out/elf-badentry`
images. `Test-Qemu.ps1 -ExpectedFault ElfReject` requires that exact fixture identity,
preallocation parser rejection, no private-root/execution marker, unchanged QMP
physical allocation count, and halted CPL0/IF0. It does not compare every ledger byte.
Normal user campaigns now execute the ELF entry beyond an INT3 prefix and verify
initialized data/BSS. QMP also compares the actual load plan to independently read
ELF headers. Mutation coverage adds plan entry, initialized data and BSS corruption;
final rerun counts/results belong in validation.md, not inferred from this test list.

## Keyboard preparation (issue #38)

Linux host suites `Test-Keyboard.ps1`, `Test-KeyboardAuthority.ps1` and
`Test-KeyboardController.ps1` execute actual Assembly policies. Current results:
180,744 decoder/queue checks including40,000 mixed operations;1,890 ACPI/ISA/GSI
checks including500 generated maps;503 modeled-port command/ACK/order/timeout/ABI
checks. Each policy call has eight integer ABI sentinels. Authority shares the actual
ACPI read predicate, modeling paging lookup only; controller callbacks model ports.
These counts are cumulative assertions, not independent scenarios or real IRQ evidence.

Normal QEMU ISO/FAT now require KEYBOARD_PREPARED and capture the actual controller,
authority and decoder records. Snapshot mutations include scanning/IRQ12/GSI/modifier
corruption. In normal images the selected IOAPIC route stays masked. Explicit
KeyboardTest images exercise the input window and QMP key injection described below. Full user/fault/APIC regressions and real delivery/
retirement negatives remain mandatory when the live IRQ stage is wired.

The Linux `Test-KeyboardFrame.ps1` suite executes the actual read-only IRQ-frame
checker with eight integer nonvolatile sentinels. It checks exact frame location,
4-KiB helper reserve, current root, phase-specific continuation, CS/SS, old RSP and
normalized metadata flags, optional architectural RF in hardware frames, and the
explicit DF variant. Invalid frame pointers are
rejected before dereference, including a protected unreadable page. Its 5,996
cumulative checks are host policy evidence; they do not establish actual IRQ or
IRETQ behavior. Explicit keyboard fixtures call it before any EOI or return. Normal boot still
leaves the keyboard prepared and masked.


Experimental live fixtures use separate outputs: build with `-KeyboardTest Text`,
`Df` or `Missing`, then run `tools/Test-KeyboardRuntime.ps1 -OutputDirectory <path>`
(and `-BootMedia Fat`). Text/Df inject actual QMP PS2 key events and require ordered
`a`, `A`, `B`, Ctrl-C, Left, Backspace and Enter. Missing injects no key and requires
exactly16 deadline deliveries/EOIs before quiescence. Physical QMP capture checks
window/controller/decoder storage, queue index invariants, the masked IOAPIC low
entry and actual LAPIC timer mask/counts/empty ISR+IRR. Post-CLI DF flags are captured
from memory. The shared target predicates have8,338 cumulative mutation checks.
This target suite complements rather than replaces the private-root/user/fault
campaign. The expanded campaign below exercises forced pending/drain/overflow and invalid-frame negatives.


`tests/Test-KeyboardCampaign.ps1 -IncludeX2` builds17 isolated keyboard profiles and
runs each on ISO/FAT, plus Text/PendingBoth/OverflowPending with x2APIC on both media
(40 target invocations). Windows CI owns this as a separate15-minute job. Exact
negative predicates retain ISR/IRR evidence; no catch-all failure counts as a pass.
Shared actual-oracle predicates now pass8,373 cumulative checks, including sticky
second-refill-timeout and unsupported-port-response mutations. The old8,338 count above describes the first positive
checkpoint. OverflowPending uses fixture-only bounded refill waits on the real
PS2 path; it does not qualify natural production timing. See the design's controlled
fixture section for the failed batch and strict replacement acceptance conditions.


## Console component host fixtures (issue #40)

On Linux with pinned NASM2.16.03, ld, Python3 and PowerShell, run:

```powershell
./tests/Test-ConsoleEditor.ps1
./tests/Test-ConsoleGrid.ps1
./tests/Test-ConsoleCommand.ps1
./tests/Test-ConsoleResponse.ps1
./tests/Test-ConsoleModel.ps1
```

These assemble actual Assembly helpers into host libraries, exercising integer ABI
sentinels and independent state/grammar/pixel/schema/model oracles. Current local
cumulative counts are344,659/5,055/95,317/28,687/17,279 respectively, not independent
scenario counts. All suites are in Linux CI; no CI success is asserted here.
The model runner exports an actual Assembly-rendered host preview with synthetic
facts. No component test establishes actual keyboard delivery, boot snapshot
provenance, interactive console operation or physical compatibility. These remain
explicit target/integration gates. See docs/console-design.md and docs/validation.md.


## Native console target smoke

Build `./tools/Build.ps1 -Console`, then run `./tools/Test-Console.ps1` for ISO and
`./tools/Test-Console.ps1 -BootMedia Fat`. Reproducibility uses
`./tests/Test-Reproducibility.ps1 -Console`. Optional explicit x2APIC delivery uses
`./tools/Build.ps1 -Console -TimerTest X2` then the smoke with
`-OutputDirectory out/console-x2 -CpuModel 'qemu64,+x2apic'`.
Linux additionally needs the absolute QEMU path and `.tools/firmware` directory.

The smoke verifies artifact hashes/fixture isolation before starting read-only
media. Actual QMP key events perform editing, JSON submission, history, cancellation
and halt, with at least30 seconds of timer-free idle. Stopped physical captures
check real CPU root/RSP/RIP, model/editor/result/facts, IRQ/EOI and queue invariants,
actual LAPIC retirement and IOREGSEL/route/port state. Actual display pixels are
compared across the entire visible framebuffer against a separate rasterization of
the captured grid, including margins and cursor. This checks grid/display consistency;
it shares the authored font and does not independently prove transcript provenance
or GOP stride padding. The constant halt row and cursor retirement have additional
expectations independent of the grid.
A final success summary is published only after clean QEMU exit; stale summaries
are invalidated before a run. Use `-Features` for18 additional submissions covering implemented commands, human/JSON
responses, grammar/errors, empty input and clear, then127-character capacity, rejected
insertion and Home/Delete/End/Backspace. Each submission checks exact command/result
and response bytes; JSON values are compared with captured facts, not an independent
source-record oracle. Run `python tests/test_console_pixels.py out/console` after an
ISO pass for24 baseline/mutation checks. Negative console fixtures, saturation,
independent live transcript/source-facts oracles and broader snapshot mutations
remain separate gates.
The new Windows console CI job is wired; execution remains unverified here.

## Isolated console rejection and saturation targets

`./tools/Build.ps1 -Console -ConsoleTest BadEditor` creates separate
`out/console-test-badeditor` media. Run `./tools/Test-Console.ps1 -Fixture BadEditor`
and repeat with `-BootMedia Fat`; BadFacts and Saturation use the same commands
with their own mode names and directories. Linux requires explicit QEMU/firmware
paths as above. `Test-Reproducibility.ps1 -Console -ConsoleTest <mode>` can compare
fixture builds without overwriting interactive artifacts. Explicit x2APIC profiles
need matching output-directory/CPU arguments; no x2 fixture result is inferred.

BadEditor requires only the authored invalid length mutation and no editor/output
publication. BadFacts captures a valid source before poisoning, exact parser/editor
submission state and zero facts/response after source rejection. Both require exact
permanent owned-root CPL0/IF0 halt with phase1, no normal HALTED, and no unrelated
fault. They do not claim source retirement. Saturation runs Features with near-limit
seeds, observes MAX-1 then MAX and later MAX for submissions/rejections/cancellations,
model completions and IRQ accounting. A separate fixture-only actual ISR44 trace
provides delivery evidence independent of saturated counters. Pixel, idle and safe
retirement assertions remain enabled. All modes check manifest/media isolation and
publish final summaries only after clean QEMU exit0. These controlled fixtures do
not qualify natural lifetime limits, active-timer/drain rejection or physical input.

ConsoleTest also accepts UnexpectedTimer, PendingTimer and DrainExhaust. Use the
same Build/Test commands and isolated directories on ISO/FAT. UnexpectedTimer
requires real phase1 timer delivery to stop before EOI, with actual ISR40 retained.
PendingTimer follows an actual halt submission and requires one phase2 timer EOI,
no ticks/rearm, matching copied return fields and complete physical retirement.
DrainExhaust suppresses STI only in its controlled fixture loop: counter1025,
actual IRR40 with empty ISR, zero EOIs and no HALTED are mandatory. The fixture
record copies saved fields only after frame/ISR proof and before stack reset.
These are controlled pending injections, not natural asynchronous timing evidence.

After all three ISO captures, `python tests/test_console_timer_mutations.py out`
requires215 baseline/mutation checks against the same timer predicates used during
capture. Mutations include return fields, stage/pending/EOI/limit state, exact LAPIC
sets/masking/counts and every saved-flags bit. RF has separate accepted cases;
metadata flags stay strict. This is targeted corruption coverage; complete capture
provenance and all possible corruptions are not established. Windows CI runs the
six fixture profiles on both media and propagates mutation failures explicitly.

`python tests/test_console_ready.py` extracts the actual driver regex and checks255
complete/partial/malformed READY cases. The target driver awaits all nine fixed-width
addresses and the line terminator under its existing deadline, so interrupted serial
writes cannot be parsed prematurely. It retains exact address/profile validation.

## Independent live console reporting and composition

Normal Console tests now require a newly built manifest containing hashed boot.lst.
The actual loaded TSS base, PE bounds and listing bind console/source physical
addresses. Stopped owned-source captures supply expected diagnostic numbers;
complete editor/model/grid/command/facts/response bytes are predicted from real
qcodes. Pixels are compared against the reference grid, with the authored font
still shared. The source payloads are trusted subsystem records, not independently
reproved allocator/ACPI data or malicious-firmware protection.

`python tests/test_console_live_mutations.py out/console` replays the completed
Features ISO session; add `--boot-media fat` for FAT or use `out/console-x2` for its
ISO. Each current Features profile passes561 cumulative baseline/event/mutation checks.
It requires the published20-command independent summary and reconstructs facts
from frozen pre-submit sources, not diagnostic expected-state JSON. Actual Enter
make/break separation gives an exact predicted interrupt increment. The CPU/LAPIC/
queue/port quiet checkpoint is frozen before the make. Captured trace delivery is
trusted host evidence; unexpected extra delivery/repeat fails the live test.

Mutations cover complete records, source stability and listing/EFI/TR/address
identity. A coordinated valid memory-diagnostic JSON/facts/grid/pixel forgery passes
the older self-consistency predicate and must fail the independent reference. No
synthetic pixels or responses are substituted into the actual QEMU success campaign.
CI uploads the listing and captures and runs replay for ISO/FAT/x2APIC with immediate
native-exit checks. Windows execution and physical qualification remain separate.

The Features path also traverses all eight wrapped history slots, checks oldest/live
Up/Down no-ops, preserves browsing during cursor movement and restores an edited
draft at its saved cursor. Each Left/Right/Home/End/Delete/Backspace transition is
captured separately, including exact whole-editor no-ops at line boundaries. It
still submits20 commands; the larger replay count includes these added events and
checkpoints, not additional independent corruption scenarios.
