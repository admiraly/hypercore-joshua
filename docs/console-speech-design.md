# Console speech integration design (issue #52; review draft)

The user wants Joshua to speak the text Joshua presents, using the Piper voice
accepted in the separate announcement test. The target is an offline spoken
console response, including live numbers and error/JSON responses. The existing
Piper ISO is a different, stop-after-announcement prototype; its old HDA path
cannot be assumed safe under the newer owned page tables and keyboard lifecycle.
Do not infer physical-audio support or change the existing speech branches.

## Fidelity and voice assets

The source of speech is the exact validated `CONSOLE_RESPONSE.body[0..length)`,
after successful `response_build` and display. Host-only deterministic asset
generation uses the pinned John model/configuration and records provenance. It
must not check model weights or generated binaries into Git. The image contains
only bounded audio assets; no Piper inference or Linux runtime executes in the OS.
Tokenization consumes every byte in the response. Use whole-word clips for the
fixed response vocabulary and exact character-name fallback for other printable
bytes; digit clips express changing numbers digit by digit. Define whitespace as
pauses and speak punctuation with named clips, including JSON symbols. This is
literal and verifiable, not a claim of natural prosody. Matching is explicit about
case and fallback so unknown text cannot silently disappear. Test all response
bytes including maximum length and all supported printable characters. A bounded
asset index and offset/length checks precede every audio read; malformed indexes or
unknown bytes disable speech safely, never reinterpret text as pointers or code.

A host oracle independently tokenizes captured actual response bytes and compares
ordered runtime token IDs and the PCM produced from the pinned clips. Cover every
fixed command/result, decimal extremes, errors, JSON symbols, empty/long responses,
case, bounds and fallback. Success evidence must include two different live status
values, not only static prerecorded phrases. Preserve the user-approved voice;
listen to actual assembled clips and report any quality limitation honestly.

## Controller, mappings and DMA safety gates

Before runtime implementation, specify and independently review a narrow QEMU HDA
profile. Device discovery alone does not confer authority over arbitrary PCI devices.
No PCI configuration write or MMIO access before validating the device identity,
BAR type/range/size and every overlap with reserved/owned RAM and other devices.
Before a load/store to a BAR, prove identity mapping, supervisor permissions and
strict effective UC with retained selectors and PAT/MTRR rules. If that proof is
unavailable, report speech unavailable and leave the console usable. No new cache
conversion, firmware reclamation or page-table promotion is permitted. Prepare all
needed page-table structures before the existing whole-chunk ownership proof.

Audio DMA buffers, ring/BDL metadata and playback storage require exact physical
ownership, alignment, address-width, bounds and coherent cache assumptions. Device
bus mastering may be enabled only after valid owned DMA storage is ready. On every
success, failure, cancellation and halt, stop the stream and prove DMA/interrupt
sources quiescent before changing/freeing backing or returning ownership. If that
cannot be proven, quarantine the backing and take a defined fail-closed path;
never present failed shutdown as successful retirement. The inherited prototype's
poll loops, BAR validation, stop timeout and DMA assumptions need explicit audit;
a simple source copy does not satisfy these gates. No protection against a
malicious PCI device, firmware or missing IOMMU is claimed.

## Console lifecycle

Keep the current exact interrupt-frame, fixed wait/drain RSP and owned-root
contracts. Decide and prove how keyboard input is handled during audio: indefinite
IF0 polling cannot silently drop input or overflow the bounded queue. Audio work
must have bounded duration and preserve all required GPR/flags/stack contracts;
controller interrupts stay masked unless a separately reviewed interrupt path is
introduced. Display publication precedes speech for the same response, and halt
must not claim completion until its final speech and device retirement are done.
An unavailable device must not prevent an otherwise healthy text console from
working; DMA or controller invariant failure follows the fail-closed policy.

Before implementation, resolve stream chunking and limits, controller mapping and
authority, reentrancy, failure semantics, and exact privilege/stack/state ownership.
Prove the implementation on ISO and FAT with actual QEMU HDA output, absent-device
and induced failure cases, complete text/audio matching and independent review.
Current design is a proposal, not an implemented or qualified capability.

## Initial review and bounded profile refinement

Independent early review identified five implementation blockers: BAR discovery
authority and size, complete partial-initialization DMA retirement, responsive
input during playback, precise response/whitespace/cancellation semantics, and
end-to-end actual audio capture. The legacy driver enables decoding/bus mastering
before proving storage, does not fully retire CORB/RIRB on all failures, and can
report success after a stop poll expires. It must be redesigned, not copied.

For an initial QEMU-only profile, use an explicit emulated device identity and
fixed16-KiB BAR0 aperture, with no destructive BAR-sizing probe. A read-only QMP
`query-pci` observation in the existing q35/qemu64/standard-VGA/OVMF profile with
`ich9-intel-hda` found 8086:293e, subsystem 1af4:1100, bus0/device2/function0,
BAR0 nonprefetchable 32-bit memory of16,384 bytes. This is an emulator observation,
not authority for an arbitrary physical device or a general PCI sizing mechanism.
The final profile must also establish topology uniquely and revalidate all config
fields. Every page of the aperture must satisfy the reviewed overlap, identity and
strict effective-UC proofs before use; absent proof produces unavailable.

Candidate scheduling requires bounded fixed-RSP interrupt windows between short
foreground progress polls, not interrupts enabled inside the legacy polling loop.
The exact permitted saved RIP/flags, keyboard queue service and no-input wakeup
still need a complete proof before implementation. Later input must not overwrite
a response snapshot being spoken; define cancellation/backpressure and explicitly
cover final HALTED text. Distinguish an ordered recoverable token sequence from
perceptual intelligibility; pauses alone are not proof that all whitespace was
expressed. Bound the expanded response and stop on capacity failure rather than
truncate. Capture actual QEMU audio and compare its complete stream against the
independent response-to-token-to-PCM oracle, then listen to assembled clips.

A host-only feasibility sample with the pinned John model synthesized eight
isolated words/digits at roughly0.57–0.94 seconds each (22.05-kHz mono source,
noise scales zero, length scale1.05). This is not a device playback test or an
intelligibility assessment. It shows why maximum-response duration, responsive
keyboard handling and cancellation need explicit bounds before choosing a clip
strategy; no unbounded blocking speech may be introduced on that basis.
