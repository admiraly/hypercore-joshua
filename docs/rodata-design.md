# Read-only constant section design

Status: design reviewed before implementation, after #19.

Add a 4-KiB `.rodata` section at RVA/raw offset 0x6000 with PE flags 0x40000040
(readable initialized data, not writable or executable). Text remains 0x1000–0x5000,
data 0x5000–0x6000, BSS moves to 0x7000. Relocation/file/image bounds shift through
shared constants and exact PE tests. Boot-info v0/v1 definitions remain unchanged.

Move immutable firmware strings, protocol GUIDs, hex digits, serial markers, native
screen text and font bytes into rodata. Keep handles/pointers, GOP/map data, input,
hex output buffers, CPU-brand buffers, descriptor-table state, allocator/range state
and owned-root metadata writable. Separate kernel data/rodata include files avoid
implicit section switching. Firmware helpers only read constants; no relocation
fixups or pointer-bearing initialized constant objects are introduced.

Owned mapping splits `.data`, `.rodata` and BSS at their page boundaries. The
existing image proof requires rodata RO/NX and mutable sections RW/NX while retaining
cache selectors and inherited-leaf boundaries. Add a ConstWrite fixture after the
owned switch that writes screen_title and expects #PF error3 with exact instruction
RIP and constant address CR2. All normal/fault/repro and host regressions repeat.
This closes constant storage hardening only; no process isolation, user-fault
recovery, mapping lifecycle or new physical qualification is implied.
