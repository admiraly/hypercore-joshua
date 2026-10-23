# Early boot and Assembly contract v0

Status: proposed internal contract, not a frozen public syscall or application ABI.

## Runtime provenance and executable format

All shipped instructions originate in `boot/uefi.asm` and
`kernel/arch/x86_64/entry.inc`. NASM emits PE32+ directly, with separate RX code,
RW data/BSS, and read-only relocation sections. No imported libraries, third-party
bootloader, generated C startup, or host-language interpreter is shipped.
Position-independent RIP-relative code needs no pointer fixups; an ABSOLUTE-only
base relocation block keeps the image relocatable. Its preferred base is zero.
The image is an EFI application, not yet a separate ELF64 kernel loader.

## Firmware ABI

UEFI x64/Microsoft ABI: RCX, RDX, R8, R9 arguments; RAX status; 32 bytes of caller
shadow space; RSP aligned to 16 immediately before CALL. RAX, RCX, RDX, R8-R11 and
XMM0-XMM5 are volatile. RBX, RBP, RSI, RDI, R12-R15 and XMM6-XMM15 are preserved
when returning to firmware. No red zone. Direction flag is clear. No SIMD is used.
UEFI offsets and boot-info offsets are centralized in `sdk/boot.inc`.

`print` takes a UTF-16 string in RDX and follows this ABI. `print_hex` takes a
64-bit value in RCX. `debug_write` takes an ASCII string in RCX. Other helpers
are internal and use this ABI unless their entry explicitly switches to the
non-returning kernel path. No undocumented cross-module register clobbers are allowed.

## Boot-info v0

RDI points to the versioned record defined in `sdk/boot.inc` at kernel entry.
Fields are fixed-width little endian, naturally aligned. The record includes
magic/version/size, framebuffer base/size/width/height/stride/pixel format,
memory-map base/used size/descriptor size/version, and ACPI RSDP when present.
The map is variable stride; never assume an array of a compiler-defined struct.
Its final copy is captured immediately before ExitBootServices. No allocations,
console calls, or logging occur between that call and ExitBootServices.
An invalid map key is retried at most eight times, reacquiring the map each time.
After an attempted ExitBootServices, failures are handled without console or
other non-memory boot-service calls: execution stops rather than returning into
possibly partially shut-down firmware. This avoids unsafe firmware reuse.

Kernel entry never returns. It disables maskable interrupts, selects an owned
64 KiB stack, clones the active firmware GDT into owned storage, appends its own
code/data descriptors, installs that table and a 256-entry stop-only IDT, and retains the
firmware's existing page tables. It does not free/reuse firmware memory. Exceptions
stop the CPU and emit a bounded serial marker if COM1 exists. There is not yet a
TSS, IST stack, page-table ownership, interrupt routing, or recoverable exception
handling; a stop-only IDT does not constitute production fault containment.
Retaining all original GDT selectors during the transition keeps the still-active
firmware IDT's selectors valid until the replacement IDT is loaded. Firmware GDTs
without room for two appended descriptors, or with a partial entry, stop safely.

Before interactive diagnostics, the firmware watchdog is disabled. Failure to
disable it aborts the application rather than silently allowing a timed reset.

## Memory and ownership invariants

- No physical frame allocator exists yet; no firmware memory is reclaimed.
- Loader, stack, boot-info, IDT, and map remain within the loaded PE image.
- Framebuffer writes are bounded by validated width, height, stride and byte size.
- The loader does not open Block I/O, Disk I/O, Simple File System, or network protocols.
- No firmware calls occur after a successful ExitBootServices.
- Raw COM1 is enabled only for the explicitly recognized QEMU TCG vendor. Physical
  machines do not probe or program legacy UART ports. Early QEMU markers use the
  firmware-initialized UART; configuration is changed only after firmware exit.
- The stub executes on the bootstrap CPU only and leaves maskable interrupts off.

## Deferred contracts

The public handle/syscall ABI, interrupt lock ordering, PMM/VMM ownership,
DMA/IOMMU policy, and AI capability broker remain design tasks. They must be
reviewed before their implementations; illustrative syscall names in the spec
are not assigned stable numeric IDs here.

## Primary references

- UEFI Specification 2.10: x64 calling conventions, EFI System Table, Boot
  Services, GetMemoryMap, ExitBootServices, Graphics Output Protocol, ResetSystem.
- Microsoft PE/COFF specification: PE32+, section flags, base relocation blocks.
- Intel 64 and IA-32 Software Developer's Manual, Volume 3: GDT, IDT, long mode.

## Optional Piper test image

The host-rendered announcement adds a read-only, non-executable fifth `.voice`
section without changing boot-info v0 or BSS/DMA offsets. Playback copies bounded
PCM into the existing HDA buffer. See [Piper test contract](piper-test.md).
