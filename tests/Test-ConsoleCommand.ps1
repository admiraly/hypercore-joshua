param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host console command fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/console-command-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -w-reloc-rel-dword -o (Join-Path $output 'console-command.o') tests/console-command.asm
    if ($LASTEXITCODE -ne 0) { throw 'console command assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'console-command.so') (Join-Path $output 'console-command.o')
    if ($LASTEXITCODE -ne 0) { throw 'console command linking failed.' }
    & python3 tests/test_console_command.py (Join-Path $output 'console-command.so')
    if ($LASTEXITCODE -ne 0) { throw 'console command fixtures failed.' }
}
finally { Pop-Location }
