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
