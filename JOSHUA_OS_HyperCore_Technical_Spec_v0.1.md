---
title: "JOSHUA OS / HYPERCORE"
subtitle: "Architecture & Engineering Specification — All-Assembly, AI-Native, Performance-First Operating System"
author: "Technical Design Draft"
date: "22 October 2026"
version: "0.1 — Proposed, Not Implemented"
geometry: margin=23mm
fontsize: 10pt
colorlinks: true
---

> **Document status:** Design proposal. No kernel, driver, AI runtime, benchmarks, or hardware compatibility described here should be construed as implemented or demonstrated. Targets are hypotheses requiring measurement.

> **Publication decision, 22 October 2026:** The owner selected the MIT License for original JOSHUA source; see `LICENSE`. This supersedes the historical undecided-license entry below. Current implementation and owner-reported boot evidence are tracked in `README.md` and `docs/validation.md`, not inferred from this proposal.

## Executive abstract

**JOSHUA OS** is a proposed independent 64-bit operating system with an original **HyperCore** kernel, a command-first interface inspired by the visual language of the 1983 film *WarGames*, and a native artificial-intelligence execution environment. The central engineering constraints are (a) authored runtime software written in Assembly, (b) extraordinary measurable performance across latency, throughput, footprint, scalability, and energy efficiency, and (c) AI integration that does not place probabilistic model execution in kernel privilege.

JOSHUA is **not** a Linux distribution, a Linux kernel fork, or a terminal skin. Linux and contemporary systems may be studied as references and used as performance baselines, but JOSHUA's kernel and native system software will be independent implementations. The architecture prioritizes sound isolation and correctness as much as raw speed. Every performance assertion must be supported by reproducible experiments on matched hardware.

### Project identity

| Property | Decision |
|---|---|
| Product | JOSHUA OS |
| Kernel | HyperCore |
| Initial architecture | x86-64, UEFI, ACPI, SMP |
| Kernel organization | Modular monolithic; isolation boundaries for user-space services |
| Source policy | All **shipped CPU runtime code** authored in x86-64 Assembly |
| Development assembler | NASM (initial choice), Intel syntax |
| Boot path | Assembly-authored UEFI loader in final architecture; bring-up may use a third-party bootloader on development hosts |
| Binary format | ELF64 for native applications; PE/COFF as required by UEFI boot services |
| Interface | Text-first WOPR/JOSHUA shell, later a custom compositor |
| AI placement | Native user-space services, sandboxed agents, assembly inference runtime |
| Compatibility | New native ABI; Linux compatibility not included in v1 |
| Initial hardware | One documented x86-64 reference machine and QEMU |
| License | Undecided; must be selected before source publication |

## 1. Non-negotiable requirements

**R-001 Independence.** No Linux kernel or distribution forms the operating-system foundation. Any external specifications, algorithms, and licensed reference implementations must be distinguished from code incorporated in JOSHUA.

**R-002 Assembly-only runtime.** Kernel, first-party drivers, native libraries, shells, system services, inference runtime, and first-party native applications shall be authored in Assembly. Assemblers, linkers, packagers, emulators, documentation generators, and benchmark analysis tools running on the *development host* may use other languages. CPU microcode, device firmware, and model weights are external binary/data dependencies, not authored OS source.

**R-003 Performance evidence.** Maintain a continuous performance corpus, published test conditions, per-commit regression checks and matched tuned-Linux comparisons. Do not substitute a faster, less secure operation for an equivalent secured operation.

**R-004 Security.** Enforce user/kernel memory isolation, validated pointers, authenticated resource handles, DMA confinement where supported, and least-privilege agent execution. Maintain security mitigation policies appropriate to the supported CPU.

**R-005 AI-native design.** Provide OS-native agent identity, capabilities, resource budgets, auditable actions, model management, and inference scheduling. Model outputs shall never be treated as authoritative kernel policy.

**R-006 Offline functionality.** The OS, terminal, filesystem, and administrative commands must function without AI services, internet connectivity, or a downloaded model.

**R-007 Usability.** Boot to a responsive command shell even on configurations without hardware GPU acceleration. Graphical phosphor effects must be optional and never block command execution.

**R-008 Maintainability.** A documented assembly calling convention, stable module contracts, mechanically checked offsets, interrupt discipline, testable invariants and source-level debugging are mandatory.

## 2. Reference architecture

```text
+------------------------------------------------------------------+
| JOSHUA UI / WOPR SHELL / NATIVE PROGRAMS / AGENT CONTROL PANEL   |
+------------------------------------------------------------------+
| User-space: init | VFS helpers | net services | AI orchestration   |
| model router | inference workers | context index | capability UI   |
+------------------------------------------------------------------+
| HyperCore ABI: process, memory, handle, IPC, async I/O, timers    |
+------------------------------------------------------------------+
| HYPERCORE: SMP scheduler | VM | interrupts | VFS | IPC | drivers   |
|            DMA/IOMMU | networking fast paths | instrumentation    |
+------------------------------------------------------------------+
| x86-64 CPU | NUMA RAM | PCIe | NVMe | NIC | graphics | firmware    |
+------------------------------------------------------------------+
```

**Boundary rule:** The kernel provides deterministic enforcement, resource management, scheduling, storage, networking and mechanisms. User-space AI services make model-dependent decisions, request authorized capabilities and manage inference. A user-space crash must not corrupt the kernel.

### 2.1 Kernel mode and protection

- Long-mode x86-64 with ring 0 kernel and ring 3 user processes; NX/W^X, SMEP, SMAP, PCID and other features enabled when supported and validated.
- Per-address-space page tables and explicit kernel mapping policy; guard pages for critical stacks; structured exception handling and early fault diagnostics.
- Fully preemptive SMP where lock and interrupt context permit; explicit lock ordering, per-CPU state, memory-ordering rules and verified entry/exit paths.
- Capabilities represented by kernel-checked handles, with revocation and explicit rights for files, processes, IPC endpoints, devices and privileged actions.

## 3. CPU, scheduler and synchronization

**Scheduler baseline:** per-CPU run queues, work-conserving operation, affinity-aware wakeup, fair timeslicing and separate deadline/reservation class. Defer policy optimization until repeatable scheduler microbenchmarks and workloads exist.

**Optimization agenda:** cache locality; minimal cross-core IPIs; topology-aware SMT and heterogeneous-core placement; NUMA locality; tickless idle; batched wakeups when they preserve deadlines; avoid false sharing with cache-line-isolated hot fields.

**Synchronization portfolio:** spinlocks for short non-sleeping critical regions, mutexes for sleeping operations, per-CPU counters, seqlocks where safe, read-copy-update-like mechanisms when reclamation correctness is proven, and lock-free queues only when benchmarks and proofs justify them. Lock-free is not a default synonym for faster.

**Required measurements:** wakeup latency, same-core and cross-core context-switch distributions, scheduler overhead per CPU-second, throughput under contention, migration rate, cache misses and p99 response time under simultaneous AI inference.

## 4. Memory management

- Boot memory map validation; physical frame ownership accounting; buddy or bitmap-based physical allocator selected by benchmarks.
- Independent virtual-memory subsystem: per-process mappings, demand paging, copy-on-write, shared memory, protection faults, user/kernel isolation.
- Slab/size-class allocator with per-CPU magazines for small allocations; bounded metadata overhead and fragmentation stress tests.
- NUMA policy, page placement, huge-page mapping and split/merge on supported hardware; huge pages must be opt-in or policy-driven according to workload evidence.
- Batched shootdowns, TLB-conscious mapping strategies, safe pinned buffers, IOMMU-managed DMA where available, and explicit cache coherency rules.
- Optional model-backed immutable weight mappings shared among inference workers; KV cache accounted and reclaimable per workload budget.

**Safety invariant:** No untrusted process or agent can map another process's writable memory without an explicitly authorized kernel capability.

## 5. Native ABI and IPC

Version a documented 64-bit syscall ABI from its first public release. Syscalls must use stable numeric IDs, integer widths, alignment rules, error codes and concurrency semantics. An **illustrative** namespace, subject to change before ABI freeze:

| Group | Illustrative operations |
|---|---|
| Tasks | `task_create`, `task_exit`, `task_wait`, `thread_create` |
| Memory | `vm_map`, `vm_unmap`, `vm_protect`, `shared_create` |
| Objects | `handle_dup`, `handle_close`, `handle_query` |
| IPC | `channel_create`, `channel_send`, `channel_recv` |
| Async I/O | `io_ring_create`, `io_submit`, `io_wait` |
| Time | `clock_read`, `timer_create`, `timer_arm` |
| Security | `cap_restrict`, `cap_transfer`, `cap_revoke` |

For the first ABI revision, prioritize correctness and simple handle-based objects. Later optimize frequent calls via batched submission, shared-memory command/completion rings, registered buffers, and bounded busy-poll options. Never permit unchecked user pointers simply to improve a microbenchmark.

## 6. Storage, devices and networking

**Bring-up sequence:** serial console → timer/APIC → keyboard → PCI discovery → storage → filesystems → NIC → USB → accelerated graphics. Each driver receives an explicit lifecycle, IRQ/DMA contract, fault-reporting path and test hardware profile.

- **Storage:** native NVMe submission/completion queues, multiqueue dispatch, scatter/gather I/O, flush and durability semantics. Initial read-only initramfs followed by a simple reliable writable filesystem; advanced journaling/COW filesystem only after crash-recovery tests.
- **VFS:** handle-based file operations, explicit metadata and permissions, page cache with bounded reclamation policies.
- **Network:** first-class TCP/IP with checksum offload and multiqueue integration where hardware supports it. Validate interoperability, congestion behavior, packet loss, retransmission and latency under load.
- **DMA:** IOMMU configuration and per-device DMA domains when available; reject unsafe mappings and device-controlled address access by default.
- **GPU:** do not promise generic vendor acceleration in v1. Commit to a specific publicly documented accelerator path only after driver feasibility review. Early releases use CPU text rendering/framebuffer output.

## 7. AI-native architecture

The intelligence subsystem operates in isolated user space and consists of:

1. **JOSHUA Core:** terminal-facing dialogue, planning, persistent sessions, execution status and deterministic command parser for ordinary commands.
2. **Agent Supervisor:** each agent receives an identity, workspace scope, tool contract, quotas, cancellation, process isolation and durable audit log.
3. **Capability Broker:** validates actions against permissions independently of model text; approvals required for destructive or high-impact actions.
4. **Model Router:** selects among installed local models by capability, throughput, latency, memory pressure and user policy.
5. **Inference Engine:** all-Assembly CPU runtime with quantized operators, tokenization, KV-cache handling, decode loops and model loading. GPU acceleration is a separate architecture-specific driver and kernel workstream.
6. **Context Service:** file and project indexing, semantic retrieval, access-aware embeddings and memory retention controls.
7. **Compute Manager:** schedules model residency, memory bandwidth, CPU affinity, prefill/decode batching and inference budgets in cooperation with OS facilities.

### 7.1 Assembly inference design

**First supported workload:** one precisely specified small decoder-only model architecture and one quantized weight format. Avoid claiming arbitrary model compatibility initially. Build reference-correct kernels for quantized matrix-vector multiplication, normalization, rotary position transforms, attention, sampling and KV-cache access. Add optimized AVX2 dispatch first; AVX-512 and vendor-specific alternatives only on CPUs that support them.

GPU shader or compute machine code is an exception to **x86-64** assembly rather than an exception to the intent of the Assembly-only requirement: device-executed kernels must use the supported device's native instruction format, and the build pipeline must document whether it accepts an assembler intermediate representation.

**AI performance metrics:** tokens/s by model and quantization, time to first token, p95/p99 request latency, concurrent-session throughput, RAM/VRAM footprint, bytes transferred per decoded token and joules/token. Compare to mature inference engines only with identical weights, numerical quality tolerances and hardware.

### 7.2 AI safety and reliability

Model output is untrusted. Retrieved documents, web content, filenames and tool outputs cannot grant capabilities. All tool invocations are parsed, validated, policy-checked and recorded before side effects. Agents run with user-defined budgets; cancellation must release model memory, handles and worker processes without rebooting the OS.

## 8. WarGames-inspired interface

**Visual specification:** near-black canvas, blue-white phosphor typography, optional green monochrome profile, fixed-width bitmap/vector fonts, rectangular strokes, sparse layouts, visible cursor, and restrained animation. Use original naming/assets and avoid copying protected film graphics or logos into a distributed product without rights review.

**Native modes:** `JOSHUA` (blue-white WOPR), `IMSAI` (green phosphor) and `NORAD` (multi-pane telemetry). An accessibility configuration disables flicker, scanlines and motion. Input echo and command dispatch must not wait for CRT shader effects or AI inference.

**Core commands:** `help`, `files`, `ps`, `mem`, `net`, `devices`, `model`, `agents`, `grant`, `revoke`, `bench`, `logs`, `shutdown`. All commands must work with scripted arguments, exit codes and machine-readable optional output.

## 9. Performance policy and benchmark contract

**Objective:** make JOSHUA Pareto-competitive across responsiveness, raw throughput, memory efficiency, multicore scaling and energy; do not claim simultaneous theoretical maxima that conflict physically. Optimize policies per workload subject to explicit latency, memory and power constraints.

| Dimension | Required benchmark categories | Guardrails |
|---|---|---|
| Latency | syscall cycles, scheduler wakeup, terminal input, p99 I/O | matching isolation and mitigations |
| CPU | scalar/SIMD kernels, compile workload, cross-core scaling | equal CPU clock/power envelope |
| Memory | allocation rate, fragmentation, page-fault time, RSS | leak and memory-pressure tests |
| Storage | sequential/random IOPS, fsync, tail latency | equivalent durability semantics |
| Network | throughput, packets/s, p99 RTT, CPU cost | equivalent MTU, offloads, loss profile |
| UI | input-to-render and dropped-frame rate | identical resolution and refresh |
| AI | TTFT, tokens/s, concurrency, energy/token | same model, quantization, output quality |
| Efficiency | idle watts, joules/work, thermal throttling | equal platform and instrumentation |

**Benchmark procedure:** pin firmware version, CPU microcode, topology, BIOS settings, compiler/assembler, kernel configuration, hardware mitigations, ambient temperature and workload seeds. Record independent warm/cold runs, confidence intervals, CPU frequencies and temperature. Use at least three repeated trials for exploratory development; establish greater statistical power for release claims. A comparison is invalid when one side uses disabled security protections, no durable writes, or a materially different workload.

**Initial gates (not numerical promises):** successful boot; no kernel crashes over the stress suite; deterministic syscall behavior; no regressions without justification; minimum supported hardware reproducibility; documented latency/throughput curves under contention; publish comparison results whether JOSHUA wins or loses.

## 10. Assembly engineering standards

- Separate architecture-dependent entry/exit code from policy modules; use named include files for layouts and calling conventions.
- Standardize register preservation, stack alignment, red-zone rules, interrupt frames, SIMD context save/restore, errno conventions and unwinding/debug symbols.
- Ban undocumented clobbers. Require review and tests for each handwritten assembly fast path.
- Use structured macros sparingly; inspect actual disassembly, code size, cache footprint and branch layout.
- Generate debug symbols and source maps; capture crash dumps and trace events without depending on a graphical interface.
- Unit-test pure routines on a development host; use QEMU for privilege and interrupt paths and bare metal for timing-sensitive measurements.
- Enforce static checking for section permissions, stack alignment, syscall table completeness and forbidden instruction-set use on the baseline CPU.

**CPU policy:** an x86-64-v3-class *optimized release target* is acceptable, but provide a separately defined conservative bring-up baseline to avoid inadvertently preventing test hardware from booting. CPU features are detected at runtime before feature-specific code paths execute.

## 11. Build, release and reproducibility

The development host may run Linux, Windows or another OS. Host tooling may include NASM, a linker, QEMU, debugger, disassembler, CI scripts and image-building utilities in non-Assembly languages; **none of this becomes shipped OS runtime code**. Document every included third-party binary, its license and provenance. Native release artifacts are bootable disk images and signed manifests with reproducible hashes.

A proposed repository layout:

```text
joshua-os/
  docs/             architecture, ABI, performance methodology
  boot/             x86-64 / UEFI Assembly loader
  kernel/arch/      entry, paging, APIC, IDT/GDT, CPU feature dispatch
  kernel/mm/        physical and virtual memory, allocators
  kernel/sched/     tasks, per-CPU queues, timers, context switches
  kernel/ipc/       capabilities, channels and async I/O
  kernel/fs/        VFS and native filesystem integration
  drivers/          serial, input, PCI, NVMe, network, graphics
  user/init/        first user process
  user/shell/       WOPR-inspired command interface
  ai/runtime/       inference ops, tokenizer, decode, quantization
  ai/agents/        supervisor, permissions, audit and context
  sdk/              macros, ABI headers, native application examples
  bench/            microbenchmarks and matched comparison harness
  tests/            host tests, QEMU tests, bare-metal validation
  tools/            host-only build and diagnostic scripts
```

## 12. Milestones and exit criteria

| Stage | Deliverable | Exit test |
|---|---|---|
| **M0 - Contract** | Architecture, assembly ABI, threat model, hardware spec | Reviewed interfaces; reproducible toolchain |
| **M1 - Boot** | UEFI → long mode → framebuffer + serial diagnostics | Boot repeatably in QEMU and reference PC |
| **M2 - Memory** | Page tables, PMM/VMM, allocation, page faults | Isolation, fault injection, allocator stress |
| **M3 - SMP** | AP startup, interrupts, preemption, scheduler | Multi-core correctness, latency histograms |
| **M4 - Userland** | Ring-3 ELF programs, handles, syscalls, shell | Process isolation and terminal usability |
| **M5 - Persistence** | NVMe, VFS, native writable filesystem | Crash recovery and verified data durability |
| **M6 - Network** | Native Ethernet/IP/TCP, system utilities | Protocol conformance and load testing |
| **M7 - Native AI** | Quantized CPU inference, agents, broker | Model-quality parity and sandbox tests |
| **M8 - Acceleration** | Feasible GPU/device compute backend | End-to-end verified throughput gains |
| **M9 - Release** | Installer/image, docs, hardening, benchmarks | Public reproducible release qualification |

**Parallel workstreams:** UI can evolve after M1, the CPU-only inference math library can be tested on the host before M7, and benchmark harnesses should exist from M0. Their integration does not bypass foundational isolation and kernel correctness milestones.

## 13. Principal risks and mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| Driver coverage and modern GPU complexity | Critical | Narrow reference hardware; CPU inference first; feasibility spike early |
| All-Assembly codebase maintenance cost | Critical | ABI discipline, tooling, unit tests, design reviews, small modules |
| Micro-optimizations reduce real-world performance | High | Workload-driven profiling and mandatory regression evidence |
| Concurrency, interrupt and memory safety defects | Critical | Invariants, isolation tests, fault injection and stress suites |
| AI agents exceed authority or misinterpret data | Critical | Independent capability broker, audit, sandboxing, approvals |
| Compatibility expectations exceed actual support | High | Native ABI scope; explicit supported-device/application matrix |
| Security mitigations slow benchmark paths | High | Equal protection in comparisons; never disable silently |
| Unrealistic timelines | High | Milestone gates; release only on demonstrated correctness |

## 14. Definition of 'operating system complete'

A standalone JOSHUA release must boot without a host OS; correctly isolate concurrent user processes; recover cleanly from routine user-process crashes; provide durable storage, input, networking on specified hardware, native programs and an interactive shell; start and stop AI services safely; document hardware and security constraints; and pass a published regression and benchmark suite. Booting into a terminal alone is a milestone, **not** a complete general-purpose OS.

## 15. Immediate engineering decisions

1. Select one exact reference platform (CPU, motherboard/firmware, NVMe, NIC, optional GPU) and QEMU machine configuration.
2. Freeze the assembler choice and written calling convention for early modules.
3. Decide whether the first UEFI boot artifact is itself entirely Assembly or whether a temporary development-only bootloader is acceptable.
4. Draft the v0 syscall/handle ABI and interrupt/memory ownership invariants.
5. Define the first supported local model and quantization format for the host-testable assembly inference prototype.
6. Establish CI with boot smoke tests and a benchmark results schema **before** optimizing scheduler or memory paths.

---

### Architecture position

**JOSHUA OS is an original all-Assembly system, not a Linux derivative. HyperCore is a deterministic, performance-oriented, SMP-capable monolithic kernel. AI is native to the OS user-space architecture, with capability-controlled agents and an Assembly inference runtime. The WarGames aesthetic belongs to the interface, never to the correctness-critical execution path. Performance superiority is a testable objective, not an assumed property of Assembly.**
