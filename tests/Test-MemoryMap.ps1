param([string]$NasmPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'This host-only ELF test requires Linux, native NASM and ld. QEMU boot validation remains cross-platform.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/map-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -w-reloc-rel-dword -w-reloc-abs-dword -w-reloc-abs-qword -Werror -o (Join-Path $output 'map.o') tests/memory-map.asm
    if ($LASTEXITCODE -ne 0) { throw 'Map test assembly failed.' }
    & ld -z noexecstack -o (Join-Path $output 'map-test') (Join-Path $output 'map.o')
    if ($LASTEXITCODE -ne 0) { throw 'Map test linking failed.' }
    & (Join-Path $output 'map-test')
    if ($LASTEXITCODE -ne 0) { throw ('Memory map fixture failed at case ' + $LASTEXITCODE) }
}
finally { Pop-Location }
Write-Host 'PASS: 24 Assembly memory-map fixtures (valid, extended stride, unknown type, bounds, overflow and overlap).'
