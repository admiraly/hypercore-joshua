# BSP APIC timer and interrupt return foundation

Status: independently reviewed for issue #28, based on #27. Effective-UC revision
reviewed before cache-classifier implementation. Goal: sixteen real one-shot timer deliveries with EOI/rearm/IRETQ on the BSP,
then a masked/stopped timer and IF clear. No scheduler, AP startup, calibrated clock,
user execution or interrupt-driven allocator is introduced. First-release isolation
and hardware qualification remain open.

## Controller discovery and owned mappings

Extend the bounded MADT parser with type1 IOAPIC records (minimum12 bytes), normalized
into eight static 16-byte slots (ID, physical base, GSI base, reserved). Reject excess
capacity, duplicate IDs or page bases, zero/unaligned/out-of-limit addresses and local-
APIC/IOAPIC aliasing before controller access. Require at least one IOAPIC for this
initial PC interrupt profile. Unknown records grant no authority. Existing CPU/BSP/
checksum/read-boundary rules remain intact; boot-info layouts do not change.

Before owned CR3 activation, reserve an initially absent 2-MiB controller window at
`0xffff808000000000` (PML4 index257). Prepare its three parent/PT pages in exact
paging-owned chunks, before existing whole-chunk/cache proof. Retain all table pages;
no active-root private builder or post-activation table allocation is permitted.
Slots0..8 map one LAPIC and up to eight IOAPIC pages; no arena, image or guard leaf
is modified. x2APIC does not use the LAPIC MMIO slot.

After ACPI success and with IF clear/current owned CR3/PCIDE-PGE off, preflight every
needed device page before publishing any leaf. Require validated-map exclusion checks: any overlapping descriptor must be
Reserved(type0) or MMIO(type11), without RP/RO/WP. An omitted page is accepted only
under the explicit controller-authority profile below; it is never treated as RAM.
Reject overlap with known RAM, image, framebuffer, paging/frame ownership or another
controller. Require
retained firmware-root lookup to be present physical identity across the entire
page. Preserve that leaf's PWT/PCD/PAT selector in a supervisor RW/NX 4-KiB alias;
its effective type must be proved strict UC from a read-only MTRR/PAT snapshot,
not inferred from PAT alone. PAT/MTRRs stay unchanged, and no promotion across
inherited leaves occurs. Unsupported cache layouts stop; do not force a new type. Parent table
walk/data compatibility was already proved before activation.

Publish all device leaves only after preflight, then INVLPG each controller VA.
Controller records/window tables remain permanently reserved. Invariant failures
stop with stage status and IF clear; no mapped frames or controllers are reclaimed.
A failed MMIO operation can still fault; IST diagnostics remain best effort.

## Controller-authority profile for omitted MMIO pages

A second read-only QEMU probe found no UEFI descriptor covering FEC00000/FEE00000,
despite inherited identity mappings and valid MADT records. Complete descriptor
coverage cannot be a prerequisite for this firmware's controller MMIO. A UEFI map
gap grants no allocation or arbitrary-device access authority by itself.

For a LAPIC omitted page require CPUID APIC/BSP, enabled xAPIC IA32_APIC_BASE and
exact MADT/MSR base agreement before staging. x2APIC needs no LAPIC MMIO. For an
IOAPIC omitted page require a unique checked MADT type1 record and the initial PC
IOAPIC aperture profile FEC00000..FECFFFFF; other omitted addresses are unsupported.
Apply this IOAPIC aperture restriction to described pages too. Before any controller
register access require the inherited identity/effective-UC proof and exclusion of
all known RAM/owned image/framebuffer/frame records. Validate actual ID/version and
route formats before controller configuration writes (IOREGSEL selection writes
are the minimum read interface). No controller data write occurs merely because
a page is absent from the UEFI map. This is a documented PC platform profile,
not a universal ACPI MMIO policy or protection against malicious firmware.

Map descriptors remain authoritative to reject conflicting ownership/permissions.
Scan every descriptor overlapping the whole page; partial coverage or a gap cannot
hide even a partial prohibited overlap. Gaps never enter the physical allocator.
The snapshot/UC/parent table proof and no-type-transition rules remain unchanged.
This policy revision requires independent review before controller alias execution.

## Effective UC proof (revision after firmware probe)

The first design review found no issues, but a read-only QEMU firmware-table probe
then showed both FEE00000 and FEC00000 covered by inherited 2-MiB identity leaves
with PAT index0/type6(WB). PAT=0007040600070406. A PAT-only UC rule therefore cannot
qualify this profile. No controller alias/access is implemented yet; the three
preallocated window tables and IOAPIC inventory are the only runtime additions.

Require CPUID MTRR support and CR0.CD/NW clear. Snapshot IA32_MTRRCAP,
IA32_MTRR_DEF_TYPE and at most255 variable base/mask pairs into one owned BSS page;
use actual CPUID physical width (32..52), not the lower identity-address cap.
No MTRR/PAT write, cache disable or cache flush is introduced. Require controller
pages >=1MiB so fixed MTRRs cannot affect them. Reject reserved/unsupported register
encodings and unsupported WC types. Every active pair must have a contiguous
physical mask, power-of-two size >=4KiB, aligned base and no reserved/high bits.
Invalid/inactive pairs do not authorize memory; inactive payload is ignored.

Resolve each whole aligned controller page using architectural variable-range
precedence: if MTRRs disabled, resolved type is UC; otherwise matching identical
types agree, UC wins any defined matches, WB+WT resolves WT, and other mixed types
are rejected as undefined. If no match, use validated default type. A minimum4KiB
aligned range cannot introduce an interior boundary in the target page. Reject
unknown types/reserved state; do not infer UC from undefined overlap. Require the
selected PAT byte to be a defined type and explicitly reject selected PAT WC before
UC acceptance: MTRR UC + PAT WC is WC. Accept strict effective UC only when either
resolved MTRR type is UC or selected PAT type is strict UC(type0). UC-minus alone
is insufficient. Other potentially UC combinations are conservatively unsupported.
The old/new leaf selectors and MTRRs are identical, so this introduces no memory-
type transition or unproven large-leaf promotion.

Host oracle tests must cover disabled/default UC, matching UC+WB, WB/WT precedence,
undefined overlap, malformed masks/bases/widths/types, unsupported WC, default WB
rejection, PAT UC with valid resolved types, UC-minus distinctions, full-range
masks and page boundaries. QEMU ready evidence records the snapshot-derived UC
qualification separately from delivery. The added BSS snapshot page shifts shared
image/relocation and exact reservation/PE bounds; boot-info is unchanged.

## Modes and controller interface

Read IA32_APIC_BASE only after CPUID APIC support. Require global enable and BSP
bits, reject invalid mode; preserve the inherited mode in normal builds. For xAPIC,
require the MSR base to match validated MADT local-APIC base and use the proved UC
window. For x2APIC, require CPUID support and use architecturally defined x2APIC MSRs;
never access LAPIC MMIO or change mode in normal builds. Controller ID must match
CPUID/MADT BSP ID (xAPIC ID high byte; x2APIC full32 bits). Validate version and only
use advertised standard LVT entries; reject unsupported versions/max-LVT layouts.

Use a small common register interface for ID/version, SVR, TPR, LVTs, divide/count,
ISR/IRR and EOI. MMIO uses aligned 32-bit reads/writes and same-device readback to
flush posted writes. x2APIC WRMSR writes zero upper32 bits to the applicable 32-bit
registers and fences prior ordinary stores before EOI/controller writes. No ICR,
IPI, DFR/LDR or AP control writes occur. The x2 fixture alone may enable EXTD in
IA32_APIC_BASE while EN stays set after checking support/BSP/IF-clear; APs do not run
JOSHUA. Its manifest identifies this controlled mode transition.

## Quiescence before STI

First inspect every IOAPIC ID/version and redirection route while IF clear. Require
matching IDs, supported version11h/20h, 1..120 entries (bounded IOREGSEL profile) and non-overlapping, non-
overflowing GSI ranges. IOREGSEL/IOWIN access is BSP-only serialized; do not expose
this interface to ISR or another CPU. Validate all controllers/routes before writes.

Mask fixed, lowest-priority and ExtINT routes, preserving other entry fields and
reading back mask bits. Preserve SMI/NMI firmware routes because they do not depend
on IF and may carry platform handling; do not redirect them to JOSHUA. An unmasked
INIT or reserved delivery mode is unsupported and stops before programming. Do not
rewrite destinations or infer ISO routing; external device delivery stays disabled.
Mask legacy PICs at0x21/0xa1 only if MADT PCAT_COMPAT declares them; no remap/unmask.

Mask supported maskable local LVT sources, preserving SMI/NMI assignments. Timer is
owned/reprogrammed; LINT ExtINT and fixed sources stay masked. Validate active INIT/
reserved local routing as unsupported. Configure SVR enabled/vectorFF while retaining
unrelated defined state, set TPR0 and keep error/unused maskable sources masked.
No firmware SMI/NMI handling is claimed implemented. Require local ISR/IRR empty
before the timer window; inherited pending maskable state stops rather than being
EOI-drained without an owner. Recheck routing masks as part of ready evidence.

## Entry, return and critical sections

Reserve vector40h for timer, 41h for the software return self-test and FFh for genuine
APIC spurious delivery. These DPL0 interrupt gates use IST0/current kernel stack;
all other stop-only exception gates keep their guarded IST assignments. No user
software INT access exists. Unexpected vectors still stop. Gate changes happen IF0.

Returning entries save all fifteen GPRs, preserve interrupted RSP via the hardware
frame and restore RFLAGS/SS:RSP with IRETQ. CLD occurs only after the hardware frame
captures interrupted flags. Align call sites with 32-byte shadow space; no red zone
or SIMD is used. The interrupted path is exclusively Assembly without live SIMD;
this is not yet a user/FPU context contract. Gate entry clears IF, handlers never
STI and do not allocate, call firmware, log, block, schedule or invoke callbacks.

Timer ISR increments its BSP count, sends exactly one local EOI, and rearms the
one-shot initial count only if count<16. At the sixteenth delivery mask/zero the
timer instead, before EOI; no seventeenth rearm. Divide/count values are a bounded
bring-up interval, not calibrated time or a performance assertion. Handler returns
to the interrupted wait with all GPRs/flags restored. Genuine spurious entry returns
without EOI (no ISR ownership); the software test vector also sends no EOI.

The boot smoke runs only after memory/lifecycle/ACPI setup. All controller preparation
and normal kernel work keep IF0. The only enabled window is STI/HLT followed by a flags snapshot and CLI in a
serialized wait loop; no allocator/mapping API runs there. Full GPR sentinels survive
all sixteen hardware returns. A separate INT41h self-test checks all GPRs, exact RSP
and selected arithmetic/direction flags across the same returning-entry save/restore
protocol. IRETQ restores IF; the explicit CLI closes each wait before normal work.
After completion verify timer masked/count0 and no leaked timer ISR/IRR, then emit
TIMER_OK with deliveries/EOIs/returns/mode and leave IF0 at the existing screen.
A missing timer can hang this early HLT smoke; QEMU's bounded host timeout rejects
it. An independent runtime timeout/wakeup source is not yet qualified.

## Validation and scope

Host fixtures use the exact Assembly controller policy with synthetic read/write
backends. Independently check MMIO/MSR register selection, route/LVT mask preservation,
SMI/NMI handling, invalid mode/base/cache/ID/version/count/GSI/capacity rejection,
write/readback ordering, no writes after failed preflight, and exact one-shot/EOI
sequences. Device-window fixtures decode leaves, ownership and untouched boot maps;
read failures retain IF0/stop status. Host adapters cannot establish actual APIC
semantics or IRETQ correctness.

QEMU normal ISO/FAT must observe sixteen real xAPIC timer deliveries and returned
GPR/flags/RSP evidence, quiesced routing and final IF0. Add an explicitly identified
x2APIC fixture under `qemu64,+x2apic` to test mode-transition/MSR delivery separately;
normal qemu64 profile stays unchanged. The host capability probe accepted x2APIC
without warnings on QEMU10.2.2, which alone is not delivery evidence. Both paths
must pass actual target validation before being claimed. Include a missing-EOI or
no-rearm negative fixture rejected by the QEMU timeout/count checker.

Repeat nine exception fixtures, all host memory/ACPI/descriptor suites, Build and
reproducibility. Independent implementation review precedes linked stacked PR.
Physical xAPIC/x2APIC compatibility, MSI devices, AML, IOAPIC interrupt routing,
NMI/MC nesting, SMP, context scheduling, user isolation and clock calibration remain
unqualified. These foundations advance toward the release; they do not close M2/M3.

References: Intel SDM Vol3A chapter10 (local APIC, EOI/spurious, timer and x2APIC),
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
and Intel500-series PCH datasheet Vol2 section25.1 (IOAPIC ID/version/redirection),
https://cdrdv2-public.intel.com/631120/631120-002.pdf . ACPI MADT structure rules remain
those in acpi-topology-design.md. Conservative caps reject unqualified layouts.


## Review response and current evidence

Initial design review found no actionable issues. A subsequent firmware probe exposed
the PAT-WB/MTRR qualification gap; revised design review found no actionable issues
and called out two implementation details. The classifier explicitly rejects selected
PAT WC before UC acceptance. The raw 4-KiB snapshot is exactly CAP+DEF_TYPE+255 pairs;
CACHE_CONTEXT (readiness, actual width, raw pointer and PAT) lives outside that page.
No cache-control registers are written. Maximum-count and UC-overlap permutations
are explicit host fixtures. Snapshot capture is privileged and still needs target
execution evidence; classifier host tests do not establish actual controller access
or interrupt return. Full implementation review and all timer gates remain pending.


## Implementation review response

The runtime review found that IOAPICVER's low version byte was discarded. The
preflight now accepts only11h (82093AA) and20h (integrated PCH profile), before any
redirection read. All254 other byte values have host rejection fixtures requiring
no route reads or configuration writes. The exact Assembly policy runs against a
modeled controller backend; this does not qualify real MMIO or privileged adapters.
The shared PAT-index helper was extracted without behavioral changes so controller
staging fixtures execute the production helper.

The enabled timer wait sequence includes PUSHFQ/POP snapshots after HLT before CLI;
no memory API or other runtime work is permitted in that short IF-enabled window.
