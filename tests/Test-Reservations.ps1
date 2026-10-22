param([string]$NasmPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'This host-only shared-library oracle requires Linux, native NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/reservation-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -o (Join-Path $output 'reservations.o') tests/reservations.asm
    if ($LASTEXITCODE -ne 0) { throw 'Reservation test assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'reservations.so') (Join-Path $output 'reservations.o')
    if ($LASTEXITCODE -ne 0) { throw 'Reservation test linking failed.' }
    & python3 tests/test_reservations.py (Join-Path $output 'reservations.so')
    if ($LASTEXITCODE -ne 0) { throw 'Reservation oracle failed.' }
}
finally { Pop-Location }
