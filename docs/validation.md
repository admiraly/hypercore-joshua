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

## Speech output (issue #4)

Completed locally on 22 October 2026, QEMU only:

- ISO and raw-FAT boots with `ich9-intel-hda` + `hda-output` reported
  `JOSHUA:SPEECH_OK:00` between ExitBootServices and the kernel-halt marker.
- v1 (first formant engine): the owner listened to the capture and judged it
  poor on rhythm, intelligibility, buzz and clicks.
- v2 (Klatt-style rewrite) `wav` capture passed `Test-Qemu.ps1`: 16-bit PCM,
  4.76 s, RMS about 3,600 (-19.2 dBFS), peak 23,967 (unclipped).
- `tools/Measure-Speech.ps1` on that capture:
  - 0 detected clicks; crest factor 16.4 dB.
  - Measured vowel formants within about 2% of their targets.
  - Diphthong glides present; pauses and stop closures where expected.
  - Bursts and fricatives were retuned twice after measurement.
  - Results are recorded in `docs/speech.md`.
- `-NoAudio` boot reported `JOSHUA:SPEECH_UNAVAILABLE:01` and still reached the
  checkpoint halt.
- Reproducibility passed with the speech code and enlarged PE layout.

Not done: any listening review of v2, a listener intelligibility rating, any
bare-metal audio, and the Dell's audio topology. See `docs/speech.md`.

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

## Piper test ISO (issue #33, 23 October 2026)

This isolated test branch extends PR #5's speech prototype, not the later memory
and scheduling stack. Local host: Linux, PowerShell 7.4.6, NASM 2.16.03, QEMU
10.2.2 TCG/q35/qemu64, distribution OVMF. Piper 1.8.0 with ONNX Runtime 1.31.0
rendered John medium on the host. No model inference executes in the image.

Completed locally:

- Piper build: PE and 23,806 FAT/ISO assertions; two independent EFI/FAT/ISO and
  manifest hashes match, including native extensionless NASM and relative paths.
- Two host renders produced identical PCM bytes. Preview WAV, provenance hash and
  actual EFI `.voice` PCM match byte for byte; peak/duration/stereo checks pass.
- QEMU ISO and FAT with HDA: Piper marker and `SPEECH_OK:00`, then intended halt.
  Both captured approximately 4.00 s at 48 kHz stereo, RMS 2,700, peak 25,877.
- No-audio ISO: `SPEECH_UNAVAILABLE:01`, then intended halt. Framebuffer pixels,
  ring 0, HLT and IF-clear assertions pass for all three profiles.
- NASM rejects 0-, 255-, 257- and 1,573,120-byte clips; accepts the full bounded
  1,572,864-byte clip. That maximum fixture is an assembly-bound check, not playback.
- Default formant build reproducibility and ISO HDA boot still pass.
- Independent source review found two P2 host issues (NASM resolution and bundled
  attribution); both were fixed and follow-up review found no remaining finding.

Windows execution/remote CI and physical speaker output are not established by
these local runs. PowerShell legacy argument passing is checked locally as an
additional compatibility fixture; it does not replace Windows testing. No listening
rating, SOF/SoundWire support, hardware audio qualification or milestone completion
is claimed. Artifact identities are supplied with the test bundle, not inferred
from earlier prototype images. See [test instructions](piper-test.md).
