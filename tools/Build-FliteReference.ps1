param(
    [string]$Text = 'Joshua online. HyperCore boot checkpoint reached.',
    [ValidateSet('rms', 'slt')][string]$Voice = 'slt',
    [string]$OutputDirectory
)

# Host-only (issue #6): builds unmodified Flite 2.2 + a cmu_us clustergen voice (rms or slt) with the pinned
# Zig C compiler and renders a reference WAV plus a segment dump. Requires
# tools/Get-Toolchain.ps1 -IncludeFlite. Nothing here ships in the boot image.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $root ('out/flite-reference/' + $Voice) }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$zig = Join-Path $root '.tools/zig-windows-x86_64-0.13.0/zig.exe'
$flite = Join-Path $root '.tools/flite-2.2'
foreach ($required in @($zig, (Join-Path $flite 'include/flite.h'))) {
    if (-not (Test-Path -LiteralPath $required)) { throw ('Missing ' + $required + '. Run tools/Get-Toolchain.ps1 -IncludeFlite.') }
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$excluded = @('au_alsa.c', 'au_command.c', 'au_oss.c', 'au_palmos.c', 'au_pulseaudio.c', 'au_sun.c', 'au_win.c', 'au_wince.c',
    'auclient.c', 'auserver.c', 'cst_file_palmos.c', 'cst_file_wince.c', 'cst_mmap_posix.c', 'cst_mmap_win32.c', 'cst_socket.c',
    # Included by cmu_lex_data.c rather than compiled (LEX_DATA_INCLUDES in lang/cmulex/Makefile).
    'cmu_lex_data_raw.c', 'cmu_lex_num_bytes.c', 'cmu_lex_phones_huff_table.c', 'cmu_lex_entries_huff_table.c')
$sources = @(Get-ChildItem -LiteralPath (Join-Path $flite 'src') -Recurse -Filter *.c | Where-Object { $excluded -notcontains $_.Name })
foreach ($language in @('usenglish', 'cmulex', ('cmu_us_' + $Voice))) {
    $sources += @(Get-ChildItem -LiteralPath (Join-Path $flite ('lang/' + $language)) -Filter *.c | Where-Object { $excluded -notcontains $_.Name })
}
$sources = @($sources | Sort-Object FullName | ForEach-Object { $_.FullName })
$sources += (Join-Path $PSScriptRoot 'flite-reference/joshua_flite_reference.c')
$executable = Join-Path $OutputDirectory 'joshua_flite_reference.exe'
$arguments = @('cc', '-O2', '-std=gnu99', '-w', '-DCST_AUDIO_NONE', '-DCST_NO_SOCKETS', '-DCST_NO_MMAP', ('-DJOSHUA_VOICE_REGISTER=register_cmu_us_' + $Voice),
    '-I', (Join-Path $flite 'include'), '-I', (Join-Path $flite 'lang/usenglish'), '-I', (Join-Path $flite 'lang/cmulex'),
    '-o', $executable) + $sources
& $zig @arguments
if ($LASTEXITCODE -ne 0) { throw 'Reference Flite build failed.' }
$wave = Join-Path $OutputDirectory 'reference.wav'
$segments = Join-Path $OutputDirectory 'reference-segments.tsv'
& $executable $Text $wave $segments
if ($LASTEXITCODE -ne 0) { throw 'Reference synthesis failed.' }
Write-Host ('Reference voice: ' + $wave)
Write-Host ('Segment dump: ' + $segments)
