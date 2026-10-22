# Contributing to JOSHUA OS

JOSHUA is an early independent Assembly OS, not a complete general-purpose system.
Original project source is MIT licensed; external tools retain their own licenses.

## Before implementation

Read the technical spec, README, AGENTS.md, docs/boot-contract.md, docs/security.md,
and docs/validation.md. Start with a scoped issue and observable acceptance criteria.
Discuss architectural/ABI/security changes before coding. Do not add driver, model,
or platform promises that have not been demonstrated.

Claim an issue in a comment with your human/agent identity and affected paths.
Use one branch/worktree per independently implemented issue. Dependencies should
be explicit. Coordinate overlapping edits before starting, not after a conflict.

## Build and test

The documented host path is Windows PowerShell 5.1. Setup is in docs/toolchain.md.

```powershell
./tools/Get-Toolchain.ps1 -IncludeQemu
./tools/Build.ps1
./tests/Test-Reproducibility.ps1
./tools/Test-Qemu.ps1
./tools/Test-Qemu.ps1 -BootMedia Fat
```

Host tooling is not OS runtime. Only Assembly-authored runtime code belongs in
the native boot/kernel/userland paths. No tool archives, .tools, out, crash dumps,
private host configuration, model weights, or credentials belong in a commit.

## Pull requests and handoffs

Link the issue; state the exact scope and non-goals; explain contract/security
changes; list tests actually run with results; state untested paths and risks.
For kernel, firmware, memory, concurrency, and capability changes, request an
independent review and preserve findings and responses in the PR discussion.
The maintainer decides whether to merge; agents must not manufacture approvals.

Keep commits focused and reviewable. Do not change the historical proposal to
pretend future milestones have shipped. When reporting bare-metal results, include
image hash, hardware/firmware, boot method, repeated trial count, and a redacted
photo when possible. Do not publish device serial numbers, service tags or secrets.
