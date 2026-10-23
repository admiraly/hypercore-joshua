# M2 memory ownership contract draft

Status: evolving implementation contract for issue #3; map, reservation and
frame-allocation and initial owned-paging stages are implemented. Lifecycle,
isolation and broader qualification remain open. Review each stage before runtime changes.
The 88-byte boot-info v0 definition and RDI register contract remain unchanged;
the current loader/kernel explicitly exchange the 104-byte v1 extension.

## Implemented first validation slice

`kernel/mm/map.inc` validates the final map before the normal checkpoint. The
caller checks boot-info magic/version/size, descriptor version 1 and exact map
buffer address. The validator accepts strides of at least 40 bytes divisible by
8, a nonempty whole-descriptor buffer bounded by 128 KiB, and physical widths
32 through 52 from CPUID leaf 0x80000008. It rejects zero-page, unaligned,
out-of-width, overflowing and overlapping ranges, even for reserved/unknown types.
Unsorted adjacent descriptors and extended strides are accepted. It does not
interpret type or attribute as permission to allocate. Unknown types remain
unavailable under the future ownership policy. The routine is read-only and
requires the caller to own a readable buffer for the declared length.

This validation alone does not establish allocatable memory. No frame becomes
free as a result of validation. Unsupported physical-width discovery fails closed.

## Implemented image handoff and candidate accounting

Boot-info v1 adds checked Loaded Image bounds and requires complete coverage by
loader-code/data map descriptors. `memory_candidates_build` selects conventional
ranges with WB support and only UC/WC/WT/WB cache-capability bits, then subtracts
page zero, the complete image and the rounded framebuffer. All candidate metadata
is BSS inside the reserved image. Split capacity failure invalidates the whole
result and stops boot. `range_reserve` is shared with a native host oracle suite.

After candidate construction, the kernel inventories the active four-level
paging structures and subtracts every discovered table frame. The frame allocator
consumes the completed exclusions. No firmware memory is reclaimed and this is
not evidence of process isolation.

## Input validation and range accounting (M2a)

Consume the final map only after successful ExitBootServices. Walk its reported
stride, not a packed host structure. Validate boot magic, supported version, size,
map pointer/length, descriptor version and stride before reading fields. Require
whole descriptors, a supported descriptor layout, page-aligned physical starts,
and checked page-count multiplication and end-address addition. Bound addresses
by detected physical-address width. Reject contradictory overlapping descriptors,
invalid counts and insufficient ledger capacity. Define and test unknown-type
handling as reserved. Never silently truncate the map or assume it is sorted.
Use half-open physical ranges [start,end); validate before sorting/coalescing.

Initially only validated EfiConventionalMemory is eligible. Reserve page zero,
the entire loaded PE allocation (including BSS, map, stacks, GDT and IDT), allocator
metadata and all active paging structures. Reserve framebuffer/MMIO, ACPI reclaim
and NVS, runtime services, reserved/unusable memory and every unknown type. Preserve
firmware boot/loader allocations conservatively until their live references are
inventoried. Absence from the usable ledger means unavailable, never implicitly free.

Boot-info v0 does not carry image allocation bounds; the versioned v1 extension
now supplies those bounds through Loaded Image protocol. Do not silently grow v0.
The active paging inventory now supplies explicit table-frame exclusions.
Capture CR3 and inventory the active page-table hierarchy without freeing it.
Do not reclaim ACPI or firmware memory based solely on ExitBootServices success.

## Implemented active paging exclusions

`paging_inventory_build` traverses present entries using a bounded 16,384-frame
breadth-first queue in reserved image BSS. Four-level IA-32e mode is required;
LA57 is rejected. Every table target must be nonzero, page aligned, within CPU
physical width and below 2^47, and wholly covered by a known RAM descriptor
without read-protection. It relies on retained UEFI identity mappings. It handles
PT leaves, supported 1-GiB/2-MiB leaves and same-level sharing; cross-level cycles,
malformed targets and capacity exhaustion fail closed. This is not a complete
page-permission or reserved-bit verifier. Firmware is still a trusted input.

`paging_inventory_reserve` excludes all successful inventory frames from the
candidate ledger. The normal boot marker records captured CR3 and table count;
the runtime requires unchanged CR3 during inventory; after the later transition,
QEMU requires the distinct owned root as final CR3. The inventory does not modify page entries or reclaim tables. The later owned
transition leaves every inherited table frame reserved.
A nonconforming or unreadable firmware mapping may fault; there is no recovery.
See [the paging inventory design](paging-inventory-design.md) for assumptions.

## Implemented frame allocator (M2b)

`frame_allocator_init` accepts the completed excluded ranges and image-owned
record storage. It validates range alignment, disjointness, metadata bounds and
nonzero low-canonical physical addresses; initialization never clears RAM. It
rejects reinitialization. All metadata must be owned by the trusted caller and
excluded from candidates; initialization is not a public pointer-validation API.

`frame_alloc` returns contiguous zeroed pages with power-of-two page alignment,
recording exact base/page-count/nonzero internal owner. It distinguishes invalid
requests, physical exhaustion and metadata limits, with no mutation on failure.
Every success clears every allocated byte before returning. `frame_free` requires
an exact live record and rejects wrong owner, wrong size, misalignment, reserved
or unallocated addresses and double free. It coalesces adjacent free ranges.

The range capacity invariant reserves room to return every live allocation:
free-range count plus live-allocation count never exceeds range capacity. Up to
4,096 simultaneous allocations use 96 KiB of image-owned records. This is a bounded
bring-up implementation, not a performance claim or a general-purpose heap. Owner
IDs are privileged internal tags, not authenticated user handles. Clearing faults
are fatal; no rollback/recovery is promised for hardware faults. No freed frame
crosses an ownership boundary without clearing on its next allocation.

The BSP calls this with interrupts disabled, after paging-frame exclusion. No
firmware memory is reclaimed and CR3 remains inherited. No concurrency is supported.
Future address-space and user-capability APIs must add their own lifetime and
validation rules. See [the allocator design](frame-allocator-design.md).

## Initial owned paging transition (M2c)

The kernel requires four-level IA-32e/NX and constructs an inactive owned root
from zeroed allocator-owned chunks (64 pages, falling back to one only on physical
exhaustion/fragmentation). Whole chunks remain tracked, including unused pages.
Failure poisons the root and halts; no partial root is activated or reclaimed.

Known readable RAM is mapped at physical identity addresses. Eligible conventional
RAM is RW/NX; other retained RAM is RO/NX. The image uses RX text, RO/NX headers
constants and relocation, RW/NX data/BSS, and an absent page below the kernel stack. Page
zero is absent. Firmware WP/RO/XP attributes are not relaxed for image permissions.
No user/global mappings or alternate virtual aliases are introduced. Immutable strings, GUIDs and font bytes occupy a dedicated `.rodata` page.
Mutable buffers, descriptor state and memory metadata remain in data/BSS.

Each mapping is bounded by a policy segment and a covering inherited leaf.
Retain/subdivide leaves only; never promote across inherited boundaries without
an MTRR-uniformity proof. Preserve leaf PWT/PCD/PAT selectors and leave PAT/MTRRs
unchanged, including for framebuffer. This avoids an intentional effective cache
type transition. PAT entry zero must select WB for new parent walks, and all chunk
pages must select the same PAT memory type through inherited/new identity leaves.
MTRRs may override WB: no effective-WB claim is inferred from PAT alone.

Software lookup proves image permissions/identity, guard/page-zero absence, every
chunk page's writable identity/cache compatibility, and framebuffer mapping before
activation. Enable EFER.NXE/CR0.WP, select PCID zero if needed, disable PCIDE/PGE to
flush translations while preserving other CR4 bits, and load the new root. The
normal QEMU test observes distinct captured/owned CR3, WP/NXE and the final screen.
Dedicated fixtures exercise text write, NX execution and guard-read page faults.

This is BSP-only bootstrap paging, not process isolation or a complete mapping
lifecycle. Unmapping, TLB shootdowns, owned TSS/IST and recoverable user faults need
separate contracts/tests. No inherited firmware memory is reclaimed. See the
[owned paging design](owned-paging-design.md) for review findings and responses.

## Failures and qualification

Stop on invalid input or incomplete transition; do not continue with guessed
ownership. Exception reporting remains best-effort and nonrecovering until TSS/IST
and independent fault stacks exist. A stack/page-table failure can prevent any
report. CR2 identifies an address only for page faults; otherwise it is historical.

Required evidence: malformed-map fixtures, reserved-range exclusion, exhaustion
and ownership stress, observed CR3 switch, inspected page permissions and hardware
fault injection. Repeat normal ISO/FAT boot and reproducibility for runtime changes.
Keep QEMU correctness, physical compatibility and workload performance distinct.
Ring-3 isolation is a later gate requiring separate address spaces and validated
entry/syscall paths; owned kernel page tables alone do not establish it.
