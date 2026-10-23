# M0 source-interface audit

Scope: issue #1's boot-info/register/stack review, against runtime source `e1555ba`
(PR #47) and its current contracts. The active branch makes documentation changes
only. This is technical review evidence, not maintainer signoff, M0/M1 closure or
a development-release declaration. The governing proposal remains the destination;
README and dated validation.md records describe what has actually been exercised.

## Review boundary

The independent reviewer received the original autonomous first-development-release
brief, the issue #1 acceptance criteria, prior component findings/responses and
unresolved qualification limits. Review covered the combined interface paths in
source, not an exhaustive instruction-by-instruction audit of every helper. The
reviewer made no edits and ran no tests, builds, generated-artifact inspection or
network operations. Parent test results were supplied context, not reproduced.

The source audit found one P3: boot-contract.md's provenance inventory omitted
shipped architecture, scheduler, user/ELF/handle, input and conditional console
modules. Its corrected statement names the transitive Assembly graph and the
profile-specific Build.ps1 input registry. No new concrete runtime/ABI defect was
identified in the reviewed interface paths. Earlier PE layouts are historical
checkpoint values: the current console integration section explicitly supersedes
them and agrees with sdk/boot.inc. The audit does not turn absence of a finding
into proof of all runtime correctness.

## Current interface crosswalk

| Boundary | Authoritative source/contract | Current invariant and evidence scope |
| --- | --- | --- |
| Authored runtime and packaging | boot/uefi.asm includes; tools/Build.ps1 inputs; toolchain.md | NASM-authored runtime, separate host tools. Literal include traversal covers 71 Assembly/SDK files across all conditional profiles; every path occurs in the build input registry. No incbin directive is present in that closure. This is a source registry check, not proof of malicious-build resistance or code semantics. |
| Firmware return ABI | efi_entry/print/print_hex/read_key in boot/uefi.asm; boot-contract.md Firmware ABI | RCX/RDX/R8/R9 arguments, RAX result, 32-byte shadow space, 16-byte pre-CALL alignment, no red zone; integer nonvolatiles preserved. Runtime does not use SIMD; firmware-return paths must preserve XMM6–15. Private non-returning/internal adapters have separately documented conventions. |
| Versioned handoff | sdk/boot.inc BOOT_INFO/BOOT_INFO_V1; image_info/get_memory_map/boot_kernel; kernel_entry | Frozen 88-byte prefix, version1/size104 with bounds at offsets88/96; kernel requires version1/size104, never guesses v0. RDI points at the image-owned record; kernel parks it in R12 after CLI/CLD and an owned stack switch. No public syscall ABI follows from the boot layout. |
| Final firmware exit | boot_kernel/get_memory_map; boot-contract.md Boot-info | Final map capture immediately precedes ExitBootServices without logging/allocation between. Invalid map-key retries are bounded to eight. Failure after an attempted exit stops through bounded emulator diagnostics, without firmware console recovery; success never returns to firmware. |
| Image and memory ownership | memory-contract.md; map/ranges/candidates/paging-inventory/frames/owned-start | Map bounds/stride/physical width and overlaps are checked before candidate use. Full image, page0, framebuffer and active tables are excluded. Exact frame ownership and active mapping retirement precede reuse. Caller-owned readable metadata, retained identity-map assumptions and IF0 serialization remain explicit preconditions. No firmware reclamation is inferred. |
| Current PE layout | sdk/boot.inc; boot-contract.md Current console integration | Text RVA1000h/sizeD000h; dataE000h/1000h; rodataF000h/5000h; BSS14000h/E8000h; relocationFC000h/raw14000h; imageFD000h/file15000h. Nine guarded kernel stacks; boot/user ABI sizes remain unchanged. Historical chapter layouts do not override these constants. |
| Root and cache transition | owned-paging-design.md; owned-map/owned-start/paging-pat | Owned CR3 and whole-chunk ownership/cache proof, supervisor W^X/NX and CR0.WP. Inherited selectors/leaf boundaries are preserved without promotion across old leaves; PAT/MTRRs are not programmed. Effective-WB, arbitrary firmware layouts, DMA and physical compatibility are not claimed. |
| Descriptor authority | descriptors.inc, user/descriptors.inc, retire.inc, machine.inc | Owned segment reload/LTR establishes descriptor-cache provenance. TSS/IST and current IDT targets remain valid while copied firmware prefix is retired; LDTR/FS/GS and fast-entry state are checked before CPL3. No later firmware selector dependency is allowed. Early-transition, nesting and NMI/MC resilience remain limited. |
| Exception versus returning frame | exception stubs/common path; irq_save/irq_restore; context/user/input frame helpers | Returning IRQ frame is192 bytes: shadow32 +15 integer GPRs +five hardware qwords. Normalized discarded exception frames separately carry vector/error/RIP/CS/RFLAGS/oldRSP/SS. These are distinct ABIs. Error-vector software INT is unsupported. Bounds precede frame reads, but readable owned stacks are caller proofs. |
| BSP interrupt and context ownership | apic/context/keyboard designs; interrupts.inc; sched/context/start | Actual ISR ownership, EOI ordering, exact phase/root/RIP/RSP/frame checks and bounded retirement govern returning handlers. IF1 is confined to documented waits, trusted-worker intervals and user execution; memory helpers remain IF0 and are not interrupt-safe allocation APIs. Trusted shared-root workers prove a bounded preemption smoke, not a general scheduler or SMP. |
| Private ELF/user authority | elf-loader/user-isolation designs; user/frame/map/proof/start | Two fixed single-use private contexts, bounded authored ELF profile, exact entry and private code/data/stack permissions. Saved CPL3 origin permits discarded containment; kernel-origin invariant faults stop. Watchdog/exit/fault paths restore the separate bootstrap continuation after owned drain. Not an arbitrary hostile ELF/process system. |
| Diagnostic handles | handle-design/handle-gate-design; handles/handle-start/payload | Process-derived tables/owners, distinct nonreused fixed tags1/2, 40-bit generations and rights attenuation. Known calls change saved RAX/RDX only; immutable object1 returns a nonpointer diagnostic value. Revocation under IF0/kernel root precedes drain/bootstrap publication. General resources, transfer/refcount, dynamic incarnation allocation and a frozen public capability ABI remain absent. |
| Terminal boundary | console-design.md; input/window and console/start/model | Actual owned IRQ44 input, timer-free prompt idle, IF0 editing/model/rendering and exact retirement before HALTED. Fixed read-only diagnostics only, without arbitrary command-text execution, AI, filesystem or network authority. Shared font and trusted build/QMP trace remain evidence assumptions. |

## Evidence binding and limits

The latest implementation record is validation.md's **Experimental ring3 handle
adapter** section at `e1555ba`. Confirmed local parent evidence includes all 24
host Assembly suites, 41,585 handle model/ABI/guard checks, the 25-profile TCG user
campaign, eight normal/full xAPIC/x2 ISO/FAT targets, nine faults plus context/ELF/
APIC regressions, normal/full/Console reproducibility and a 20-command Console
Features ISO session. Snapshot baselines reject 101 mutations each and the actual
level validator rejects 49 separate reserved/cache mutations. Counts are targeted
operations/mutations, not exhaustive state-space or hardware qualification.

Those results were not rerun for this documentation-only audit. The 71-file literal
include/registry check was newly run here; it includes inactive conditional
profiles and does not assert every file emits instructions in every image. It
checks registration of paths, not the selected manifest bytes or package contents.
The existing Build/PE/media tests supply the separately scoped packaging evidence.

Known TCG #SS/#AC exact-vector and KVM/mixed-media caveats remain in validation.md.
No new KVM/Windows execution, raw physical-machine result, performance comparison,
formal proof or independent reproduction is claimed. The normal kernel profile
halts intentionally; the separate Console profile reaches the interactive prompt.
The first release target's single-core scope does not satisfy the proposal's M3
SMP or M5–M9 persistence/network/AI/acceleration/release exit criteria.

## Remaining M0/M1 and release decisions

| Required evidence/decision | Status after this audit |
| --- | --- |
| Boot-info/register/stack technical review | This crosswalk plus independent source review/response are concrete review evidence. Maintainer signoff is still open; no approval was manufactured. |
| Reproducible toolchain | Local pinned-toolchain evidence is recorded; final Windows/remote CI qualification remains unconfirmed in this record. PR monitoring owns external check updates. |
| Exact Latitude CPU/NVMe/NIC/GPU inventory | Owner-reported baseline remains in hardware.md; pending exact IDs/model evidence was not filled from marketing names or another machine. The read-only collector is separate [PR #43](https://github.com/admiraly/hypercore-joshua/pull/43) and has no physical qualification by itself. |
| Firmware settings/photos | Not supplied. This workflow neither changes settings nor enrolls keys. Redact service tags, serials and private identifiers before publishing evidence. |
| Repeated reference boots and failure paths | Issue #2 remains open. Preserve image hash, boot method/version/mode, display mode, per-trial outcomes and redacted checkpoint photos. Earlier owner-reported bring-up does not qualify the current console. |
| Current execution-host identity | A read-only allowlist comparison established that this host is not the designated Latitude7430; no unrelated host inventory was emitted or substituted for reference evidence. |
| Maintainer disposition | Focused stacked PRs remain unmerged. This audit does not authorize merging, close issue #1/#2, or declare any milestone/release complete. |

The remaining physical observations require the designated reference device and
its operator. They cannot be established by more QEMU passes or source review.
