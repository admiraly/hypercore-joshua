param([Parameter(Mandatory)][string]$ImagePath,[ValidatePattern('^[A-Z0-9-]{1,32}$')][string]$ReferenceId='UNASSIGNED',[string]$OutputRoot)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if ($env:OS -ne 'Windows_NT') { throw 'Reference collection requires Windows CIM; use synthetic fixtures on other hosts.' }
$root=Split-Path $PSScriptRoot -Parent
if (-not $OutputRoot) { $OutputRoot=Join-Path $root 'out/reference-evidence' }
Import-Module (Join-Path $PSScriptRoot 'ReferenceEvidence.psm1') -Force
try {
    $snapshot=@{
        System=Get-CimInstance -ClassName Win32_ComputerSystem -Property Manufacturer,Model,TotalPhysicalMemory
        Bios=Get-CimInstance -ClassName Win32_BIOS -Property SMBIOSBIOSVersion
        Processors=@(Get-CimInstance -ClassName Win32_Processor -Property Name,NumberOfCores,NumberOfLogicalProcessors)
        Disks=@(Get-CimInstance -ClassName Win32_DiskDrive -Property Model,FirmwareRevision)
        Pnp=@(Get-CimInstance -ClassName Win32_PnPEntity -Property HardwareID,CompatibleID)
    }
} catch { throw 'Reference CIM collection failed; no qualification result was created.' }
$now=[datetime]::UtcNow
$inventory=ConvertTo-ReferenceInventory -Snapshot $snapshot -CapturedUtc $now -ReferenceId $ReferenceId
$directory=Join-Path $OutputRoot ($now.ToString('yyyyMMddTHHmmssZ')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$written=Write-ReferenceEvidence -Inventory $inventory -ImagePath $ImagePath -OutputDirectory $directory
Write-Host ('Collected local unverified inventory and NotRun trial template: '+$written)
