# Physical frame allocator design

Status: proposed before runtime edits, next M2b slice after #15.

Use a simple range allocator, without performance claims. Initialization accepts
only the fully excluded, validated candidate RANGE_SET after paging reservation.
Validate nonzero/disjoint/page-aligned ranges below 2^47, count/capacity bounds
and checked total pages. Retained UEFI identity mappings must make these RAM
pages writable; nonconforming firmware may fault and stop. No firmware memory
is reclaimed. Metadata is image-owned BSS and therefore already reserved.

Successful initialization transfers exclusive mutation of the range set to the
allocator. Reservation helpers must not run again on that live range set.

A FRAME_ALLOCATOR contains readiness magic, mutable range-set pointer, allocation
records pointer/count/capacity and total/free page counters. Up to 4,096 live
allocation records contain base, page count and nonzero owner ID (24 bytes each).
A privileged caller chooses owner IDs; these are internal ownership tags, not
public user capabilities. Reinitializing a ready allocator is rejected.

`frame_alloc`: RCX=allocator, RDX=pages, R8=owner, R9=power-of-two alignment in
pages. EAX=0 succeeds with RDX=base; EAX=1 invalid, 2 physical exhaustion, 3 metadata
capacity. Failure returns RDX=0 with no state or RAM mutation. First-fit searches
unsorted ranges; rounding/addition are checked. Skip fits that cannot preserve
metadata capacity and try later ranges. Zero every allocated byte before returning
ownership. A hardware fault while clearing is fatal; no recovery is promised.

`frame_free`: RCX=allocator, RDX=base, R8=pages, R9=owner. EAX=0 success or 1 invalid.
Require an exact live record; reject wrong size/owner, misalignment, unallocated
and repeated frees. Merge adjacent ranges, removing entries by last-entry swap.
Free does not expose memory to another owner; allocation always clears it.

Maintain `free-range count + live-allocation count <= range capacity`. Allocation
checks the projected sum (including any split) before committing. This reserves
space to return every live allocation even if no coalescing is possible. Metadata
limits are explicit, without partial allocations. Free cannot run out of metadata
for a valid live record. Update exact allocation/free page counters on commit.
All helpers preserve firmware-ABI nonvolatile registers, use no firmware/SIMD,
and run on the BSP with interrupts disabled. SMP/interrupt-context use is deferred.

Host oracle tests use mapped RAM and the exact Assembly helpers via SysV wrappers.
Check zeroing, alignment, ownership, invalid operations without mutation, exhaustion,
metadata boundaries, disjoint reservations, free/coalesce, seeded random sequences
and accounting. QEMU integration initializes after paging exclusions, allocates
multiple owners/alignments, checks zeroes, writes patterns, rejects wrong frees,
releases allocations and verifies restored accounting before a FRAME_ALLOCATOR_OK
marker. Repeat ISO/FAT, fault, map, reservation, paging and reproducibility suites.
Owned paging is the next stage; this allocator does not itself establish isolation.
