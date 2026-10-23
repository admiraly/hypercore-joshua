# Native BSP development console (issue #40)

Status: experimental native console profile integrated; target qualification in progress. This builds on issue #38/PR #39's real PS2 path.
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

## Exact parser contract (review before implementation)

`command_parse(RCX editor,RDX result)` accepts an entire checked editor state,
using only its submitted buffer/length. The caller proves trusted mapped canonical,
backing-disjoint extents (including physical aliases), serialized BSP/IF0/DF0.
Helpers reject null, unaligned, wrapping or directly overlapping extents before
reads driven by submitted text or result stores. The result is40 bytes, aligned8,
five qwords: ready, command, status, json, action. `COMMAND_MAGIC` is
0x31444e4d43484f4a; every successful parse publishes exactly this ready value
last. Later result consumers must validate this magic and all field combinations
against the grammar before indexing or executing a clear/halt request. Explicit parsing replaces any
previous result, including corrupt storage, only after complete input validation;
readiness is published last. All successes return EAX0/EDX0, including usage/unknown/
unavailable results. Invalid input/pointers return EAX1/EDX0 with both extents
unchanged. Preserve all eight integer nonvolatile registers and existing stack ABI.

Command IDs are0 empty;1 help;2 about;3 status;4 mem;5 ps;6 devices;7 clear;8 halt;
9 files;10 net;11 model;12 agents;13 grant;14 revoke;15 bench;16 logs;17 shutdown;
255 unknown. Names match ASCII letters case-insensitively, exactly, with no prefixes.
Only ASCII space32 separates tokens; no quoting, escapes, expansion or punctuation
syntax exists. Submitted bytes already satisfy the editor's printable32..126 and
zero-tail rules. Trim leading/trailing spaces; multiple interior spaces collapse
as separators. Never change the original submitted text or truncate tokens.

Precedence is exact: all-space/empty => command0/status0/json0/action0. Otherwise
identify the first token's command ID, then count remaining tokens. More than one
argument => status2 for every command, including unknown. With zero/one argument:
unknown=>status127 regardless of its single argument; unavailable IDs9..17=>69
only without an argument, otherwise2; implemented IDs1..8=>0 without arguments;
only IDs3..6 accept the single exactly case-sensitive argument `--json`, yielding
status0/json1. Every other implemented single argument =>2. json is zero for all
other outcomes. Action is1 clear or2 halt only for successful IDs7/8, otherwise0.
Empty input does not request clearing or halting. The parser performs neither
operation, reads no hardware, and never executes a response or command pointer.

The fixed-size result remains authoritative independently of bounded transcript
wrapping. Response generation will consume a fresh checked result plus an owned
snapshot into a separately bounded buffer; exact snapshot/schema/append contracts
must be reviewed before implementing that later helper. This parser slice does
not itself generate JSON, read hardware or expose a public shell ABI.

## Planned response snapshot boundary

Responses will use immutable labels plus decimal unsigned integers copied from
owned records while IF0. They will not trust input as a format string or reread
firmware tables. The exact fixed snapshot layout and JSON key schemas remain a
review gate; no response helper is implemented by the parser checkpoint. All
responses must fit a4095-byte payload plus terminator. The authoritative result
record survives transcript wrapping, and long text is wrapped into the bounded
transcript rather than claiming the visible final screen contains the whole reply.


Parser design response: readiness now has the exact named COMMAND_MAGIC value
0x31444e4d43484f4a, published last on every successful parse. Rejected calls retain
the full previous result; later consumers validate all combinations as well as magic.


## Parser implementation checkpoint

`kernel/console/command.inc` and its immutable name table implement only parsing,
not responses or actions. The actual Assembly helper passed95,317 cumulative host
checks against an independent Python grammar model: command case permutations,
exact option spelling, token precedence,127-byte boundaries,15,000 seeded mixed
inputs, complete editor corruption rejection, direct overlap, protected extents
and all eight integer nonvolatile sentinels. Source-only independent review found
no actionable runtime/oracle findings; it did not reproduce the test run. Linux
CI now invokes this suite; CI execution remains pending. The readiness-design P2
was fixed and re-reviewed before coding. No boot call site exists yet.

## Exact response contract (review before implementation)

`CONSOLE_FACTS` is128 bytes, aligned8, with sixteen qwords: ready, image_bytes,
free_pages, live_allocations, candidate_ranges, owned_table_pages, enabled_cpus,
apic_mode, timer_eois, keyboard_irqs, keyboard_eois, keyboard_drops,
process_a_state, process_a_status, process_b_state, process_b_status.
`FACTS_MAGIC=0x3154434146484f4a`. Nonzero image_bytes/owned_table_pages,
enabled_cpus1..256, apic_mode1/2, process states0..5 and keyboard_eois<=keyboard_irqs
are required; all other values are unsigned64. Status values are raw unsigned64,
not pointers or signed formatting directives. Boot adapter copies from current
owned BOOT_INFO/image bounds, FRAME_ALLOCATOR.free_pages/count, its RANGE_SET.count,
OWNED_PAGING.table_count, ACPI_CONTEXT.enabled, APIC_CONTEXT.mode/eois,
KEYBOARD_WINDOW IRQ/EOI counters, decoder drops and both USER_PROCESS state/status
records. It must check source readiness, source count bounds and stable IF0 before
publishing the snapshot. Pure response tests do not qualify that future adapter.

`CONSOLE_RESPONSE` is4112 bytes, aligned8: ready and length qwords, then4096 bytes.
`RESPONSE_MAGIC=0x3150534552484f4a`; length0..4095, body bytes printable32..126 or
LF10, then zero bytes through body[4095]. No arbitrary pointers/reserved fields.
`response_build(RCX command,RDX facts,R8 response)` validates all three complete
extents, alignment, nonwrapping arithmetic and pairwise direct virtual disjointness
before mutation. Caller proves canonical mapped stable backing disjointness,
including physical aliases, BSP/IF0/DF0 and exclusive output. Previous output may
be corrupt and is intentionally replaced. Every error returns EAX1/EDX0 with all
three extents unchanged. Success returns EAX0/EDX0, leaves inputs unchanged, zeroes
unused output, and publishes readiness last. Preserve eight integer nonvolatiles
and aligned shadow space, no SIMD/red zone.

Command validation is exact, independent of magic: ID0 only status0/json0/action0;
IDs1..8 permit status0 or2; status2 requires json0/action0. Successful IDs3..6 allow
json0/1, others require0; successful ID7 action1, ID8 action2, all others action0.
IDs9..17 allow69 or2 with json0/action0; ID255 allows127 or2 with json0/action0.
All other combinations reject. A forged clear/halt action on an error never passes.
The response builder only produces data; it never clears, halts or dispatches code.

Output is bounded before publication: perform a deterministic measuring pass using
only validated immutable template labels and snapshot integers, rejecting any append
above4095 before touching output. Then zero output and repeat the identical emission
with the proved exact length; inputs and templates remain stable under the caller's
ownership preconditions. Per-number scratch is at most20 digits; decimal zero emits
one digit, unsigned2^64-1 emits20. Every append checks available capacity in the
measuring pass; no unbounded strlen or unchecked input interpolation exists. The
emission pass uses the proved length without a recoverable failure after stores.
The owned mappings are trusted: hardware faults are permanent-stop events, not
rollback. Response stack usage must be audited before foreground integration.

Exact human output uses LF endings. Empty command emits empty body. Help emits
`HELP ABOUT STATUS MEM PS DEVICES CLEAR HALT\nSTATUS|MEM|PS|DEVICES --json\n`.
About emits `JOSHUA / HYPERCORE\nBSP DEVELOPMENT CONSOLE\nASSEMBLY RUNTIME. NO AI EXECUTION.\nQEMU EVIDENCE; PHYSICAL QUALIFICATION OPEN.\n`.
Clear emits `CONSOLE CLEARED\n`; halt emits `HALTING: QUIESCING KEYBOARD\n`, never
claiming that hardware has halted before retirement. Errors use exactly
`USAGE ERROR (2)\n`, `UNAVAILABLE (69)\n` or `UNKNOWN COMMAND (127)\n`, never echoing
input into labels or formats.

Diagnostic field order is fixed:
- status: image_bytes, enabled_cpus, apic_mode, timer_eois.
- mem: free_pages, live_allocations, candidate_ranges, owned_table_pages.
- ps: process_a_state, process_a_status, process_b_state, process_b_status.
- devices: apic_mode, enabled_cpus, keyboard_irqs, keyboard_eois, keyboard_drops.

Human diagnostic output is the uppercase command name plus LF, then each exact
field name followed by `=`, unsigned decimal and LF. JSON output is one object
with no spaces: `{"command":"<lowercase name>","status":0,<fields>}\n`, where
<fields> is the same ordered flat set of quoted field names and unsigned decimal
integers. No array, escape, arbitrary user string or optional key is emitted.
Schemas distinguish process A/B explicitly; they do not claim a task-list syscall.
The fixed result/response remain authoritative even if old rows scroll offscreen.
These precise schemas supersede the earlier generic error-JSON suggestion: usage,
unknown and unavailable results have json0 and the exact constant human errors.


## Response implementation checkpoint

The pure Assembly response builder passed28,687 cumulative checks against an
independent Python byte/schema oracle. Coverage includes command/status/json/action
combinations,1,000 seeded snapshots, raw unsigned64 extremes, JSON decoding, complete
zero tails and immutable inputs, all overlap/pointer/semantic rejection paths and
protected boundaries. A focused adapter invokes the actual internal append gate at
4095-byte and unsigned-overflow boundaries; it does not substitute a modeled gate.
All public builder calls use eight integer nonvolatile sentinels. Current fixed
responses cannot fill the full4095-byte buffer; the separate append tests cover
that capacity boundary. Independent source review found no actionable findings.
The reviewer did not run tests or independently reproduce results.

Static stack audit is288 bytes below response-builder entry RSP (296 including
its incoming return address), deepest through format/number/append. Decimal scratch
is outside shadow space. No ISR calls this formatter. Actual foreground integration
must establish sufficient owned stack reserve and stable source provenance. No
snapshot adapter or boot call site exists yet, and `CONSOLE CLEARED` must be
published to the visible transcript only after the model clear succeeds.

## Exact screen-model contract (review before implementation)

`CONSOLE_MODEL` is64 bytes/aligned8, eight qwords: ready, transcript_row,
transcript_column, completed, command, status, json, action. `MODEL_MAGIC` is
0x314c444f4d484f4a. Row is2..grid.rows-3 inclusive; column is0..grid.columns
inclusive. Column==columns is a pending wrap, not an out-of-range cell index.
Completed saturates at2^64-1. Last command/status/json/action must satisfy the exact
command-result combination validator; initialized values are all zero. No pointers,
reserved fields or implicit cursor live in this record. All helpers are BSP/IF0/DF0,
preserve eight integer nonvolatiles and use aligned shadow space without SIMD.

Public signatures (Microsoft integer ABI):
- `model_init(RCX model,RDX grid,R8 anchor)` resets inactive exclusive model storage,
  requiring an already valid grid and anchor. No prior model validity required.
- `model_check(RCX model,RDX grid,R8 anchor)` is read-only.
- `model_prompt(RCX model,RDX grid,R8 anchor,R9 editor)` validates all records and
  updates the prompt with the exact earlier viewport formula. It explicitly enables
  the cursor at column8+C-S, row rows-2, and dirties both old/new cursor rows and the
  prompt. Prefix is exactly `JOSHUA> `; remaining prompt cells are spaces.
- `model_complete(RCX model,RDX grid,R8 anchor,R9 editor,stack+40 command,stack+48 response)`
  validates all records, appends the submitted command and response, records the
  command result, and updates the prompt. Stack offsets are at helper entry.

All return EAX0/EDX0 success, EAX1/EDX0 failure. All returning failures preserve
all metadata and framebuffer bytes. No helper writes pixels or calls port I/O.
Before any mutation validate complete grid/anchor and model (except init), all
other record extents, null/alignment/nonwrap and every pairwise direct virtual
range overlap, including the full framebuffer extent. Caller proves stable mapped
canonical backing-disjoint storage including aliases for model/grid/anchor/editor/
command/response/framebuffer and stack, plus ownership/exclusivity. Grid's existing
surface proof remains authoritative. Invalid arbitrary pointer mapping is outside
the contract. All source records are read-only. Init publishes model magic last;
other helpers have no recoverable failure after their first store.

`command_check(RCX command)` will extract the existing response builder's exact
result-combination validation into a shared read-only helper, EAX0/1/EDX0, including
magic and pointer extent. Response behavior must remain unchanged. Model_check
validates its last fields using the same combinations, not magic alone.
`response_check(RCX response)` validates readiness, length0..4095, printable/LF
body and all-zero tail through4095, read-only. Complete calls command_parse into
exclusive local scratch and compares all40 bytes against the provided command
before mutation. Thus a valid-looking clear/halt result for different submitted
text is rejected. Caller supplies response_build's output for that same command;
model_complete validates its bounds/alphabet but does not recompute its text from
facts. No response byte becomes a control directive.

Init clears active cells to spaces (inactive cells stay zero), puts
`JOSHUA / HYPERCORE` in row0, `BSP DEVELOPMENT CONSOLE | NO AI EXECUTION` in row1,
`JOSHUA> ` in row rows-2 and `READY / BSP DEVELOPMENT CONSOLE` in row rows-1.
It enables cursor at8,rows-2, marks every active row dirty and sets transcript2,0,
completed0 and last fields0. Strings fit the minimum48 columns. It writes no
margin pixels, preserving the renderer's separate store contract.

Transcript append is deterministic: a printable character first advances if a
wrap is pending, then writes at current row/column and increments column. LF
advances once, including from a pending-wrap column. Advancing at the final
transcript row copies only transcript rows upward by one and clears the last row
to spaces; headers/prompt/result/inactive cells are preserved. Every changed row
is dirty. All copy loops are bounded by the already validated geometry.

Complete first handles successful clear: empty transcript and reset row2,column0;
only afterward may `CONSOLE CLEARED` become visible. For every other command it
starts a fresh transcript row if column!=0, appends `JOSHUA> `, the entire submitted
printable line, and LF. Then append the response body, wrapping/scrolling as above.
For clear skip command echo and append its response after clearing. Empty Enter
still echoes an empty command. After appending, saturate completed and copy exact
command/status/json/action. Clear the result row to spaces and display exactly one
of `RESULT 0 / OK`, `RESULT 2 / USAGE`, `RESULT 69 / UNAVAILABLE`,
`RESULT 127 / UNKNOWN`; mark it dirty. Recompose the current editor prompt, enabling
the cursor. Integration must retire input immediately after a successful action2;
pure model helpers do not halt. Actual post-retirement cursor disabling and HALTED
marking remain part of the lifecycle contract before boot integration.


## Screen-model implementation checkpoint

The actual Assembly parse/response/model pipeline passed17,279 cumulative checks
against an independent complete-grid/model oracle. Tests cover all cursor positions
for selected lengths through127, three geometries,100 seeded command completions
per geometry, exact-width/newline and scroll behavior, saturation, clear ordering,
a mismatched valid clear/halt result, all metadata-pair overlaps, corruption and
inaccessible boundaries for all six record extents. Inputs and the framebuffer
remain unchanged during composition. The generated host preview uses the actual
Assembly renderer after composing the model, and has been visually inspected at
`out/console-model-tests/console-model-preview.png`. Its diagnostic values are
synthetic host facts, not boot hardware observations. It is not a live console.

The existing editor/grid/parser/response suites passed344,659/5,055/95,317/28,687
checks again after shared command validation extraction. Independent read-only
implementation review found no actionable defects. The
reviewer did not reproduce tests or inspect output artifacts. Static maximum stack
depth is616 bytes below model entry RSP (624 including the incoming return address).
A source-only follow-up found no actionable issues in the full4095-byte synthetic
body fixtures; these exercise defensive composition bounds, not production
response provenance. Boot-owned facts capture, interactive input and retirement are
still not integrated; no console ISO is claimed by host composition evidence.

## Boot and live-console integration contract (review before implementation)

Add `Build.ps1 -Console`, defaulting to a separate `out/console` directory, with
manifest console=true and an additional clearly named `JOSHUA-Console-Test.iso`
copy/hash. Keep normal internal ISO/FAT names for existing tooling. Console rejects
FaultTest/ContextTest/UserTest/ElfTest/KeyboardTest and MissingEoi combinations;
TimerTest X2 is permitted as an explicit delivery profile with `out/console-x2`.
`Test-Reproducibility.ps1 -Console` must rebuild that profile twice and compare its
named ISO too. There is no speech inclusion or firmware input after boot-services
exit. The old checkpoint profile and every diagnostic fixture remain distinct.

Measured host console code/data are6781/1853 bytes including a wrapper. Reserve
headroom consistently across profiles: PE text0x1000/size0xd000, data0xe000/0x1000,
rodata0xf000/0x5000, BSS0x14000/0xe8000, relocation RVA0xfc000 with raw offset0x14000,
image size0xfd000 and file size0x15000. Update actual PE checks, reservation coverage
fixtures and image contracts together. Append three fixed BSS pages after the
existing user-entry stacks, containing separate eight-aligned anchor/editor/grid/
model/command/facts/response records; assert the sum fits12288 bytes. Nine guards,
existing stack extents and boot/user ABI layouts remain unchanged. All new state
is within loaded-image ownership and RW/NX proof; new text RX and constants RO/NX.
ELF envelope retains its existing offset within rodata. Bounds assertions still
reject every oversized profile rather than weakening section checks.

Refactor the existing `keyboard_window_run` activation and retirement bodies into
shared NASM source macros in `kernel/input/window-common.inc`. They expand at each
caller's fixed aligned RSP, preserving the exact saved-frame/oldRSP relationship;
there is no interrupt-enabled helper call with a different stack. Macro arguments
are trusted fixed wait/drain labels, flags and failure labels. They retain all
current authority/route/controller checks, F4/ACK/staging, mask/disable/flush, joint
40h/44h drain and1024-window limit. KeyboardTest hook conditionals stay explicit
and absent in console images. Compile the fixture consumer only for KeyboardTest;
console images contain the real collector and IRQ entries but no test consumer,
producer wait, injected frame or missing-input deadline shortcut.

Console ownership begins only after user/worker checks returned, root is the owned
root, IF0/DF0, no active user, and the APIC timer is stopped/masked with zero initial/
current counts and all ISR/IRR empty. `console_run` saves integer nonvolatiles and
uses56 bytes at fixed RSP for shadow plus outgoing command/response stack arguments.
It initializes the checked surface/editor/grid/model, clears only visible pixels
(background packed per GOP format), renders the initial screen, then activates
keyboard delivery through the shared macro. Full framebuffer/image direct overlap
is rejected before clearing; caller identity/ownership proofs establish backing
separation from all image records/immutable code. Anchor is initialized once from
validated owned boot data and never mutated afterward. Revalidate GOP extent,
geometry/format/arithmetic via grid_init before any pixel store. Margin clear is
an adapter operation; renderer remains limited to active cells and never padding.

Only `PUSH2;POPFQ;STI;HLT;CLI` opens the active window, with exact wait continuation
and the existing fifteen GPR sentinel check, oldRSP and post-CLI RFLAGS==2. No timer
is armed, so idle has no deadline or polling loop. IRQ44 validates the exact frame,
root/selectors/flags and actual ISR ownership before collection/EOI. Its IRQ/EOI
counters saturate in console builds, as do editor/model/decoder counters. ISR does
no formatting, rendering or command execution. IRQ40 dispatches to the keyboard
owner whenever phase!=0: active phase1 is a permanent-stop error before EOI;
only phase2 may validate its drain frame/actual ISR40 and EOI without ticks/rearm.
Phase3 rejects both entries. No unexpected event is treated as successful input.

Foreground IF0 pops real decoder events and calls editor_event. Changed/cancelled
input recomposes prompt and renders dirty rows; ignored/full events execute no
command. Submission runs command_parse, owned facts capture, response_build,
model_complete and render in that order. Use only fixed direct helper calls; action
metadata is validated by fresh parse before clear or halt. No host string supplies
native input and no user text supplies a function pointer or format string.

Facts capture reads fixed owned globals under IF0/current owned root: BOOT magic/
v1-size/image base/size; FRAME_ALLOCATOR magic, fixed ranges/records pointers and
capacity4096/count<=capacity/free<=total; RANGE_SET fixed entries/capacity8192/count
bound; OWNED_PAGING ready1/failed0/root/current CR3 and nonzero table_count<=4096*64;
ACPI magic/count<=256/enabled1..count; APIC magic/mode1..2; USER magic/stage3/failed0/
active0/phase0 and terminal process states3..5; keyboard window magic/phase1/failure0,
IRQ==EOI, and a checked decoder with bounded queue indices/count. Copy only the
frozen fifteen integer fields and publish FACTS_MAGIC last. No firmware table is
reread or hardware authority inferred from displayed counts. Capture failure stops
without displaying a fabricated result; its partial output is not resumed.

On successful halt, first render the honest HALTING response, then use shared
retirement at the same fixed RSP. Stop/mask timer and verify zero counts, mask IRQ44,
write-only disable controller, bounded flush, publish phase2/flags202h and drain
until all ISR/IRR are empty or stop at1025. Only after phase3/failure0, source
mask/quiet checks and successful model/grid validation disable the visible cursor,
replace result row with `HALTED / INTERRUPTS RETIRED` and render. Then emit console
halt evidence, return to kernel's existing IF0 permanent halt marker. Collection
failure2 retires sources through the same path but never emits normal HALTED; other
corruption/unowned-event paths stop IF0 and do not claim recovery. No further input
window can open after terminal phase3.

Boot and source integration still require independent implementation review,
actual ISO/FAT/x2 keyboard sessions,30-second idle, editing/history/capacity/JSON/
scroll/clear/unknown/usage and halt evidence, independent framebuffer/metadata and
actual LAPIC/IOAPIC/port retirement checks. Re-run the full existing input, user,
fault, context, ELF and host regressions and reproducibility. Negative console
fixtures must reject active timer delivery and invalid frames/state without EOI
or success markers, and exercise counter saturation. Windows and physical gates
remain separate; the first interactive success cannot close all qualification.


## First live integration checkpoint

The boot profile and source-checked facts adapter now exist. Shared fixed-RSP
keyboard activation/retirement macros serve the original fixture consumer and
native foreground loop; production ISR counters saturate and active vector40 is
fatal before EOI. The first actual boot reached READY and handled input but faulted
while drawing the halt row: two indexed symbol addresses encoded image-relative
absolute RVAs. Both now load the RIP-relative grid base and add the row offset;
independent focused review found no analogous new addressing hazards. The failed
log was preserved privately. A later ad-hoc smoke reached native HALTED but its
QMP quit reader failed; that invocation is not counted as an automation pass.

The final reviewed smoke verifies fixture/artifact identity, real CPU root/RSP/RIP,
physical records/LAPIC/IOREGSEL/route/port state, constant halt pixels in the actual
QMP display and no underline across the prompt row. Summaries are invalidated
before a run and published only after clean QEMU exit. Hardened xAPIC ISO/FAT and
x2APIC ISO runs passed actual PS2 editing/JSON/history/Ctrl-C,30-second timer-free
idle and retirement. Actual target screenshots were visually inspected. Normal
ISO/FAT and the original Text keyboard fixture also passed after layout/refactor.
Normal and Console reproducibility passed, including the named console ISO.
All24 Linux host suites and15 stack-walker fixtures passed. Nine fault fixtures,
three context profiles, two ELF rejections and x2APIC ISO/FAT plus missing-EOI
regression passed. The larger input/user batches are still running separately
and are not counted as passes yet.

Source-only integration/address-fix/oracle follow-ups have no unresolved findings.
Reviews did not independently reproduce runs or inspect artifacts. The console
CI job is wired but Windows execution remains pending. Full command breadth,
negative console fixtures, counter saturation targets, independent full pixel/
snapshot mutations and physical qualification are open; issue #40 and milestones
remain incomplete. Earlier host-only checkpoint descriptions above are historical.
