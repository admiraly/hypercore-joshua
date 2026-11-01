# Host-only console voice-bank research

Issue #52 seeks spoken output that follows the interactive console's actual
responses. This tool is an experimental asset source, not integrated playback.
`tools/Render-ConsoleVoice.py` uses the same pinned Piper1.8.0 John model/config
and model/repository cards as the separately accepted boot-voice preview. It
requires those files locally in ignored `.tools/piper-voices`, verifies all four
SHA256 pins, and never downloads anything. Piper inference remains host-only.

It extracts alphabetic word runs from the numeric byte literals in
`kernel/console/response-data.inc`, rejecting unsupported literal syntax. It adds
ten digit names, 26 letter-name fallbacks and names for all32 ASCII punctuation
symbols. The generated manifest records ordered token names, spoken phrases,
contiguous byte offsets, lengths and individual hashes, the response-source SHA256,
and synthesis settings. Each clip is checked for
nonempty mono 22.05-kHz signed16 input, converted with integer interpolation to
48-kHz signed16 stereo, peak bounded, and padded to256 bytes. A clip is bounded to
five seconds and the bank to64 MiB. It is generated under a new named directory
inside ignored `out/`; existing targets and a redirected `out` root are rejected.
The host and output path must remain stable throughout creation. The model and
output bytes are not checked into Git. License cards accompany generated output.

Example (host venv and pinned files already installed):

```
.tools/piper-venv/bin/python tools/Render-ConsoleVoice.py --name my-voice-research
python3 tests/test_console_voice_assets.py
```

Two local runs against the same inputs produced123 clips,19,904,256 PCM bytes and
SHA256 `24646c4de2c8a2467c45f2f5a5d29220076595af0677fc6e2221f84d76eb0802`.
An independent host script checked every generated clip's contiguous offset,
256-byte alignment, length and SHA256 against actual PCM bytes. The unit tests
exercise vocabulary including split/uppercase/tab/inline directives and rejected
unsupported syntax, complete ASCII punctuation/letter fallback, conversion,
padding, silence/malformed input rejection, bank layout and deterministic fake
synthesis. This is local reproducibility evidence, not cross-platform inference
bit identity or audio intelligibility validation. The first run caught and rejected
a mistyped model-card checksum before generation; the pin was corrected to the
existing reviewed voice card hash before the successful runs.

The bank currently exceeds the16-MiB FAT image; future image-layout work must
accommodate it without truncation. Asset lookup, speech timing, case and whitespace
semantics, keyboard responsiveness, PCI authority, DMA retirement and audio
capture remain unresolved; the bank alone produces no console sound. Existing
Piper voice licensing and physical-hardware limits remain as in PR #34.
