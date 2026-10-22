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
The initial checkpoint deliberately stops before physical frame allocation.
The full M0 and M1 exit criteria remain open until review and real-machine results.

Firmware-menu keyboard commands, watchdog failure, invalid map-key retry, oversized
maps, unsupported GOP formats, and exception/NMI injection are not yet automated.
Those paths require additional fixtures; do not infer their dynamic correctness
from the successful default boot path.

## Linux host execution

PowerShell 7 can run the host scripts with native NASM 2.16.03. Supply native QEMU
with `-QemuPath` and a `-FirmwareDirectory` containing the code/vars pair named
`edk2-x86_64-code.fd` and `edk2-i386-vars.fd` (local symlinks are sufficient). Use
a matching, non-Secure-Boot OVMF pair. Record actual versions and hashes; this is
separate host evidence from the pinned Windows toolchain. The test checks P6
framebuffer bytes without System.Drawing and retains PNGs for visual inspection.

The fault suite builds separate Ud2 and Gp fixtures and verifies vector, error
code and exact instruction RIP, plus CR2 field formatting and halted ring-0 state.
It does not inject page faults, NMI, double faults or damaged stacks. Firmware-menu
and map-retry failures remain unqualified. Fixture images must not be distributed
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
profile additionally requires a nonempty candidate count; no frame is allocated.
