param([switch]$IncludeQemu, [switch]$IncludeFlite)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Set-StrictMode -Version Latest
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$root = Split-Path $PSScriptRoot -Parent
$tools = Join-Path $root '.tools'
$downloads = Join-Path $tools 'downloads'
New-Item -ItemType Directory -Force -Path $downloads | Out-Null

function Get-VerifiedFile([string]$Url, [string]$Path, [string]$Algorithm, [string]$Hash) {
    if (-not (Test-Path -LiteralPath $Path)) { Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Path }
    if ((Get-FileHash -LiteralPath $Path -Algorithm $Algorithm).Hash -ne $Hash) {
        throw ('Tool archive hash mismatch: ' + $Path + '. Remove that archive manually before retrying.')
    }
}

$nasmArchive = Join-Path $downloads 'nasm.zip'
Get-VerifiedFile 'https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip' $nasmArchive 'SHA256' '3EE4782247BCB874378D02F7EAB4E294A84D3D15F3F6EE2DE2F47A46AA7226E6'
Expand-Archive -LiteralPath $nasmArchive -DestinationPath $tools -Force
& (Join-Path $tools 'nasm-2.16.03/nasm.exe') -v
if ($LASTEXITCODE -ne 0) { throw 'NASM cannot execute.' }

if ($IncludeQemu) {
    $sevenZip = Get-Command 7z -ErrorAction SilentlyContinue
    if ($sevenZip) { $extractor = $sevenZip.Source }
    else { $extractor = Join-Path $env:ProgramFiles '7-Zip/7z.exe' }
    if (-not (Test-Path -LiteralPath $extractor)) { throw 'Optional QEMU extraction needs 7-Zip. Install it separately or supply QEMU manually.' }
    $qemuArchive = Join-Path $downloads 'qemu.exe'
    Get-VerifiedFile 'https://qemu.weilnetz.de/w64/2025/qemu-w64-setup-20251224.exe' $qemuArchive 'SHA512' 'F37D9EE8F498879B7CA92A9959D30CE7971F9894F584DE574F2AC6F5BA925A7265E9D8556D0F56BC75E4E384FC7CAC6BAED78A89C2E42CDBEF109724F01EC16C'
    $destination = Join-Path $tools 'qemu'
    & $extractor x $qemuArchive ('-o' + $destination) -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'QEMU extraction failed.' }
    & (Join-Path $destination 'qemu-system-x86_64.exe') --version
    if ($LASTEXITCODE -ne 0) { throw 'QEMU cannot execute.' }
}
if ($IncludeFlite) {
    # Host-only: Zig's C compiler builds the reference Flite; the tarball is the
    # source of voice data. Neither is committed or shipped as CPU runtime code.
    $zigArchive = Join-Path $downloads 'zig-0.13.0.zip'
    Get-VerifiedFile 'https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip' $zigArchive 'SHA256' 'D859994725EF9402381E557C60BB57497215682E355204D754EE3DF75EE3C158'
    Expand-Archive -LiteralPath $zigArchive -DestinationPath $tools -Force
    & (Join-Path $tools 'zig-windows-x86_64-0.13.0/zig.exe') version
    if ($LASTEXITCODE -ne 0) { throw 'Zig cannot execute.' }
    $fliteArchive = Join-Path $downloads 'flite-2.2.tar.gz'
    Get-VerifiedFile 'https://github.com/festvox/flite/archive/refs/tags/v2.2.tar.gz' $fliteArchive 'SHA256' 'AB1555FE5ADC3F99F1D4A1A0EB1596D329FD6D74F1464A0097C81F53C0CF9E5C'
    & tar -xzf $fliteArchive -C $tools
    if ($LASTEXITCODE -ne 0) { throw 'Flite extraction failed.' }
}
Write-Host 'Project-local toolchain ready. Nothing installed globally; no boot settings changed.'
