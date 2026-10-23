param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host mapping fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/apic-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    # Host fixtures intentionally reference BSS/model state through ELF relocations.
    & $NasmPath -f elf64 -Wall -Werror -w-reloc-rel-dword -o (Join-Path $output 'apic.o') tests/apic.asm
    if ($LASTEXITCODE -ne 0) { throw 'APIC policy/window assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'apic.so') (Join-Path $output 'apic.o')
    if ($LASTEXITCODE -ne 0) { throw 'APIC policy/window linking failed.' }
    & python3 tests/test_apic.py (Join-Path $output 'apic.so')
    if ($LASTEXITCODE -ne 0) { throw 'APIC policy/window fixtures failed.' }
}
finally { Pop-Location }
