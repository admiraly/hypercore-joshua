# Console preview bundle contract

Issue #49. Host-only packaging of an already-built normal Console profile. No
runtime, image contents, speech, tool download, firmware/device/disk operation,
network/upload or boot execution is added. Packaging is not boot qualification.
The script is tools/New-ConsoleTestBundle.ps1; tests live in
 tests/Test-ConsoleTestBundle.ps1. Both Windows PowerShell5.1 and Linux PowerShell
are intended host paths; actual results are recorded separately.

## Inputs and publication

`-InputDirectory` defaults to `out/console`. `-BundleName` is an optional chosen
ASCII label matching `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`; without it, use a unique
UTC/GUID directory label, which is never embedded in bundle contents. Output is
always `out/console-test-bundles/<label>`. Existing outputs reject without edits.
Both input and output must remain strictly below repository-local ignored out.
Reject UNC/network drives and existing symlink/junction/reparse components, checking
parents before descendants. This assumes a trusted host and stable paths during
operations, not protection against malicious concurrent redirect replacement.

Before creating owned staging, validate the source manifest and artifacts:

- Root has exactly stage, console, consoleTest, faultTest, timerTest, contextTest,
  userTest, elfTest, keyboardTest, assembler, assemblerSha256, inputs, artifacts.
  console must be Boolean true and all seven fixture fields exactly `None`.
  Stage must match Build.ps1's current unsigned/non-installing prototype string;
  assembler must report pinned NASM2.16.03 with bounded ASCII text and a hex hash.
- Accept Build-shaped JSON, not arbitrary extensions. Parse and reserialize, then
  compare after removing only JSON whitespace outside strings. This tolerates host
  indentation/newlines while rejecting duplicate object keys collapsed by parsing.
  Reject unknown keys, nested leaf objects, invalid scalar types and excessive size.
- Input entries have exactly path/sha256. The exact case-sensitive path set is the
  literal Assembly/SDK include closure of boot/uefi.asm across conditional profiles
  plus tools/Build.ps1 and tools/New-BootImage.ps1. This is appropriate for Console,
  which includes every conditional console source. Require unique paths and exact
  current source hashes, without traversal/absolute paths or unregistered reads.
  Hash binding assumes trusted build/source provenance, not malicious-build attestation.
- Artifacts have exactly path/bytes/sha256 and exactly the five current names:
  BOOTX64.EFI, joshua-uefi.img, joshua-m0.iso, JOSHUA-Console-Test.iso, boot.lst.
  Reject duplicates, missing/unknown names, noninteger/zero sizes and bad hashes.
  Hash/size every regular local file. Both ISO names must have equal size/hash.
- Invoke the existing Test-Pe.ps1 and Test-BootImage.ps1 validators on that input.
  No image is mounted or booted; static format checks do not establish runtime success.

Only after preflight, create an unpredictable owned staging directory below the
bundle root. Copy the ISO and original manifest bytes with new hashes checked
against the preflight values. On any error remove only that staging directory;
never remove existing user output or alter input artifacts. Publish by moving the
complete staging directory to the unused final directory. No partial success marker.

## Exact payload and archive

The ZIP contains only these seven files, in this fixed order:

1. JOSHUA-Console-Test.iso — exact input named ISO.
2. build-manifest.json — exact original manifest bytes, not a rewritten attestation.
3. LICENSE.txt — exact project MIT license bytes.
4. TEST-INSTRUCTIONS.txt — static UTF-8/no-BOM instructions from the packager.
5. TRIAL-TEMPLATE.json — deterministic UTF-8/no-BOM manual record.
6. bundle.json — deterministic version1 metadata.
7. SHA256SUMS.txt — hashes of the preceding six exact files, fixed order/LF.

The trial template binds ISO SHA256/size and build-manifest SHA256, with trial
number null, reference label UNASSIGNED, boot method/type/display/Secure Boot
Unknown, boot/input/halt NotRun, photo NotProvided and qualification NotEstablished.
It collects no host inventory. A human may copy/fill it for each actual trial;
blank records never count as boot evidence.

bundle.json states schemaVersion1, profile Console, fixture None, qualification
NotEstablished and a files array for the first five payloads (path/bytes/sha256).
It has no host paths, current time, assumed source revision or success claim.
Manifest source/artifact hashes provide the existing build-byte binding. The full
build manifest also describes validated EFI/FAT/listing files not shipped in this
lean ISO bundle; instructions must distinguish those from bundled payload files.

ZIP entries use fixed1980 timestamps, zero external attributes, no compression
and the exact allowlist. Reopen the finished ZIP, reject extra/duplicate names,
and verify each entry's extracted bytes/size/hash against the staged files before
publication. Keep the ZIP and its SHA256 sidecar alongside the seven unpacked
files in the final directory; neither is recursively included in the ZIP.
Payload bytes must repeat for identical inputs on a given host; local repeated
ZIP identity is tested. No cross-.NET/OS ZIP-byte reproducibility claim is required.
No downloaded tools, firmware, models, Piper assets or private logs enter the ZIP.

## User instructions and validation

Instructions must explain the unsigned non-installing development status, verify
SHA256SUMS, and show a QEMU q35/TCG/qemu64/two-vCPU/512-MiB/no-NIC UEFI session with
only this read-only CD-ROM and a regular disposable OVMF-vars copy. QEMU/OVMF are
external prerequisites, not bundled binaries. Never attach an internal/physical
disk, modify host firmware, enroll keys or imply installation/physical compatibility.
After the firmware countdown, type help, status, mem/ps/devices --json, exercise
editing/history, then halt. Record failures as well as success and preserve exact
image hashes, observed modes/tool versions and redacted photos for reference trials.
The bundle performs none of those trials and uploads nothing. Piper stays separate.

Tests start from a real freshly built Console artifact set. Exercise successful
export, exact files/ZIP/extracted bytes/license/hash/trial defaults, same-host repeat
identity, every profile mismatch, missing/tampered artifacts/source hashes,
duplicate/unknown/traversal metadata, nested leaves, existing output and local-path
redirect rejection. Returning failures leave no new final bundle and preserve
source artifacts/earlier bundles. Static validators must reject malformed media
even when a fixture updates its declared hash. Windows and Linux test results,
existing runtime evidence and physical/manual gates must remain distinct.

## Create and test a bundle

```powershell
./tools/Build.ps1 -Console
./tools/New-ConsoleTestBundle.ps1 -InputDirectory out/console -BundleName my-console-test
./tests/Test-ConsoleTestBundle.ps1
```

The final directory is `out/console-test-bundles/my-console-test`. Share its
`JOSHUA-Console-Test.zip` and `.zip.sha256` sidecar. Extract the ZIP, verify
`SHA256SUMS.txt`, then follow `TEST-INSTRUCTIONS.txt`; preserve the unfilled
`TRIAL-TEMPLATE.json` until an actual trial is observed. This normal Console
bundle contains no Piper announcement; the separate Piper preview remains separate.

Local Linux PowerShell 7.4.6 validation passed 138 checks, including a fresh
Console build and a final existing-input rerun. Windows PowerShell 5.1 execution
is wired in CI but remains unverified here. Linux redirect tests exercise
symlinks; Windows junction tests require Windows execution, and Windows leaf-file
symlink coverage remains explicitly unclaimed. Source-only independent review
resolved the array-valued stage schema finding; it did not reproduce these tests.
