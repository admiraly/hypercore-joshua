param([string]$NasmPath, [string]$OutputDirectory, [ValidateSet('None', 'Ud2', 'Gp', 'ReadOnly', 'Nx', 'Guard', 'ConstWrite', 'Unmapped', 'StackFault', 'DoubleFault')][string]$FaultTest = 'None', [ValidateSet('None', 'X2', 'MissingEoi')][string]$TimerTest = 'None', [ValidateSet('None', 'InheritedId', 'BadFrame', 'WorkerGuard')][string]$ContextTest = 'None')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root 'out'
    if ($ContextTest -ne 'None') { $OutputDirectory = Join-Path $OutputDirectory ('context-' + $ContextTest.ToLowerInvariant()) }
    if ($TimerTest -ne 'None') { $OutputDirectory = Join-Path $OutputDirectory ('timer-' + $TimerTest.ToLowerInvariant()) }
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
    if ($FaultTest -eq 'ReadOnly') { $defines = @('-DJOSHUA_FAULT_TEST=3') }
    if ($FaultTest -eq 'Nx') { $defines = @('-DJOSHUA_FAULT_TEST=4') }
    if ($FaultTest -eq 'StackFault') { $defines = @('-DJOSHUA_FAULT_TEST=8') }
    if ($FaultTest -eq 'DoubleFault') { $defines = @('-DJOSHUA_FAULT_TEST=9') }
    if ($FaultTest -eq 'Unmapped') { $defines = @('-DJOSHUA_FAULT_TEST=7') }
    if ($FaultTest -eq 'ConstWrite') { $defines = @('-DJOSHUA_FAULT_TEST=6') }
    if ($FaultTest -eq 'Guard') { $defines = @('-DJOSHUA_FAULT_TEST=5') }
    if ($TimerTest -eq 'X2') { $defines += '-DJOSHUA_TIMER_TEST=1' }
    if ($TimerTest -eq 'MissingEoi') { $defines += '-DJOSHUA_TIMER_TEST=2' }
    if ($ContextTest -eq 'InheritedId') { $defines += '-DJOSHUA_CONTEXT_TEST=1' }
    if ($ContextTest -eq 'BadFrame') { $defines += '-DJOSHUA_CONTEXT_TEST=2' }
    if ($ContextTest -eq 'WorkerGuard') { $defines += '-DJOSHUA_CONTEXT_TEST=3' }
    & $NasmPath @defines -f bin -Wall -w-reloc-rel-dword -w-reloc-abs-dword -Werror -l (Join-Path $OutputDirectory 'boot.lst') -o $efi boot/uefi.asm
    if ($LASTEXITCODE -ne 0) { throw 'Assembly failed; no image was packaged.' }
    & (Join-Path $root 'tests/Test-Pe.ps1') -EfiPath $efi
    & (Join-Path $root 'tools/New-BootImage.ps1') -EfiPath $efi -OutputDirectory $OutputDirectory
    & (Join-Path $root 'tests/Test-BootImage.ps1') -EfiPath $efi -OutputDirectory $OutputDirectory
    $inputs = @('boot/uefi.asm', 'kernel/arch/x86_64/entry.inc', 'kernel/arch/x86_64/descriptors.inc', 'kernel/arch/x86_64/acpi.inc', 'kernel/arch/x86_64/acpi-start.inc', 'kernel/arch/x86_64/controller-window.inc', 'kernel/arch/x86_64/cache-type.inc', 'kernel/arch/x86_64/cache-snapshot.inc', 'kernel/arch/x86_64/apic-access.inc', 'kernel/arch/x86_64/apic-start.inc', 'kernel/arch/x86_64/apic-policy.inc', 'kernel/arch/x86_64/interrupts.inc', 'kernel/arch/x86_64/data.inc', 'kernel/arch/x86_64/rodata.inc', 'kernel/mm/map.inc', 'kernel/mm/ranges.inc', 'kernel/mm/candidates.inc', 'kernel/mm/paging-inventory.inc', 'kernel/mm/frames.inc', 'kernel/mm/frame-check.inc', 'kernel/mm/owned-map.inc', 'kernel/mm/paging-pat.inc', 'kernel/mm/owned-start.inc', 'kernel/mm/lifecycle.inc', 'kernel/mm/lifecycle-platform.inc', 'kernel/mm/lifecycle-check.inc', 'sdk/boot.inc', 'sdk/context.inc', 'kernel/sched/context.inc', 'kernel/sched/start.inc', 'tools/Build.ps1', 'tools/New-BootImage.ps1')
    $manifest = [ordered]@{
        stage = 'M0/M1 boot prototype - unsigned, no installer'
        faultTest = $FaultTest
        timerTest = $TimerTest
        contextTest = $ContextTest
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
