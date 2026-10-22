param(
    [Parameter(Mandatory = $true)][string]$WavPath,
    [ValidateRange(5, 100)][int]$WindowMs = 20,
    [switch]$Spectrum
)

# Host-only analysis of a 16-bit PCM WAV: per-window level, zero-crossing rate,
# optional spectral peaks (formant check) and click detection.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$wav = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $WavPath).Path)
$offset = 12
$format = $null
$dataOffset = $null
$dataBytes = 0
while ($offset + 8 -le $wav.Length) {
    $id = [Text.Encoding]::ASCII.GetString($wav, $offset, 4)
    $size = [BitConverter]::ToUInt32($wav, $offset + 4)
    if ($id -eq 'fmt ') { $format = $offset + 8 }
    if ($id -eq 'data') { $dataOffset = $offset + 8; $dataBytes = [Math]::Min([long]$size, [long]$wav.Length - $dataOffset) }
    $offset += 8 + $size + ($size % 2)
}
if ($null -eq $format -or $null -eq $dataOffset) { throw 'Not a PCM WAV file.' }
$channels = [BitConverter]::ToUInt16($wav, $format + 2)
$rate = [BitConverter]::ToUInt32($wav, $format + 4)
$count = [int]($dataBytes / (2 * $channels))
$x = New-Object double[] $count
for ($i = 0; $i -lt $count; $i++) { $x[$i] = [BitConverter]::ToInt16($wav, $dataOffset + $i * 2 * $channels) }

function Get-Fft([double[]]$Real) {
    $n = $Real.Length
    $re = [double[]]$Real.Clone()
    $im = New-Object double[] $n
    $j = 0
    for ($i = 1; $i -lt $n; $i++) {
        $bit = $n -shr 1
        while ($j -band $bit) { $j = $j -bxor $bit; $bit = $bit -shr 1 }
        $j = $j -bxor $bit
        if ($i -lt $j) { $t = $re[$i]; $re[$i] = $re[$j]; $re[$j] = $t }
    }
    for ($len = 2; $len -le $n; $len *= 2) {
        $angle = -2 * [Math]::PI / $len
        for ($i = 0; $i -lt $n; $i += $len) {
            for ($k = 0; $k -lt $len / 2; $k++) {
                $wr = [Math]::Cos($angle * $k); $wi = [Math]::Sin($angle * $k)
                $a = $i + $k; $b = $a + $len / 2
                $tr = $re[$b] * $wr - $im[$b] * $wi
                $ti = $re[$b] * $wi + $im[$b] * $wr
                $re[$b] = $re[$a] - $tr; $im[$b] = $im[$a] - $ti
                $re[$a] += $tr; $im[$a] += $ti
            }
        }
    }
    $magnitude = New-Object double[] ($n / 2)
    for ($i = 0; $i -lt $n / 2; $i++) { $magnitude[$i] = [Math]::Sqrt($re[$i] * $re[$i] + $im[$i] * $im[$i]) }
    return , $magnitude
}

$window = [int]($rate * $WindowMs / 1000)
$fftSize = 1
while ($fftSize -lt $window) { $fftSize *= 2 }
$sum = 0.0
$peak = 0.0
$peakAt = 0
foreach ($i in 0..($count - 1)) {
    $sum += $x[$i] * $x[$i]
    if ([Math]::Abs($x[$i]) -gt $peak) { $peak = [Math]::Abs($x[$i]); $peakAt = $i }
}
$rms = [Math]::Sqrt($sum / [Math]::Max($count, 1))
for ($start = 0; $start + $window -le $count; $start += $window) {
    $energy = 0.0
    $crossings = 0
    for ($i = $start; $i -lt $start + $window; $i++) {
        $energy += $x[$i] * $x[$i]
        if ($i -gt $start -and (($x[$i] -ge 0) -ne ($x[$i - 1] -ge 0))) { $crossings++ }
    }
    $level = [Math]::Sqrt($energy / $window)
    $db = if ($level -gt 1) { 20 * [Math]::Log10($level / 32768) } else { -99 }
    $line = '{0,6:N3}s {1,6:N1} dB zcr {2,4} {3}' -f ($start / $rate), $db, $crossings, ('#' * [Math]::Max(0, [int](($db + 60) / 2)))
    if ($Spectrum -and $db -gt -45) {
        $frame = New-Object double[] $fftSize
        for ($i = 0; $i -lt $window; $i++) {
            $frame[$i] = $x[$start + $i] * (0.5 - 0.5 * [Math]::Cos(2 * [Math]::PI * $i / ($window - 1)))
        }
        $magnitude = Get-Fft $frame
        $binHz = $rate / $fftSize
        $smooth = New-Object double[] $magnitude.Length
        for ($i = 2; $i -lt $magnitude.Length - 2; $i++) {
            $smooth[$i] = ($magnitude[$i - 2] + 2 * $magnitude[$i - 1] + 3 * $magnitude[$i] + 2 * $magnitude[$i + 1] + $magnitude[$i + 2]) / 9
        }
        $peaks = @()
        $limit = [int](4000 / $binHz)
        for ($i = [int](150 / $binHz); $i -lt $limit; $i++) {
            if ($smooth[$i] -gt $smooth[$i - 1] -and $smooth[$i] -ge $smooth[$i + 1]) {
                $peaks += [pscustomobject]@{ Hz = [int]($i * $binHz); Level = $smooth[$i] }
            }
        }
        $top = @($peaks | Sort-Object Level -Descending | Select-Object -First 3 | Sort-Object Hz | ForEach-Object { $_.Hz })
        $high = 0.0
        $total = 0.0
        for ($i = 1; $i -lt $magnitude.Length; $i++) {
            $power = $magnitude[$i] * $magnitude[$i]
            $total += $power
            if ($i * $binHz -ge 4000) { $high += $power }
        }
        $line += ('  peaks {0}  >4k {1:P0}' -f ($top -join '/'), ($high / [Math]::Max($total, 1e-9)))
    }
    Write-Output $line
}

# A click is a sample-to-sample jump far above the surrounding 2 ms jump level.
$span = [int]($rate / 500)
$clicks = @()
for ($i = $span; $i -lt $count - $span; $i++) {
    $jump = [Math]::Abs($x[$i] - $x[$i - 1])
    if ($jump -lt 2000) { continue }
    $local = 0.0
    for ($k = $i - $span; $k -lt $i + $span; $k++) { if ($k -ne $i) { $local += [Math]::Abs($x[$k] - $x[$k - 1]) } }
    $local /= (2 * $span - 1)
    if ($jump -gt 6 * $local) { $clicks += $i; $i += $span }
}
Write-Output ('Duration {0:N2} s, {1} Hz x{2}; RMS {3:N0} ({4:N1} dBFS); peak {5:N0} at {6:N3} s; crest {7:N1} dB; clicks {8}' -f ($count / $rate), $rate, $channels, $rms, (20 * [Math]::Log10([Math]::Max($rms, 1) / 32768)), $peak, ($peakAt / $rate), (20 * [Math]::Log10($peak / [Math]::Max($rms, 1))), $clicks.Count)
foreach ($click in $clicks | Select-Object -First 10) { Write-Output ('  click at {0:N4} s' -f ($click / $rate)) }
