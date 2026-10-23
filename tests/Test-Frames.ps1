param([string]$NasmPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'This host-only allocator oracle requires Linux, native NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/frame-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -o (Join-Path $output 'frames.o') tests/frames.asm
    if ($LASTEXITCODE -ne 0) { throw 'Frame test assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'frames.so') (Join-Path $output 'frames.o')
    if ($LASTEXITCODE -ne 0) { throw 'Frame test linking failed.' }
    & python3 tests/test_frames.py (Join-Path $output 'frames.so')
    if ($LASTEXITCODE -ne 0) { throw 'Frame allocator oracle failed.' }
}
finally { Pop-Location }
