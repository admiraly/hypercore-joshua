# Active paging inventory design

Status: proposed before runtime edits; next M2a slice after #13.

The bootstrap CPU captures CR3 and requires IA-32e paging with CR4.LA57 clear.
No CR3 write or page-table modification is permitted. Mask CR3's low control/PCID
bits to obtain its root frame; preserve the captured raw value for diagnostics.
Read EFER.NXE and CPUID 1-GiB-page support for entry interpretation.

A bounded breadth-first queue stores up to 16,384 unique table frames with their
level (4 through 1). Before dereferencing any table, require a nonzero aligned
physical address below both CPU physical width and 2^47 (canonical low identity
address). Require the entire page to lie in a validated descriptor of loader,
boot-service, runtime, conventional, ACPI reclaim or NVS RAM, without EFI_MEMORY_RP.
Unknown/reserved/MMIO/unaccepted types are not dereferenced. This uses the retained
UEFI x64 identity mapping; unreadable or nonconforming firmware may still fault,
and stop-only exception reporting is best-effort, not recovery.

Only present entries are followed. Interpret PS at PDP/PD as 1-GiB/2-MiB leaf,
with feature/alignment checks; PS in PML4 is rejected. PT entries are leaves,
including bit-7 PAT. Check physical address bits and NX interpretation. This is a
paging-frame inventory, not a complete verifier of every permission/cache/reserved
bit or proof that every existing mapping is safe. Same-level shared tables are
deduplicated. Reject references to a queued frame at a different level (including
recursive mappings/cycles) rather than guessing. Non-present entries are ignored.
Capacity or validation failure clears inventory count and halts without allocating.

The queue lives in image BSS, so is already excluded with the full loaded image.
After successful inventory, subtract every table frame using range_reserve; any
failure invalidates the candidate set. Emit root/count diagnostics and an ordered
PAGING_RESERVED marker only after exclusion. Verify CR3 unchanged. A later frame
allocator must consume this completed ledger, preserve all exclusions and track
allocation ownership. This step alone does not implement an allocator, change
paging ownership, establish isolation or complete all M2 qualification.

Host tests call the same Assembly walker with aligned synthetic page tables and
validated RAM descriptors. Exercise 4-KiB/2-MiB/1-GiB leaves, sharing, ignored absent
entries, malformed pointers/levels, inaccessible ranges, cycles and queue exhaustion;
check that table frames are absent from resulting candidates. Repeat normal boot,
reproducibility and prior fixture suites.

References: [UEFI 2.10 x64 execution environment](https://uefi.org/specs/UEFI/2.10/02_Overview.html#x64-platforms)
defines identity mappings of described memory, subject to protection attributes;
[Intel system-programming manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
define IA-32e entries, PS/PAT, NXE and physical-address widths.

Capacity refinement after integration: the 512-MiB OVMF profile required 1,033
table frames. A 16-GiB address range mapped entirely with 4-KiB pages needs 8,192
PT frames plus upper levels, so the initial 4,096-frame proposal was too small
for the reference-machine target. The bound is now 16,384 frames in 256 KiB of
image-owned BSS. This is sizing arithmetic, not a hardware compatibility result.
