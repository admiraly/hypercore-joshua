param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host ELF fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/elf-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -w-reloc-rel-dword -o (Join-Path $output 'elf.o') tests/elf.asm
    if ($LASTEXITCODE -ne 0) { throw 'ELF assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'elf.so') (Join-Path $output 'elf.o')
    if ($LASTEXITCODE -ne 0) { throw 'ELF linking failed.' }
    & python3 tests/test_elf.py (Join-Path $output 'elf.so')
    if ($LASTEXITCODE -ne 0) { throw 'ELF fixtures failed.' }
}
finally { Pop-Location }
