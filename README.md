# JOSHUA OS / HyperCore

An independent, Assembly-authored operating-system project. The governing proposal
is `JOSHUA_OS_HyperCore_Technical_Spec_v0.1.md`; it is not a list of implemented features.

## Current implementation

This is an **M0/M1 boot prototype with experimental M2 memory bring-up**,
not a usable operating system.
The unsigned x86-64 UEFI application is authored entirely in NASM Assembly. It
provides firmware diagnostics, captures the graphics framebuffer and memory map,
exits boot services, and enters an embedded HyperCore stub with its own stack,
GDT/TSS, guarded emergency stacks, fault-stop IDT, and framebuffer text renderer. The stub deliberately halts.
It validates the final memory map and records conservative memory candidates
excluding the loaded image, page zero, framebuffer and inventoried active paging
structures. A bounded physical frame allocator tracks exact ownership, zeroes
allocations and checks allocate/write/free during boot. It builds and activates
owned page tables with supervisor-only W^X/NX permissions and an unmapped stack
guard and dedicated read-only constant storage, preserving inherited cache selectors.
A bounded high-half kernel arena tracks allocations and invalidates translations
before physical frame reuse. Bounded ACPI root/MADT validation identifies the BSP
and local APIC topology before future interrupt work. These paths are QEMU-tested; the
new memory implementation has no physical qualification.
There is no scheduler, process isolation, storage driver, network stack, shell,
installer, or AI runtime. No disk-write protocol is opened.

**Bare-metal status:** on 22 October 2026, the owner confirmed a flawless boot
through Ventoy on the Dell Latitude 7430 (i7-1265U, 16 GB RAM, BIOS 1.35.0),
reaching the intended checkpoint. This is an owner-reported successful boot,
not a completed repeated-cold-boot or hardware qualification campaign.

## Build on Windows

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1
```

NASM 2.16.03 must be available on PATH or in `.tools/nasm-2.16.03/nasm.exe`.
See `docs/toolchain.md` for the project-local tool setup. Image packaging uses
only Windows PowerShell/.NET; no administrator privileges, mounted images, Python,
WSL, C compiler, linker, or third-party bootloader are required.

Outputs:

- `out/BOOTX64.EFI`: unsigned native UEFI application and embedded kernel stub.
- `out/joshua-uefi.img`: FAT UEFI boot image, **not** an installer or hybrid disk image.
- `out/joshua-m0.iso`: UEFI-only El Torito ISO for initial Ventoy testing.
- `out/manifest.json`: deterministic build inputs and SHA-256 artifact hashes.
- `out/boot.lst`: NASM source listing for diagnostics.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/Test-BootImage.ps1 -EfiPath out/BOOTX64.EFI
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Test-Qemu.ps1
```

The QEMU test needs the optional local emulator and its EDK II firmware. It tests
the actual ISO boot, not a host imitation. See `docs/testing.md` for bare-metal
instructions and the distinction between static, emulator, and laptop validation.

See [the execution roadmap](docs/roadmap.md) for staged implementation and the
WOPR/Joshua interface direction, and [the M2 memory contract](docs/memory-contract.md)
for proposed ownership and paging invariants.

## Next gates

M0 remains open pending full contract review and exact NVMe/NIC/GPU inventory.
MIT licensing is selected and the first Latitude boot is owner-confirmed.
M1 remains open until repeated Latitude boots and failure-path evidence are
recorded. M2 ownership, mapping lifecycle, stress and hardware qualification
remain open; the first owned-paging transition does not establish process isolation.

## Collaboration and license

Original project source is licensed under the MIT License; see `LICENSE`.
External host tools and firmware retain their own licenses and are not committed.
Use issues for scoped work and evidence, and pull requests for reviewable changes.
See `CONTRIBUTING.md`, `AGENTS.md`, and `SECURITY.md` before contributing.
