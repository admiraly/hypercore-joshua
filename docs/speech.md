# Speech output

Tracking issue: #4. JOSHUA speaks; it does not listen. The kernel announces the boot
checkpoint ("Joshua, online. HyperCore boot checkpoint reached.") after
ExitBootServices, then halts as before.

## Next engine: Flite cmu_us_rms (issue #6, in progress)

Phase 1 is a host-only reference:

```powershell
./tools/Get-Toolchain.ps1 -IncludeFlite
./tools/Build-FliteReference.ps1 [-Text "..."]
```

It compiles unmodified Flite 2.2 with the pinned Zig C compiler and writes
`out/flite-reference/reference.wav` (16 kHz). It also writes a segment dump
(phone, end time, word, stress) to use as golden data for the assembly port.
Two runs produce byte-identical WAVs. The boot image still uses the formant
engine below until the port lands.

`Measure-Speech.ps1` counts Flite's impulse excitation as "clicks" (one per
pitch period), so its click count is not comparable between engines.

## Engine (v2, Klatt-style formant synthesis)

`kernel/speech/synth.inc` and `data.inc`, all x86-64 assembly:

1. **Input**: space-separated ARPAbet-style phonemes. A `1` suffix marks primary
   stress and `2` secondary stress. `_` ends a word, `,` is a phrase break and
   `.` ends a sentence. Up to 255 entries.
2. **Durations**: Klatt (1979) inherent and minimum durations with multiplicative
   rules:
   - unstressed ×0.65
   - non-final syllable ×0.85
   - phrase-final ×1.4
   - vowel before a voiceless consonant ×0.8
   - consonant cluster ×0.75
   - final consonant ×1.2
   - global tempo ×0.875
3. **Intonation**: a hat pattern over a baseline declining from 118 to 90 Hz in
   each sentence. Accented vowels rise. The last accent peaks, then falls into a
   low tail. A comma gets a continuation rise.
4. **Coarticulation**: parameters glide linearly from the previous values, with
   40 ms formant and 5-10 ms amplitude transitions. Stops and nasals use F2/F3
   loci coupled to the nearest vowel in the phrase. Diphthongs glide to a second
   target. Stops render as closure (voice bar if voiced), then a 10 ms burst,
   then 50 ms aspiration into the vowel. Affricates add 60 ms frication.
5. **Source**: a KLGLOTT88 glottal flow pulse (open quotient 0.6) with spectral
   tilt, about 1% period jitter, about 6% shimmer, and breathiness gated by the
   glottal open phase.
6. **Filters at 48 kHz**: the cascade runs nasal zero/pole, then F1-F5. A
   parallel bank of F2-F5 plus a frication resonance and a bypass shapes noise
   for fricatives and bursts. Lip radiation is applied at the output.
7. **Smoothness**: amplitudes and pitch ramp every sample, and filter
   coefficients update every 5 ms. No upsampling stage is needed.
8. **Output**: peak-normalised 48 kHz, 16-bit stereo PCM.

All arithmetic is IEEE single precision with fixed operation order, and the
cosine/exponential are polynomial, not x87 transcendental, so a given phrase
renders identically on any x86-64 CPU.

`kernel/drivers/hda.inc` plays the PCM on an Intel HD Audio controller found by
`kernel/drivers/pci.inc` (class 04h/03h). It resets the controller and sends
codec commands over CORB/RIRB. It routes a speaker, then headphone, then
line-out pin to a DAC, directly or through one mixer/selector. One output stream
uses a two-entry buffer descriptor list. Everything is polled, and interrupts
stay masked.

## Analysis

`tools/Measure-Speech.ps1 <wav> [-Spectrum]` (host-only) prints per-window level,
zero-crossing rate, the three strongest spectral peaks below 4 kHz and the share
of energy above 4 kHz. It also counts clicks: sample jumps of at least 6× the
surrounding 2 ms level.

Measured on the QEMU capture (22 October 2026):

| Check | v1 | v2 |
| --- | --- | --- |
| Vowel formants vs target | not checked | AA ≈690/1206 vs 700/1220; IY ≈301/2024 vs 310/2020; EH ≈517/1680 vs 530/1680 |
| Diphthong glide | none | AY 646/1206 → 388/1895 |
| Loudness (RMS) | -20.3 dBFS | -19.2 dBFS |
| Crest factor | 17.0 dB | 16.4 dB |
| Clicks detected | not measured | 0 |
| Fricative vs vowel level | not measured after retune | about -8 dB |
| Stop bursts | single resonator, not measured | below vowel level, after retune |

These numbers are acoustic measurements, not listening results.

## Limits

- Phoneme input only. There is no English text-to-phoneme rule set yet.
- Still a formant synthesizer, so it will sound synthetic (DECtalk-class at
  best). Nobody has rated intelligibility with listeners. The owner rated v1
  poorly (rhythm, intelligibility, buzz, clicks); v2 targets each, but has not
  been listened to by anyone.
- Maximum utterance: 8.19 s (393,216 samples at 48 kHz).
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
