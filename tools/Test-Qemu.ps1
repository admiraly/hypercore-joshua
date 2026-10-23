param(
    [string]$QemuPath,
    [string]$FirmwareDirectory,
    [string]$OutputDirectory,
    [ValidateRange(15, 300)][int]$TimeoutSeconds = 90,
    [ValidateSet('Iso', 'Fat')][string]$BootMedia = 'Iso',
    [ValidateSet('None', 'Ud2', 'Gp', 'ReadOnly', 'Nx', 'Guard', 'ConstWrite', 'Unmapped', 'StackFault', 'DoubleFault', 'WorkerGuard')][string]$ExpectedFault = 'None',
    [ValidateSet('qemu64', 'qemu64,+x2apic')][string]$CpuModel = 'qemu64',
    [ValidateSet('Xapic', 'X2apic')][string]$ExpectedApicMode = 'Xapic'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root 'out' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $QemuPath) { $QemuPath = Join-Path $root '.tools/qemu/qemu-system-x86_64.exe' }
$QemuPath = (Resolve-Path -LiteralPath $QemuPath).Path
if (-not $FirmwareDirectory) { $FirmwareDirectory = Join-Path (Split-Path $QemuPath -Parent) 'share' }
$firmware = (Resolve-Path -LiteralPath (Join-Path $FirmwareDirectory 'edk2-x86_64-code.fd')).Path
$varsTemplate = (Resolve-Path -LiteralPath (Join-Path $FirmwareDirectory 'edk2-i386-vars.fd')).Path
$tag = 'qemu-' + $BootMedia.ToLowerInvariant()
$vars = Join-Path $OutputDirectory ($tag + '-vars.fd')
$serial = Join-Path $OutputDirectory ($tag + '-serial.log')
$screenshot = Join-Path $OutputDirectory ($tag + '-screen.png')
$errorLog = Join-Path $OutputDirectory ($tag + '-stderr.log')
$outputLog = Join-Path $OutputDirectory ($tag + '-stdout.log')
Copy-Item -LiteralPath $varsTemplate -Destination $vars -Force
[IO.File]::WriteAllText($serial, '')
$listener = New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()
$arguments = @('-machine', 'q35', '-accel', 'tcg', '-cpu', $CpuModel, '-smp', '2', '-m', '512', '-vga', 'std', '-nic', 'none', '-display', 'none', '-no-reboot', '-no-shutdown', '-boot', 'order=d,menu=off', '-drive', ('if=pflash,format=raw,unit=0,readonly=on,file=' + $firmware), '-drive', ('if=pflash,format=raw,unit=1,file=' + $vars), '-serial', ('file:' + $serial), '-qmp', ('tcp:127.0.0.1:' + $port + ',server=on,wait=off'))
if ($BootMedia -eq 'Iso') {
    $media = (Resolve-Path -LiteralPath (Join-Path $OutputDirectory 'joshua-m0.iso')).Path
    $arguments += @('-drive', ('media=cdrom,readonly=on,format=raw,file=' + $media))
}
else {
    $media = (Resolve-Path -LiteralPath (Join-Path $OutputDirectory 'joshua-uefi.img')).Path
    $arguments += @('-drive', ('if=none,id=bootdisk,readonly=on,format=raw,file=' + $media), '-device', 'qemu-xhci', '-device', 'usb-storage,drive=bootdisk,removable=on', '-boot', 'order=c,menu=off')
}
$quotedArguments = ($arguments | ForEach-Object { [char]34 + $_ + [char]34 }) -join ' '
$process = $null
$client = $null
try {
    $startOptions = @{ FilePath = $QemuPath; ArgumentList = $quotedArguments; PassThru = $true; RedirectStandardError = $errorLog; RedirectStandardOutput = $outputLog }
    if ($env:OS -eq 'Windows_NT') { $startOptions.WindowStyle = 'Hidden' }
    $process = Start-Process @startOptions
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $log = ''
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        if ($process.HasExited) { throw ('QEMU exited: ' + [IO.File]::ReadAllText($errorLog)) }
        $stream = [IO.File]::Open($serial, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        $reader = New-Object IO.StreamReader($stream)
        try { $log = $reader.ReadToEnd() } finally { $reader.Dispose() }
        if ($log -match 'JOSHUA:BOOT_FAILURE' -or ($ExpectedFault -eq 'None' -and $log -match 'JOSHUA:KERNEL_FAULT')) { throw ('Boot failed: ' + $log) }
    } until ($log.Contains('JOSHUA:KERNEL_HALTED') -or ($ExpectedFault -ne 'None' -and $log -match 'JOSHUA:FAULT_STACK[^\r\n]+ IST=[0-9A-F]{16}\r?\n') -or [DateTime]::UtcNow -ge $deadline)
    if ($log -notmatch 'JOSHUA:TSS_READY SELECTOR=([0-9A-F]{16}) BASE=([0-9A-F]{16})') { throw 'Missing owned TSS evidence.' }
    $tssSelector = [Convert]::ToUInt64($Matches[1], 16)
    $tssBase = [Convert]::ToUInt64($Matches[2], 16)
    $stackBounds = @{}
    $previousEnd = [uint64]0
    foreach ($stack in [regex]::Matches($log, 'JOSHUA:IST INDEX=([0-9A-F]{16}) START=([0-9A-F]{16}) END=([0-9A-F]{16})')) {
        $index = [Convert]::ToUInt64($stack.Groups[1].Value, 16)
        $start = [Convert]::ToUInt64($stack.Groups[2].Value, 16)
        $end = [Convert]::ToUInt64($stack.Groups[3].Value, 16)
        if ($index -ne ($stackBounds.Count + 1) -or $end - $start -ne 16384 -or ($start % 4096) -ne 0 -or $start -lt ($previousEnd + 4096)) { throw 'Invalid/distinct IST bounds.' }
        $stackBounds[$index] = @($start, $end)
        $previousEnd = $end
    }
    if ($stackBounds.Count -ne 4) { throw 'Expected four owned IST stacks.' }
    $previousIndex = -1
    $markers = @('UEFI_ENTRY', 'GOP_OK', 'TSS_READY', 'EBS_OK')
    $ownedExpected = $ExpectedFault -in @('None', 'ReadOnly', 'Nx', 'Guard', 'ConstWrite', 'Unmapped', 'StackFault', 'DoubleFault', 'WorkerGuard')
    if ($ownedExpected) { $markers += @('MEMORY_MAP_VALID', 'MEMORY_CANDIDATES_READY', 'PAGING_RESERVED', 'FRAME_ALLOCATOR_OK', 'OWNED_PAGING', 'MAPPING_LIFECYCLE_OK', 'ACPI_TOPOLOGY_READY', 'APIC_READY', 'TIMER_OK') }
    if ($ownedExpected -and $ExpectedFault -ne 'WorkerGuard') { $markers += 'CONTEXT_SWITCH_OK' }
    if ($ExpectedFault -eq 'None') { $markers += 'KERNEL_HALTED' } else { $markers += 'KERNEL_FAULT' }
    foreach ($marker in $markers) {
        $index = $log.IndexOf('JOSHUA:' + $marker, [StringComparison]::Ordinal)
        if ($index -le $previousIndex) { throw ('Missing or out-of-order ' + $marker + '. Inspect ' + $serial) }
        $previousIndex = $index
    }
    $client = New-Object Net.Sockets.TcpClient
    $client.Connect('127.0.0.1', $port)
    $client.ReceiveTimeout = 5000
    $client.SendTimeout = 5000
    $qmpReader = New-Object IO.StreamReader($client.GetStream())
    $qmpWriter = New-Object IO.StreamWriter($client.GetStream())
    $qmpWriter.AutoFlush = $true
    $greeting = $qmpReader.ReadLine() | ConvertFrom-Json
    if (-not $greeting.QMP) { throw 'Missing QMP greeting.' }
    function Invoke-Qmp([string]$Command, [hashtable]$Parameters = @{}) {
        $qmpWriter.WriteLine((@{ execute = $Command; arguments = $Parameters } | ConvertTo-Json -Compress -Depth 5))
        do {
            $response = $qmpReader.ReadLine() | ConvertFrom-Json
            if (-not $response) { throw 'QMP disconnected.' }
            if ($response.PSObject.Properties['error']) { throw ($response.error | ConvertTo-Json -Compress) }
        } until ($response.PSObject.Properties['return'])
        return $response.return
    }
    Invoke-Qmp 'qmp_capabilities' | Out-Null
    Invoke-Qmp 'screendump' @{ filename = $screenshot; format = 'png' } | Out-Null
    if ($ownedExpected) {
        $modeHex = '0000000000000001'
        $ucPagesHex = '0000000000000002'
        if ($ExpectedApicMode -eq 'X2apic') { $modeHex = '0000000000000002'; $ucPagesHex = '0000000000000001' }
        if ($log -notmatch ('JOSHUA:APIC_READY MODE=' + $modeHex + ' UC_PAGES=' + $ucPagesHex + ' IOAPICS=0000000000000001')) { throw 'Missing validated controller/cache/mode evidence.' }
        if ($log -notmatch ('JOSHUA:TIMER_OK MODE=' + $modeHex + ' TICKS=0000000000000010 EOIS=0000000000000010 RETURNS=0000000000000010')) { throw 'Timer delivery/EOI/return evidence incomplete.' }
        if ($ExpectedFault -ne 'WorkerGuard' -and ($log -notmatch 'JOSHUA:CONTEXT_SWITCH_OK TICKS=0000000000000040 EOIS=0000000000000040 RETURNS=0000000000000040 PROGRESS1=([0-9A-F]{16}) PROGRESS2=([0-9A-F]{16})' -or [Convert]::ToUInt64($Matches[1],16) -eq 0 -or [Convert]::ToUInt64($Matches[2],16) -eq 0)) { throw 'Missing bounded context-switch/progress evidence.' }
        if ($log -notmatch 'JOSHUA:ACPI_TOPOLOGY_READY BSP=0000000000000000 CPUS=0000000000000002 ENABLED=0000000000000002 LAPIC=([0-9A-F]{16})' -or [Convert]::ToUInt64($Matches[1], 16) -eq 0) { throw 'Unexpected QEMU BSP/ACPI topology.' }
    }
    if ($ExpectedFault -eq 'None') {
        if ($log -notmatch 'JOSHUA:MEMORY_CANDIDATES_READY COUNT=([0-9A-F]{16})' -or [Convert]::ToUInt64($Matches[1], 16) -eq 0) { throw 'Expected nonempty memory candidates on the QEMU profile.' }
        if ($log -notmatch 'JOSHUA:FRAME_ALLOCATOR_OK FREE_PAGES=([0-9A-F]{16})' -or [Convert]::ToUInt64($Matches[1], 16) -eq 0) { throw 'Allocator checkpoint missing or exhausted.' }
        # QEMU writes a simple P6 image; inspect bytes without platform graphics libraries.
        $ppm = Join-Path $OutputDirectory ($tag + '-screen.ppm')
        Invoke-Qmp 'screendump' @{ filename = $ppm } | Out-Null
        $pixels = [IO.File]::ReadAllBytes($ppm)
        $header = [Text.Encoding]::ASCII.GetString($pixels, 0, [Math]::Min(128, $pixels.Length))
        if ($header -notmatch '\AP6\n([0-9]+) ([0-9]+)\n255\n') { throw 'Unexpected QEMU PPM header.' }
        $offset = $Matches[0].Length
        $width = [int]$Matches[1]
        $height = [int]$Matches[2]
        if ($width -lt 640 -or $height -lt 400 -or $pixels.Length -ne $offset + $width * $height * 3) { throw 'Invalid framebuffer dimensions or payload.' }
        foreach ($sample in @(@(0, 0, 8), @(38, 32, 224), @(32, 280, 224))) {
            $index = $offset + ($sample[1] * $width + $sample[0]) * 3
            if ($pixels[$index] -ne $sample[2] -or $pixels[$index+1] -ne $sample[2] -or $pixels[$index+2] -ne $sample[2]) {
                throw ('Native framebuffer rendering mismatch at ' + $sample[0] + ',' + $sample[1])
            }
        }
    }
    else {
        if ($log.Contains('JOSHUA:KERNEL_HALTED')) { throw 'Fault fixture reached the normal checkpoint.' }
        if ($log -notmatch 'JOSHUA:FIXTURE_RIP=([0-9A-F]{16})') { throw 'Missing fixture instruction address.' }
        $expectedRip = $Matches[1]
        $vector = '0000000000000006'
        $errorCode = '0000000000000000'
        if ($ExpectedFault -eq 'Gp') { $vector = '000000000000000D'; $errorCode = '000000000000FFFC' }
        $cr2Pattern = '[0-9A-F]{16}'
        if ($ExpectedFault -in @('ReadOnly', 'Nx', 'Guard', 'ConstWrite', 'Unmapped', 'StackFault', 'DoubleFault', 'WorkerGuard')) {
            $vector = '000000000000000E'
            if ($ExpectedFault -in @('ReadOnly', 'ConstWrite')) { $errorCode = '0000000000000003' }
            if ($ExpectedFault -eq 'Nx') { $errorCode = '0000000000000011' }
            if ($log -notmatch 'JOSHUA:FIXTURE_TARGET=([0-9A-F]{16})') { throw 'Missing fixture fault target.' }
            $cr2Pattern = $Matches[1]
        }
        if ($ExpectedFault -eq 'DoubleFault') { $vector = '0000000000000008'; $expectedRip = '[0-9A-F]{16}'; $cr2Pattern = '[0-9A-F]{16}' }
        if ($log -notmatch ('JOSHUA:FAULT VECTOR=' + $vector + ' ERROR=' + $errorCode + ' RIP=' + $expectedRip + ' CR2=' + $cr2Pattern + '\r?\n')) {
            throw ('Fault frame did not match fixture: ' + $log)
        }
    }
    if ($ExpectedFault -ne 'None') {
        if ($log -notmatch 'JOSHUA:FAULT_STACK FRAME=([0-9A-F]{16}) OLD_RSP=([0-9A-F]{16}) IST=([0-9A-F]{16})') { throw 'Missing exception stack evidence.' }
        $frame = [Convert]::ToUInt64($Matches[1], 16)
        $oldRsp = [Convert]::ToUInt64($Matches[2], 16)
        $ist = [Convert]::ToUInt64($Matches[3], 16)
        $expectedIst = [uint64]4
        if ($ExpectedFault -eq 'DoubleFault') { $expectedIst = [uint64]1 }
        if ($ist -ne $expectedIst -or $frame -ne ($stackBounds[$ist][1] - 56)) { throw 'Fault frame did not use selected IST.' }
        if ($ExpectedFault -eq 'StackFault') {
            if ($log -notmatch 'JOSHUA:FIXTURE_OLD_RSP=([0-9A-F]{16})' -or $oldRsp -ne [Convert]::ToUInt64($Matches[1], 16)) { throw 'Interrupted unusable RSP not preserved.' }
        }
        if ($ExpectedFault -eq 'DoubleFault') {
            if ($log -notmatch 'JOSHUA:FAULT VECTOR=[^\r\n]+ CR2=([0-9A-F]{16})') { throw 'Missing double-fault CR2.' }
            $dfCr2 = [Convert]::ToUInt64($Matches[1], 16)
            if ($dfCr2 -lt ($stackBounds[[uint64]4][0] - 4096) -or $dfCr2 -ge $stackBounds[[uint64]4][0]) { throw 'Double fault did not follow failed ordinary-stack delivery.' }
        }
    }
    $cpus = @(Invoke-Qmp 'query-cpus-fast')
    if ($cpus.Count -ne 2) { throw 'Expected two configured CPUs.' }
    $registers = Invoke-Qmp 'human-monitor-command' @{ 'command-line' = 'info registers' }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory ($tag + '-registers.log')), $registers)
    if ($registers -notmatch 'TR =([0-9A-Fa-f]+) ([0-9A-Fa-f]+) 00000067' -or [Convert]::ToUInt64($Matches[1], 16) -ne $tssSelector -or [Convert]::ToUInt64($Matches[2], 16) -ne $tssBase) { throw 'TR does not reference the owned TSS.' }
    if ($registers -notmatch '\bHLT=1\b' -or $registers -notmatch '\bCPL=0\b') {
        throw 'Bootstrap CPU did not remain halted in ring 0.'
    }
    if ($ownedExpected) {
        if ($log -notmatch 'JOSHUA:PAGING_RESERVED CR3=([0-9A-F]{16}) TABLES=([0-9A-F]{16})') { throw 'Missing paging inventory diagnostics.' }
        $capturedCr3 = [Convert]::ToUInt64($Matches[1], 16)
        $tableCount = [Convert]::ToUInt64($Matches[2], 16)
        if ($tableCount -eq 0 -or $tableCount -gt 16384) { throw 'Paging inventory outside supported capacity.' }
        if ($log -notmatch 'JOSHUA:OWNED_PAGING CR3=([0-9A-F]{16})') { throw 'Missing owned paging marker.' }
        $ownedCr3 = [Convert]::ToUInt64($Matches[1], 16)
        if ($ownedCr3 -eq $capturedCr3) { throw 'Owned CR3 did not change.' }
        if ($ExpectedFault -ne 'WorkerGuard') {
            function Read-PhysicalQword([uint64]$Address) {
                $dump = Invoke-Qmp 'human-monitor-command' @{ 'command-line' = ('xp /1gx 0x{0:x}' -f $Address) }
                if ($dump -notmatch ':\s+0x([0-9a-fA-F]{16})') { throw 'Cannot decode physical page-table entry.' }
                return [Convert]::ToUInt64($Matches[1],16)
            }
            function Get-OwnedLeaf([uint64]$Address) {
                $table = $ownedCr3
                foreach ($shift in @(39,30,21,12)) {
                    $slot = ($Address -shr $shift) -band 511
                    $leaf = Read-PhysicalQword ($table + $slot * 8)
                    if (($leaf -band 1) -eq 0) { return [uint64]0 }
                    if (($leaf -band 4) -ne 0) { throw 'User-accessible kernel stack translation.' }
                    if (($leaf -band 2) -eq 0) { throw 'Read-only kernel stack translation.' }
                    if ($shift -eq 12 -or (($shift -eq 30 -or $shift -eq 21) -and ($leaf -band 128) -ne 0)) { return $leaf }
                    $table = $leaf -band [uint64]0x000ffffffffff000
                }
                throw 'Invalid page-table walk.'
            }
            $allStacks = @()
            foreach ($index in 1..4) { $allStacks += ,$stackBounds[[uint64]$index] }
            $contextStackCount = 0
            foreach ($stack in [regex]::Matches($log, 'JOSHUA:CONTEXT_STACK INDEX=([0-9A-F]{16}) START=([0-9A-F]{16}) END=([0-9A-F]{16}) TICKS=([0-9A-F]{16})')) {
                $index = [Convert]::ToUInt64($stack.Groups[1].Value,16)
                $start = [Convert]::ToUInt64($stack.Groups[2].Value,16)
                $end = [Convert]::ToUInt64($stack.Groups[3].Value,16)
                $ticks = [Convert]::ToUInt64($stack.Groups[4].Value,16)
                $size = 16384; $expectedTicks = 32
                if ($index -eq 0) { $size = 65536; $expectedTicks = 0 }
                if ($index -ne $contextStackCount -or $end -le $start -or $end - $start -ne $size -or ($start % 4096) -ne 0 -or $ticks -ne $expectedTicks) { throw 'Invalid context stack/count evidence.' }
                foreach ($other in $allStacks) {
                    if ($start -lt $other[1] -and $end -gt $other[0]) { throw 'Context/emergency stack overlap.' }
                }
                $allStacks += ,@($start,$end)
                $contextStackCount++
            }
            if ($contextStackCount -ne 3 -or $allStacks.Count -ne 7) { throw 'Expected seven distinct guarded stacks.' }
            $permissionMask = [Convert]::ToUInt64('8000000000000007',16)
            $expectedPermission = [Convert]::ToUInt64('8000000000000003',16)
            foreach ($stack in $allStacks) {
                if ((Get-OwnedLeaf ($stack[0] - 4096)) -ne 0) { throw 'Stack guard is mapped.' }
                for ($page = $stack[0]; $page -lt $stack[1]; $page += 4096) {
                    if (((Get-OwnedLeaf $page) -band $permissionMask) -ne $expectedPermission) { throw 'Stack page is not supervisor RW/NX.' }
                }
            }
        }

        if ($registers -notmatch 'CR3=([0-9A-Fa-f]+)' -or [Convert]::ToUInt64($Matches[1], 16) -ne $ownedCr3) { throw 'Final CR3 does not match owned root.' }
        if ($registers -notmatch 'CR0=([0-9A-Fa-f]+)' -or ([Convert]::ToUInt64($Matches[1], 16) -band 0x10000) -eq 0) { throw 'CR0.WP is not enabled.' }
        if ($registers -notmatch 'EFER=([0-9A-Fa-f]+)' -or ([Convert]::ToUInt64($Matches[1], 16) -band 0x800) -eq 0) { throw 'EFER.NXE is not enabled.' }
        if ($registers -notmatch 'CR4=([0-9A-Fa-f]+)' -or ([Convert]::ToUInt64($Matches[1], 16) -band 0x20080) -ne 0) { throw 'PCID/global pages unexpectedly enabled.' }
    }
    if ($registers -notmatch 'RFL=([0-9A-Fa-f]+)') { throw 'Cannot inspect bootstrap CPU flags.' }
    if (([Convert]::ToUInt64($Matches[1], 16) -band 0x200) -ne 0) {
        throw 'Maskable interrupts unexpectedly enabled in the halted stub.'
    }
    # Some QEMU versions close QMP before delivering the quit response.
    $qmpWriter.WriteLine('{"execute":"quit"}')
    if (-not $process.WaitForExit(5000)) { throw 'QEMU did not exit after quit.' }
    Write-Host ('PASS: ' + $BootMedia + ' UEFI boot -> GOP -> ExitBootServices -> halted ring 0; expected fault: ' + $ExpectedFault)
    Write-Host ('Screenshot: ' + $screenshot)
}
finally {
    if ($client) { $client.Dispose() }
    if ($process) {
        $process.Refresh()
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.Dispose()
    }
}
