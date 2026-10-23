# Memory gate evidence audit — 23 October 2026

The technical specification's M2 exit test is isolation, fault injection and
allocator stress. Issue #3 also requires a versioned physical map, owned PML4/W^X,
QEMU and reproducibility. This audit records evidence without marking M2 complete.

| Requirement | Evidence | Remaining gate |
|---|---|---|
| Versioned physical map | v0 retained; v1 image bounds; variable-stride validation and reservation oracle (#11/#13) | Real-firmware malformed-map injection unqualified |
| Owned PML4/W^X/NX | #19 CR3 transition, permission proof, #21 RO constants | No process boundary; broad supervisor identity map retained |
| Allocation ownership | #17 exact records/zeroing/free; #23 active virtual retirement | BSP/IF-clear only; no table reclamation/general VM |
| Allocator/translation stress | 6,297 frame, 6,249 lifecycle, 1,061 owned-map checks plus boot smoke | Full live-record and hardware stress not established |
| Fault injection | Seven controlled QEMU fixtures through #23 | Controlled invalid-normal-RSP and failed-PF-delivery cases now pass in #24; arbitrary nesting/NMI/MC remain unqualified |
| Isolation | Supervisor W^X protection fixtures only | Separate user address spaces, entry/syscalls and process crash isolation missing |
| Build/media correctness | Build/repro/normal ISO/FAT through #24 locally pass; #21 CI passed | #24 CI pending; latest physical qualification absent |

No PR has been merged by the agent. Parent issue acceptance and milestone closure
remain maintainer decisions; green checks do not establish the missing isolation.
M0/M1 hardware inventory/repeated cold boots remain explicit owner gates.

Owned TSS with guarded exception stacks (#24) now supplies controlled stop-only
fault-stack evidence. Next dependencies are reviewed single-core interrupt/timer/
context contracts and ring-3 address spaces.
Keeping M2 open does not require delaying those dependencies. SMP, storage, network,
speech and AI remain outside this release path. The checkpoint still halts; keyboard
input and a functional WOPR-inspired command shell remain required future work.
