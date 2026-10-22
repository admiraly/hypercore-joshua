# M2a reservation slice design

Status: proposed before runtime changes; issue under #3, stacked after #11.

Preserve BOOT_INFO v0's 88-byte definition and offsets. Add BOOT_INFO_V1 with
an identical prefix followed by image_base and image_size (104 bytes total).
The loader emits version 1; the embedded kernel accepts that version explicitly.
This is an internal version bump, not a public ABI freeze. Capture Loaded Image
protocol through HandleProtocol while firmware is live, before final map capture;
verify its base equals the actual PE start, size covers SizeOfImage, both are page
aligned, and base+size does not wrap. Verify later against physical width and map
coverage. No boot services between final GetMemoryMap and ExitBootServices.

Build bounded candidate ranges only from already validated type-7 descriptors
with EFI_MEMORY_WB supported and only known cache-capability bits UC/WC/WT/WB
(mask 0xF); protection/runtime/unknown attributes and types remain unavailable.
Candidates are half-open page-aligned ranges stored in image-owned BSS, with count
and capacity. No memory is allocated or reclaimed. Subtract page zero, full loaded
image range and the page-rounded framebuffer range. A shared Assembly routine
handles full deletion, front/back trims and splitting. It may mutate earlier
entries before capacity failure; any failure invalidates the whole candidate
ledger and stops boot. No consumer may use a failed or unfinished ledger.

Expand the handcrafted PE layout consistently if code/BSS outgrow current bounds;
update exact static format checks and manifest inputs. Image bounds include all
BSS ledger/metadata storage. Keep executable and writable sections separate.

This slice does not establish allocatable frames: active CR3/page-table inventory
and explicit exclusion remain the next mandatory stage. Keep firmware loader,
boot-service, runtime, ACPI and unknown types unavailable. Do not reclaim firmware
memory or claim M2a complete. Test the exact range-subtraction routine on Linux
with deletion, trims, splits, disjoint/adjacent/overlapping reservations, exhaustion
and malformed bounds; repeat boot and reproducibility tests for integration.

Design refinement from integration: requiring attributes exactly 8 yielded no
candidates on OVMF, whose conventional ranges advertise multiple cache policies.
The accepted capability mask is now 0xF with WB required; this does not change
cache configuration or treat protection attributes as allocatable. QEMU tests
require a nonzero candidate count for the fixed 512 MiB profile.
