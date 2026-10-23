param([string]$OutputDirectory, [string]$PiperPcmPath, [string]$NasmPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'out' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if ($PiperPcmPath) { $PiperPcmPath = (Resolve-Path -LiteralPath $PiperPcmPath).Path }
if (-not $NasmPath) {
    $localNasm = Join-Path $root '.tools/nasm-2.16.03/nasm.exe'
    if (Test-Path -LiteralPath $localNasm) { $NasmPath = $localNasm }
    else { $NasmPath = (Get-Command nasm -ErrorAction Stop).Source }
}
$NasmPath = (Get-Command $NasmPath -ErrorAction Stop).Source
$artifacts = @('BOOTX64.EFI', 'joshua-uefi.img', 'joshua-m0.iso', 'manifest.json')
& (Join-Path $root 'tools/Build.ps1') -OutputDirectory $OutputDirectory -PiperPcmPath $PiperPcmPath -NasmPath $NasmPath
$hashes = @{}
foreach ($artifact in $artifacts) {
    $hashes[$artifact] = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory $artifact) -Algorithm SHA256).Hash
}
Push-Location (Split-Path $root -Parent)
try {
    $relativeNasm = Resolve-Path -LiteralPath $NasmPath -Relative
    & (Join-Path $root 'tools/Build.ps1') -NasmPath $relativeNasm -OutputDirectory $OutputDirectory -PiperPcmPath $PiperPcmPath
}
finally { Pop-Location }
foreach ($artifact in $artifacts) {
    $actual = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory $artifact) -Algorithm SHA256).Hash
    if ($actual -ne $hashes[$artifact]) { throw ('Reproducibility failed: ' + $artifact) }
}
Write-Host 'PASS: Two independent builds produce identical EFI, FAT, ISO and manifest hashes, including relative NASM invocation from outside the repo.'

$fixture = Join-Path $OutputDirectory 'invalid-pe.fixture'
$bytes = [IO.File]::ReadAllBytes((Join-Path $OutputDirectory 'BOOTX64.EFI'))
$bytes[0] = 0
[IO.File]::WriteAllBytes($fixture, $bytes)
$rejected = $false
try { & (Join-Path $root 'tests/Test-Pe.ps1') -EfiPath $fixture -PiperVoice:([bool]$PiperPcmPath) }
catch { $rejected = $true }
if (-not $rejected) { throw 'PE validator accepted a corrupted executable.' }
Write-Host 'PASS: Corrupted PE fixture rejected.'
