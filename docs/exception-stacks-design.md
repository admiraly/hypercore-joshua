# Owned TSS and emergency exception stacks

Status: independently reviewed for issue #24, based on #23. This foundation keeps IF clear and every exception non-returning; it adds
neither interrupt routing, user execution nor recovery. The memory exit audit keeps
M2 open while its isolation requirement remains unmet.

## Descriptors and transition

Preserve the cloned firmware GDT during the transition so its still-active IDT
selectors remain valid. Require original GDT bytes nonzero, divisible by eight,
and at most 65,504; append the existing eight-byte code/data descriptors and a
16-byte available 64-bit TSS descriptor. GDT limit becomes original byte count +31.
New selectors are original byte count, +8 and +16. The TSS descriptor has present
DPL0 system type9, byte granularity, limit103, full 64-bit base and zero reserved
bits. LTR sets its busy type; do not reload the busy descriptor. STR and descriptor
inspection verify the loaded selector and busy type before the owned IDT is loaded.

A zeroed, eight-byte-aligned 104-byte TSS stays wholly within the writable `.data`
page. RSP0 points to the existing kernel stack top (future entry foundation only).
IST1=double fault, IST2=NMI, IST3=machine check, IST4=all other stop-only vectors.
Unused RSP/IST/reserved bytes remain zero. I/O bitmap offset104 is beyond limit103;
this supplies no user I/O permissions. This slice does not yet enter CPL3 or test
user I/O denial. The GDT remains writable for the hardware busy/accessed updates.

Build the complete IDT with explicit choices: vectors8/2/18 select IST1/2/3;
all remaining gates select IST4. Gate offsets/selectors/type/DPL remain unchanged.
Load TR before LIDT exposes these choices. Early faults before that point retain
the old limitations; no temporary firmware-IDT resilience claim is added.

## Owned memory and frame handling

Append four blocks to image BSS, each a 4-KiB lower guard and 16-KiB stack, all
page-aligned and distinct. BSS grows by0x14000 (0xba000->0xce000); text/data/rodata
RVAs and boot-info layouts stay unchanged. Image minimum/relocation bounds and exact
fixtures shift through shared constants. Maintain a runtime five-guard address
array (main stack plus four IST guards), filled with RIP-relative addresses. Owned
mapping splits writable runs at every guard; the whole-image proof requires all
guards absent and every TSS/GDT/stack page at its required identity permission.
No allocator-owned or firmware storage is repurposed for these stacks.

Stubs retain normalized vector/error/RIP/CS/RFLAGS/oldRSP/oldSS. The common stop
path captures vector/error/RIP/CR2 plus frame RSP and old RSP. It stays on the
hardware-selected IST stack: align below the intact frame and reserve shadow
space; never reset to kernel_stack_top. The existing fault line remains stable;
a separate FAULT_STACK line adds FRAME, OLD_RSP and IST index. Normal boot emits
TSS_READY with selector and stack bounds for checker evidence; QMP inspects TR.
Serial output is still only on the recognized QEMU path, bounded and firmware-free.

This is not a nesting protocol. Reentry on the same IST restarts at the same top
and may overwrite an earlier frame. Distinct DF/NMI/MC storage limits overlap but
does not prove arbitrary nesting, NMI/MC recovery, bad emergency stacks or early
transition fault containment. No IRET occurs, so no task resumption is claimed.

## Controlled fixtures and validation

Retain all seven existing fixtures and require their FRAME to lie within ordinary
IST4. Add StackFault: announce instruction/target/oldRSP while the normal stack is
valid, then set RSP at the main stack guard's upper boundary and read page zero.
The #PF handler must run on IST4 with exact RIP, CR2 zero and expected old RSP,
without touching the unusable interrupted stack. Error code0 is expected.

Add DoubleFault: point IST4 into its own unmapped guard, then read page zero. Failure
to deliver #PF escalates to #DF on distinct IST1. Require vector8/error0 and frame
inside DF stack, plus CR2 inside the deliberately bad ordinary guard on the QEMU
profile. Do not require saved #DF RIP to identify the original instruction: the
architecture does not define it for reliable restart. Old RSP is logged, not treated
as restart evidence. The changed TSS field exists only in this fixture image.

Host fixtures decode exact descriptor/TSS/gate construction helpers, boundary GDT
sizes and guard-policy decisions; malformed sizes must fail before copying or
publishing descriptors. QEMU checks STR/TR, all reported stack bounds, frame/oldRSP
and the injected delivery cases. Re-run normal ISO/FAT, full fault matrix, all memory
host suites, build and reproducibility. Independent implementation review is required.
Physical qualification, NMI injection and machine-check injection remain unclaimed.

## Architectural reference

Intel SDM Volume3A, sections6.14.2/6.14.5 and7.7: 64-bit frame layout, IST stack
selection and TSS layout; exception reference for double-fault restart limitations.
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
