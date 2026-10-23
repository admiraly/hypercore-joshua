[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$EfiPath,
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'out')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Number {
    param([byte[]]$Buffer, [int]$Offset, [uint64]$Value, [int]$Width, [switch]$BigEndian)
    for ($index = 0; $index -lt $Width; $index++) {
        $position = $Offset + $index
        if ($BigEndian) { $position = $Offset + $Width - 1 - $index }
        $Buffer[$position] = [byte](($Value -shr (8 * $index)) -band 255)
    }
}

function Write-Both {
    param([byte[]]$Buffer, [int]$Offset, [uint64]$Value, [int]$Width)
    Write-Number $Buffer $Offset $Value $Width
    Write-Number $Buffer ($Offset + $Width) $Value $Width -BigEndian
}

function Write-Text {
    param([byte[]]$Buffer, [int]$Offset, [string]$Text)
    $encoded = [Text.Encoding]::ASCII.GetBytes($Text)
    [Array]::Copy($encoded, 0, $Buffer, $Offset, $encoded.Length)
}

function Write-FatEntry {
    param([byte[]]$Buffer, [int]$Offset, [string]$Name, [byte]$Attributes, [int]$Cluster, [int]$Size)
    Write-Text $Buffer $Offset $Name
    $Buffer[$Offset + 11] = $Attributes
    Write-Number $Buffer ($Offset + 16) 33 2
    Write-Number $Buffer ($Offset + 18) 33 2
    Write-Number $Buffer ($Offset + 24) 33 2
    Write-Number $Buffer ($Offset + 26) $Cluster 2
    Write-Number $Buffer ($Offset + 28) $Size 4
}

function Write-IsoRecord {
    param([byte[]]$Buffer, [int]$Offset, [byte[]]$Identifier, [int]$Extent, [int]$Size, [byte]$Flags)
    $length = 33 + $Identifier.Length
    if (($length % 2) -ne 0) { $length++ }
    $Buffer[$Offset] = [byte]$length
    Write-Both $Buffer ($Offset + 2) $Extent 4
    Write-Both $Buffer ($Offset + 10) $Size 4
    $Buffer[$Offset + 18] = 80
    $Buffer[$Offset + 19] = 1
    $Buffer[$Offset + 20] = 1
    $Buffer[$Offset + 25] = $Flags
    Write-Both $Buffer ($Offset + 28) 1 2
    $Buffer[$Offset + 32] = [byte]$Identifier.Length
    [Array]::Copy($Identifier, 0, $Buffer, $Offset + 33, $Identifier.Length)
    return $length
}

$source = (Resolve-Path -LiteralPath $EfiPath).ProviderPath
if (-not [IO.File]::Exists($source)) { throw 'EfiPath must name a regular EFI file.' }
$sourceLength = (Get-Item -LiteralPath $source).Length
if ($sourceLength -lt 1 -or $sourceLength -gt 2097152) {
    throw 'EFI payload must be between 1 byte and 2 MiB.'
}
$payload = [IO.File]::ReadAllBytes($source)
if ($payload.Length -ne $sourceLength) { throw 'EFI input changed while reading.' }
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if ($destination.StartsWith('\\.\') -or $destination.StartsWith('\\?\')) {
    throw 'Device and extended device paths are not supported.'
}
$imagePath = Join-Path $destination 'joshua-uefi.img'
$isoPath = Join-Path $destination 'joshua-m0.iso'
if ($source -eq $imagePath -or $source -eq $isoPath) { throw 'EFI input must not be an output artifact.' }

$sectorSize = 512
$sectorCount = 32768
$sectorsPerFat = 32
$clusterSize = 2048
$fatOffset = $sectorSize
$rootOffset = (1 + 2 * $sectorsPerFat) * $sectorSize
$dataOffset = $rootOffset + 512 * 32
$image = New-Object byte[] ($sectorSize * $sectorCount)
$image[0] = 0xEB
$image[1] = 0x3C
$image[2] = 0x90
Write-Text $image 3 'JOSHUA  '
Write-Number $image 11 $sectorSize 2
$image[13] = 4
Write-Number $image 14 1 2
$image[16] = 2
Write-Number $image 17 512 2
Write-Number $image 19 $sectorCount 2
$image[21] = 0xF8
Write-Number $image 22 $sectorsPerFat 2
Write-Number $image 24 32 2
Write-Number $image 26 64 2
$image[36] = 0x80
$image[38] = 0x29
Write-Number $image 39 0x4A4F5348 4
Write-Text $image 43 'JOSHUA     '
Write-Text $image 54 'FAT16   '
$image[510] = 0x55
$image[511] = 0xAA
Write-Number $image $fatOffset 0xFFF8 2
Write-Number $image ($fatOffset + 2) 0xFFFF 2
Write-Number $image ($fatOffset + 4) 0xFFFF 2
Write-Number $image ($fatOffset + 6) 0xFFFF 2
$payloadClusters = [int][Math]::Ceiling($payload.Length / [double]$clusterSize)
for ($index = 0; $index -lt $payloadClusters; $index++) {
    $cluster = 4 + $index
    $nextCluster = $cluster + 1
    if ($index -eq $payloadClusters - 1) { $nextCluster = 0xFFFF }
    Write-Number $image ($fatOffset + 2 * $cluster) $nextCluster 2
}
[Array]::Copy($image, $fatOffset, $image, $fatOffset + $sectorsPerFat * $sectorSize, $sectorsPerFat * $sectorSize)
Write-FatEntry $image $rootOffset 'EFI        ' 0x10 2 0
Write-FatEntry $image $dataOffset '.          ' 0x10 2 0
Write-FatEntry $image ($dataOffset + 32) '..         ' 0x10 0 0
Write-FatEntry $image ($dataOffset + 64) 'BOOT       ' 0x10 3 0
Write-FatEntry $image ($dataOffset + $clusterSize) '.          ' 0x10 3 0
Write-FatEntry $image ($dataOffset + $clusterSize + 32) '..         ' 0x10 2 0
Write-FatEntry $image ($dataOffset + $clusterSize + 64) 'BOOTX64 EFI' 0x20 4 $payload.Length
[Array]::Copy($payload, 0, $image, $dataOffset + 2 * $clusterSize, $payload.Length)

$blockSize = 2048
$rootBlock = 19
$littlePathBlock = 20
$bigPathBlock = 21
$catalogBlock = 22
$readmeBlock = 23
$imageBlock = 24
$volumeBlocks = $imageBlock + $image.Length / $blockSize
$iso = New-Object byte[] ($volumeBlocks * $blockSize)
foreach ($block in @(16, 17, 18)) {
    Write-Text $iso ($block * $blockSize + 1) 'CD001'
    $iso[$block * $blockSize + 6] = 1
}
$pvd = 16 * $blockSize
$iso[$pvd] = 1
Write-Text $iso ($pvd + 8) ('JOSHUA'.PadRight(32))
Write-Text $iso ($pvd + 40) ('JOSHUA_M0'.PadRight(32))
Write-Both $iso ($pvd + 80) $volumeBlocks 4
Write-Both $iso ($pvd + 120) 1 2
Write-Both $iso ($pvd + 124) 1 2
Write-Both $iso ($pvd + 128) $blockSize 2
Write-Both $iso ($pvd + 132) 10 4
Write-Number $iso ($pvd + 140) $littlePathBlock 4
Write-Number $iso ($pvd + 148) $bigPathBlock 4 -BigEndian
$null = Write-IsoRecord $iso ($pvd + 156) ([byte[]]@(0)) $rootBlock $blockSize 2
Write-Text $iso ($pvd + 190) (' '.PadRight(623))
Write-Text $iso ($pvd + 190) ('JOSHUA_M0'.PadRight(128))
Write-Text $iso ($pvd + 574) ('JOSHUA HOST IMAGE PACKAGER'.PadRight(128))
foreach ($offset in @(813, 830, 864)) { Write-Text $iso ($pvd + $offset) '1980010100000000' }
Write-Text $iso ($pvd + 847) '0000000000000000'
$iso[$pvd + 881] = 1
$bootRecord = 17 * $blockSize
Write-Text $iso ($bootRecord + 7) 'EL TORITO SPECIFICATION'
Write-Number $iso ($bootRecord + 71) $catalogBlock 4
$iso[18 * $blockSize] = 255
foreach ($pathBlock in @($littlePathBlock, $bigPathBlock)) {
    $offset = $pathBlock * $blockSize
    $iso[$offset] = 1
    if ($pathBlock -eq $littlePathBlock) {
        Write-Number $iso ($offset + 2) $rootBlock 4
        Write-Number $iso ($offset + 6) 1 2
    } else {
        Write-Number $iso ($offset + 2) $rootBlock 4 -BigEndian
        Write-Number $iso ($offset + 6) 1 2 -BigEndian
    }
}
$readme = [Text.Encoding]::ASCII.GetBytes("JOSHUA OS M0 - Assembly-authored x64 UEFI application.`r`nBoot via EFI/BOOT/BOOTX64.EFI in the embedded FAT16 image.`r`nDevelopment artifact; not a complete operating system. Secure Boot signing is not provided.`r`n")
$directoryOffset = $rootBlock * $blockSize
$directoryOffset += Write-IsoRecord $iso $directoryOffset ([byte[]]@(0)) $rootBlock $blockSize 2
$directoryOffset += Write-IsoRecord $iso $directoryOffset ([byte[]]@(1)) $rootBlock $blockSize 2
$directoryOffset += Write-IsoRecord $iso $directoryOffset ([Text.Encoding]::ASCII.GetBytes('BOOT.CAT;1')) $catalogBlock $blockSize 0
$directoryOffset += Write-IsoRecord $iso $directoryOffset ([Text.Encoding]::ASCII.GetBytes('EFI.IMG;1')) $imageBlock $image.Length 0
$null = Write-IsoRecord $iso $directoryOffset ([Text.Encoding]::ASCII.GetBytes('README.TXT;1')) $readmeBlock $readme.Length 0
$catalog = $catalogBlock * $blockSize
$iso[$catalog] = 1
$iso[$catalog + 1] = 0xEF
Write-Text $iso ($catalog + 4) 'JOSHUA UEFI'
$iso[$catalog + 30] = 0x55
$iso[$catalog + 31] = 0xAA
$checksum = 0
for ($index = 0; $index -lt 32; $index += 2) {
    $checksum += [int]$iso[$catalog + $index] + 256 * [int]$iso[$catalog + $index + 1]
}
Write-Number $iso ($catalog + 28) ((65536 - ($checksum -band 65535)) -band 65535) 2
$iso[$catalog + 32] = 0x88
Write-Number $iso ($catalog + 38) $sectorCount 2
Write-Number $iso ($catalog + 40) $imageBlock 4
[Array]::Copy($readme, 0, $iso, $readmeBlock * $blockSize, $readme.Length)
[Array]::Copy($image, 0, $iso, $imageBlock * $blockSize, $image.Length)

$null = [IO.Directory]::CreateDirectory($destination)
[IO.File]::WriteAllBytes($imagePath, $image)
[IO.File]::WriteAllBytes($isoPath, $iso)
Write-Host ('Created {0} ({1} bytes)' -f $imagePath, $image.Length)
Write-Host ('Created {0} ({1} bytes)' -f $isoPath, $iso.Length)
Write-Host 'Fixed 1980 timestamps; FAT16 superfloppy; UEFI-only no-emulation El Torito ISO.'
