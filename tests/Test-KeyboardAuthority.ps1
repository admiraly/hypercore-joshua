param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host keyboard authority fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/keyboard-authority-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -w-reloc-rel-dword -o (Join-Path $output 'keyboard-authority.o') tests/keyboard-authority.asm
    if ($LASTEXITCODE -ne 0) { throw 'keyboard authority assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'keyboard-authority.so') (Join-Path $output 'keyboard-authority.o')
    if ($LASTEXITCODE -ne 0) { throw 'keyboard authority linking failed.' }
    & python3 tests/test_keyboard_authority.py (Join-Path $output 'keyboard-authority.so')
    if ($LASTEXITCODE -ne 0) { throw 'keyboard authority fixtures failed.' }
}
finally { Pop-Location }
