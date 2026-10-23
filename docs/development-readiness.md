# Development preview readiness

This is an evidence audit of issue #40 and the first development-release target in
roadmap.md. It is not a release declaration, milestone closure or merge approval.
The current deliverable is an experimental non-installing BSP diagnostic console.
The governing specification remains the destination; a passing QEMU test does not
establish Latitude compatibility or a complete general-purpose operating system.

## Console acceptance

| Requirement | Evidence and scope | Remaining gate |
| --- | --- | --- |
| Assembly runtime; reviewed contracts; no speech overlap | Console/editor/grid/model/response runtime is Assembly. Versioned layouts and integer ABI are retained. Independent design/code/oracle reviews and responses are recorded in PR #41 and validation.md; reviews were source-only. | Maintainer review/merge remains separate. |
| Bounded line editing and history | Actual Features sessions check 127-character capacity/rejection, zero tails and all editor bytes. Host editor tests cover malformed state, guard boundaries and ABI. Live ring-wrap/draft/Left/Right and per-key boundary cases pass on ordinary ISO/FAT/x2APIC ISO and Saturation ISO. | Physical keyboard qualification remains open; this is controlled real-key QEMU evidence. |
| Checked framebuffer and original interface | Host grid/model fixtures check bounded/strided RGB/BGR surfaces and inaccessible boundaries. Live reference grid, entire visible pixel comparisons, cursor retirement and visually inspected target screenshots cover the tested GOP mode. | Shared font content remains a trusted authored asset; arbitrary GOP/physical GPU qualification is open. |
| Real keyboard idle and safe halt | Actual i8042/IOAPIC/LAPIC input, exact owned CPU root/RSP/RIP, 30-second timer-free idle, physical route/controller/timer retirement and IF0 halt checks pass. Separate fixtures prove timer rejection, one-EOI drain and bounded exhaustion. | Natural asynchronous timing, NMI/MC resilience and physical keyboard compatibility are unqualified. |
| Fixed read-only commands and honest errors | Twenty real submissions cover help/about/status/mem/ps/devices/clear/halt, human/JSON, unknown/unavailable/usage results and case/spacing. Independent source/reference bytes constrain reporting/composition. | No storage/network/AI/general task or public capability implementation is inferred. |
| Failure and corruption evidence | BadEditor/BadFacts/Saturation, timer/drain targets, host malformed-state tests and captured replay/mutations distinguish stops from success. Coordinated facts/JSON/grid/pixel forgery is rejected. | Targeted tests are not exhaustive corruption or malicious-firmware/host protection. |
| Regression and reproducibility | Frozen-runtime campaigns include24 host suites,40 keyboard targets,24 TCG user profiles and fault/context/ELF/timer checks. Console listing-inclusive reproducibility and source/trace binding passed. See dated validation.md for exact source and scope. | Historical campaigns are not newly run results. Windows execution for the final PR remains unconfirmed in this record. |
| Clearly named interactive media and instructions | `out/JOSHUA-Console-Test-23a66e3.iso` is the frozen local test copy. That frozen preview predates the issue #46 diagnostic-handle adapter; the newer source has separate target evidence below. README/testing.md describe profiles. | Local artifacts are ignored, not committed binaries. Public CI artifact availability is separate from local delivery. |

## First development-release gates

The roadmap also requires reliable boot and owned memory/interrupt/context/ELF
isolation on the reference Latitude. Existing QEMU evidence demonstrates bounded
private embedded processes and fault containment, with trusted BSP assumptions.
It does not close the broader memory/process milestone exits or physical gates.
Known TCG #SS/#AC exact-vector limits and KVM/mixed-media caveats remain in
validation.md; console testing neither resolves nor hides them.

Still open:

- M0/M1 interface review, exact reference-hardware inventory, repeated Latitude boots
  and failure-path evidence. The earlier owner-reported Latitude checkpoint boot
  predates this console and is not evidence of native keyboard/shell operation.
- Windows qualification of the final source/CI workflow and reference hardware
  qualification of the delivered console, including actual input and safe halt.
- Maintainer decisions on the stacked PRs. No agent merge is authorized.

Speech is a separate later roadmap gate. The already delivered Piper announcement
media belongs to PR #34; it is not silently included in the console or required for
this first interactive preview. Storage, networking, SMP and AI are also separate.

Issue #46 adds an experimental checked diagnostic-handle adapter before console
startup, with13 normal calls per process and termination revocation. Its normal/
full-table xAPIC/x2 ISO/FAT targets,25-profile user campaign and host/regression
evidence are recorded in validation.md. These new runtime bytes differ from the
frozen23a66e3 preview; historical full console Features evidence must not be
reported as newly run validation of the handle adapter. No public capability ABI,
physical qualification or release/milestone closure follows.

The current handle-adapter console ISO is `out/JOSHUA-Console-Handles-Test.iso`
(with SHA256 sidecar). Its final20-command Features ISO session passes; the subsequent current-image Console Features FAT and x2APIC ISO sessions also
pass, each with20 commands, independent live state/facts/pixel checks and30 seconds
of timer-free idle. See the dated final-media evidence in validation.md. The older23a66e3 preview is retained.

The M0 [source-interface audit](interface-contract-audit.md) maps final combined
interfaces to source and scoped evidence. It addresses technical review evidence,
not maintainer signoff or the still-missing reference inventory/photos/boots.

The host-only [Console bundle exporter](console-test-bundle.md) packages the
verified normal Console ISO, original manifest, license, checksums and manual
trial instructions. Trial records start NotRun/Unknown. Packaging is separate
from Windows execution, physical qualification and maintainer signoff.
