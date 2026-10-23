param([string]$NasmPath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if ($env:OS -eq 'Windows_NT') { throw 'Host mapping fixtures require Linux, NASM, ld and Python 3.' }
if (-not $NasmPath) { $NasmPath = Join-Path $root '.tools/nasm-2.16.03/nasm.exe' }
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -notmatch '^NASM version 2\.16\.03\b') { throw 'Native NASM 2.16.03 required.' }
$output = Join-Path $root 'out/cache-type-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    & $NasmPath -f elf64 -Wall -Werror -o (Join-Path $output 'cache.o') tests/cache-type.asm
    if ($LASTEXITCODE -ne 0) { throw 'Cache type assembly failed.' }
    & ld -shared -z noexecstack -o (Join-Path $output 'cache.so') (Join-Path $output 'cache.o')
    if ($LASTEXITCODE -ne 0) { throw 'Cache type linking failed.' }
    & python3 tests/test_cache_type.py (Join-Path $output 'cache.so')
    if ($LASTEXITCODE -ne 0) { throw 'Cache type fixtures failed.' }
}
finally { Pop-Location }
