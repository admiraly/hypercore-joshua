# Reference-machine evidence collection

This host-only workflow supports issue #42. It collects an unverified Windows
inventory and prepares a manual trial record; it does not qualify the machine.
Run from the existing Windows installation, with the exact test ISO available:

```powershell
./tools/Collect-ReferenceEvidence.ps1 -ImagePath C:/Tests/JOSHUA-Console-Test.iso -ReferenceId LAT7430-01
```

No elevation, network upload, firmware change, device configuration or boot-media
write is performed. Default output is a unique directory under ignored
`out/reference-evidence`. Keep the files private until reviewed for publication.
The reference ID is a chosen label, never a service tag or serial number.

`inventory.json` projects only vendor/model, CPU brand/core counts, RAM, BIOS
version, disk model/firmware and normalized PCI vendor/device/subsystem/revision/
class codes. Serial numbers, UUIDs, machine/user/domain names, MAC/IP addresses,
raw PNP instance paths and arbitrary notes are excluded. Descriptive OEM fields
are still provider-supplied text: inspect them before sharing. Missing fields stay
null or appear in `missingEvidence`; no hardware is inferred from a product name.
Windows-enumerated IDs do not prove physical presence, JOSHUA discovery or driver
support. RAID/VMD or unavailable providers may hide the NVMe controller; disk to
controller association always remains Unknown.

The source properties are documented by Microsoft for
[Win32_PnPEntity](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-pnpentity)
and [Win32_DiskDrive](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-diskdrive).
Only the explicit allowlist is serialized, rather than the complete CIM objects.

`trial-template.json` binds the inventory bytes and ISO bytes using SHA-256 and
records the ISO size. All outcomes start NotRun/Unknown and qualification starts
NotEstablished. Hashes identify bytes; they do not attest the build or a boot.
Existing output directories are never overwritten. Output must stay below the
repository’s ignored `out/reference-evidence` directory. Network drives, UNC paths
and existing reparse/symlink components in image/output paths are rejected. This
assumes a trusted host and stable filesystem paths during the operation; it is
not protection against a malicious concurrent host changing junctions.

For each manual trial, copy the template to a separately named local trial file.
Keep its inventory/image binding unchanged. Record trial number, ColdBoot or
WarmReboot, boot method and tool version, observed Secure Boot state, display
mode, boot/nativeInput/safeHalt outcomes, last stage and a redacted photo reference.
Record failures as well as successes. Use at least three cold boots as described
in testing.md, exercising actual input and `halt`; retain each trial separately.
The collector does not perform these operations or infer success. Do not change
firmware settings, enroll keys or write an internal disk through this workflow.
Photos must omit service tags, serials, accounts and other private information.
Earlier owner-reported checkpoint boots remain distinct from these console trials.

## Tool validation

```powershell
./tests/Test-ReferenceEvidence.ps1
# Windows only: also exercise the read-only local CIM provider with a synthetic ISO
./tests/Test-ReferenceEvidence.ps1 -CollectHost
```

Synthetic fixtures test projection, redaction, malformed/conflicting PCI IDs,
missing fields, deterministic serialization, hash binding and overwrite rejection.
The provider smoke is not a boot test; a CI runner is not the reference Latitude.
Generated fixture/provider records are removed and never uploaded by this workflow.
Windows execution, actual reference inventory and physical qualification remain
separate evidence gates.
