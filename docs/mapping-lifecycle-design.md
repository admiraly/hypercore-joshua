# Active kernel mapping lifecycle

Status: independently reviewed for issue #22, after read-only constants (#21). This advances M2d; it does not complete M2 or add
user mappings, executable allocation, firmware reclamation or fault recovery.

## Arena and ownership

Reserve one 2-MiB arena at canonical virtual address `0xffff800000000000` (PML4
index 256), with 512 independently present 4-KiB supervisor RW/NX pages. The owned
boot map occupies only the lower canonical half. The arena is initially absent.
It never replaces low identity leaves, image mappings, guard, framebuffer or tables.
No executable or user permission is accepted. Existing identity aliases remain
RW/NX for eligible allocator RAM; this is explicitly not process isolation.

Prepare three zeroed parent/PT pages with `owned_table_alloc` before activation,
after the low map is built and before the existing whole-chunk ownership/cache
proof. Require root slot 256 zero; install supervisor writable parents to a single
PT. Allocate these pages from existing exact paging chunks, never separately free
them. A failed preparation poisons the inactive root and stops. The existing proof
must include any new whole chunks and their identity/cache compatibility. Retain
all arena tables, even when empty; table reclamation is a later capability.

A separate trusted context references the allocator, owned builder, PT and a
bounded 64-record array. Each record contains VA, physical base, page count,
nonzero caller owner and state (staged/live/quarantined). A private physical owner
constant distinguishes all arena allocations from table and boot smoke owners;
caller owner is validated in the virtual ledger and never grants raw frame access.
The exact allocator record covers the complete physical allocation. No independent
physical frees are permitted for arena frames. Metadata is static kernel BSS/data;
no versioned firmware handoff changes.

## Operations and ordering

Allocation accepts 1..512 contiguous pages and a nonzero caller owner. Find a free
record and first-fit contiguous zero PT run before physical allocation. Allocate
and zero the whole physical run with alignment one. Exhaustion, bad requests or
metadata/virtual capacity rejection return without changing live mappings or
records. After physical success stage its ownership record, then preflight every
backing identity mapping: exact physical identity, supervisor RW/NX and no global
bit, with a valid leaf. Derive the existing PWT/PCD/PAT selector, converting a large
leaf PAT bit to the 4-KiB position. Leave PAT/MTRRs unchanged. Map only 4-KiB leaves
with the same selector; never coalesce or change effective cache type. Preflight
all pages before publishing any PTE. A violated internal invariant quarantines the
record and disables the context, retaining frames for stop-only diagnosis.

Publish all PTEs, invalidate each arena address locally, then mark the record live
and return VA. On x86 the aligned 64-bit PTE stores precede INVLPG; no non-temporal
stores are used. No caller can observe partial publication: BSP only, IF clear,
no callbacks/concurrency, and the API returns after invalidation. The initialized
owned root must be current, with PCIDE/PGE disabled as established by activation.
Runtime entry checks these assumptions and fails closed if they are violated.

Release requires exact VA, page count and caller owner matching a live record.
Reject interior/unaligned/out-of-arena, zero/oversized, wrong-owner and double frees
without mutation. Preflight every PTE against the record's physical run and expected
RW/NX/cache flags (hardware Accessed/Dirty bits may differ). On inconsistency disable
context, retain frames and stop; do not free a possibly mapped page. On success,
mark retiring, clear all PTEs, INVLPG every former VA, then call exact `frame_free`
with the private owner. Only after successful free remove the ledger record. An
unexpected frame-free failure quarantines the record and stops, leaving no arena
mapping but retaining physical ownership. No virtual address or frame is reused
before retirement invalidation completes. Interrupts remain disabled throughout.

Status values distinguish invalid request, virtual/record/physical exhaustion and
fatal invariant failure; failures return zero VA. Context disablement is permanent
for this boot. Caller halts on fatal status. Ordinary exhaustion remains recoverable.
These trusted kernel APIs are not a security boundary against malicious ring-0 code.

## Validation

Host wrappers exercise the exact Assembly lifecycle core and real allocator against
an independent Python model. A tiny privileged adapter performs current-root/IF/
PCID/PGE checks and INVLPG on target; host adapters record invalidation order and
observe PTE/allocator state at each invalidation. No simulated invalidation is used
in shipped runtime. Assert clearing/invalidation precedes free/reuse, zeroed fresh
physical runs, exact ownership, unchanged boot mappings, non-overlap, all-or-nothing
ordinary rejections, fragmentation, 64-record/512-page limits, physical exhaustion,
corrupted PTE quarantine, A/D tolerance, cache selectors and mixed operations.
Verify all eight firmware integer nonvolatile registers with sentinels.

QEMU boot smoke allocates/writes through high-half VA, rejects wrong/interior/double
release, releases and reallocates with zeroing, restores frame counters and leaves
the arena empty. Check software translation and boot mappings, then reach the normal
screen. A new fixture reads a released address and requires #PF/non-present read,
exact RIP and high-half CR2. Repeat all existing host/fault, ISO/FAT, build and
reproducibility checks; independently review implementation. This provides local
BSP invalidation evidence only, not SMP shootdowns or adversarial process isolation.
