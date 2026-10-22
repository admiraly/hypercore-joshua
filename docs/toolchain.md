# Host toolchain and provenance

Windows PowerShell 5.1 / .NET are host-only build tools. NASM produces all shipped
runtime instructions directly; no linker is required. Packaging scripts write
regular project files and never mount images or open physical disks.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Get-Toolchain.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1
```

For optional emulator testing, install 7-Zip separately or use the existing copy:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Get-Toolchain.ps1 -IncludeQemu
```

The helper verifies pinned archive hashes, extracts into `.tools`, and does not
run installers, require elevation, alter PATH, change firmware, or modify boot
configuration. The optional QEMU download is about 174 MiB and expands to about
1.1 GiB. Do not commit `.tools` or `out`.

| Dependency | Pinned source | Use / provenance |
|---|---|---|
| NASM 2.16.03 Win64 | NASM release archive | Host assembler; BSD-2-Clause license in archive |
| QEMU Windows 2025-12-24, reports 10.2.0 | Stefan Weil's Windows build linked by QEMU | Optional host emulator; GPL and bundled library licenses |
| EDK II firmware | QEMU archive `share/edk2-x86_64-code.fd`, `edk2-i386-vars.fd` | Emulator-only firmware; see `share/edk2-licenses.txt` |
| 7-Zip | Existing host installation | Optional NSIS extraction, not shipped |

Exact download URLs and checksums are recorded in `tools/Get-Toolchain.ps1`.
These versions are deliberately pinned, not represented as the latest available.
No external runtime binary or model weight is included in the JOSHUA ISO.
Original JOSHUA source is **MIT licensed**, selected by the owner on 22 October
2026; see `LICENSE`. This does not relicense any external host tool or firmware.

## Reproducibility

PE, FAT, ISO, catalog, serial number, and timestamps have deterministic values.
`out/manifest.json` records source hashes, assembler version/binary hash, artifact
sizes and SHA-256 hashes without embedding the current time or absolute host paths.
Rebuild with the pinned NASM and unchanged sources to reproduce the image bytes.
Emulator logs, screenshots, NVRAM copies and listings are diagnostics, not release
artifacts or performance measurements.

NASM's generic section-crossing 32-bit relocation warnings are disabled for this
flat-binary build: explicit section virtual addresses resolve them at assembly
time. Other warnings are errors. Static PE checks and actual UEFI boot tests
exercise the resulting addresses. This exception is not a waiver for truncation
or new linker relocations in future ELF modules.

Current debugging uses a NASM listing and serial checkpoints. DWARF/unwinding and
symbolized crash dumps remain required future engineering, not completed features.
