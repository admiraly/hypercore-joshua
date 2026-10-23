# Owned PS/2 keyboard foundation (issue #38)

Status: proposed driver design, not runtime evidence. The first input driver targets
an explicit ACPI PC/i8042 profile and QEMU q35, not arbitrary USB/I2C/Latitude input.
It advances the real interactive-shell roadmap; firmware input and host terminal
emulation are not substitutes. No shell, USB stack or physical qualification is claimed.

## Authority before hardware access

Use the existing validated XSDT and retained ACPI read predicate. A separate bounded
discovery helper finds exactly one FACP table, proves its36-byte header before
length reads, requires revision>=3 and length244..65536 (revision5 needs268, revision6+ needs276),
then proves the entire
range/checksum before reading IA_PC_BOOT_ARCH at109. Require bit1 (8042), reject
reserved boot-architecture bits above5, and grant only ports60h/64h. Missing/clear
8042 is an explicit unsupported result, not permission to probe. Recheck the retained
Madt header/length/coverage/checksum and bounded record traversal before using ISO
records. This helper publishes a separate keyboard-authority record only at the end;
existing ACPI_CONTEXT layout and CPU/controller discovery contracts remain unchanged.

ISA IRQ1 defaults to GSI1/active-high/edge. Examine every type2 record (minimum10
bytes), requiring bus0, source<=15, defined polarity/trigger values and no reserved
flags. Conflicting/duplicate source records reject rather than selecting the last.
Conforming flags for ISA mean high/edge; IRQ1 overrides are accepted only if they
resolve high/edge in this initial keyboard profile. Reject another ISA source
aliasing the chosen GSI. Find exactly one existing validated IOAPIC range containing
that GSI; reject overflow, absent or ambiguous ownership. BSP APIC ID must fit8 bits
for physical IOAPIC destination delivery even in x2APIC mode. Existing controller
identity/effective-UC mappings, ID/version/count and quiescence proof remain mandatory.

Before any keyboard/controller write, verify this route is masked and uses a
maskable fixed/lowest/ExtINT mode; reject SMI/NMI/INIT/reserved assignments even if
masked. No preserved SMI/NMI route may be repurposed. All other external routes stay
masked. Only the selected entry may become fixed/physical/high/edge/vector44h,
destination BSP. Program low masked first, then high, read back destination and
writable routing fields, and unmask last. Serialize IOREGSEL/IOWIN under IF0 outside
ISR. Keep both legacy PIC masks atFFh; no IRQ1/PIC remapping fallback.

## Controller/keyboard initialization and lifecycle

Fresh state -> authority -> route masked/gate installed -> controller prepared ->
ready (still masked) -> enabled input window -> quiesced. All mutation requires BSP,
IF0, owned kernel root, inactive trusted/user contexts and a ready APIC context.
Publish no ready marker if a stage fails; partial hardware changes leave the route
masked and stop with a stage diagnostic. Never reset CPU, manipulate A20/output
port, enable the mouse, or invoke firmware/SMM control services. USB legacy/SMM
interception remains an unqualified platform limit.

Controller polls have a fixed100,000 status-read budget per individual wait (not a claimed
wall-clock duration); command phases have at most the bounded ACK/discard/retry
counts below; stale-output drain has32-byte capacity. Disable first and
second ports (AD/A7), drain to quiet, read command byte20h, clear IRQ1/IRQ12 bits,
disable second port and enable translation while preserving system/keylock fields.
Write via command60h and verify readback. Reserved bit7 must be zero. Test the first interface while it is still disabled (AB expects00), then enable
first port (AE), send F5 to stop any inherited scanning, then F0/02 (set2),
while scanning stays disabled. Each byte requires FA ACK, FE permits at most three retransmissions; FC/FD,
unexpected replies, AUX/error status or timeout reject initialization. The initial
F5 exchange alone may discard up to32 inherited scan bytes before its ACK, since
AE can resume an already-scanning device; they are never decoded under an unknown
inherited scan set. FA is the distinct ACK token in this supported keyboard profile.
No controller read-response transaction occurs with that inherited producer enabled;
AB and initial command-byte readback happen with the first interface disabled. Do not issue
keyboard/controller reset commands. This initial profile relies on verified scan-set
selection/translation, not firmware defaults. Before F4, set IRQ1 only in the command byte and verify the final configuration
while scanning is disabled and the IOAPIC entry remains masked. Publish prepared
ready without sending F4. Enable-window publishes its owned phase under IF0, sends
F4, consumes exactly its FA ACK, then permanently switches that transaction from
reply parsing to scancode decoding. It never performs a configuration readback after
F4. Unmask the proved route, stage any OBF bytes through the decoder before STI,
and tolerate a queued IRQ whose byte was already staged (it still requires actual
LAPIC ISR ownership before EOI). Host fixtures inject bytes immediately after ACK,
between staging and unmask, and after unmask; no arbitrary config byte is parsed
as input and no real scan byte is mistaken for a configuration reply.

The boot checkpoint initializes/proves the driver but leaves its IOAPIC route masked.
An explicit live test opens a bounded input window after the trusted worker/user
smokes have returned. Future shell code may use that same live lifecycle; normal
boot must not begin accepting arbitrary keyboard interrupts during user runs.
Enable verifies ISR/IRR state for44h is clear, records exact owned stack/wait bounds,
then unmasks route under IF0. Consumer uses CLI around queue operations, and waits
with STI;HLT followed by CLI. A timer deadline prevents a missing keyboard from
hanging a test; vector40h remains owned by an explicit keyboard-window timer adapter,
never falls through to the old16-delivery smoke counter. Cleanup first masks/stops the local deadline timer40h, masks the selected IOAPIC
route44h and disables controller IRQ1 generation under IF0 using the retained
command byte (write only, no post-F4 config reply). Disable the first interface AD;
no output-port/A20 or keyboard-reset write is used. Boundedly consume remaining OBF
bytes as terminal/discarded input. The terminal drain phase still owns both40h and
44h: accept either only at its exact drain continuation, send one EOI per actual
ISR ownership, never rearm either source. Require ISR and IRR clear for both vectors
before releasing ownership, with at most1024 STI/NOP/CLI windows. Drain exhaustion
stops rather than returning. Forced actual IRR40-only,44-only and simultaneous
40/44 fixtures plus suppressed-STI exhaustion must prove this retirement. Then
leave IF0. Failure to
quiesce stops; no new user process or console continuation resumes on a poisoned path.

## IRQ and event contracts

Vector44h is a DPL0 interrupt gate, IST0/current proved kernel stack, installed only
after prior user authority auditing has ended; its target cannot introduce CPL3
entry authority. IRQ save/restore preserves all15 GPRs, old RSP/SS/RFLAGS and DF via
IRETQ; helpers use aligned Microsoft x64 shadow space. Require kernel-origin CS,
expected kernel CR3 and active keyboard phase. Before frame reads, require the
saved frame address equals align_down(recorded_wait_RSP,16)-192, lies in the proved
kernel stack with at least4KiB helper reserve below it, and its entire192 bytes fit.
After that proof, require saved oldRSP equals recorded_wait_RSP, SS equals owned
kernel data, CS equals owned kernel code, and RIP is exactly keyboard_wait_close
(active phase) or keyboard_drain_close (draining phase). Each owned loop normalizes
IF0 flags immediately before STI;HLT;CLI or STI;NOP;CLI, so saved RFLAGS must be
exactly202h, or602h in the explicit DF preservation fixture only. No RF/IOPL/NT/VM/
AC/ID or other flag is accepted. The wait stack position is frozen across each
interrupt-enabled interval; consumer/helper calls occur only after CLI/CLD. Check
invalid RIP/SS/RFLAGS/oldRSP/frame-address/CR3 and out-of-phase fixtures before EOI
or IRETQ. Bound all nested handler/helper stack consumption below4KiB by static
accounting in implementation review. No FPU/SIMD state is used. An unexpected keyboard IRQ outside its
owned window is fatal rather than silently EOIing an unowned vector.

The ISR reads at most32 status/data pairs. It consumes OBF only, discards AUX bytes,
counts timeout/parity errors and resets prefix/modifier state on corrupted input.
If OBF remains full at the bound, mark failure and mask IRQ generation through the
owned controller port (not IOAPIC selector access), then EOI exactly once before
returning to the exact failure continuation. No allocator, printing, framebuffer,
IOAPIC accesses, firmware calls or unbounded loop runs in ISR. Check actual LAPIC
ISR44 ownership before EOI; no EOI on a software/spurious unowned entry.

Pure `keyboard_decode(context,byte)` handles translated set1 make/break, both shift/
control/alt states, caps lock edge changes (repeat make does not retoggle), E0 and
E1 Pause sequences and PrintScreen fake shifts. Rejected/unknown sequences do not
leave modifiers stuck. Ignore ACK/error protocol bytes in running input, counting
protocol errors and clearing transient prefix state; no command reply becomes text.
US mapping emits printable ASCII, Enter13, Backspace8, Tab9, Escape27; Ctrl letters
emit1..26, Alt text is suppressed. Extended arrows/home/end/delete may use explicit
non-ASCII event tokens; no arbitrary Unicode/layout promise. Queue holds64 word
events, head/tail indices0..63 and count0..64. Overflow drops newest event and
increments a checked/saturating drop counter without changing older queued bytes.
Consumer and decoder are BSP/IF0 serialized. Corrupt queue metadata rejects before
any queue read/write. State/queue memory is caller-owned and separately bounded.

## Evidence and next shell step

Host exact-Assembly authority/routing/command/decoder helpers use independently
constructed ACPI/controller models, malformed/bounds/error/retry/queue sequences,
random input and eight integer ABI sentinels. Test no unauthorized writes and
failure-before-ready. Prefix/malformed input, caps repeat, overlapping shift releases,
Ctrl/Alt, Pause/PrintScreen and full queue require explicit oracles.

QMP send-key events must cross actual PS2->IOAPIC->ISR, producing exact ordered
normal/shift/caps/control/edit events with releases and BSS-backed queue evidence.
Require nonzero real IRQ/EOI counts, preserved wait-register/DF sentinels, bounded
missing-input failure, overflow handling and out-of-window rejection. Independently
read actual IOAPIC route/LAPIC/queue state rather than trusting a success string.
Run ISO/FAT/x2, all host/user/fault/APIC/context/ELF/repro regressions. Keyboard test
images are separate; no injected QMP shortcut calls the decoder directly.

After this driver gate, implement the real WOPR/Joshua line editor and deterministic
command dispatch on this queue with framebuffer echo; retain kernel/user isolation
and input latency independent of AI/audio. No keyboard-foundation result marks the
interactive shell or broad OS objective complete.

Sources: [ACPI6.6](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html),
QEMU's [controller](https://github.com/qemu/qemu/blob/master/hw/input/pckbd.c),
[keyboard](https://github.com/qemu/qemu/blob/master/hw/input/ps2.c) and
[FADT construction](https://github.com/qemu/qemu/blob/master/hw/i386/acpi-build.c).
These define the tested emulator profile, not physical-device qualification.


## Design review responses

P1 configuration race: final command-byte readback now precedes F4 while scanning
is disabled; enable-window has an explicit FA-ACK-to-input transition, stages
immediate input and never asks for an arbitrary controller reply after scanning starts.
P2 deadline retirement: stop/mask both sources first, retain exact drain ownership
for40h/44h, require both ISR/IRR empty, and test individual/simultaneous pending and
exhaustion. P2 frame contract: exact phase-specific RIP, selectors, normalized flags,
oldRSP and align_down(oldRSP,16)-192 relationship precede EOI/return; malformed-frame
fixtures and a4KiB helper-reserve audit are mandatory. Live IRQ/window runtime remains open.


## Pure component interfaces and evidence

KEYBOARD_DECODER is216 caller-owned bytes: ready, six modifier bits, caps/latch,
E0/Pause state, head/tail/count, saturating drop/protocol counters and64 inline word
events. Init/check/decode/pop/resync use the Microsoft x64 integer contract. Pop
returns EAX0/EDX event,2 empty,1 corrupt; no writable event pointer is accepted.
Metadata includes tail==(head+count) mod64. Resync preserves queued events/caps,
clearing transient modifiers/prefixes. Num-lock/keypad layout modes are not supported;
E0 keypad Enter/slash have explicit events. Ctrl is letters-only; Alt text and
unsupported modified navigation are suppressed. No user pointer reaches this API.

Pure decoder tests pass180,744 cumulative checks including40,000 mixed operations;
source review found no actionable findings. Authority tests pass1,890 checks with
500 generated ISA maps, sharing the actual ACPI read predicate while modeling only
paging lookup. Review's P3 coverage fixture now puts a complete readable FADT header
before an unproved PROT_NONE payload page; follow-up cleared. FADT revision length
floors are244 for3/4,268 for5 and276 for6+, rather than trusting an early-field prefix.

The controller command policy is separate from fixed port adapters. Its prepare
phase checks a positive authority record before any port access, prepares scanning
OFF, verifies final configuration before F4 and publishes ready last. Enable consumes
F4 ACK only; immediate scan bytes remain for input. Disable writes retained IRQ-off
configuration without readback and disables the first interface. It cannot unmask
IOAPIC or grant interrupt-window ownership. Current503 modeled-port checks exercise
actual policy code through host-only callbacks; they establish no real port/IRQ
behavior. Integration and the full target/negative/drain campaign remain open.


The exact frame helper now has a 288-byte trusted KEYBOARD_WINDOW metadata contract.
It accepts only active/draining phases and the exact 192-byte frame derived from
recorded wait RSP. Read-only host fixtures cover 4,694 cumulative assertions,
including protected unreadable-frame rejection and eight integer ABI sentinels.
Independent source review found no actionable findings under trusted metadata and
proved-readable-stack preconditions. No privileged adapter, actual interrupt return,
helper-stack audit or deadline retirement is established by this component review.
