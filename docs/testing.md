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
Windows uses `python`. It requires the baseline to pass, then rejects47 corrupted
snapshots and49 synthetic reserved/cache-policy mutations through actual oracle
code. These counts describe mutations, not hardware qualification. CI runs the
same mutation suite. Source review closed parent-cache/reserved-bit findings.
Final whole-issue regressions and Windows CI remain pending; PR #35 remains draft.
