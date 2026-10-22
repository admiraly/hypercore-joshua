[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'out'),
    [string]$EfiPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:assertions = 0

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw ('FAIL: ' + $Message) }
    $script:assertions++
}

function Read-Number {
    param([byte[]]$Buffer, [int]$Offset, [int]$Width, [switch]$BigEndian)
    Assert-True ($Offset -ge 0 -and $Width -gt 0 -and [long]$Offset + $Width -le $Buffer.Length) 'Numeric field is in bounds'
    [uint64]$value = 0
    for ($index = 0; $index -lt $Width; $index++) {
        $position = $Offset + $Width - 1 - $index
        if ($BigEndian) { $position = $Offset + $index }
        $value = ($value -shl 8) -bor $Buffer[$position]
    }
    return $value
}

function Read-Both {
    param([byte[]]$Buffer, [int]$Offset, [int]$Width)
    $little = Read-Number $Buffer $Offset $Width
    $big = Read-Number $Buffer ($Offset + $Width) $Width -BigEndian
    Assert-True ($little -eq $big) ('ISO little/big endian fields agree at byte ' + $Offset)
    return $little
}

function Read-Text {
    param([byte[]]$Buffer, [int]$Offset, [int]$Length)
    Assert-True ($Offset -ge 0 -and $Length -ge 0 -and [long]$Offset + $Length -le $Buffer.Length) 'Text field is in bounds'
    return [Text.Encoding]::ASCII.GetString($Buffer, $Offset, $Length)
}

function Assert-BytesEqual {
    param([byte[]]$Left, [int]$LeftOffset, [byte[]]$Right, [int]$RightOffset, [int]$Length, [string]$Message)
    Assert-True ($LeftOffset -ge 0 -and $RightOffset -ge 0 -and $Length -ge 0 -and [long]$LeftOffset + $Length -le $Left.Length -and [long]$RightOffset + $Length -le $Right.Length) ($Message + ' bounds')
    Assert-True ([Convert]::ToBase64String($Left, $LeftOffset, $Length) -ceq [Convert]::ToBase64String($Right, $RightOffset, $Length)) $Message
}

function Read-FatDirectory {
    param([int]$Offset, [int]$Length, [string[]]$ExpectedNames)
    Assert-True ($Offset -ge 0 -and [long]$Offset + $Length -le $image.Length) 'FAT directory extent is in bounds'
    $entries = @{}
    $ended = $false
    for ($position = $Offset; $position -lt $Offset + $Length; $position += 32) {
        if ($image[$position] -eq 0) { $ended = $true; continue }
        Assert-True (-not $ended) 'FAT directory has no entries beyond its end marker'
        Assert-True ($image[$position] -ne 0xE5 -and $image[$position + 11] -ne 0x0F) 'FAT directory has only live short entries'
        $name = Read-Text $image $position 11
        Assert-True (-not $entries.ContainsKey($name)) ('Unique FAT name: ' + $name)
        Assert-True ((Read-Number $image ($position + 20) 2) -eq 0) 'FAT16 high cluster word is zero'
        foreach ($dateOffset in @(16, 18, 24)) {
            Assert-True ((Read-Number $image ($position + $dateOffset) 2) -eq 33) 'FAT timestamps use fixed 1980-01-01 date'
        }
        foreach ($timeOffset in @(14, 22)) {
            Assert-True ((Read-Number $image ($position + $timeOffset) 2) -eq 0) 'FAT timestamps use midnight'
        }
        $entries[$name] = [pscustomobject]@{
            Name = $name
            Attributes = $image[$position + 11]
            Cluster = [int](Read-Number $image ($position + 26) 2)
            Size = [int](Read-Number $image ($position + 28) 4)
        }
    }
    Assert-True ($ended) 'FAT directory has an end marker'
    Assert-True ((($entries.Keys | Sort-Object) -join '|') -ceq (($ExpectedNames | Sort-Object) -join '|')) 'FAT directory contains exactly the expected names'
    return $entries
}

function Read-FatChain {
    param([int]$FirstCluster, [int]$ExpectedClusters)
    $chain = New-Object 'Collections.Generic.List[int]'
    $cluster = $FirstCluster
    while ($true) {
        Assert-True ($cluster -ge 2 -and $cluster -le $clusterCount + 1) 'FAT chain cluster is within the data region'
        Assert-True (-not $usedClusters.ContainsKey($cluster)) 'FAT chains have no loops or cross-links'
        $usedClusters[$cluster] = $true
        $chain.Add($cluster)
        Assert-True ($chain.Count -le $ExpectedClusters) 'FAT chain does not exceed the expected length'
        $next = Read-Number $image ($fatOffset + 2 * $cluster) 2
        if ($next -ge 0xFFF8) { break }
        Assert-True ($next -ge 2 -and $next -lt 0xFFF0) 'FAT chain contains no free, bad, or reserved link'
        $cluster = [int]$next
    }
    Assert-True ($chain.Count -eq $ExpectedClusters) 'FAT chain has exactly the required allocation'
    return $chain.ToArray()
}

function Read-IsoRecord {
    param([int]$Offset)
    Assert-True ($Offset -ge 0 -and $Offset -lt $iso.Length) 'ISO directory record offset is in bounds'
    $length = [int]$iso[$Offset]
    Assert-True ($length -ge 34 -and ($length % 2) -eq 0 -and [long]$Offset + $length -le $iso.Length) 'ISO directory record length is legal'
    Assert-True (($Offset % 2048) + $length -le 2048) 'ISO record does not cross a logical block'
    $nameLength = [int]$iso[$Offset + 32]
    $expectedLength = 33 + $nameLength
    if (($expectedLength % 2) -ne 0) { $expectedLength++ }
    Assert-True ($length -eq $expectedLength -and $nameLength -gt 0) 'ISO identifier and padding lengths are correct'
    $extent = Read-Both $iso ($Offset + 2) 4
    $size = Read-Both $iso ($Offset + 10) 4
    Assert-True ($extent -ge 19 -and $size -gt 0 -and $extent * 2048 + $size -le $iso.Length) 'ISO file extent lies within the volume'
    Assert-True ($iso[$Offset + 1] -eq 0 -and $iso[$Offset + 26] -eq 0 -and $iso[$Offset + 27] -eq 0) 'ISO records have no extended attributes or interleaving'
    Assert-True ((Read-Both $iso ($Offset + 28) 2) -eq 1) 'ISO directory volume sequence is one'
    Assert-True ($iso[$Offset + 18] -eq 80 -and $iso[$Offset + 19] -eq 1 -and $iso[$Offset + 20] -eq 1) 'ISO record date is fixed at 1980-01-01'
    for ($index = 21; $index -le 24; $index++) {
        Assert-True ($iso[$Offset + $index] -eq 0) 'ISO record time is fixed at UTC midnight'
    }
    if ($length -gt 33 + $nameLength) {
        Assert-True ($iso[$Offset + $length - 1] -eq 0) 'ISO directory identifier padding is zero'
    }
    return [pscustomobject]@{
        Length = $length
        Name = Read-Text $iso ($Offset + 33) $nameLength
        Extent = [int]$extent
        Size = [int]$size
        Flags = [int]$iso[$Offset + 25]
    }
}

$directory = (Resolve-Path -LiteralPath $OutputDirectory).ProviderPath
$image = [IO.File]::ReadAllBytes((Join-Path $directory 'joshua-uefi.img'))
$iso = [IO.File]::ReadAllBytes((Join-Path $directory 'joshua-m0.iso'))
Assert-True ($image.Length -eq 16777216) 'Raw image is exactly 16 MiB'
Assert-True ($image[0] -eq 0xEB -and $image[2] -eq 0x90 -and $image[510] -eq 0x55 -and $image[511] -eq 0xAA) 'FAT boot sector has jump and signature'
$bytesPerSector = [int](Read-Number $image 11 2)
$sectorsPerCluster = [int]$image[13]
$reserved = [int](Read-Number $image 14 2)
$fatCount = [int]$image[16]
$rootEntries = [int](Read-Number $image 17 2)
$totalSectors = [int](Read-Number $image 19 2)
$sectorsPerFat = [int](Read-Number $image 22 2)
Assert-True ($bytesPerSector -eq 512 -and $sectorsPerCluster -eq 4 -and $reserved -eq 1 -and $fatCount -eq 2 -and $rootEntries -eq 512 -and $sectorsPerFat -eq 32) 'FAT BPB geometry matches the deterministic layout'
Assert-True ($totalSectors * $bytesPerSector -eq $image.Length -and (Read-Number $image 32 4) -eq 0 -and (Read-Number $image 28 4) -eq 0) 'FAT total sectors and hidden sectors describe a superfloppy'
Assert-True ($image[21] -eq 0xF8 -and $image[38] -eq 0x29 -and (Read-Text $image 54 8) -ceq 'FAT16   ') 'FAT16 media and extended BPB are correct'
Assert-True ((Read-Number $image 39 4) -eq 0x4A4F5348 -and (Read-Text $image 43 11) -ceq 'JOSHUA     ') 'FAT serial and label are deterministic'
$rootSectors = [int][Math]::Ceiling($rootEntries * 32 / [double]$bytesPerSector)
$dataSectors = $totalSectors - $reserved - $fatCount * $sectorsPerFat - $rootSectors
$clusterCount = [int][Math]::Floor($dataSectors / [double]$sectorsPerCluster)
$clusterSize = $bytesPerSector * $sectorsPerCluster
$fatOffset = $reserved * $bytesPerSector
$rootOffset = ($reserved + $fatCount * $sectorsPerFat) * $bytesPerSector
$dataOffset = $rootOffset + $rootSectors * $bytesPerSector
Assert-True ($clusterCount -ge 4085 -and $clusterCount -lt 65525) 'Cluster count legally selects FAT16 rather than FAT12/FAT32'
Assert-True (($clusterCount + 2) * 2 -le $sectorsPerFat * $bytesPerSector) 'FAT has room for every data cluster'
Assert-BytesEqual $image $fatOffset $image ($fatOffset + $sectorsPerFat * $bytesPerSector) ($sectorsPerFat * $bytesPerSector) 'Both FAT copies are byte-identical'
Assert-True ((Read-Number $image $fatOffset 2) -eq 0xFFF8 -and (Read-Number $image ($fatOffset + 2) 2) -eq 0xFFFF) 'FAT reserved entries and media descriptor are valid'
Write-Host ('PASS: FAT16 BPB, signatures, mirrored tables, legal cluster count ({0}).' -f $clusterCount)

$usedClusters = @{}
$root = Read-FatDirectory $rootOffset ($rootEntries * 32) @('EFI        ')
$efiEntry = $root['EFI        ']
Assert-True ($efiEntry.Attributes -eq 0x10 -and $efiEntry.Size -eq 0) 'EFI is a directory'
$efiChain = @(Read-FatChain $efiEntry.Cluster 1)
$efi = Read-FatDirectory ($dataOffset + ($efiChain[0] - 2) * $clusterSize) $clusterSize @('.          ', '..         ', 'BOOT       ')
Assert-True ($efi['.          '].Cluster -eq $efiEntry.Cluster -and $efi['..         '].Cluster -eq 0) 'EFI dot entries reference self and root'
$bootEntry = $efi['BOOT       ']
Assert-True ($bootEntry.Attributes -eq 0x10 -and $bootEntry.Size -eq 0) 'BOOT is a directory'
$bootChain = @(Read-FatChain $bootEntry.Cluster 1)
$boot = Read-FatDirectory ($dataOffset + ($bootChain[0] - 2) * $clusterSize) $clusterSize @('.          ', '..         ', 'BOOTX64 EFI')
Assert-True ($boot['.          '].Cluster -eq $bootEntry.Cluster -and $boot['..         '].Cluster -eq $efiEntry.Cluster) 'BOOT dot entries reference self and EFI'
foreach ($entries in @($efi, $boot)) {
    foreach ($name in @('.          ', '..         ')) {
        Assert-True ($entries[$name].Attributes -eq 0x10 -and $entries[$name].Size -eq 0) 'Dot entries have directory attributes and zero size'
    }
}
$file = $boot['BOOTX64 EFI']
Assert-True ($file.Attributes -eq 0x20 -and $file.Size -ge 1 -and $file.Size -le 262144) 'BOOTX64.EFI is a file between 1 byte and 256 KiB'
$fileChain = @(Read-FatChain $file.Cluster ([int][Math]::Ceiling($file.Size / [double]$clusterSize)))
$extracted = New-Object byte[] $file.Size
$copied = 0
foreach ($cluster in $fileChain) {
    $length = [Math]::Min($clusterSize, $file.Size - $copied)
    [Array]::Copy($image, $dataOffset + ($cluster - 2) * $clusterSize, $extracted, $copied, $length)
    $copied += $length
}
for ($cluster = 2; $cluster -lt $sectorsPerFat * $bytesPerSector / 2; $cluster++) {
    $value = Read-Number $image ($fatOffset + 2 * $cluster) 2
    if (-not $usedClusters.ContainsKey($cluster)) {
        Assert-True ($value -eq 0) 'Unused FAT entries are free; no orphaned allocations'
    }
}
if (-not $EfiPath) {
    $defaultEfi = Join-Path $directory 'BOOTX64.EFI'
    if ([IO.File]::Exists($defaultEfi)) { $EfiPath = $defaultEfi }
}
if ($EfiPath) {
    $expected = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $EfiPath).ProviderPath)
    Assert-True ($extracted.Length -eq $expected.Length) 'Extracted EFI length equals source'
    Assert-BytesEqual $extracted 0 $expected 0 $expected.Length 'Extracted EFI bytes exactly match the source boot file'
    Write-Host 'PASS: Extracted EFI/BOOT/BOOTX64.EFI exactly matches the source boot file.'
} else {
    Write-Host 'SKIP: Source EFI comparison (supply -EfiPath or place BOOTX64.EFI in OutputDirectory).'
}
Write-Host 'PASS: Exact FAT directory tree, fixed dates, bounded chains, no loops/cross-links/orphans.'

Assert-True ($iso.Length -ge 24 * 2048 -and $iso.Length % 2048 -eq 0) 'ISO length is a complete sequence of logical blocks'
$pvd = 16 * 2048
foreach ($block in @(16, 17, 18)) {
    Assert-True ((Read-Text $iso ($block * 2048 + 1) 5) -ceq 'CD001' -and $iso[$block * 2048 + 6] -eq 1) 'ISO descriptor identifier and version are correct'
}
Assert-True ($iso[$pvd] -eq 1 -and $iso[17 * 2048] -eq 0 -and $iso[18 * 2048] -eq 255) 'ISO has primary, boot, and terminating volume descriptors'
Assert-True ((Read-Text $iso ($pvd + 40) 32).TrimEnd() -ceq 'JOSHUA_M0') 'ISO volume identifier is correct'
Assert-True ((Read-Both $iso ($pvd + 80) 4) * 2048 -eq $iso.Length) 'ISO volume space size matches file length'
Assert-True ((Read-Both $iso ($pvd + 120) 2) -eq 1 -and (Read-Both $iso ($pvd + 124) 2) -eq 1 -and (Read-Both $iso ($pvd + 128) 2) -eq 2048) 'ISO volume set, sequence, and block size are valid'
Assert-True ($iso[$pvd + 881] -eq 1) 'ISO file structure version is one'
foreach ($offset in @(813, 830, 864)) {
    Assert-True ((Read-Text $iso ($pvd + $offset) 16) -ceq '1980010100000000' -and $iso[$pvd + $offset + 16] -eq 0) 'ISO volume timestamps are fixed UTC dates'
}
Assert-True ((Read-Text $iso ($pvd + 847) 16) -ceq '0000000000000000' -and $iso[$pvd + 863] -eq 0) 'ISO volume does not expire'
$rootRecord = Read-IsoRecord ($pvd + 156)
Assert-True ($rootRecord.Name -ceq [string][char]0 -and $rootRecord.Flags -eq 2 -and $rootRecord.Size -eq 2048) 'PVD root directory record is correct'
$pathSize = Read-Both $iso ($pvd + 132) 4
Assert-True ($pathSize -eq 10) 'Both path tables contain exactly one root entry'
$littlePath = [int](Read-Number $iso ($pvd + 140) 4)
$bigPath = [int](Read-Number $iso ($pvd + 148) 4 -BigEndian)
Assert-True ((Read-Number $iso ($pvd + 144) 4) -eq 0 -and (Read-Number $iso ($pvd + 152) 4 -BigEndian) -eq 0) 'Optional ISO path tables are absent'
foreach ($pathBlock in @($littlePath, $bigPath)) {
    Assert-True ($pathBlock -ge 19 -and [long]$pathBlock * 2048 + 10 -le $iso.Length) 'ISO path table lies within the volume'
    $offset = $pathBlock * 2048
    Assert-True ($iso[$offset] -eq 1 -and $iso[$offset + 1] -eq 0 -and $iso[$offset + 8] -eq 0 -and $iso[$offset + 9] -eq 0) 'ISO path root identifier and padding are valid'
    $isBig = $pathBlock -eq $bigPath
    Assert-True ((Read-Number $iso ($offset + 2) 4 -BigEndian:$isBig) -eq $rootRecord.Extent -and (Read-Number $iso ($offset + 6) 2 -BigEndian:$isBig) -eq 1) 'ISO path root extent and parent agree with the PVD'
}
$records = @{}
$position = $rootRecord.Extent * 2048
$directoryEnd = $position + $rootRecord.Size
while ($position -lt $directoryEnd -and $iso[$position] -ne 0) {
    $record = Read-IsoRecord $position
    Assert-True (-not $records.ContainsKey($record.Name)) 'ISO directory names are unique'
    $records[$record.Name] = $record
    $position += $record.Length
}
while ($position -lt $directoryEnd) {
    Assert-True ($iso[$position] -eq 0) 'ISO root directory remainder is zero padding'
    $position++
}
$expectedNames = @([string][char]0, [string][char]1, 'BOOT.CAT;1', 'EFI.IMG;1', 'README.TXT;1')
Assert-True ($records.Count -eq $expectedNames.Count) 'ISO root has exactly the expected entries'
foreach ($name in $expectedNames) { Assert-True ($records.ContainsKey($name)) 'ISO expected root entry exists' }
foreach ($name in @([string][char]0, [string][char]1)) {
    Assert-True ($records[$name].Extent -eq $rootRecord.Extent -and $records[$name].Size -eq $rootRecord.Size -and $records[$name].Flags -eq 2) 'ISO dot entries reference the root directory'
}
foreach ($name in @('BOOT.CAT;1', 'EFI.IMG;1', 'README.TXT;1')) {
    Assert-True ($records[$name].Flags -eq 0) ('ISO regular file flags: ' + $name)
}
$extentRanges = @(
    [pscustomobject]@{ Start = 0; End = 19 },
    [pscustomobject]@{ Start = $rootRecord.Extent; End = $rootRecord.Extent + 1 },
    [pscustomobject]@{ Start = $littlePath; End = $littlePath + 1 },
    [pscustomobject]@{ Start = $bigPath; End = $bigPath + 1 }
)
foreach ($name in @('BOOT.CAT;1', 'EFI.IMG;1', 'README.TXT;1')) {
    $record = $records[$name]
    $extentRanges += [pscustomobject]@{ Start = $record.Extent; End = $record.Extent + [int][Math]::Ceiling($record.Size / 2048.0) }
}
$orderedRanges = @($extentRanges | Sort-Object Start)
for ($index = 1; $index -lt $orderedRanges.Count; $index++) {
    Assert-True ($orderedRanges[$index].Start -ge $orderedRanges[$index - 1].End) 'ISO metadata and files have non-overlapping extents'
}
Write-Host 'PASS: ISO9660 descriptors, dual-endian fields, root records, path tables, dates, and extents.'

$bootRecord = 17 * 2048
Assert-True ((Read-Text $iso ($bootRecord + 7) 32).TrimEnd([char]0) -ceq 'EL TORITO SPECIFICATION') 'El Torito boot system identifier is correct'
$catalogBlock = [int](Read-Number $iso ($bootRecord + 71) 4)
Assert-True ($catalogBlock -eq $records['BOOT.CAT;1'].Extent -and $records['BOOT.CAT;1'].Size -eq 2048) 'Boot record points at the catalog file'
$catalog = $catalogBlock * 2048
Assert-True ($iso[$catalog] -eq 1 -and $iso[$catalog + 1] -eq 0xEF -and (Read-Number $iso ($catalog + 2) 2) -eq 0) 'El Torito validation entry declares EFI platform 0xEF'
Assert-True ($iso[$catalog + 30] -eq 0x55 -and $iso[$catalog + 31] -eq 0xAA) 'El Torito validation entry has the key signature'
$checksum = 0
for ($index = 0; $index -lt 32; $index += 2) { $checksum += Read-Number $iso ($catalog + $index) 2 }
Assert-True (($checksum -band 65535) -eq 0) 'El Torito validation entry checksum is zero modulo 65536'
Assert-True ($iso[$catalog + 32] -eq 0x88 -and $iso[$catalog + 33] -eq 0) 'EFI default boot entry is bootable and no-emulation'
Assert-True ((Read-Number $iso ($catalog + 34) 2) -eq 0 -and $iso[$catalog + 36] -eq 0 -and $iso[$catalog + 37] -eq 0) 'El Torito load segment and system type use EFI defaults'
$bootSectors = Read-Number $iso ($catalog + 38) 2
$bootImageBlock = [int](Read-Number $iso ($catalog + 40) 4)
Assert-True ($bootSectors * 512 -eq $image.Length) 'El Torito sector count covers the complete FAT image in 512-byte sectors'
Assert-True ($bootImageBlock -eq $records['EFI.IMG;1'].Extent -and $records['EFI.IMG;1'].Size -eq $image.Length) 'El Torito boot image extent and ISO file record agree'
for ($index = 44; $index -lt 2048; $index++) {
    Assert-True ($iso[$catalog + $index] -eq 0) 'Catalog contains one default EFI entry and no additional boot sections'
}
Assert-BytesEqual $image 0 $iso ($bootImageBlock * 2048) $image.Length 'Embedded FAT image is exactly identical to the raw image'
$readmeRecord = $records['README.TXT;1']
Assert-True ((Read-Text $iso ($readmeRecord.Extent * 2048) $readmeRecord.Size).Contains('JOSHUA OS M0')) 'ISO README contains the milestone identity'
Write-Host 'PASS: UEFI El Torito platform, checksum, single boot entry, sector count, and image extent.'
Write-Host 'PASS: Embedded FAT image exactly equals joshua-uefi.img.'
Write-Host ('PASS: All {0} static assertions passed. No disks mounted or written.' -f $script:assertions)
