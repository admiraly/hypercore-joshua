# Boot qualification

## Automated checks

```powershell
.\tools\Build.ps1
.\tests\Test-Reproducibility.ps1
.\tools\Test-Qemu.ps1
.\tools\Test-Qemu.ps1 -BootMedia Fat
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
