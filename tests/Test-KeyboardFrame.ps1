param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host keyboard frame fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/keyboard-frame-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -w-reloc-rel-dword -o (Join-Path $output 'keyboard-frame.o') tests/keyboard-frame.asm
    if ($LASTEXITCODE -ne 0) { throw 'keyboard frame assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'keyboard-frame.so') (Join-Path $output 'keyboard-frame.o')
    if ($LASTEXITCODE -ne 0) { throw 'keyboard frame linking failed.' }
    & python3 tests/test_keyboard_frame.py (Join-Path $output 'keyboard-frame.so')
    if ($LASTEXITCODE -ne 0) { throw 'keyboard frame fixtures failed.' }
}
finally { Pop-Location }
