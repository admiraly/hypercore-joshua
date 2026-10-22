param([Parameter(Mandatory = $true)][string]$EfiPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $EfiPath).Path)
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw ('PE validation: ' + $Message) }
}
function Read-U16([int]$Offset) { [BitConverter]::ToUInt16($bytes, $Offset) }
function Read-U32([int]$Offset) { [BitConverter]::ToUInt32($bytes, $Offset) }
Assert-True ($bytes.Length -eq 0x7000) 'Unexpected file size.'
Assert-True ((Read-U16 0) -eq 0x5a4d) 'Missing MZ signature.'
$pe = Read-U32 0x3c
Assert-True ((Read-U32 $pe) -eq 0x4550) 'Missing PE signature.'
Assert-True ((Read-U16 ($pe + 4)) -eq 0x8664) 'Not AMD64.'
Assert-True ((Read-U16 ($pe + 6)) -eq 4) 'Expected four sections.'
Assert-True ((Read-U32 ($pe + 8)) -eq 0) 'Non-reproducible timestamp.'
$optional = $pe + 24
Assert-True ((Read-U16 $optional) -eq 0x20b) 'Not PE32+.'
Assert-True ((Read-U16 ($optional + 68)) -eq 10) 'Not an EFI application.'
Assert-True ((Read-U32 ($optional + 56)) -eq 0x249000) 'Wrong image size.'
$entry = Read-U32 ($optional + 16)
Assert-True ($entry -ge 0x1000 -and $entry -lt 0x4000) 'Entry outside code.'
Assert-True ((Read-U32 ($optional + 152)) -eq 0x248000) 'Missing relocation directory.'
Assert-True ((Read-U32 ($optional + 156)) -eq 12) 'Invalid relocation size.'
Assert-True ((Read-U32 0x6004) -eq 12) 'Invalid relocation block.'
$sectionTable = $optional + (Read-U16 ($pe + 20))
$ranges = @()
for ($section = 0; $section -lt 4; $section++) {
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
Write-Host 'PASS: PE32+ AMD64, UEFI subsystem, reproducible header, RX/RW separation, relocations and section bounds.'
