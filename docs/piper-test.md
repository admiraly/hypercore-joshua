# Piper announcement test (issue #33)

This audio test is based on the speech prototype in PR #5. It does not include
the later memory/scheduling stack or unfinished issue #32 isolation work.
It announces “Joshua, online. Hypercore boot checkpoint reached.” and halts.
The ISO is unsigned, UEFI-only and non-installing; it opens no disk-write protocol.

## Build

Use NASM 2.16.03 and PowerShell as in docs/toolchain.md. Rendering additionally
needs a host Python venv (Python 3.12 is the CI profile):

```text
python -m venv .tools/piper-venv
# Windows: .tools/piper-venv/Scripts/python.exe; Linux: .tools/piper-venv/bin/python
<venv-python> -m pip install piper-tts==1.8.0 onnxruntime==1.31.0
<venv-python> tools/Render-Piper.py
```

The renderer downloads checksum-pinned `en_US-john-medium` files and the root
repository license declaration into ignored
`.tools/piper-voices`, then writes ignored `out/piper` PCM, WAV and provenance JSON.
Use the same captured PCM for both builds:

```powershell
./tools/Build.ps1 -PiperPcmPath out/piper/joshua-announcement.pcm
./tests/Test-Reproducibility.ps1 -PiperPcmPath out/piper/joshua-announcement.pcm
./tools/Test-Qemu.ps1
./tools/Test-Qemu.ps1 -BootMedia Fat
./tools/Test-Qemu.ps1 -NoAudio
```

Omitting `-PiperPcmPath` retains the formant-synthesis build. A custom output folder
must also be passed to the reproducibility and QEMU scripts. On Linux supply
`-QemuPath /usr/bin/qemu-system-x86_64 -FirmwareDirectory <firmware-folder>`;
the folder needs `edk2-x86_64-code.fd` and `edk2-i386-vars.fd`.

## Playback contract

All shipped instructions remain Assembly. Piper/ONNX inference executes only on
the development host. The test image contains audio samples, no model or host
interpreter. The optional fifth PE section `.voice` is read-only, non-executable,
at RVA 0x34d000/file offset 0x9000, capacity 0x181000. The existing BSS/DMA,
relocation and boot-info v0 layouts remain unchanged. Firmware permission
implementation and retained identity mappings remain trusted prototype assumptions.

A compile-time bound requires a nonempty clip, at most 1,572,864 bytes, divisible
by 256. Assembly copies it into the existing 128-byte-aligned `speech_pcm` buffer,
then calls the unchanged HDA player. Two equal BDL entries have aligned starts.
PCM is 48 kHz, signed 16-bit stereo, little endian; the renderer duplicates mono,
uses deterministic integer linear interpolation, limits peak to 26,000 and pads
with silence. The fixed announcement is approximately 4.03 seconds. Zero Piper
noise settings remove inference randomness; cross-host bit identity is not claimed.
The build manifest records the actual PCM SHA-256; `voice.json` records provenance. Verified model/repository cards are copied into
the voice output folder and uploaded with CI artifacts.

Serial emits `JOSHUA:PIPER_PCM` on this path, then the existing speech result.
Missing HDA still reports `SPEECH_UNAVAILABLE:01` and reaches the final halt.
Speech status text is stored outside the caller's 32-byte shadow space.

## Voice provenance and licenses

- [Piper 1.8.0](https://github.com/OHF-Voice/piper1-gpl) is a GPL-3.0 host tool.
  It is not redistributed in the test image or test bundle.
- [John medium voice](https://huggingface.co/rhasspy/piper-voices/blob/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/john/medium/MODEL_CARD):
  US-English male, model prepared by Bryce Beattie, LibriVox public-domain dataset,
  fine-tuned from Kristin. The voice repository is marked MIT; retain its model
  card and repository license declaration in the test bundle. The pinned root
  repository card declares MIT; there is no separate LICENSE file in that revision. Model/config/card checksums and
  revision are pinned in the renderer; no weights enter Git or the ISO.
- Original JOSHUA runtime and host renderer source remain MIT.

## Testing the artifact

Play `joshua-announcement.wav` first. Copy the supplied ISO as a regular file onto
an existing Ventoy filesystem, select it in UEFI mode, and let the five-second
countdown expire. Expect the checkpoint screen, announcement if supported, then
halt. This is not a shell. No raw disk write or installation is required.

QEMU HDA playback is the validation target. **Latitude 7430 speaker audio remains
unqualified**: SOF/SoundWire/I2S routing may require drivers absent here. A silent
physical boot does not prove that Piper failed; record the on-screen speech result
and whether headphones work. No hardware audio, isolation or milestone-completion
claim follows from this build. See docs/speech.md for inherited driver limits.
