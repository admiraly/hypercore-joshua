# Speech output

Tracking issue: #4. JOSHUA speaks; it does not listen. The kernel announces the boot
checkpoint ("Joshua online. HyperCore boot checkpoint reached.") after
ExitBootServices, then halts as before.

## Pipeline

1. `kernel/speech/synth.inc` parses space-separated ARPAbet-style phonemes
   (`AA`..`ZH`, `1` suffix = stress; `_` word gap, `,` phrase break,
   `.` sentence end).
2. A cascade formant synthesizer renders 16 kHz floats in 5 ms frames: impulse
   glottal source, three formant resonators, a parallel frication resonator,
   aspiration noise and lip radiation. Targets glide by per-frame smoothing;
   diphthongs switch target mid-phoneme; plosives use closure, burst and
   aspiration phases. Pitch declines through each sentence; stress raises it.
3. The render is peak-normalised and linearly upsampled to 48 kHz, 16-bit stereo.
4. `kernel/drivers/hda.inc` plays it on an Intel HD Audio controller found by
   `kernel/drivers/pci.inc` (class 04h/03h): controller reset, CORB/RIRB codec
   commands, a pin route (speaker, then headphone, then line-out) reaching a
   DAC directly or through one mixer/selector, and one output stream with a
   two-entry buffer descriptor list. Everything is polled; interrupts stay masked.

All arithmetic is IEEE single precision with fixed operation order, and the
cosine/exponential are polynomial, not x87 transcendental, so a given phrase
renders identically on any x86-64 CPU.

## Limits

- Phoneme input only. There is no English text-to-phoneme rule set yet.
- The voice is robotic, rule-based and male-pitched. Gains were balanced from an
  energy/zero-crossing analysis of the QEMU capture; nobody has rated its
  intelligibility with listeners.
- Maximum utterance: 8.19 s (131,072 samples at 16 kHz).
- Speech runs in ring 0 on firmware page tables before M2. The DMA buffers live
  in the image's BSS, identity-mapped by firmware. There is no isolation or IOMMU
  protection.
- Timeouts count `out 0x80` I/O delays, about 1 µs each on PC hardware. They are
  not calibrated timers.
- Speech failure never blocks boot. Serial reports `JOSHUA:SPEECH_OK:00`,
  `JOSHUA:SPEECH_UNAVAILABLE:NN` or `JOSHUA:SPEECH_FAILED:NN`; codes are in
  `hda.inc` (`HDA_E_*`) and `synth.inc` (`SPEECH_E_*`).

## Hardware status

QEMU only (`ich9-intel-hda` + `hda-output`). **No bare-metal audio has been
observed.** The Dell Latitude 7430 has not been tested. Alder Lake laptops often
route speakers through the audio DSP (SOF) and SoundWire or I2S amplifiers rather
than a plain HDA codec. In that case the driver reports no output path, or plays
silently. Record the audio controller class/prog-if, codec vendor ID and the
serial status code from a real boot before making any hardware claim.
