# M2 memory ownership contract draft

Status: proposed implementation contract for issue #3; no allocator or owned
paging is implemented by this document. Review each stage before runtime changes.
The existing 88-byte boot-info v0 and RDI entry contract remain unchanged.

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

Boot-info v0 does not carry image allocation bounds or a complete reservation list.
Before M2a ships, derive and verify the loaded image range while firmware is live
(e.g. Loaded Image protocol) and hand it over through a reviewed versioned extension
or an explicitly documented internal symbol contract. Do not silently grow v0.
Capture CR3 and inventory the active page-table hierarchy without freeing it.
Do not reclaim ACPI or firmware memory based solely on ExitBootServices success.

## Frame allocator (M2b)

Choose a simple bitmap or range allocator after measuring metadata bounds, not a
performance claim. Every frame has exactly one owner/state: unavailable, free,
allocator metadata, page table or allocation. Allocation returns aligned owned
frames or an explicit exhaustion result; no partial success without a documented
rollback. Free validates exact allocation ownership and rejects reserved frames,
misalignment and double free. Clear frames before exposing them across trust domains.
Single bootstrap CPU only; synchronization must be redesigned before SMP use.

## Paging transition (M2c)

Require supported paging mode and CPU features; do not assume four-level mode,
NX or physical width without checking. Document the accepted mode and fail safely
on unsupported firmware state. Allocate paging frames through the ledger. Preserve
reachable mappings for current code/data, stack, descriptor tables, boot-info/map,
new and old page tables, framebuffer and any retained firmware/ACPI dependencies
through the CR3 switch. No firmware calls after exit. Audit address translation;
UEFI physical pointers are not a general future direct-map contract.

Define virtual regions and cache attributes before constructing entries. Kernel
code is read/execute, constants read-only/NX, data and stacks read/write/NX, with no
writable executable aliases. Enable CR0.WP and EFER.NXE only after feature checks;
map framebuffer/device memory with reviewed cache policy. Guard pages are absent
mappings. Never change PAT or reuse firmware mappings without accounting for aliases.
Invalidate translations according to an explicit single-core mapping lifecycle.
Keep old paging frames reserved until no live reference remains.

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
