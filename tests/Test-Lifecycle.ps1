param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host mapping fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/lifecycle-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    # Intentional ELF RIP-relative references from code to the host event buffer.
    & $NasmPath -f elf64 -Wall -Werror -w-reloc-rel-dword -o (Join-Path $output 'lifecycle.o') tests/lifecycle.asm
    if ($LASTEXITCODE -ne 0) { throw 'Mapping lifecycle assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'lifecycle.so') (Join-Path $output 'lifecycle.o')
    if ($LASTEXITCODE -ne 0) { throw 'Mapping lifecycle linking failed.' }
    & python3 tests/test_lifecycle.py (Join-Path $output 'lifecycle.so')
    if ($LASTEXITCODE -ne 0) { throw 'Mapping lifecycle fixtures failed.' }
}
finally { Pop-Location }
