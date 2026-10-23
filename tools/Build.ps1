param([string]$NasmPath, [string]$OutputDirectory, [string]$PiperPcmPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'out' }
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
$voiceArguments = @()
if ($PiperPcmPath) {
    $PiperPcmPath = (Resolve-Path -LiteralPath $PiperPcmPath).Path
    $pcmBytes = (Get-Item -LiteralPath $PiperPcmPath).Length
    if ($pcmBytes -lt 256 -or $pcmBytes -gt 1572864 -or $pcmBytes % 256) { throw 'Piper PCM must be 256-byte aligned and fit the DMA buffer.' }
    if ($PiperPcmPath.Contains('"') -or $PiperPcmPath.Contains("'")) { throw 'Unsupported quote in PCM path.' }
    $voiceArguments = @("-DJOSHUA_PIPER_PCM='" + $PiperPcmPath.Replace('\', '/') + "'")
}
$efi = Join-Path $OutputDirectory 'BOOTX64.EFI'
Push-Location $root
try {
    & $NasmPath @voiceArguments -f bin -Wall -w-reloc-rel-dword -w-reloc-abs-dword -Werror -l (Join-Path $OutputDirectory 'boot.lst') -o $efi boot/uefi.asm
    if ($LASTEXITCODE -ne 0) { throw 'Assembly failed; no image was packaged.' }
    & (Join-Path $root 'tests/Test-Pe.ps1') -EfiPath $efi -PiperVoice:([bool]$PiperPcmPath)
    & (Join-Path $root 'tools/New-BootImage.ps1') -EfiPath $efi -OutputDirectory $OutputDirectory
    & (Join-Path $root 'tests/Test-BootImage.ps1') -EfiPath $efi -OutputDirectory $OutputDirectory
    $inputs = @('boot/uefi.asm', 'kernel/arch/x86_64/entry.inc', 'kernel/arch/x86_64/data.inc', 'kernel/drivers/pci.inc', 'kernel/drivers/hda.inc', 'kernel/speech/synth.inc', 'kernel/speech/announce.inc', 'kernel/speech/data.inc', 'sdk/boot.inc', 'tools/Build.ps1', 'tools/New-BootImage.ps1', 'tools/Render-Piper.py')
    $manifest = [ordered]@{
        stage = 'M0/M1 boot prototype - unsigned, no installer'
        assembler = $version
        assemblerSha256 = (Get-FileHash -LiteralPath $NasmPath -Algorithm SHA256).Hash
        inputs = @($inputs | ForEach-Object {
            [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $root $_) -Algorithm SHA256).Hash }
        })
        piperPcm = $(if ($PiperPcmPath) { [ordered]@{ bytes = $pcmBytes; sha256 = (Get-FileHash -LiteralPath $PiperPcmPath -Algorithm SHA256).Hash } } else { $null })
        artifacts = @(@('BOOTX64.EFI', 'joshua-uefi.img', 'joshua-m0.iso') | ForEach-Object {
            $artifact = Join-Path $OutputDirectory $_
            [ordered]@{ path = $_; bytes = (Get-Item -LiteralPath $artifact).Length; sha256 = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash }
        })
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'manifest.json'), ($manifest | ConvertTo-Json -Depth 6) + [Environment]::NewLine, (New-Object Text.UTF8Encoding($false)))
    Write-Host ('Build complete: ' + (Join-Path $OutputDirectory 'joshua-m0.iso'))
}
finally { Pop-Location }
