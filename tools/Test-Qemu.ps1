param(
    [string]$QemuPath,
    [string]$FirmwareDirectory,
    [string]$OutputDirectory,
    [ValidateRange(15, 300)][int]$TimeoutSeconds = 90,
    [ValidateSet('Iso', 'Fat')][string]$BootMedia = 'Iso',
    [switch]$NoAudio
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
if ($NoAudio) { $tag += '-noaudio' }
$vars = Join-Path $OutputDirectory ($tag + '-vars.fd')
$serial = Join-Path $OutputDirectory ($tag + '-serial.log')
$screenshot = Join-Path $OutputDirectory ($tag + '-screen.png')
$errorLog = Join-Path $OutputDirectory ($tag + '-stderr.log')
$outputLog = Join-Path $OutputDirectory ($tag + '-stdout.log')
$speech = Join-Path $OutputDirectory ($tag + '-speech.wav')
if (Test-Path -LiteralPath $speech) { Remove-Item -LiteralPath $speech -Force }
Copy-Item -LiteralPath $varsTemplate -Destination $vars -Force
[IO.File]::WriteAllText($serial, '')
$listener = New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$listener.Stop()
$arguments = @('-machine', 'q35', '-accel', 'tcg', '-cpu', 'qemu64', '-smp', '2', '-m', '512', '-vga', 'std', '-nic', 'none', '-display', 'none', '-no-reboot', '-no-shutdown', '-boot', 'order=d,menu=off', '-drive', ('if=pflash,format=raw,unit=0,readonly=on,file=' + $firmware), '-drive', ('if=pflash,format=raw,unit=1,file=' + $vars), '-serial', ('file:' + $serial), '-qmp', ('tcp:127.0.0.1:' + $port + ',server=on,wait=off'))
if (-not $NoAudio) {
    $arguments += @('-audiodev', ('wav,id=speech,out.frequency=48000,path=' + $speech), '-device', 'ich9-intel-hda', '-device', 'hda-output,audiodev=speech')
}
if ($BootMedia -eq 'Iso') {
    $media = (Resolve-Path -LiteralPath (Join-Path $OutputDirectory 'joshua-m0.iso')).Path
    $arguments += @('-drive', ('media=cdrom,readonly=on,format=raw,file=' + $media))
}
else {
    $media = (Resolve-Path -LiteralPath (Join-Path $OutputDirectory 'joshua-uefi.img')).Path
    $arguments += @('-drive', ('if=none,id=bootdisk,readonly=on,format=raw,file=' + $media), '-device', 'qemu-xhci', '-device', 'usb-storage,drive=bootdisk,removable=on', '-boot', 'order=c,menu=off')
}
function Test-SpeechCapture([string]$Path) {
    $wav = [IO.File]::ReadAllBytes($Path)
    if ($wav.Length -lt 44 -or [Text.Encoding]::ASCII.GetString($wav, 0, 4) -ne 'RIFF' -or [Text.Encoding]::ASCII.GetString($wav, 8, 4) -ne 'WAVE') { throw 'Speech capture is not a RIFF/WAVE file.' }
    $offset = 12
    $format = $null
    $data = $null
    while ($offset + 8 -le $wav.Length) {
        $id = [Text.Encoding]::ASCII.GetString($wav, $offset, 4)
        $size = [BitConverter]::ToUInt32($wav, $offset + 4)
        if ($id -eq 'fmt ') { $format = $offset + 8 }
        if ($id -eq 'data') { $data = [pscustomobject]@{ Offset = $offset + 8; Bytes = [Math]::Min([long]$size, [long]$wav.Length - $offset - 8) } }
        $offset += 8 + $size + ($size % 2)
    }
    if ($null -eq $format -or $null -eq $data) { throw 'Speech capture lacks fmt or data chunk.' }
    $channels = [BitConverter]::ToUInt16($wav, $format + 2)
    $rate = [BitConverter]::ToUInt32($wav, $format + 4)
    $bits = [BitConverter]::ToUInt16($wav, $format + 14)
    if ([BitConverter]::ToUInt16($wav, $format) -ne 1 -or $bits -ne 16 -or $channels -lt 1) { throw 'Speech capture is not 16-bit PCM.' }
    $frames = [long]($data.Bytes / (2 * $channels))
    $seconds = $frames / $rate
    $sum = 0.0
    $peak = 0
    $loud = 0
    for ($frame = 0; $frame -lt $frames; $frame++) {
        $sample = [Math]::Abs([int][BitConverter]::ToInt16($wav, $data.Offset + $frame * 2 * $channels))
        $sum += [double]$sample * $sample
        if ($sample -gt $peak) { $peak = $sample }
        if ($sample -gt 1000) { $loud++ }
    }
    $rms = if ($frames) { [Math]::Sqrt($sum / $frames) } else { 0 }
    Write-Host ('Speech capture: {0:N2} s at {1} Hz x{2}, RMS {3:N0}, peak {4}, active {5:P0}' -f $seconds, $rate, $channels, $rms, $peak, ($loud / [Math]::Max($frames, 1)))
    if ($seconds -lt 2.5 -or $seconds -gt 10) { throw 'Speech capture duration is outside 2.5-10 s.' }
    if ($rms -lt 500) { throw 'Speech capture is silent or nearly silent.' }
    if ($peak -ge 32767) { throw 'Speech capture is clipped.' }
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
        if ($log -match 'JOSHUA:(BOOT_FAILURE|KERNEL_FAULT)') { throw ('Boot failed: ' + $log) }
    } until ($log.Contains('JOSHUA:KERNEL_HALTED') -or [DateTime]::UtcNow -ge $deadline)
    $manifest = Get-Content -LiteralPath (Join-Path $OutputDirectory 'manifest.json') -Raw | ConvertFrom-Json
    if (-not $NoAudio -and $manifest.piperPcm -and -not $log.Contains('JOSHUA:PIPER_PCM')) { throw 'Piper playback marker missing.' }
    $previousIndex = -1
    $speechMarker = 'SPEECH_OK:00'
    if ($NoAudio) { $speechMarker = 'SPEECH_UNAVAILABLE:01' }
    foreach ($marker in @('UEFI_ENTRY', 'GOP_OK', 'EBS_OK', $speechMarker, 'KERNEL_HALTED')) {
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
    $cpus = @(Invoke-Qmp 'query-cpus-fast')
    if ($cpus.Count -ne 2) { throw 'Expected two configured CPUs.' }
    $registers = Invoke-Qmp 'human-monitor-command' @{ 'command-line' = 'info registers' }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory ($tag + '-registers.log')), $registers)
    if ($registers -notmatch '\bHLT=1\b' -or $registers -notmatch '\bCPL=0\b') {
        throw 'Bootstrap CPU did not remain halted in ring 0.'
    }
    if ($registers -notmatch 'RFL=([0-9A-Fa-f]+)') { throw 'Cannot inspect bootstrap CPU flags.' }
    if (([Convert]::ToUInt64($Matches[1], 16) -band 0x200) -ne 0) {
        throw 'Maskable interrupts unexpectedly enabled in the halted stub.'
    }
    # QEMU can close QMP before sending the quit response.
    $qmpWriter.WriteLine('{"execute":"quit"}')
    if (-not $process.WaitForExit(5000)) { throw 'QEMU did not exit after quit.' }
    if (-not $NoAudio) { Test-SpeechCapture $speech }
    $speechResult = 'spoken checkpoint captured to ' + $speech
    if ($NoAudio) { $speechResult = 'no HDA controller reported without blocking boot' }
    Write-Host ('PASS: ' + $BootMedia + ' UEFI boot -> GOP -> ExitBootServices -> ' + $speechResult + ' -> kernel halt, without AVX requirements.')
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
