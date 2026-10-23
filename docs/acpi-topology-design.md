# Validated ACPI topology before interrupt programming

Status: independently reviewed for issue #26, based on #25. This slice supplies trustworthy BSP/local-APIC discovery, not delivery,
masking, controller initialization, AP startup, AML interpretation or ACPI reclaim.
The first usable release still needs interrupt/timer/context and ring-3 work.

## Trusted boundary and bounded reads

Run after final-map validation, owned CR3 activation and the lifecycle smoke, with
IF clear. Existing boot-info v1 supplies RSDP; the validated map and owned builder
supply physical limit and trusted root. Firmware tables are untrusted bytes. Before
reading any fixed header, prove its entire byte range; then read a bounded length,
prove the full range, and only then checksum/parse it. Never follow a pointer based
on signature/checksum alone. No firmware calls or memory reclamation occur.

The read predicate rejects zero address/length, addition overflow, physical-limit
excess and addresses beyond the low canonical physical identity range. Require
complete coverage by validated UEFI type9/type10 descriptors without RP, allowing
adjacent ACPI descriptors. For every covered page require the owned lookup present,
supervisor and translating to the same physical address. No MMIO or unknown memory
classification is read. A root/header crossing an unproven page is rejected before
access; the page-table root itself remains trusted and already owned/proven. This
predicate reads metadata only; it does not attempt fault recovery or probe memory.

Require revision>=2 RSDP from the UEFI ACPI2 GUID. Prove 36 bytes, then length36..4096,
verify signature `RSD PTR `, checksum of first20 and checksum of full declared size.
Use nonzero XSDT only; RSDT fallback is explicitly unsupported for this stage.
Validate XSDT's 36-byte header, signature, full bounded length/checksum and eight-byte
entry divisibility. Bound entries to256 and total table length to64KiB. Require each
nonzero child pointer's header readable before inspecting it. Unrelated signatures
are ignored without interpreting fields or claiming their validity; their full
payload/checksum is not trusted or consumed. Zero child pointers are rejected.
Exactly one APIC/MADT is required, with header+fixed fields >=44 bytes, <=64KiB and
full checksum. Duplicate APIC pointers/tables are rejected even if identical.

## MADT authority

Bound every record by the enclosing MADT before reading it: at least two bytes,
length>=2, and length<=remaining bytes. Known types require at least their defined
length; extension tails are skipped. Unknown types are skipped by their validated
length and grant no controller/routing authority. Reserved bits/fields are ignored,
not converted into permissions or rejected as future extensions.

Normalize type0 Local APIC (8-byte minimum) and type9 Local x2APIC (16-byte minimum)
into 16-byte records: 32-bit APIC ID, ACPI UID, original flags and source kind.
Include only Enabled or Online-Capable entries (flags&3); count disabled entries as
irrelevant to active topology, not as startable CPUs. Capacity is256. Reject duplicate
active APIC IDs or active ACPI UIDs across both kinds. Only flags bit0 identifies an
enabled CPU. Exactly one enabled record must match the running BSP's CPUID APIC ID;
online-capable-only is not sufficient. No AP is started or claimed operational.

The runtime obtains BSP ID with CPUID0Bh/EDX when the leaf exists and EBX is nonzero,
otherwise leaf1 EBX[31:24]. Require leaf1 APIC support. This is an identification
input only; APIC mode/base MSRs and controller availability still need validation
before future programming. x2APIC records do not authorize changing APIC mode.

Take local APIC base from the MADT header, replaced by at most one type5 override
(minimum12 bytes). Require final base nonzero, 4KiB-aligned and a whole page inside
the supported physical limit. Record PCAT_COMPAT as flags&1 for a future reviewed
legacy-controller decision; it causes no port access here. IOAPIC, ISO and NMI
structures are not interpreted by this slice and must be validated before routing.

## Publication and layout

A trusted context holds boot/map/root references, BSP ID, caller-supplied record
array/capacity and staged result fields. On entry clear ready/count. Publish a magic
ready value only after the full root/MADT parse, uniqueness/BSP/base checks succeed.
On any failure clear ready/count; partially staged fields/records are not consumable.
Distinct statuses identify unreadable/root/checksum/layout/topology/capacity stages;
entry prints ACPI_FAILURE CODE before stop. Successful boot emits ACPI_TOPOLOGY_READY
with BSP ID, active CPU count, enabled count and local APIC base. The QEMU profile
requires two enabled records and BSP0 matching CPUID; other hardware is not inferred.

Append one page of static BSS for256 CPU records. Expand bounded RX capacity from
0x4000 to0x6000 for the parser and coming interrupt entry code: data RVA7000, rodata
8000, BSS9000. BSS size becomes0xcf000, relocation RVAd8000, image sized9000 and raw
file sizea000. Exact PE and reservation fixtures shift together. Code/constant/data
permissions and all five guard proofs still derive from shared boundaries; boot-info
v0/v1 layouts and firmware ownership reservations stay unchanged. New context state
must fit existing data page; no pointer-bearing initialized objects are introduced.

## Evidence and remaining gates

Host wrappers call the exact Assembly reader/parser using synthetic validated map
and identity tables. Exercise extended RSDP/known MADT records, unknown records,
checksums, boundary/overflow/count caps, adjacent descriptors, RP/wrong-type/unmapped
memory, duplicate MADT/override/CPU IDs, disabled BSP and x2APIC/override semantics.
Test snapshots require no source-table writes and failed-ready/count invalidation.
Eight integer nonvolatile sentinels cover the parser and read predicate. No host
adapter makes arbitrary table pointers readable or bypasses production range checks.

Normal QEMU ISO/FAT must validate real OVMF ACPI tables and CPUID BSP identity before
checkpoint. Repeat all nine exception fixtures, all host memory/descriptor suites,
build and reproducibility. No new physical qualification follows; unsupported ACPI
layouts stop explicitly rather than guessing controllers. Independent implementation
review precedes linked stacked PR. Later timer work still needs APIC mode/base/cache,
mask/EOI/entry/return and interrupt critical-section contracts before STI.

Reference: ACPI6.6 sections5.2.5.3,5.2.8,5.2.12 (RSDP/XSDT/MADT and CPU/override
structures): https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html
