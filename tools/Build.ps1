param([string]$NasmPath, [string]$OutputDirectory, [ValidateSet('None', 'Ud2', 'Gp')][string]$FaultTest = 'None')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root 'out'
    if ($FaultTest -ne 'None') { $OutputDirectory = Join-Path $OutputDirectory ('fault-' + $FaultTest.ToLowerInvariant()) }
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $NasmPath) {
    $localNasm = Join-Path $root '.tools/nasm-2.16.03/nasm.exe'
    if (Test-Path -LiteralPath $localNasm) { $NasmPath = $localNasm }
    else {
        $command = Get-Command nasm -ErrorAction SilentlyContinue
        if (-not $command) { throw 'NASM missing. See docs/toolchain.md.' }
        $NasmPath = $command.Source
    }
}
$NasmPath = (Get-Command $NasmPath -ErrorAction Stop).Source
$version = (& $NasmPath -v | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot run NASM.' }
if ($version -notmatch '^NASM version 2\.16\.03\b') {
    throw ('Pinned NASM 2.16.03 required. Found: ' + $version)
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$efi = Join-Path $OutputDirectory 'BOOTX64.EFI'
Push-Location $root
try {
    $defines = @()
    if ($FaultTest -eq 'Ud2') { $defines = @('-DJOSHUA_FAULT_TEST=1') }
    if ($FaultTest -eq 'Gp') { $defines = @('-DJOSHUA_FAULT_TEST=2') }
    & $NasmPath @defines -f bin -Wall -w-reloc-rel-dword -w-reloc-abs-dword -Werror -l (Join-Path $OutputDirectory 'boot.lst') -o $efi boot/uefi.asm
    if ($LASTEXITCODE -ne 0) { throw 'Assembly failed; no image was packaged.' }
    & (Join-Path $root 'tests/Test-Pe.ps1') -EfiPath $efi
    & (Join-Path $root 'tools/New-BootImage.ps1') -EfiPath $efi -OutputDirectory $OutputDirectory
    & (Join-Path $root 'tests/Test-BootImage.ps1') -EfiPath $efi -OutputDirectory $OutputDirectory
    $inputs = @('boot/uefi.asm', 'kernel/arch/x86_64/entry.inc', 'kernel/arch/x86_64/data.inc', 'sdk/boot.inc', 'tools/Build.ps1', 'tools/New-BootImage.ps1')
    $manifest = [ordered]@{
        stage = 'M0/M1 boot prototype - unsigned, no installer'
        faultTest = $FaultTest
        assembler = $version
        assemblerSha256 = (Get-FileHash -LiteralPath $NasmPath -Algorithm SHA256).Hash
        inputs = @($inputs | ForEach-Object {
            [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $root $_) -Algorithm SHA256).Hash }
        })
        artifacts = @(@('BOOTX64.EFI', 'joshua-uefi.img', 'joshua-m0.iso') | ForEach-Object {
            $artifact = Join-Path $OutputDirectory $_
            [ordered]@{ path = $_; bytes = (Get-Item -LiteralPath $artifact).Length; sha256 = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash }
        })
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'manifest.json'), ($manifest | ConvertTo-Json -Depth 6) + [Environment]::NewLine, (New-Object Text.UTF8Encoding($false)))
    Write-Host ('Build complete: ' + (Join-Path $OutputDirectory 'joshua-m0.iso'))
}
finally { Pop-Location }
