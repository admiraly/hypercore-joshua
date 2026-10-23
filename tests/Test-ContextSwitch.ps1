param([string]$NasmPath, [string]$QemuPath, [string]$FirmwareDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$id = Join-Path $root 'out/context-inheritedid'
& (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -ContextTest InheritedId -OutputDirectory $id
& (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $id
$log = [IO.File]::ReadAllText((Join-Path $id 'qemu-iso-serial.log'))
if ($log -notmatch 'JOSHUA:CONTEXT_INHERITED_ID_SET' -or $log -notmatch 'JOSHUA:CONTEXT_SWITCH_OK') { throw 'Inherited ID normalization fixture did not complete.' }
$bad = Join-Path $root 'out/context-badframe'
& (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -ContextTest BadFrame -OutputDirectory $bad
$rejected = $false
try {
    & (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $bad
}
catch {
    if ($_.Exception.Message -notmatch 'Boot failed:') { throw }
    $log = [IO.File]::ReadAllText((Join-Path $bad 'qemu-iso-serial.log'))
    if ($log -notmatch 'JOSHUA:CONTEXT_FAILURE STATE=0000000000000000 TICKS=0000000000000000 EOIS=0000000000000000 CURRENT=0000000000000000' -or $log -match 'JOSHUA:CONTEXT_SWITCH_OK|JOSHUA:FAULT VECTOR=|JOSHUA:KERNEL_HALTED') { throw 'Bad frame was not rejected before publication/IRETQ.' }
    $rejected = $true
}
if (-not $rejected) { throw 'Invalid saved frame incorrectly passed.' }
$guard = Join-Path $root 'out/context-workerguard'
& (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -ContextTest WorkerGuard -OutputDirectory $guard
& (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $guard -ExpectedFault WorkerGuard
Write-Host 'PASS: inherited ID normalized, invalid frame rejected before publication, worker guard faults on IST4.'
