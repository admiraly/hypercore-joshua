param([string]$QemuPath, [string]$FirmwareDirectory)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
foreach ($fixture in @('Ud2', 'Gp', 'ReadOnly', 'Nx', 'Guard', 'ConstWrite', 'Unmapped')) {
    $output = Join-Path $root ('out/fault-' + $fixture.ToLowerInvariant())
    & (Join-Path $root 'tools/Build.ps1') -FaultTest $fixture -OutputDirectory $output
    & (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $output -ExpectedFault $fixture
}
Write-Host 'PASS: Ud2/Gp frame normalization and owned-paging write/NX/guard/unmapped faults report exact vector/error/RIP/CR2 and halt.'
