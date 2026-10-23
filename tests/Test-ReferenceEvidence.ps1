param([switch]$CollectHost)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $root 'tools/ReferenceEvidence.psm1') -Force
$script:checks=0
function Require($Condition,[string]$Message) { $script:checks++; if (-not $Condition) { throw $Message } }
function Reject([scriptblock]$Action) { $rejected=$false; try { & $Action | Out-Null } catch { $rejected=$true }; Require $rejected 'Invalid evidence accepted.' }
$time=[datetime]::Parse('2026-10-23T12:00:00Z').ToUniversalTime()
$private='PRIVATE_SENTINEL_DO_NOT_EMIT'
$snapshot=@{
    System=[pscustomobject]@{Manufacturer=' Test Vendor ';Model='Test Model';TotalPhysicalMemory=17179869184;Name=$private;Domain=$private;UserName=$private;UUID=$private}
    Bios=[pscustomobject]@{SMBIOSBIOSVersion='1.2.3';SerialNumber=$private}
    Processors=@([pscustomobject]@{Name='Test CPU';NumberOfCores=4;NumberOfLogicalProcessors=8;ProcessorId=$private})
    Disks=@([pscustomobject]@{Model='Test Disk';FirmwareRevision='FW01';SerialNumber=$private;PNPDeviceID=$private})
    Pnp=@(
        [pscustomobject]@{HardwareID=@('pci\ven_8086&dev_1234&subsys_10203040&rev_02','PCI\VEN_8086&DEV_1234&CC_030000');CompatibleID=@('PCI\CC_0300');PNPDeviceID=$private;MACAddress=$private;Name=$private},
        [pscustomobject]@{HardwareID=@('PCI\VEN_1111&DEV_2222');CompatibleID=@('PCI\CC_020000')},
        [pscustomobject]@{HardwareID=@('PCI\VEN_3333&DEV_4444&REV_01');CompatibleID=@('PCI\CC_010802')},
        [pscustomobject]@{HardwareID=@('USB\VID_1234&PID_1234\'+$private);CompatibleID=@()}
    )
}
$inventory=ConvertTo-ReferenceInventory -Snapshot $snapshot -CapturedUtc $time
Require ($inventory.referenceId -eq 'UNASSIGNED' -and $inventory.qualification -eq 'Unverified') 'Qualification was inferred.'
Require ($inventory.system.vendor -eq 'Test Vendor' -and $inventory.system.ramBytes -eq 17179869184) 'System projection failed.'
Require ($inventory.processors.Count -eq 1 -and $inventory.processors[0].logicalProcessors -eq 8) 'CPU array/fields failed.'
Require ($inventory.disks.Count -eq 1 -and $inventory.disks[0].model -eq 'Test Disk') 'Disk model projection failed.'
Require ($inventory.pciDevices.Count -eq 3 -and $inventory.missingEvidence.Count -eq 0) 'PCI coverage failed.'
$gpu=@($inventory.pciDevices | Where-Object { $_.role -eq 'Graphics' })[0]
Require ($gpu.vendor -ceq '8086' -and $gpu.device -ceq '1234' -and $gpu.subsystem -ceq '10203040' -and $gpu.revision -ceq '02' -and $gpu.classCode -ceq '030000') 'PCI normalization failed.'
Require ($inventory.diskControllerAssociation -eq 'Unknown') 'Disk/controller association inferred.'
$json=$inventory | ConvertTo-Json -Depth 12
Require ($json -notmatch $private -and $json -notmatch 'PNPDeviceID|MACAddress|ProcessorId|SerialNumber|UserName|Domain|UUID') 'Private fields leaked.'
$empty=ConvertTo-ReferenceInventory -Snapshot @{} -CapturedUtc $time
Require ($empty.processors.Count -eq 0 -and $empty.disks.Count -eq 0 -and $empty.pciDevices.Count -eq 0) 'Empty arrays changed shape.'
Require ($empty.missingEvidence.Count -eq 10) 'Missing evidence must stay explicit.'
$duplicate=$snapshot.Clone();$duplicate.Pnp=@($snapshot.Pnp[0],$snapshot.Pnp[0])
$d=ConvertTo-ReferenceInventory -Snapshot $duplicate -CapturedUtc $time
Require ($d.pciDevices.Count -eq 1 -and $d.pciDevices[0].count -eq 2) 'Duplicate function count failed.'
$permuted=$snapshot.Clone();$permuted.Pnp=@($snapshot.Pnp[3],$snapshot.Pnp[2],$snapshot.Pnp[0],$snapshot.Pnp[1])
Require (($json) -ceq ((ConvertTo-ReferenceInventory -Snapshot $permuted -CapturedUtc $time) | ConvertTo-Json -Depth 12)) 'Provider order changed report.'
foreach ($ids in @(
    @('PCI\VEN_0001&DEV_0002','PCI\VEN_0001&DEV_0003'),
    @('PCI\VEN_0001&DEV_0002&REV_01','PCI\VEN_0001&DEV_0002&REV_02'),
    @('PCI\VEN_0001&DEV_0002&SUBSYS_00000001','PCI\VEN_0001&DEV_0002&SUBSYS_00000002'),
    @('PCI\VEN_0001&DEV_0002&CC_010802','PCI\CC_020000'),
    @('PCI\VEN_0001&DEV_0002&CC_010802','PCI\CC_010801')
)) { Reject { ConvertTo-ReferenceInventory -Snapshot @{Pnp=@{HardwareID=$ids}} -CapturedUtc $time } }
foreach ($id in @('PCI\VEN_0001&DEV_0002\PRIVATE','PCI\VEN_000G&DEV_0002','PCI\VEN_0001&DEV_0002&PRIVATE=SECRET')) {
    $bad=ConvertTo-ReferenceInventory -Snapshot @{Pnp=@{HardwareID=@($id)}} -CapturedUtc $time
    Require ($bad.pciDevices.Count -eq 0) 'Raw instance/malformed ID accepted.'
}
Reject { ConvertTo-ReferenceInventory -Snapshot @{System=@{Model="bad`nmodel"}} -CapturedUtc $time }
Reject { ConvertTo-ReferenceInventory -Snapshot @{Bios=@{SMBIOSBIOSVersion=('x'*161)}} -CapturedUtc $time }
Reject { ConvertTo-ReferenceInventory -Snapshot $snapshot -CapturedUtc ([datetime]::SpecifyKind($time,[DateTimeKind]::Unspecified)) }
Reject { ConvertTo-ReferenceInventory -Snapshot $snapshot -CapturedUtc $time -ReferenceId 'private/path' }
$number=ConvertTo-ReferenceInventory -Snapshot @{System=@{TotalPhysicalMemory='-1'};Processors=@{Name='X';NumberOfCores='oops';NumberOfLogicalProcessors=0}} -CapturedUtc $time
Require ($null -eq $number.system.ramBytes -and $null -eq $number.processors[0].cores -and $null -eq $number.processors[0].logicalProcessors) 'Malformed numeric fields inferred.'
$directory=Join-Path $root ('out/reference-evidence/tests/'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
try {
    $image=Join-Path $directory 'fixture.iso'
    [IO.File]::WriteAllBytes($image,[byte[]](1,2,3,4,5))
    $before=(Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
    $one=Write-ReferenceEvidence -Inventory $inventory -ImagePath $image -OutputDirectory (Join-Path $directory 'one')
    $two=Write-ReferenceEvidence -Inventory $inventory -ImagePath $image -OutputDirectory (Join-Path $directory 'two')
    $trial=Get-Content -Raw (Join-Path $one 'trial-template.json') | ConvertFrom-Json
    Require ($trial.inventorySha256 -eq (Get-FileHash (Join-Path $one 'inventory.json') -Algorithm SHA256).Hash.ToLowerInvariant()) 'Inventory byte hash binding failed.'
    Require ($trial.image.sha256 -eq $before.ToLowerInvariant() -and $trial.image.bytes -eq 5) 'ISO binding failed.'
    Require ($trial.boot -eq 'NotRun' -and $trial.nativeInput -eq 'NotRun' -and $trial.safeHalt -eq 'NotRun' -and $trial.qualification -eq 'NotEstablished' -and $trial.secureBootObserved -eq 'Unknown') 'Template invented a trial.'
    Require ($null -eq $trial.trialNumber -and $trial.observationSource -eq 'ManualUnverified') 'Template invented a trial identity.'
    foreach ($file in @('inventory.json','trial-template.json')) { Require ([IO.File]::ReadAllText((Join-Path $one $file)) -ceq [IO.File]::ReadAllText((Join-Path $two $file))) 'Identical inputs changed output.' }
    Require ((Get-FileHash $image -Algorithm SHA256).Hash -eq $before) 'ISO was modified.'
    Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath $image -OutputDirectory $one }
    Require ($trial.inventorySha256 -eq (Get-FileHash (Join-Path $one 'inventory.json') -Algorithm SHA256).Hash.ToLowerInvariant()) 'Earlier record overwritten.'
    $dirty=$inventory | ConvertTo-Json -Depth 12 | ConvertFrom-Json
    $dirty | Add-Member -NotePropertyName SerialNumber -NotePropertyValue $private
    Reject { Write-ReferenceEvidence -Inventory $dirty -ImagePath $image -OutputDirectory (Join-Path $directory 'dirty') }
    Require (-not (Test-Path (Join-Path $directory 'dirty'))) 'Failed validation published output.'
    Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath (Join-Path $directory 'missing.iso') -OutputDirectory (Join-Path $directory 'missing') }
    foreach ($field in @('model','ramBytes')) {
        $dirty=$inventory | ConvertTo-Json -Depth 12 | ConvertFrom-Json
        $dirty.system.$field=[pscustomobject]@{SerialNumber=$private}
        Reject { Write-ReferenceEvidence -Inventory $dirty -ImagePath $image -OutputDirectory (Join-Path $directory 'nested') }
    }
    $dirty=$inventory | ConvertTo-Json -Depth 12 | ConvertFrom-Json
    $dirty.missingEvidence=@([pscustomobject]@{SerialNumber=$private})
    Reject { Write-ReferenceEvidence -Inventory $dirty -ImagePath $image -OutputDirectory (Join-Path $directory 'nested') }
    Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath $image -OutputDirectory (Join-Path $root 'not-ignored-evidence') }
    Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath '//server/share/private.iso' -OutputDirectory (Join-Path $directory 'network') }
    Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath $image -OutputDirectory '//server/share/evidence' }
    $link=Join-Path $directory 'redirect'
    $linkType='SymbolicLink'
    if ($env:OS -eq 'Windows_NT') { $linkType='Junction' }
    try {
        New-Item -ItemType $linkType -Path $link -Target $directory | Out-Null
        Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath $image -OutputDirectory (Join-Path $link 'escaped') }
        Reject { Write-ReferenceEvidence -Inventory $inventory -ImagePath (Join-Path $link 'fixture.iso') -OutputDirectory (Join-Path $directory 'linked-image') }
    } finally { if (Test-Path -LiteralPath $link) { Remove-Item -LiteralPath $link -Force } }
    if ($CollectHost) {
        if ($env:OS -ne 'Windows_NT') { throw 'CollectHost requires Windows.' }
        & (Join-Path $root 'tools/Collect-ReferenceEvidence.ps1') -ImagePath $image -OutputRoot (Join-Path $directory 'host')
        $runs=@(Get-ChildItem -Directory (Join-Path $directory 'host'))
        Require ($runs.Count -eq 1) 'Provider smoke did not publish one run.'
        $real=Get-Content -Raw (Join-Path $runs[0].FullName 'inventory.json') | ConvertFrom-Json
        Require ($real.referenceId -eq 'UNASSIGNED' -and $real.qualification -eq 'Unverified') 'CI host mistaken for reference qualification.'
    } else { Write-Host 'Windows provider execution not exercised by these synthetic fixtures.' }
} finally { Remove-Item -LiteralPath $directory -Recurse -Force }
Write-Host ('PASS: '+$script:checks+' reference evidence projection/privacy/binding checks.')
