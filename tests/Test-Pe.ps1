param([Parameter(Mandatory = $true)][string]$EfiPath, [switch]$PiperVoice)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $EfiPath).Path)
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw ('PE validation: ' + $Message) }
}
function Read-U16([int]$Offset) { [BitConverter]::ToUInt16($bytes, $Offset) }
function Read-U32([int]$Offset) { [BitConverter]::ToUInt32($bytes, $Offset) }
$voiceBytes = 0
$sectionCount = 4
if ($PiperVoice) { $voiceBytes = 0x181000; $sectionCount = 5 }
Assert-True ($bytes.Length -eq (0x9000 + $voiceBytes)) 'Unexpected file size.'
Assert-True ((Read-U16 0) -eq 0x5a4d) 'Missing MZ signature.'
$pe = Read-U32 0x3c
Assert-True ((Read-U32 $pe) -eq 0x4550) 'Missing PE signature.'
Assert-True ((Read-U16 ($pe + 4)) -eq 0x8664) 'Not AMD64.'
Assert-True ((Read-U16 ($pe + 6)) -eq $sectionCount) 'Unexpected section count.'
Assert-True ((Read-U32 ($pe + 8)) -eq 0) 'Non-reproducible timestamp.'
$optional = $pe + 24
Assert-True ((Read-U16 $optional) -eq 0x20b) 'Not PE32+.'
Assert-True ((Read-U16 ($optional + 68)) -eq 10) 'Not an EFI application.'
Assert-True ((Read-U32 ($optional + 56)) -eq (0x34d000 + $voiceBytes)) 'Wrong image size.'
$entry = Read-U32 ($optional + 16)
Assert-True ($entry -ge 0x1000 -and $entry -lt 0x5000) 'Entry outside code.'
Assert-True ((Read-U32 ($optional + 152)) -eq 0x34c000) 'Missing relocation directory.'
Assert-True ((Read-U32 ($optional + 156)) -eq 12) 'Invalid relocation size.'
Assert-True ((Read-U32 0x8004) -eq 12) 'Invalid relocation block.'
$sectionTable = $optional + (Read-U16 ($pe + 20))
$ranges = @()
for ($section = 0; $section -lt $sectionCount; $section++) {
    $offset = $sectionTable + $section * 40
    $flags = Read-U32 ($offset + 36)
    $virtualSize = Read-U32 ($offset + 8)
    $rva = Read-U32 ($offset + 12)
    $rawSize = Read-U32 ($offset + 16)
    $rawOffset = Read-U32 ($offset + 20)
    Assert-True ((($flags -band 0x20000000) -eq 0) -or (($flags -band 0x80000000L) -eq 0)) 'Writable executable section.'
    Assert-True (($rva % 0x1000) -eq 0) 'Unaligned section RVA.'
    Assert-True (($rawOffset % 0x200) -eq 0) 'Unaligned raw section.'
    Assert-True (($rawOffset + $rawSize) -le $bytes.Length) 'Section outside file.'
    foreach ($range in $ranges) {
        Assert-True (($rva -ge $range.End) -or (($rva + $virtualSize) -le $range.Start)) 'Overlapping virtual sections.'
    }
    $ranges += [pscustomobject]@{ Start = $rva; End = $rva + $virtualSize }
}
if ($PiperVoice) {
    $voice = $sectionTable + 4 * 40
    Assert-True ([Text.Encoding]::ASCII.GetString($bytes, $voice, 6) -eq '.voice') 'Missing voice section.'
    Assert-True ((Read-U32 ($voice + 36)) -eq 0x40000040) 'Voice must be read-only, non-executable data.'
    Assert-True ((Read-U32 ($voice + 12)) -eq 0x34d000 -and (Read-U32 ($voice + 20)) -eq 0x9000) 'Wrong voice bounds.'
}
Write-Host 'PASS: PE32+ AMD64, UEFI subsystem, reproducible header, RX/RW separation, relocations and section bounds.'
