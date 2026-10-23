param([string]$OutputDirectory, [switch]$Console)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root $(if ($Console) { 'out/console' } else { 'out' }) }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$artifacts = @('BOOTX64.EFI', 'joshua-uefi.img', 'joshua-m0.iso', 'manifest.json')
if ($Console) { $artifacts += 'JOSHUA-Console-Test.iso' }
& (Join-Path $root 'tools/Build.ps1') -OutputDirectory $OutputDirectory -Console:$Console
$hashes = @{}
foreach ($artifact in $artifacts) {
    $hashes[$artifact] = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory $artifact) -Algorithm SHA256).Hash
}
Push-Location (Split-Path $root -Parent)
try {
    $relativeNasm = Join-Path (Split-Path $root -Leaf) '.tools/nasm-2.16.03/nasm.exe'
    & (Join-Path $root 'tools/Build.ps1') -NasmPath ('.\' + $relativeNasm) -OutputDirectory $OutputDirectory -Console:$Console
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
try { & (Join-Path $root 'tests/Test-Pe.ps1') -EfiPath $fixture }
catch { $rejected = $true }
if (-not $rejected) { throw 'PE validator accepted a corrupted executable.' }
Write-Host 'PASS: Corrupted PE fixture rejected.'
