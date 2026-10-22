# Execution roadmap

This narrows execution of the v0.1 proposal; it does not redefine completed work.
M0/M1 remain open: interface review, exact hardware inventory, repeated physical
boots and failure-path qualification still need evidence. Issue #8 prepares M2;
issue #3 tracks memory implementation. Work can proceed in QEMU while these gates
remain visible; it cannot substitute for physical qualification.

## First usable development release

Target one reference Latitude and QEMU, with reliable boot, owned memory management,
single-core interrupts and scheduling, isolated ring-3 ELF programs, keyboard input,
and a basic command shell. Demonstrate that a faulty user program terminates without
corrupting another process or the kernel. Start with a read-only embedded program
image. Writable storage, networking, SMP, speech, AI and GPU acceleration are separate
later gates, not dependencies of this first interactive release. This development
release does not satisfy the proposal's full operating-system completion definition.

## Ordered implementation gates

1. M2 preparation: actionable exception stops, memory ownership contract, normal
   ISO/FAT boot regression and CPU error/no-error frame fixtures. No allocator yet.
2. M2a: validate variable-stride memory descriptors, derive usable ranges, reserve
   occupied memory and establish a physical-frame ledger. Reject malformed maps,
   overflow, overlap and exhausted metadata without reclaiming ambiguous memory.
3. M2b: bounded frame allocator with explicit allocation/free ownership. Exercise
   exhaustion, double free, reserved frames, alignment and randomized stress against
   a host oracle; no firmware or live page-table memory may be allocated.
4. M2c: build and activate owned page tables. Map all transition dependencies,
   separate executable/read-only/writable regions, enable NX and write protection
   where validated, and add an unmapped stack guard. Verify CR3 and permissions;
   inject read-only writes, NX execution and unmapped accesses. No isolation claim.
5. M2d: page mapping/unmapping lifecycle and richer fault diagnostics; test allocator
   and translation stress together. Complete M2 only when its recorded exit tests pass.
6. Single-core interrupts, timer, context switching and explicit interrupt/locking
   contracts; then ring-3 entry, validated syscalls, handles and separate address spaces.
7. Read-only program loading, keyboard input and shell. Verify process crash isolation
   and responsive command input before adding concurrency from SMP or AI workloads.

Each implementation gets a scoped issue, contract changes, focused branch, actual
validation evidence and independent review. Do not merge without maintainer approval.

## Interface direction

WOPR/Joshua from *WarGames* inspires the command-first presentation: near-black
background, restrained blue-white phosphor text, fixed-width type, sparse status
lines and a visible cursor. Offer green as an optional profile later. Use original
assets and wording. Input and diagnostics must work without animation, sound,
models or connectivity; flicker/scanlines/motion must be optional. The current
checkpoint remains an honest status display, not a simulated working shell.
