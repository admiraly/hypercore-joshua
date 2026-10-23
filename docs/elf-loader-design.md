# Bounded embedded ELF64 loader design (issue #36)

Status: proposed initial static profile, not implemented or a public application ABI.
This is the next read-only program-loading step after the private-context foundation.
The first profile deliberately matches the already proved six-page backing layout;
expanding segment/page counts requires a later explicit mapping/ownership revision.
It is actual ELF parsing and loading, not recognition of a fixed binary hash.

## Image profile v1

The immutable source is a caller-owned readable buffer, 64 through12,288 bytes.
ELF identification must be ELF64, little endian, current version, System V ABI0,
ABI version0 and zero identification padding. Require ET_EXEC, EM_X86_64, version1,
flags0, ELF header size64, program header offset64, entry size56 and count2. There
is no section table: shoff/shentsize/shnum/shstrndx are zero. Header plus both
program headers (176 bytes) must fit before any program-header read.

Exactly two PT_LOAD entries, sorted by increasing virtual address, define the profile. One has PF_R|PF_X
and virtual address USER_BASE; the other PF_R|PF_W and USER_BASE+4096. p_paddr must
be0. Both p_align and p_memsz are4096; code filesz is1..4096 and data filesz0..4096.
File offsets are page aligned, at least4096, and offset+filesz must fit source with
nonwrapping arithmetic, including the zero-length data range. Nonempty file ranges
must not overlap; each permission class appears once. Entry is inside file-backed
code, not zero-fill. These constraints imply canonical/nonoverlapping user ranges,
W^X, file/virtual congruence, bounded BSS, and no implicit interpreter/dynamic/TLS/
relocation/stack-authority program headers. Unsupported images fail explicitly.

The four existing stack pages at USER_BASE+3000h..6FFFh and guard at+2000h are not
ELF segments; the trusted launch ABI supplies them. No dynamic linking, relocation,
filesystem access, writable backing store, arbitrary binary compatibility, or new
syscall/capability authority is introduced. Programs remain authored in Assembly.

## Parser and validated plan

`elf_validate(RCX source,RDX length,R8 plan)` returns EAX0 success or1 rejection.
Source readability and output storage ownership are trusted caller preconditions;
the parser proves every offset/length before dereference. Source pointer+length and
output pointer+64 must not wrap, and ranges must be disjoint before any output store.
On malformed headers, invalidate all eight plan qwords; no allocation/copy/publication
occurs. Invalid pointer arithmetic or overlapping source/output rejects without any
store, since writable output authority cannot safely be inferred in that case.

The64-byte ELF_PLAN holds source, length, code offset/size, data offset/size, entry
offset and ready magic `JOSHELF1`. Build the plan only after complete validation;
ready is the final store. Reject duplicate/missing load classes and unsupported
fields rather than using the last record. Parser preserves the eight integer
nonvolatile registers and uses the project's Microsoft x64/shadow-space convention.

`elf_copy(RCX plan,RDX destination,R8 destination_bytes)` accepts exactly six pages
of trusted privately owned backing (its prior bytes may be nonzero), checks ready and revalidates source bytes against
a temporary plan before the first destination store, and checks all plan fields
against that fresh plan. Destination length is exactly24KiB, page aligned, nonwrapping
and disjoint from source and plan. The caller proves exact ownership and physical
identity access before calling. The helper cannot independently establish a virtual
pointer's hardware ownership. Rejected inputs make no destination writes.

On success, copy code and data into pages0 and1, explicitly zero the remaining
code/data bytes and four stack pages. No stack/guard virtual mapping is changed.
This preserves existing physical alias sealing and inherited PAT policy. The source
must remain immutable between validation and copy; IF0 and trusted image storage
supply this precondition in boot integration. Failure quarantines partial allocations
through the existing storage contract; there is no speculative allocation rollback.

## Integration

Build an ELF envelope around the existing Assembly payload and initialized data,
with labels for actual payload byte bounds. Source provenance stays in NASM; no
runtime third-party loader/linker is shipped. Reserve a separate image plan and
use a boot-only adapter around the existing raw-backing preparation helper. The
adapter owns the separate plan, validates before allocation, supplies the validated
code source/size to existing preparation, then invokes elf_copy on each exact owned
six-page record and restores the per-process marker. The initial raw code copy is
redundant but bounded; it grants no readiness. USER_CONTEXT's18-qword layout remains
unchanged. Copy failure sets failed/ready0 and halts before roots/readiness, retaining
quarantined owned backing; stage1 alone is not permission to run. A fresh-plan and
ready check precedes launch, whose RIP uses the plan's validated entry offset.
No process record/layout, stack addresses, public marker offsets or entry register
scrubbing changes. Launch RIP is USER_BASE+validated entry offset. Existing code
boundary/proof logic still seals exactly page0 and verifies every private leaf.

The embedded program checks initialized data outside its reserved128-byte bootstrap/scratch
header and verifies a zero BSS region before changing private state. Its entry may
have a nonzero code offset; target evidence must exercise that path. A malformed
embedded-image fixture must stop before private allocation/root readiness or CPL3.
The independent QMP oracle derives expected code/data from actual ELF headers, not
the loader plan, and checks file bytes, zero padding/BSS, permissions and ownership.
Existing exact-RIP fixture labels remain relative to the actual payload code page.

## Evidence gates

Before runtime implementation: independent design review with original brief and
all prior findings. Host fixtures execute actual Assembly validation/copy with an
independent ELF oracle, reversed-header rejection, boundary lengths, malformed fields,
truncations, overflow, duplicate classes, overlap, invalid entry, source/output/
destination aliases, stale/mutated plans and eight integer ABI sentinels. Verify
rejection before destination writes and plan invalidation policy.

Target: real ELF entry at a nonzero offset, initialized data/BSS checks, A fault/B
continuation, rejected-image preallocation stop, independent physical snapshots.
Run Build/reproducibility, normal ISO/FAT/x2, current user campaign, host/fault/APIC/
context regressions. Code/evidence independent review, linked stacked PR, no merge.
QEMU/KVM and physical evidence remain distinct. No complete M2/M4, general process
loader, speculative protection or hardware qualification claim follows.


ELF field and PT_LOAD ordering/zero-fill semantics follow the primary
[ELF header](https://gabi.xinuos.com/elf/02-eheader.html) and
[program loading](https://gabi.xinuos.com/elf/07-pheader.html) definitions. The extra
fixed-page/flag/section restrictions above are JOSHUA profile policy, not claims
that all otherwise-valid ELF files have those restrictions.


## Design review response

P2 bootstrap reservation: the reserved data range is now[0,128), covering kernel
marker/progress/probe inputs at8/16/24/32/40 and payload far-pointer scratch at56/64.
Initialized ELF data lives at128 or above, and the initial BSS probe at256 or above.
The independent oracle compares loader bytes while explicitly accounting for the
specified terminal bootstrap/scratch mutations; it must not demand untouched
loader output in those reserved locations. No other data/BSS overwrite is allowed.
The kernel writes bootstrap inputs only after copy, before entry, as at the prior
stage. Later program writes remain private and require fixture-specific expectations.

## Parser slice evidence

The initial parser/copy component passes6,198 host checks against an independent
profile oracle, including guarded truncated buffers, boundary/field mutations,
zero-fill, source/plan/destination aliases and eight integer ABI sentinels per call.
Independent source review found no runtime defect. P2 alias-test evidence was fixed
by page-aligned mmap fixtures with full/partial source and plan overlap, whole-arena
unchanged checks and baseline validation. Follow-up review found no remaining issue.
This component is not yet included in the boot image; no ELF entry, initialized-data
or target rejection evidence is inferred. Boot integration and full regressions are
still mandatory before issue #36 is complete.


Adapter-review response: elf_copy does not require pre-zeroed backing. It clears
all six pages after complete preflight; old raw code/marker bytes are therefore
safe inputs and need no additional clearing pass. Exact ownership remains mandatory.
