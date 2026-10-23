param([string]$NasmPath, [string]$QemuPath, [string]$FirmwareDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$x2 = Join-Path $root 'out/timer-x2'
& (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -TimerTest X2 -OutputDirectory $x2
& (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $x2 -CpuModel 'qemu64,+x2apic' -ExpectedApicMode X2apic
& (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $x2 -CpuModel 'qemu64,+x2apic' -ExpectedApicMode X2apic -BootMedia Fat
$negative = Join-Path $root 'out/timer-missingeoi'
& (Join-Path $root 'tools/Build.ps1') -NasmPath $NasmPath -TimerTest MissingEoi -OutputDirectory $negative
$rejected = $false
try {
    & (Join-Path $root 'tools/Test-Qemu.ps1') -QemuPath $QemuPath -FirmwareDirectory $FirmwareDirectory -OutputDirectory $negative -TimeoutSeconds 15
}
catch {
    if ($_.Exception.Message -notmatch 'Missing or out-of-order TIMER_OK') { throw }
    $log = [IO.File]::ReadAllText((Join-Path $negative 'qemu-iso-serial.log'))
    if ($log -notmatch 'JOSHUA:APIC_READY MODE=0000000000000001' -or $log -match 'JOSHUA:TIMER_OK|JOSHUA:KERNEL_FAULT') { throw 'Negative fixture did not reach the intended timer stall.' }
    $rejected = $true
}
if (-not $rejected) { throw 'Missing-EOI fixture incorrectly passed.' }
Write-Host 'PASS: x2APIC ISO/FAT deliver 16 timers; missing-EOI fixture rejected at bounded checkpoint timeout.'
