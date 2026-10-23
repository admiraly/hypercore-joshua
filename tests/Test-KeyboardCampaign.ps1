param(
    [string]$NasmPath,
    [string]$QemuPath,
    [string]$FirmwareDirectory,
    [string]$OutputRoot,
    [ValidateSet('Iso','Fat')][string[]]$BootMedia = @('Iso','Fat'),
    [ValidateSet('Text','Df','Missing','Pending40','Pending44','PendingBoth','DrainExhaust','BadRip','BadSs','BadFlags','BadRsp','BadAddress','BadRoot','UnexpectedTimer','UnexpectedKeyboard','Overflow','OverflowPending')][string[]]$Modes = @('Text','Df','Missing','Pending40','Pending44','PendingBoth','DrainExhaust','BadRip','BadSs','BadFlags','BadRsp','BadAddress','BadRoot','UnexpectedTimer','UnexpectedKeyboard','Overflow','OverflowPending'),
    [switch]$IncludeX2
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputRoot) { $OutputRoot = Join-Path $root 'out' }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$pythonName = 'python3'
if ($env:OS -eq 'Windows_NT') { $pythonName = 'python' }
$python = Get-Command $pythonName -ErrorAction Stop
& $python.Source (Join-Path $root 'tests/test_keyboard_runtime_oracle.py')
if ($LASTEXITCODE -ne 0) { throw 'Keyboard target-oracle mutations failed.' }
foreach ($mode in $Modes) {
    $output = Join-Path $OutputRoot ('keyboard-' + $mode.ToLowerInvariant())
    & (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -KeyboardTest $mode -OutputDirectory $output
    foreach ($media in $BootMedia) {
        & (Join-Path $root 'tools/Test-KeyboardRuntime.ps1') -OutputDirectory $output -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -BootMedia $media
    }
}
if ($IncludeX2) {
    foreach ($mode in @('Text','PendingBoth','OverflowPending')) {
        $output = Join-Path $OutputRoot ('keyboard-' + $mode.ToLowerInvariant() + '-x2')
        & (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -KeyboardTest $mode -TimerTest X2 -OutputDirectory $output
        foreach ($media in $BootMedia) {
            & (Join-Path $root 'tools/Test-KeyboardRuntime.ps1') -OutputDirectory $output -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -BootMedia $media -CpuModel 'qemu64,+x2apic'
        }
    }
}
Write-Host 'PASS: requested actual keyboard target campaign; physical hardware remains unqualified.'
