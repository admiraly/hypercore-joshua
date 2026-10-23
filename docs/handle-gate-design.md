# Experimental INT80 handle adapter

Issue #46, dependent on issue #44 / PR #45. This refines the existing bounded
single-use two-process experiment, not a public ABI freeze or general capability
system. All target instructions remain Assembly. No storage/device/network/speech
work, dynamic process creation, user pointers or object lifetime/refcounts enter.
The pure1056-byte table contract remains in handle-design.md.

## Trusted preparation and authority

Both process records are already privately backed, root-proved, single-use and
bound to frame owners FRAME_OWNER_USER_BASE+1/2. After machine/descriptor checks
and before launching either process, an owned boot adapter initializes both fresh
all-zero tables exactly once, using those exact expected owners and fixed tags1/2.
There is no later process incarnation or table reset in this bounded boot. A
second preparation attempt fails on nonzero readiness/initialized storage. These
fixed distinct tags are never reused; this is not a general global tag allocator.
Any future process creation must supply a separately reviewed nonreuse allocator.

The adapter grants one immutable diagnostic object ID1 with READ|DUP to each
table, stores the returned token at that process's private data offset48, then
validates both tables and publishes
`USER_HANDLES_READY=0x3152444c4848534a` last. No user program
has run and private data is owned RW/NX. Object1 is a compile-time diagnostic
identity; a successful query returns `USER_HANDLE_VALUE=0x314741494448534a`.
Neither the object ID nor query value is an address or a resource-operation right.
No user-selected object/owner/table may be granted. Failure permanently stops;
partial preparation is never launched, retried or rolled back into reuse.

Two tables (2112 bytes), two72-byte counter records and one8-byte ready field
occupy2264 bytes of the2544-byte existing context_metadata padding immediately
after keyboard_fixture. Keep the4096-byte metadata extent and all following
stack guards/console/PE section offsets unchanged. Static checks must prove this
fit and eight-byte alignment. The new fields are supervisor RW/NX image BSS,
covered by existing reservation, owned mapping and whole-image permission proof.
The immutable text/rodata additions must fit existing section bounds or fail build.

## Experimental gate ABI

The gate still performs irq_save, normalization and exact user_frame_check before
reading syscall fields. Invalid origin/return handling remains unchanged. Exit0
still consumes saved RDI status and never returns; unknown IDs still return
RAX=-38 and preserve every other saved integer register. Known new IDs are:

| Saved RAX | Operation | Saved RDI | Saved RSI | Return RAX | Return RDX |
| --- | --- | --- | --- | --- | --- |
|1|query|token|must be0|0 or error|diagnostic value on success, otherwise0|
|2|duplicate|token|requested rights1..3|0 or error|new token on success, otherwise0|
|3|close|token|must be0|0 or error|0|

All other saved integer registers, RSP and permitted user RFLAGS are restored
unchanged, including DF; only RAX/RDX change for these three known calls. No
syscall argument is dereferenced as a pointer. Existing SYSCALL/SYSENTER policy
and exact user selectors/root/frame checks remain in force.

The adapter checks its ready marker; derives index from bounded active identity;
requires the exact returned USER_PROCESS pointer, running state/phase, root and
expected frame owner; derives table/counter addresses from the fixed owned arrays.
Require the table tag to equal that derived one-based index and every live
entry object to equal1; a generic nonzero object/tag is insufficient for this
fixed adapter. No table pointer, owner, tag or object identity comes from user registers. Call
handle_check with the derived owner first; malformed privileged metadata stops
rather than appearing as a normal user error. Stable IF0 ownership prevents a
change between this check and the pure operation. Validate query/close reserved
RSI before token resolution; duplicate delegates supported rights and attenuation
to the pure helper. Query uses required READ and accepts only returned object1.
An unexpected resolved object permanently stops rather than exposes its value.

Pure statuses map: invalid request1 to-22, bad/stale/foreign token2 to-9,
denied3 to-13, full4 to-24. Query/close reserved-argument error precedes bad token.
Unknown pure statuses stop. Failure return RDX is always0. Errors are negative
signed64 values; token values remain full unsigned64 in RDX, avoiding status/token
sign ambiguity. Each returning known call increments exactly one calls counter
and one corresponding result counter. All counter increments saturate at U64_MAX;
finite target fixtures must begin at zero and assert exact counts.

## Termination and stack discipline

Every existing normal exit, discarded user fault, invalid returning frame and
timeout reaches user_terminate. It first records terminal state/status and switches
to the trusted kernel root/entry continuation as before. Before publishing any
bootstrap continuation or opening the retirement interrupt window, derive and
check the exact active table/owner again, call handle_revoke, then handle_check,
and prove no live entry remains. Record exactly one revocation for this single-use
process. Any unexpected metadata, helper failure or repeated revocation stops.
Retired/max-generation slots stay retired; free generations are not reset.

Revocation must occur under IF0 before the fixed drain loop, with no user state
resumed. Existing timer stop/mask, one-EOI ownership and pending40 drain semantics
remain unchanged. In particular a timer-origin terminal path still stops/masks
and EOIs its actual delivery before user_terminate; a fault/exit path does not
invent an EOI. All process and bootstrap states retain existing publication order.

Gate helper calls use the192-byte saved-frame base and existing32-byte shadow
area. The adapter must preserve RBX active record and R12 frame base plus the
integer nonvolatiles, aligned calls and shadow space. Audit nested depth including
adapter saves, pure helper120-byte caller depth and termination calls against the
4-KiB reserve. The fixed retirement RSP and exact drain frame relation must not
change. No allocator, firmware, logging or user-memory access occurs in a returning
gate. Boot preparation may log only through the existing bounded diagnostics.

## Payload and independent target evidence

After existing initial-register/unknown-call/DF checks and before incrementing
progress or selecting the existing fault fixture, both authored programs execute
this sequence, checking every status/result and preserved registers/DF/RSP:

1. Query initial token: value succeeds.
2. Duplicate initial token with READ into slot2; store token at private offset72.
3. Query slot2: value succeeds.
4. Duplicate READ-only slot2: denied, zero result.
5. Duplicate initial token with READ|DUP into slot3; store token at offset80.
6. Close initial token: succeeds.
7. Query initial stale token: bad handle.
8. Close initial stale token again: bad handle.
9. Close stale token with reserved RSI=1: invalid request.
10. Query a token with the other fixed table tag: foreign handle.
11. Close slot2: succeeds.
12. Query closed slot2: bad handle.
13. Query live slot3: value succeeds; leave it live for termination revocation.

Only then write `USER_HANDLE_PROGRESS=0x314b4f4c444e484a` at private offset88,
restore public RDI=dataVA, and continue the original marker/progress and fixture
path. All these fields stay inside the existing reserved bootstrap bytes[0,128).
Far-pointer scratch56/64, initialized ELF bytes>=128 and BSS>=256 remain distinct.
The source-derived paging/data oracle must explicitly account for each authored
write; do not weaken full-byte comparisons or substitute kernel markers for proof.

Each72-byte counter record has qwords: calls, query, duplicate, close, badToken,
denied, invalid, full, revoked. Expected terminal values per process are
`[13,3,2,2,4,1,1,0,1]`. Before launch all are zero; live slots after step13 are
only slot3. After termination every slot is free: first three generations2,
remaining29 generations1, object/rights zero, header owner/tag/magic unchanged.
The initial token at48 and retained slot2/3 token values at72/80 have generation1
and the process's own tag. The foreign probe changes only tag1<->2, never grants
cross-table authority. Progress at88 and independent process-B success are required.

QMP capture must independently derive table/counter/ready addresses from the
listing and image binding, capture physical bytes after the final IF0 halt and
compare both entire tables/counters/data against these authored expectations.
A new typed `UserTest HandleFull` profile (mode26) makes processA duplicate
its still-live slot3 token31 times after the normal13-call sequence, filling all
32 slots. The next duplicate must return-24/RDX0 before completion is published.
ProcessB runs the ordinary sequence. A's terminal counters must be
`[45,3,33,2,4,1,1,1,1]`; B retains the ordinary values. After A revocation,
first two generations are3 (their freed slots were reused), remaining30 are2;
all states/object/rights are zero. The31 successful duplicate results must be
checked individually against lowest-free-slot/generation expectations, not merely
counted. Loop scratch at private offset96 must finish zero and is included in the
full-prefix oracle. This fixture exits normally and still requires independent B
continuation. Ordinary profiles require offset96 zero too.

Add passing-baseline mutations for owner/tag/ready, live residue, generation,
rights/object, counters, tokens and missing payload completion. Source-only helper
or payload checks do not substitute for actual CPL3 execution and physical capture.

Run the pure suite, static/build checks, normal ISO/FAT and x2 profiles, existing
user fault/watchdog/authority/pending-drain campaign, fault regressions, Console
smoke and reproducibility. Preserve known TCG SS/AC and KVM/media limitations;
report every failed invocation and unrun gate separately. Windows, physical,
malicious-host/firmware, SMP, dynamic process/resource capabilities and milestone
completion remain unqualified. Independent design and final implementation review
receive original brief, prior findings/responses and remaining objections.

Design review round1 found no result counter for reachable full-table returns.
The response adds a ninth `full` qword (72 bytes per record,2264 total metadata
bytes) and explicit ordinary/full-profile counter and revocation expectations.
The real CPL3 exhaustion fixture is mandatory; host policy evidence is separate.
