# Native BSP development console (issue #40)

Status: editor and renderer implementations host-validated; console runtime integration proposed. This builds on issue #38/PR #39's real PS2 path.
The destination is an interactive, non-installing development preview, not the full
user-space shell/capability system in the specification. Runtime stays Assembly.
No speech, storage/network/AI, firmware-after-exit, process restart or merge changes.

## Authority and delivery profile

Add an explicit `-Console` build profile with a separate default output directory
and a clearly named interactive ISO. Preserve the checkpoint and all existing
fixture profiles. Document the interactive command prominently; do not call a
halted checkpoint usable. No boot menu, fake terminal, firmware input or host
command-string shortcut supplies the console. It consumes the actual IRQ queue.

The trusted console runs at BSP ring0 after both private ELF demonstrations have
returned. It is a bounded foreground diagnostic client, not a new public process
ABI. Input selects only fixed read-only handlers. It cannot become code, a pointer,
port number, format string, capability grant, model policy or unchecked allocation.
Do not weaken existing private roots, W^X, owned stacks or firmware authority.

Refactor common keyboard activation/wait/retirement from the fixture consumer.
Both test and console policies retain the existing exact192-byte saved frame,
root/selectors/oldRSP/RIP, RF-only hardware tolerance and4-KiB reserve contracts.
Console wait has its own exact CLI continuation and normalizes flags before
STI/HLT. Helpers/parser/renderer run IF0/DF0 outside interrupt-enabled intervals.
A genuine IRQ44 still proves actual ISR ownership and EOIs once; no console code
runs in ISR. Refuse unexpected vector40 while the console owns no deadline timer.

Intentional idle has no missing-input deadline: the timer is stopped/masked and
the CPU waits for keyboard input, without polling or overflowing tick counters.
On explicit halt, reuse mask/disable/flush/joint drain with exact drain continuation
and1024-window bound. Keep ownership until both ISR/IRR are empty, then IF0 halt.
On collection/metadata/permission/quiescence failure, stop rather than resume a
poisoned console. No recovery, AP/SMP, NMI/MC resilience or physical compatibility
is inferred. Instrumented producer waits remain exclusively KeyboardTest mode17.

## Pure editor contract

Caller-owned state is1568 bytes,8-byte aligned, with no padding/reserved fields:
12 little-endian qwords at offsets0..88 are ready, length, cursor, history_count,
history_next, history_position, draft_length, draft_cursor, submitted_length,
submissions, rejected and cancellations. Line/draft/submitted buffers occupy128
bytes each at96/224/352. Eight qword history lengths start480; eight128-byte slots
start544. No pointer lives in this state. BSP/IF0/DF0 serialization and Microsoft
integer ABI apply.
All returning helpers preserve RBX/RBP/RSI/RDI/R12–R15, use aligned shadow space,
and use no SIMD/red zone. Init zeroes state and publishes its magic last. Check is
read-only; event validates the entire state before mutation or event-driven indexing.
Invalid state/input width rejects atomically. Unsupported16-bit events are ignored.

One127-character printable-ASCII line plus a NUL/trailing-zero128-byte buffer; cursor
is0..length. Eight128-byte history slots each have an explicit length. Count0..8,
next0..7, browse0..count (0 live draft). Before history fills, next==count and only
slots0..count-1 are nonempty; afterward all eight are nonempty. Every line has
printable bytes before its length and zero padding from length through127. Check
active/draft/submitted buffers and every history slot before event-driven indexing.
Draft length/cursor are always bounded even while live; a valid stale draft is
permitted at browse0. While browse>0, active length/text must equal the selected
stored history entry (cursor may move). Unused slots must have length0 and all128
bytes zero. Submitted has a length but no cursor. All three counters are unsigned
64-bit, saturating at2^64-1. Trusted canonical mapped storage extent and exclusive
ownership are caller preconditions, not inferred from magic. Helpers check nonzero,
8-byte alignment and nonwrapping pointer+1567 before any access; they cannot prove
arbitrary address mapping.

Insert printable32..126 at cursor by bounded backward copying without STD. At full
length leave line/cursor/history unchanged and saturate a rejected-input counter.
Backspace/Delete shift only validated remaining bytes and zero the vacated tail.
Left/Right/Home/End adjust cursor within the line. Tab and unsupported controls do
not become terminal control sequences. Ctrl-C clears active/draft and browse state,
saturates a cancellation counter and executes no command. No audio/bell dependency.

Up first snapshots the live draft/cursor, then recalls newest through oldest;
Down returns toward newest and finally restores that draft/cursor. Actual edits
detach browsing, without modifying stored history. Cursor-only movement does not.
Enter snapshots the complete line into separate submitted storage, appends each
nonempty submission to the eight-slot history ring, then clears active/draft and
browse state. Duplicate nonempty commands are allowed. Empty Enter still submits
an empty line but adds no history. Submissions/rejections/cancellations saturate.

`editor_init(RCX state)` intentionally resets any caller-owned storage, including
an existing/corrupt editor, only when no consumer is active. It returns EAX0/EDX0;
bad pointer alignment/nonwrapping range returns EAX1/EDX0 without access. No other
validity is required before reset. `editor_check(RCX state)` returns EAX0/EDX0 valid,
EAX1/EDX0 invalid, read-only. `editor_event(RCX state, EDX event)` returns EAX0 success with EDX action0 ignored,
1 changed,2 submitted,3 full,4 cancelled; EAX1 invalid leaves state unchanged.
Every EAX1 failure sets EDX0 and leaves the entire state unchanged. Existing
KEY_LEFT/RIGHT/HOME/END/DELETE/UP/DOWN constants come from sdk/keyboard.inc; ASCII13
submits,8 backspaces,3 cancels,32..126 inserts. Tab9, Escape27, ASCII127 and other
unsupported16-bit events are ignored. Event width>65535 rejects even if its low
word is recognized. Submitted bytes/length remain unchanged until the next
successful Enter or explicit init; edits/history/cancellation cannot mutate them.
Submitted text is data, never an executable address or format string.

## Console model and framebuffer

Use a fixed maximum100-column by48-row byte-cell grid with bounded dirty-row bits.
Cell size12x16, glyph5x7 scaled2, original printable ASCII glyphs. Minimum48 columns
and20 rows; derive usable dimensions from validated framebuffer geometry with16-pixel
margins and reject a surface that cannot fit. Clamp to maxima, never write stride
padding or beyond width/height/byte size. Validate multiply/add/address overflow and
pixel format0/1 before any store. Each renderer entry receives a separate trusted
surface capability derived from validated BOOT_INFO, and compares mutable context
base/size/geometry/format against that anchor. Grid storage is inline, not a mutable
external pointer. The renderer independently checks its full state,
cell bounds, dirty mask and source glyph index; hostile metadata cannot redirect
framebuffer pointers beyond that independently trusted capability. The anchor and
its mapped framebuffer coverage remain caller proofs; arbitrary corruption of both
anchor and context is outside this contract.

Keep two header rows, a bounded scrolling transcript, one prompt row and one result
row. Original JOSHUA blue-white on near-black, fixed-width text and a static visible
cursor; no flicker, shader, sound, model or network dependency. Optional IMSAI green
can be a later change. Color packing follows the supported RGB/BGR GOP format.
Clear each changed cell before drawing; render only dirty rows during editing.
Scroll only model cells, then repaint checked rows. Never copy unproved framebuffer
ranges. All loops have fixed geometry bounds, including full clear/scroll.

The line editor remains127 characters even on a narrower screen. Show a bounded
horizontal viewport with the cursor inside the input span, accounting for the fixed
`JOSHUA> ` prompt; no wrap/index arithmetic may escape the prompt row. Submitted
commands appear in the transcript independently of the live input buffer. An
unknown or unsupported character renders a safe placeholder, never a font offset.

## Commands and output

Parse only the checked submitted length, ASCII-case-insensitive command names,
with bounded leading/trailing spaces and at most one argument. Initial implemented
commands: help, about, status, mem, ps, devices, clear, halt. Diagnostics read owned
boot/frame/process/APIC/keyboard records; they do not restart processes or allocate.
Recognize destination features only to report unavailable, never fabricate output
for files/net/model/agents/grant/revoke/bench/logs/shutdown. Halt means quiesce and
stop, not firmware power-off. No disk writes or firmware reset/power requests.

Result codes:0 success,2 usage,69 unavailable,127 unknown. An empty line is a no-op
success. A `--json` diagnostic option for status/mem/ps/devices produces bounded
machine-readable output with constant keys and integer fields. Never interpolate
unchecked user bytes; error JSON uses constant names/statuses. Other arguments
are usage errors. Use a bounded response buffer and checked append primitives;
capacity failure stops rather than printing past a terminator. The result row and
a fixed-size last-command record expose status without pretending to be a public
syscall. No arbitrary shell expansion, scripting language, redirection or execution.

## Layout and validation gates

Allocate fixed console/editor/response/grid state within owned image BSS. Immutable
font/command text stays RO/NX; executable helpers RX, state RW/NX. Measure NASM code
and constant sizes before adjusting PE layout; update SDK, headers, reservations,
protection proofs, host fixtures and documented bounds together. Preserve versioned
boot-info and USER_CONTEXT/USER_PROCESS layouts. A larger image cannot evade existing
reservation, identity/cache, whole-image permission or chunk-ownership proofs.

First implement/test the pure editor. Use independent host reference behavior,
random mixed editing/history/submit sequences, all boundaries, corruption atomicity,
protected guards and eight integer ABI sentinels. Then test parser/output/model/
renderer against independent oracles and guard bytes for every surface edge, tiny
geometry, overflowing stride/base/size, formats, cursor/scroll and invalid metadata.

Actual QMP sessions use keyboard device events only: editing, repeated commands,
history/capacity/unknown/usage/JSON, scrolling, at least30 seconds idle, then halt.
Capture real queue/editor/console/status/ownership/IRQ evidence and framebuffer
pixels; independently validate expected screen model and visually inspect QMP
screenshots. Require bounded safe halt retirement and no hidden fixture-only
entry in the interactive path. Keep diagnostic fixtures distinct from the testable
interactive ISO. Run ISO/FAT/x2 and all existing host/user/ELF/fault/interrupt/input
regressions plus reproducibility, independent source/evidence reviews and a linked
stacked PR against feature/input-ps2-keyboard. Windows and physical gates remain
explicit; no milestone completion is inferred from QEMU or a good-looking prompt.


## Initial design review responses

P2 layout/validation: exact1568-byte,8-byte aligned pointer-free layout, all buffer
length/cursor/padding/history occupancy/browse equality rules and64-bit saturation
are specified. P2 no-ops: only successful text mutations detach, full insertion
preserves browsing/draft, and all boundaries return ignored without mutation.
P2 ABI: init/check signatures, trusted extent/reinitialization policy, EDX0 on
failure, existing keyboard tokens and submitted-buffer lifetime are explicit.
Renderer/parser/lifecycle refinements remain mandatory before their implementation;
the pure editor does not establish those future components or runtime delivery.

## Editor implementation checkpoint

The Assembly helper in `kernel/console/editor.inc` implements the reviewed
1568-byte contract without a boot call site. `tests/Test-ConsoleEditor.ps1`
assembles the actual helper into a Linux host harness. An independent Python
model compares complete state bytes and result actions after every event.
The local run passed 344,659 cumulative checks (not independent scenarios),
including all 65,536 input tokens, 20,000 seeded mixed events, history wrapping,
full insertion, saturation, corruption atomicity, eight integer nonvolatile
register sentinels, and extents adjacent to inaccessible pages. The protected
pages test caller-provided extent bounds; they do not establish arbitrary pointer
mapping safety. The suite is wired into Linux CI; CI execution remains pending.

This checkpoint does not deliver an interactive ISO. No runtime image includes
the helper yet; renderer, parser, returning IRQ integration, retirement, target
validation and Windows/physical qualification remain open. Prior design review
resolved exact layout, no-op/browsing semantics and helper/result ABI findings.
Independent read-only implementation review found no actionable findings. It
did not run tests or independently reproduce the reported count.

## Exact renderer contract (review before implementation)

A separately trusted `CONSOLE_SURFACE` is 48 bytes, eight-byte aligned, containing
six qwords: mapped base, mapped byte size, visible width, visible height, stride
in pixels, and GOP format (0 RGB-reserved, 1 BGR-reserved). Caller proves this
anchor and framebuffer extent mapped, writable, exclusively owned and stable
through each call. Caller proves backing-storage disjointness among framebuffer,
anchor and grid, including distinct virtual aliases of the same physical storage.
Every helper also rejects direct virtual-range overlap among the three complete
extents before mutation. Prompt composition additionally requires a backing-
disjoint editor source and rejects its direct virtual overlap with those extents. Helpers check
nonzero base, format, width/height, stride>=width, checked stride*height*4<=size,
and nonwrapping base+size-1 before stores. Surface and grid pointers themselves
require trusted readable extents, nonzero eight-byte alignment and nonwrapping
extent arithmetic. No arbitrary virtual mapping validation is implied.

`CONSOLE_GRID` is 4904 bytes, eight-byte aligned: thirteen qwords at offsets0..96
are ready, base, size, width, height, stride, format, columns, rows, dirty,
cursor_x, cursor_y and cursor_enabled. Inline cells start104, exactly4800 bytes
in 48 physical rows of100 bytes each. No other pointer, padding or reserved field.
Surface copies must equal the independent anchor on every entry. Columns equal
min(100,floor((width-32)/12)); rows equal min(48,floor((height-32)/16)). Reject
columns<48 or rows<20, and reject width/height subtraction underflow. All active
cells are ASCII32..126. Every inactive column/row byte is zero. Dirty bits above
rows must be zero. Cursor_enabled is0/1; cursor_x<columns and cursor_y<rows even
when disabled. Magic alone grants no authority. Check validates the entire grid,
including all4800 bytes, before any mutation or framebuffer access.

The row partition is exact: headers0/1; transcript2..rows-3 inclusive (rows-4
rows); prompt rows-2; result rows-1. Model scrolling moves only transcript cells,
clears the final transcript row to spaces and marks those rows dirty. It never
moves framebuffer bytes. Full initialization fills active cells with spaces,
zeroes inactive storage, disables the cursor at0,0 and marks all active rows dirty.
Initialization is permitted only for inactive exclusive storage; it validates the
anchor first and publishes readiness last. Initialization itself writes no pixels.

`grid_init(RCX grid,RDX anchor)` and read-only `grid_check(RCX grid,RDX anchor)`
return EAX0/EDX0 or EAX1/EDX0. `grid_cell(RCX grid,RDX anchor,R8D linear_index,
R9D ASCII)` uses physical row stride100: index/100<rows and index%100<columns;
ASCII must be32..126 and index<=4799. Unchanged cells are successful no-ops;
changed cells mark their row dirty. Widths above32 bits in the qword argument
registers reject. `grid_cursor(RCX grid,RDX anchor,R8D linear_index,R9D enabled)`
uses the same checked index, enabled0/1, and marks both old/new rows dirty on
change. `grid_render(RCX grid,RDX anchor)` validates the complete grid and anchor
before stores, renders dirty rows, then clears dirty. All successful calls, including unchanged cells/cursors and clean renders, return
EAX0/EDX0. All failures return EAX1, EDX0 without state or framebuffer changes. Integer nonvolatile preservation,
aligned shadow space, DF0/BSP/IF0 and no SIMD/red-zone rules match the editor.

Cell origin is (16+12*x,16+16*y). Each active cell is fully repainted background
before foreground: its12x16 pixels, and no margins, inactive cells or stride
padding, are touched. Glyphs use seven five-bit rows, bit4 leftmost, scaled2 at
cell origin; final two columns and bottom two scanlines remain background.
An enabled cursor adds a foreground12x2 underline at scanlines14/15, independently
of glyph text. Original printable ASCII font data is immutable and indexed only
by checked character-minus32 in0..94. Background RGB is (2,6,12); foreground RGB
is (170,220,255); reserved byte is zero, packed according to format0/1. Validate
maximum cell rectangle against width/height and checked surface arithmetic;
each pixel address is base+4*((16+16*y+dy)*stride+16+12*x+dx), with bounded
x/y/dx/dy and the already proved full-surface extent. No store may exceed that
extent. Dirty state is cleared only after the deterministic non-failing store loop.
Hardware faults despite trusted mapping are stop-only, not failure atomic recovery.

The prompt prefix is exactly eight cells `JOSHUA> `. Input width W=columns-8
is at least40. For editor cursor C, viewport start S=max(0,C-(W-1)); render
line[S:S+W] truncated to line length then pad with spaces; cursor is at8+C-S.
This reserves a visible cell even when C==length==127. Prompt composition validates
both complete states before copying and marks the prompt row dirty. Transcript,
header/result composition and response wrapping are later model helpers; these
cannot bypass grid validation or the trusted surface anchor.


Renderer design review responses: direct virtual overlap is rejected for the full
framebuffer/anchor/grid extents; caller proves physical backing disjointness,
including aliases, and prompt source disjointness. All helper success/no-op returns
are explicitly EAX0/EDX0. Independent re-review accepted both fixes. Prompt composition is deferred to the
model integration slice; its helper signature and explicit cursor-enable behavior
must be frozen before implementation.


## Renderer implementation checkpoint

The five named grid helpers and the original95-glyph font now exist as Assembly
source, without any boot call site. The Linux host runner exercises the actual
helpers and all eight integer nonvolatile register sentinels. Its independent
pixel-placement/color oracle compares the complete framebuffer, including clean
rows, margins and stride padding. Font data is the authored asset; two literal
raster checks establish selected indexing/content, not an independent font oracle.
The local suite passes5,055 cumulative checks across RGB/BGR, minimum/clamped
geometry, all printable characters, cursor/dirty behavior, invalid metadata,
overflow, direct overlap and protected-page boundaries for all three extents.
It exports `out/console-grid-tests/console-grid-preview.png` from actual Assembly
pixel output; this has been visually inspected as a host-surface font preview,
not a boot or interactive-console screenshot. Source-only independent review found
no actionable runtime/oracle defect; the final guard/preview/CI additions also passed
a read-only follow-up review with no actionable findings. No prompt/model/parser/IRQ integration is implied.
