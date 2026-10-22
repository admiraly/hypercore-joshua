# Reference hardware: LAT7430-01

Owner-reported configuration, 22 October 2026:

| Component | Value | Evidence |
|---|---|---|
| System | Dell Latitude 7430; regulatory model P136G | Owner report |
| CPU | Intel Core i7-1265U | Owner checked firmware |
| Memory | 16 GB | Owner report |
| Firmware | Dell BIOS 1.35.0 | Owner checked firmware |
| Boot mode | x86-64 UEFI | Owner report |
| Graphics | Integrated Intel, exact PCI ID pending | Provisional |
| NVMe / controller | Pending model and PCI IDs | Not yet inventoried |
| Ethernet / Wi-Fi | Pending model and PCI IDs | Not yet inventoried |
| Internal disk contents | Disposable, per owner | Not permission for unattended erasure |

Keep UEFI enabled. Disable Secure Boot for this unsigned development image.
Do not change SATA/RAID mode, TPM settings, firmware version, or internal boot
entries to run this test. Do not enroll development keys as part of this stage.
Secure Boot signing and verified kernel loading are later security work, not a
performance optimization. Ventoy's own trust setup does not authenticate this
project's loader and kernel.

The first display path uses the UEFI Graphics Output Protocol (GOP), then direct
framebuffer writes after ExitBootServices. No Intel GPU driver is implemented.
Only GOP RGB/BGR 32-bit packed pixel formats are supported initially. PixelBitMask
and PixelBltOnly modes are rejected before leaving firmware. Firmware must supply
a framebuffer of at least 640x400 and a memory map fitting the bounded 128 KiB buffer.

## Emulator profile

QEMU `q35`, TCG, `qemu64`, 2 virtual CPUs, 512 MiB RAM, standard VGA, no NIC,
read-only CD-ROM, EDK II x86-64 UEFI, Secure Boot off. This conservative CPU profile
must boot without AVX2, AVX-512, or assumptions about P-core/E-core topology.
Only the bootstrap processor executes code in this milestone; SMP is not implemented.

Before driver work, record firmware photos plus CPU, NVMe, NIC, and graphics PCI
vendor/device IDs. Do not infer exact devices from a laptop marketing name.

## First physical boot

On 22 October 2026 the owner confirmed that the Latitude booted the prototype
through Ventoy flawlessly and reached the intended native checkpoint. No photo,
exact Ventoy version/mode, display resolution, independently checked image hash,
or repeated cold-boot count has yet been supplied. This is a successful reported
bring-up, not a broad hardware-compatibility or reliability claim.
