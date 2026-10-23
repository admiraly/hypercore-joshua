param([string]$NasmPath,[string]$QemuPath,[string]$FirmwareDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
foreach($mode in @('BadMagic','BadEntry')) {
    $output=Join-Path $root ('out/elf-'+$mode.ToLowerInvariant())
    & (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -OutputDirectory $output -ElfTest $mode
    & (Join-Path $root 'tools/Test-Qemu.ps1') -OutputDirectory $output -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -ExpectedFault ElfReject
}
Write-Host 'PASS: malformed ELF magic and non-file-backed entry rejected before private allocation.'
