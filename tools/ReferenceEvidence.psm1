Set-StrictMode -Version Latest

function Get-Field($Object,[string]$Name) {
    if ($null -eq $Object) { return $null }
    if ($Object -is [Collections.IDictionary]) { return $Object[$Name] }
    $property=$Object.PSObject.Properties[$Name]
    if ($null -ne $property) { return $property.Value }
    return $null
}
function Get-Text($Value) {
    if ($null -eq $Value) { return $null }
    if ($Value -isnot [string]) { throw 'Expected descriptive text.' }
    $text=$Value.Trim()
    if ($text.Length -eq 0) { return $null }
    if ($text.Length -gt 160 -or $text -match '[\x00-\x1f\x7f]') { throw 'Invalid descriptive field.' }
    return $text
}
function Get-Number($Value) {
    [uint64]$number=0
    if (-not [uint64]::TryParse([string]$Value,[ref]$number) -or $number -eq 0) { return $null }
    return $number
}
function Convert-PciRecord($Device) {
    $identities=@(); $subsystems=@(); $revisions=@(); $classes=@()
    foreach ($id in @((Get-Field $Device 'HardwareID'))+@((Get-Field $Device 'CompatibleID'))) {
        if ($null -eq $id) { continue }
        $text=([string]$id).ToUpperInvariant()
        if ($text -match '^PCI\\VEN_([0-9A-F]{4})&DEV_([0-9A-F]{4})(?:&SUBSYS_([0-9A-F]{8}))?(?:&REV_([0-9A-F]{2}))?(?:&CC_([0-9A-F]{6}|[0-9A-F]{4}))?$') {
            $identities+=($Matches[1]+':'+$Matches[2])
            if ($Matches[3]) { $subsystems+=$Matches[3] }
            if ($Matches[4]) { $revisions+=$Matches[4] }
            if ($Matches[5]) { $classes+=$Matches[5] }
        } elseif ($text -match '^PCI\\CC_([0-9A-F]{6}|[0-9A-F]{4})$') { $classes+=$Matches[1] }
    }
    $identities=@($identities | Sort-Object -Unique)
    if ($identities.Count -eq 0) { return $null }
    if ($identities.Count -ne 1) { throw 'Conflicting PCI identity.' }
    $subsystems=@($subsystems | Sort-Object -Unique)
    $revisions=@($revisions | Sort-Object -Unique)
    $classes=@($classes | Sort-Object -Unique)
    if ($subsystems.Count -gt 1 -or $revisions.Count -gt 1) { throw 'Conflicting PCI detail.' }
    $class=$null
    if ($classes.Count) {
        $prefixes=@($classes | ForEach-Object { $_.Substring(0,4) } | Sort-Object -Unique)
        $full=@($classes | Where-Object { $_.Length -eq 6 })
        if ($prefixes.Count -ne 1 -or $full.Count -gt 1) { throw 'Conflicting PCI class.' }
        $class=$classes | Sort-Object Length -Descending | Select-Object -First 1
    }
    $parts=$identities[0].Split(':')
    $role='Unknown'
    if ($class -like '03*') { $role='Graphics' }
    elseif ($class -like '02*') { $role='Network' }
    elseif ($class -like '01*') { $role='Storage' }
    return [pscustomobject][ordered]@{vendor=$parts[0];device=$parts[1];subsystem=$(if ($subsystems.Count) {$subsystems[0]} else {$null});revision=$(if ($revisions.Count) {$revisions[0]} else {$null});classCode=$class;role=$role;count=1}
}
function ConvertTo-ReferenceInventory {
    param([Parameter(Mandatory)]$Snapshot,[Parameter(Mandatory)][datetime]$CapturedUtc,[ValidatePattern('^[A-Z0-9-]{1,32}$')][string]$ReferenceId='UNASSIGNED')
    if ($CapturedUtc.Kind -ne [DateTimeKind]::Utc) { throw 'Use an explicit UTC capture time.' }
    $system=Get-Field $Snapshot 'System'
    $bios=Get-Field $Snapshot 'Bios'
    $cpus=@(); $disks=@(); $groups=@{}
    foreach ($cpu in @((Get-Field $Snapshot 'Processors'))) {
        if ($null -eq $cpu) { continue }
        $cpus += [pscustomobject][ordered]@{brand=(Get-Text (Get-Field $cpu 'Name'));cores=(Get-Number (Get-Field $cpu 'NumberOfCores'));logicalProcessors=(Get-Number (Get-Field $cpu 'NumberOfLogicalProcessors'))}
    }
    foreach ($disk in @((Get-Field $Snapshot 'Disks'))) {
        if ($null -eq $disk) { continue }
        $disks += [pscustomobject][ordered]@{model=(Get-Text (Get-Field $disk 'Model'));firmware=(Get-Text (Get-Field $disk 'FirmwareRevision'))}
    }
    foreach ($device in @((Get-Field $Snapshot 'Pnp'))) {
        $record=Convert-PciRecord $device
        if ($null -eq $record) { continue }
        $key=($record | ConvertTo-Json -Compress)
        if ($groups.ContainsKey($key)) { $groups[$key].count++ } else { $groups[$key]=$record }
    }
    $pci=@($groups.Keys | Sort-Object | ForEach-Object { $groups[$_] })
    $systemRecord=[pscustomobject][ordered]@{vendor=(Get-Text (Get-Field $system 'Manufacturer'));model=(Get-Text (Get-Field $system 'Model'));ramBytes=(Get-Number (Get-Field $system 'TotalPhysicalMemory'));biosVersion=(Get-Text (Get-Field $bios 'SMBIOSBIOSVersion'))}
    $missing=@()
    foreach ($field in @('vendor','model','ramBytes','biosVersion')) { if ($null -eq $systemRecord.$field) { $missing+=('system.'+$field) } }
    if ($cpus.Count -eq 0 -or @($cpus | Where-Object { -not $_.brand -or -not $_.cores -or -not $_.logicalProcessors }).Count) { $missing+='processorDetails' }
    if ($disks.Count -eq 0 -or @($disks | Where-Object { -not $_.model }).Count) { $missing+='diskModels' }
    foreach ($role in @('Graphics','Network','Storage')) { if (@($pci | Where-Object { $_.role -eq $role }).Count -eq 0) { $missing+=($role.ToLowerInvariant()+'PciId') } }
    if (@($pci | Where-Object { $_.classCode -eq '010802' }).Count -eq 0) { $missing+='nvmeControllerPciId' }
    # Disk model/controller association and physical presence are not inferred.
    return [pscustomobject][ordered]@{schemaVersion=1;referenceId=$ReferenceId;capturedUtc=$CapturedUtc.ToString('o');provenance='WindowsCimSnapshot';qualification='Unverified';system=$systemRecord;processors=@($cpus | Sort-Object brand,cores,logicalProcessors);disks=@($disks | Sort-Object model,firmware);pciDevices=$pci;missingEvidence=$missing;diskControllerAssociation='Unknown'}
}
function Assert-Keys($Object,[string[]]$Expected) {
    if ($null -eq $Object) { throw 'Missing evidence object.' }
    $actual=@($Object.PSObject.Properties.Name | Sort-Object)
    $wanted=@($Expected | Sort-Object)
    if (($actual -join '|') -cne ($wanted -join '|')) { throw 'Unexpected evidence fields.' }
}
function Assert-LocalPath([string]$Path) {
    if ($Path -match '^[\\/]{2}' -or $Path -match '::') { throw 'Use a local filesystem path.' }
    $full=[IO.Path]::GetFullPath($Path)
    if ($env:OS -eq 'Windows_NT') {
        $drive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot($full))
        if ($drive.DriveType -notin @([IO.DriveType]::Fixed,[IO.DriveType]::Removable,[IO.DriveType]::CDRom)) { throw 'Use a local drive.' }
    }
    $chain=@(); $cursor=$full
    while ($cursor) {
        $chain+=,$cursor
        $parent=Split-Path $cursor -Parent
        if ($parent -eq $cursor) { break }
        $cursor=$parent
    }
    # Inspect parents first: never probe a descendant through a known redirect.
    [array]::Reverse($chain)
    foreach ($component in $chain) {
        if (Test-Path -LiteralPath $component) {
            $item=Get-Item -LiteralPath $component -Force -ErrorAction Stop
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Reparse paths are unsupported.' }
        }
    }
    return $full
}
function Assert-TextLeaf($Value,[bool]$Nullable=$true) {
    if ($null -eq $Value -and $Nullable) { return }
    if ($Value -isnot [string] -or -not $Value -or (Get-Text $Value) -cne $Value) { throw 'Invalid text leaf.' }
}
function Assert-PositiveLeaf($Value,[bool]$Nullable=$true) {
    if ($null -eq $Value -and $Nullable) { return }
    if ($Value -isnot [byte] -and $Value -isnot [int] -and $Value -isnot [long] -and $Value -isnot [uint64] -and $Value -isnot [uint32]) { throw 'Invalid numeric leaf.' }
    if ($Value -le 0) { throw 'Invalid numeric leaf.' }
}
function Assert-InventoryLeaves($Inventory) {
    Assert-TextLeaf $Inventory.referenceId $false
    if ($Inventory.referenceId -cnotmatch '^[A-Z0-9-]{1,32}$') { throw 'Invalid reference label.' }
    Assert-TextLeaf $Inventory.capturedUtc $false
    if ($Inventory.capturedUtc -notmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{7}Z$') { throw 'Invalid capture time.' }
    [void][datetime]::ParseExact($Inventory.capturedUtc,'o',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind)
    foreach ($field in @('vendor','model','biosVersion')) { Assert-TextLeaf $Inventory.system.$field }
    Assert-PositiveLeaf $Inventory.system.ramBytes
    foreach ($name in @('processors','disks','pciDevices','missingEvidence')) {
        if ($Inventory.$name -isnot [array]) { throw 'Expected evidence array.' }
    }
    foreach ($cpu in $Inventory.processors) { Assert-TextLeaf $cpu.brand; Assert-PositiveLeaf $cpu.cores; Assert-PositiveLeaf $cpu.logicalProcessors }
    foreach ($disk in $Inventory.disks) { Assert-TextLeaf $disk.model; Assert-TextLeaf $disk.firmware }
    foreach ($pci in $Inventory.pciDevices) {
        foreach ($field in @('vendor','device','subsystem','revision','classCode','role')) { Assert-TextLeaf $pci.$field ($field -in @('subsystem','revision','classCode')) }
        if ($pci.vendor -cnotmatch '^[0-9A-F]{4}$' -or $pci.device -cnotmatch '^[0-9A-F]{4}$' -or ($null -ne $pci.subsystem -and $pci.subsystem -cnotmatch '^[0-9A-F]{8}$') -or ($null -ne $pci.revision -and $pci.revision -cnotmatch '^[0-9A-F]{2}$') -or ($null -ne $pci.classCode -and $pci.classCode -cnotmatch '^([0-9A-F]{4}|[0-9A-F]{6})$') -or $pci.role -cnotin @('Unknown','Graphics','Network','Storage')) { throw 'Invalid PCI leaf.' }
        Assert-PositiveLeaf $pci.count $false
    }
    foreach ($missing in $Inventory.missingEvidence) {
        Assert-TextLeaf $missing $false
        if ($missing -cnotin @('system.vendor','system.model','system.ramBytes','system.biosVersion','processorDetails','diskModels','graphicsPciId','networkPciId','storagePciId','nvmeControllerPciId')) { throw 'Invalid missing-evidence leaf.' }
    }
}
function Write-ReferenceEvidence {
    param([Parameter(Mandatory)]$Inventory,[Parameter(Mandatory)][string]$ImagePath,[Parameter(Mandatory)][string]$OutputDirectory)
    Assert-Keys $Inventory @('schemaVersion','referenceId','capturedUtc','provenance','qualification','system','processors','disks','pciDevices','missingEvidence','diskControllerAssociation')
    Assert-Keys $Inventory.system @('vendor','model','ramBytes','biosVersion')
    foreach ($cpu in $Inventory.processors) { Assert-Keys $cpu @('brand','cores','logicalProcessors') }
    foreach ($disk in $Inventory.disks) { Assert-Keys $disk @('model','firmware') }
    foreach ($pci in $Inventory.pciDevices) { Assert-Keys $pci @('vendor','device','subsystem','revision','classCode','role','count') }
    if ($Inventory.schemaVersion -ne 1 -or $Inventory.qualification -ne 'Unverified' -or $Inventory.provenance -ne 'WindowsCimSnapshot' -or $Inventory.diskControllerAssociation -ne 'Unknown') { throw 'Invalid inventory provenance.' }
    Assert-InventoryLeaves $Inventory
    $evidenceRoot=Assert-LocalPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'out/reference-evidence')
    $output=Assert-LocalPath $OutputDirectory
    $comparison=[StringComparison]::Ordinal
    if ($env:OS -eq 'Windows_NT') { $comparison=[StringComparison]::OrdinalIgnoreCase }
    if (-not $output.StartsWith($evidenceRoot+[IO.Path]::DirectorySeparatorChar,$comparison)) { throw 'Output must be below ignored out/reference-evidence.' }
    $localImage=Assert-LocalPath $ImagePath
    $image=Get-Item -LiteralPath $localImage -ErrorAction Stop
    if ($image -isnot [IO.FileInfo] -or $image.Extension -ine '.iso' -or $image.Length -eq 0) { throw 'Specify a regular nonempty ISO file.' }
    if (Test-Path -LiteralPath $output) { throw 'Evidence directory already exists; preserve earlier runs.' }
    $stream=[IO.File]::Open($image.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $imageSha=[Security.Cryptography.SHA256]::Create()
    try {
        $imageBytes=$stream.Length
        if ($imageBytes -eq 0) { throw 'Specify a nonempty ISO file.' }
        $imageHash=([BitConverter]::ToString($imageSha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()
    } finally { $stream.Dispose(); $imageSha.Dispose() }
    $utf8=New-Object Text.UTF8Encoding($false)
    $inventoryJson=($Inventory | ConvertTo-Json -Depth 12).Replace("`r`n","`n")+"`n"
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $inventoryHash=([BitConverter]::ToString($sha.ComputeHash($utf8.GetBytes($inventoryJson)))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    $trial=[pscustomobject][ordered]@{schemaVersion=1;referenceId=$Inventory.referenceId;inventorySha256=$inventoryHash;image=[pscustomobject][ordered]@{sha256=$imageHash;bytes=$imageBytes};observationSource='ManualUnverified';trialNumber=$null;bootType='Unknown';bootMethod='Unknown';bootToolVersion=$null;secureBootObserved='Unknown';displayMode=$null;boot='NotRun';nativeInput='NotRun';safeHalt='NotRun';lastStage='NotObserved';photoEvidence='NotProvided';qualification='NotEstablished'}
    $parent=Split-Path $output -Parent
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $staging=Join-Path $parent ('.reference-staging-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $staging | Out-Null
    try {
        [IO.File]::WriteAllText((Join-Path $staging 'inventory.json'),$inventoryJson,$utf8)
        [IO.File]::WriteAllText((Join-Path $staging 'trial-template.json'),($trial | ConvertTo-Json -Depth 8).Replace("`r`n","`n")+"`n",$utf8)
        [IO.Directory]::Move($staging,$output)
    } finally { if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force } }
    return $output
}
Export-ModuleMember -Function ConvertTo-ReferenceInventory,Write-ReferenceEvidence
