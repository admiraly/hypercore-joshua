# Owned paging transition design

Status: proposed before runtime edits, M2c after #17.

Require four-level IA-32e mode and CPUID NX support. Keep all existing exclusions
and allocation records, including inherited table frames. Allocate zeroed table
pages through frame_alloc under a dedicated internal paging owner. The builder
reserves 64-page chunks when contiguous memory is available, falling back to one
page on physical fragmentation/exhaustion. Every whole chunk has an exact allocator
record, including unused pages; a cursor supplies table pages without untracked
allocation or individual frees. This avoids one allocation record per table in
firmware using fine-grained leaves. Verify all chunk pages before activation. The inactive
builder uses identity addresses under retained UEFI paging. Any construction or
verification failure poisons the root and halts; partially constructed roots are
never activated, reclaimed or retried as complete roots.

Map known RAM types (loader, boot/runtime services, conventional, ACPI reclaim/NVS)
within the supported low canonical physical range. Skip read-protected regions;
unknown/reserved/MMIO/unaccepted types stay absent except the validated framebuffer.
Only conventional RAM accepted by the allocator cache/attribute policy is RW/NX;
other retained RAM is RO/NX. Image RW segments with firmware WP/RO attributes
fail rather than relaxing those protections. Override the full
loaded image with RO/NX headers and relocation, RX text and RW/NX data/BSS. Insert
a reserved, unmapped page immediately below the kernel stack. Keep page zero absent.
Split ranges at policy and inherited-leaf boundaries; use 2-MiB leaves only
within a covering inherited leaf where alignment/length permit, otherwise 4-KiB. No writable executable leaf, user mapping, global
bit or alternate virtual alias is introduced. No page-table ownership is inferred
for mapped data pages: ownership still comes only from the allocator.

Retain IA32_PAT and MTRRs without reprogramming. For every new identity range,
software lookup in the inherited root must prove physical identity and retain
that leaf's PWT/PCD/PAT selection (converting the PAT bit between page sizes).
Never promote across an inherited leaf boundary: retain or subdivide inherited
leaves only. Matching PAT indices alone do not prove uniform MTRR typing across
a larger leaf. Each mapping call is bounded by one inherited leaf and one policy
segment, so no new large leaf spans previously separate memory-type regions. This preserves the effective cache type for every retained physical
mapping because both PAT selection and physical-address MTRR effects stay
unchanged. No new alternate virtual aliases are created. Do not assert that a
WB PAT selection implies effective WB; MTRRs may override it.

Require PAT entry zero to select WB for new paging-structure accesses. Before
activation, require every new table frame's inherited identity leaf to select
that same PAT memory type; unchanged MTRRs therefore also make data accesses and
page-table walks agree in effective type. All parent entries and CR3 use WB PAT
selection zero. The framebuffer retains its inherited PAT selection as well;
there is no forced UC conversion or unverified memory-type transition. Reject
image/framebuffer overlap. Skip framebuffer while mapping RAM, then map its
page-rounded range once as RW/NX with preserved selectors. A future memory-type
policy change requires a separately reviewed architectural cache transition.

Before switching, software lookup proves every image page's identity/permissions,
page-zero/guard absence, all owned table frames' writable compatible-cache identity mapping,
and framebuffer identity/cache selection. GDT, IDT, boot-info/map, allocator metadata
and live stack are inside verified image BSS. All free RAM remains mapped for
subsequent allocator zeroing. Do not call firmware after exit.

Enable EFER.NXE and CR0.WP. If PCID is active, select PCID zero under the old root;
then disable PCIDE and PGE to flush old translations, preserving every other CR4
bit. Load the new root and leave PCID/global-page optimizations disabled for this
bring-up stage. Verify CR3, WP and NXE; emit the new-root marker only after the
switch. No CPU mitigation is disabled. Future process address spaces and PCID
policy require separate contracts. This is not ring-3 isolation.

Normal ISO/FAT boots must render the existing checkpoint under the new root.
Fixtures after activation attempt a write to RX text (#PF error3), execution from
a newly allocated RW/NX page (#PF error0x11), and a read from the stack guard
(#PF error0). Verify exact RIP/CR2 and halted state. These are deliberate accesses
from a valid stack, not stack-overflow/NMI/double-fault resilience. Existing Ud2/Gp
fixtures remain before the memory path. Host tests independently inspect the exact
Assembly builder's tables, page sizes, permissions, translations and allocation
ownership, including duplicate-map and capacity failures. Repeat prior suites and
reproducibility; physical qualification remains outstanding.

Review response: initial design forced WB/UC selections without a cache flush,
which could change effective types with cached lines present. That proposal is
replaced by verified preservation of inherited identity-leaf PAT selection and
unchanged physical MTRR effects. Only permission/address-space translation changes
remain; no effective memory-type change is intentionally performed.

Second review response: promotion across inherited leaves is prohibited. No
unproven large leaf crosses an old MTRR/leaf boundary. Chunked allocator ownership
supports the resulting fine-grained table counts while keeping every reserved
page tracked; partial-root/chunk cleanup is deferred because build failure halts.
