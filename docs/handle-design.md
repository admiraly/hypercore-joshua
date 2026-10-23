# Bounded process-owned handle authority

Issue #44. This is an internal foundation contract, not a frozen public ABI or
CPL3 delivery claim. The specification requires kernel-checked handles; current
INT80 implements only exit and unknown-call rejection. The first implementation
slice is pure Assembly policy with host fixtures, before privileged integration.
No storage, network, device, pointer transfer or dynamic object lifetime exists.

## Authority and lifetime

A trusted kernel caller owns one table per process incarnation, assigns a nonzero
owner identity and a unique nonzero 16-bit table tag. Tags must never be reused
within a boot; exhaustion rejects process creation. This slice does not implement
the global tag allocator or process creation. A gate adapter must obtain the table
and expected owner from validated active-process metadata, never from user input.
Only trusted kernel code grants object identities. They are nonzero opaque integer
IDs for immutable diagnostic objects, never addresses or user-provided authority.
Lookup results remain kernel-internal; a future syscall may return only specified
nonpointer diagnostic values. No object destruction/refcount semantics are implied.

Tokens are 64 bits: tag in bits63:48, generation in bits47:8 (40 bits), and
one-based slot in bits7:0. Slots1..32 are valid; zero and other values reject.
Generations1..0xffffffffff are valid. Close increments the generation and clears
object/rights; closing at the maximum permanently retires the slot. It never wraps.
Different tables must have distinct tags, so a token from another table rejects.
Tokens are not secrets or cryptographic authentication: authority is the validated
active process's kernel table and its rights, not unpredictability of the number.

Rights are READ=1 and DUP=2; supported nonzero masks1..3. Lookup requires a
nonzero supported mask that is a subset of stored rights. Duplicate requires DUP
on the source and a nonzero requested subset of its rights; it cannot amplify
rights. Close and revoke do not require a rights bit. Duplicate creates another
independent slot for the same immutable identity; closing one does not revoke
siblings. Process revocation closes every live slot, invalidating all of that
process's tokens. There is no cross-process transfer or object-wide revocation.

## Exact storage

All fields are unsigned qwords, eight-byte aligned. `HANDLE_TABLE` is1056 bytes:

| Offset | Field | Valid value |
| --- | --- | --- |
|0|ready|`HANDLE_MAGIC=0x314c444e4848534a`|
|8|owner|nonzero, exact expected-owner argument|
|16|tag|1..65535|
|24|reserved|zero|
|32|entries|32 consecutive32-byte entries|

Each `HANDLE_ENTRY`: state at0, generation at8, object at16, rights at24.
State0 is free: object/rights zero, generation1..MAX. State1 is live: nonzero
object, rights1..3, generation1..MAX. State2 is retired: generationMAX,
object/rights zero. No other states/unused bits are valid. All32 entries must be
validated before lookup or any mutation; invalid unrelated entries reject too.
There is no cached count or user-controlled pointer in the records.

The caller proves the complete table readable/writable, with stable exclusive
backing and no untrusted alias, at a nonzero aligned address with representable exclusive end `base+1056`. Helpers
check arithmetic/alignment before dereferencing, but cannot prove mapping or
ownership themselves. Initialization additionally requires all1056 bytes zero;
reinitialization of any initialized table rejects without changing it. Init sets
all generations1 and publishes ready last. Reusing wiped storage or a prior tag
without a new process-lifetime proof is forbidden even if zero-check would pass.

## Internal helper ABI and failure order

Microsoft integer x64 convention, all eight integer nonvolatiles preserved, aligned
calls and32-byte shadow space, no SIMD/red-zone use. BSP-only IF0 serialization,
DF0 and trusted readable metadata are caller preconditions. Helpers do not change
IF/DF or access hardware. No callback, allocation or external side effects.

| Helper | RCX | RDX | R8 | R9 | Success output |
| --- | --- | --- | --- | --- | --- |
|handle_init|table|owner|tag|zero|EDX0,R8=0|
|handle_check|table|expected owner|zero|zero|EDX0,R8=0|
|handle_grant|table|expected owner|object ID|rights|RDX token,R8=0|
|handle_lookup|table|expected owner|token|required rights|RDX object ID,R8 actual rights|
|handle_duplicate|table|expected owner|token|requested rights|RDX new token,R8=0|
|handle_close|table|expected owner|token|zero|EDX0,R8=0|
|handle_revoke|table|expected owner|zero|zero|EDX0,R8=0|

EAX status:0 success,1 invalid storage/state/request,2 invalid/stale/foreign token,
3 denied rights,4 no free slots. RDX andR8 are zero on every returning failure.
All other volatile registers are unspecified. Success/no-op returns are exact;
revoke with no live entries succeeds unchanged. Caller-reserved zero arguments
must be checked, including check/close/revoke/init reservedR9 and check/revokeR8.

For init, validate the address, nonzero owner, tag1..65535 and reservedR9=0; then require
all-zero storage before mutation. Other operations validate table address, expected
owner, header and every entry first; then validate operation arguments (including
nonzero object, supported nonzero rights or required zeros); then token encoding,
tag, state and exact generation; then required rights; then capacity. Invalid
argument status takes precedence over token rejection, rights over capacity.
Grant/duplicate choose the lowest free slot. Retired slots are unavailable.

Every returning failure preserves all1056 table bytes. Grant/duplicate preflight
all conditions before publishing a complete live slot; state is published last.
Close/revoke clear object/rights and advance generation or retire. IF0/exclusive
serialization makes intermediate stores unobservable to other users. Revoke does
not fail after beginning mutation. It leaves ready/owner/tag unchanged, preserving
retired and already-free entries exactly. No counter wraps or saturates silently.

## Evidence gates

Host fixtures must call actual Assembly through an eight-register sentinel wrapper
on success and rejection. An independent model compares statuses, outputs and all
table bytes after every operation, with deterministic randomized mixed histories,
full32-slot exhaustion, rights attenuation, foreign owners/tags, stale copies,
maximum-generation retirement, revoke/no-op, malformed whole-state and failure
precedence. Place extents against inaccessible lower/upper guard boundaries and
check invalid pointers are rejected before reads where address checks permit it.

Pure helper clearance is not launch evidence. Before privileged integration:
review global nonreuse tag assignment, trusted owner/table provenance, exact gate
frame validation, internal-result filtering, process exit/fault/timeout revocation
and stack depth. Then require actual CPL3 positive/negative queries, stale/foreign/
rights rejection and independent B continuation plus Build.ps1, ISO/FAT QEMU and
reproducibility evidence. Preserve current private roots, cache policy, fault
containment, watchdog/drain and speech boundaries. Windows, physical, malicious
firmware/host, SMP, general capabilities and milestone exits remain unqualified.

Design review round1 found contradictory init reserved-argument wording. The
response explicitly requires nonzero owner, tag1..65535 and R9=0 before the
all-zero storage check. No other design blockers were reported; review was
source-only and establishes no implementation or target evidence.

Code review round1 found that inclusive-end validation allowed the exclusive init
loop boundary to wrap at0xfffffffffffffbe0. The response rejects exclusive-end
overflow before any table read and adds that exact rejection fixture. Maximum
helper depth is112 bytes below entry RSP,120 including its incoming return address;
the host wrapper reaches288 below wrapper entry. Review remains source-only.
