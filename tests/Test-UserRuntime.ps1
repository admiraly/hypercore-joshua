param([string]$NasmPath, [string]$QemuPath, [string]$FirmwareDirectory, [switch]$IncludeKvm)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$buildOptions = @{}
if ($NasmPath) { $buildOptions.NasmPath = $NasmPath }
$qemuOptions = @{}
if ($QemuPath) { $qemuOptions.QemuPath = $QemuPath }
if ($FirmwareDirectory) { $qemuOptions.FirmwareDirectory = $FirmwareDirectory }
$modes = @('None','KernelRead','KernelWrite','CodeWrite','Nx','Guard','Io','Cli','Fpu','Spin','Syscall','Sysenter','Np','FarTransfer','BadReturn','Ud2','PendingExit','PendingFault','InheritedLdt')
if ($IncludeKvm) { $modes += @('AlignmentKvm','StackKvm','SyscallKvm','SysenterKvm') }
foreach ($mode in $modes) {
    $output = Join-Path $root ('out/user-' + $mode.ToLowerInvariant())
    & (Join-Path $root 'tools/Build.ps1') @buildOptions -OutputDirectory $output -UserTest $mode
    if ($mode -in @('AlignmentKvm','StackKvm','SyscallKvm','SysenterKvm')) {
        & (Join-Path $root 'tools/Test-Qemu.ps1') @qemuOptions -OutputDirectory $output -Acceleration Kvm -CpuModel host
    }
    else { & (Join-Path $root 'tools/Test-Qemu.ps1') @qemuOptions -OutputDirectory $output }
}
foreach ($mode in @('DrainExhaust','KernelTimer','InheritedCode','LiveSelector','KernelFault')) {
    $output = Join-Path $root ('out/user-' + $mode.ToLowerInvariant())
    & (Join-Path $root 'tools/Build.ps1') @buildOptions -OutputDirectory $output -UserTest $mode
    if ($mode -eq 'KernelFault') { & (Join-Path $root 'tools/Test-Qemu.ps1') @qemuOptions -OutputDirectory $output -ExpectedFault UserKernelFault }
    else { & (Join-Path $root 'tools/Test-Qemu.ps1') @qemuOptions -OutputDirectory $output -ExpectedUserStop $mode }
}
Write-Host ('PASS: ' + ($modes.Count + 5) + ' user execution/fault/watchdog profiles with exact oracles.')
