param([string]$InputDirectory,[string]$NasmPath)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
$id=[guid]::NewGuid().ToString('N')
$work=Join-Path $root ('out/console-bundle-tests/'+$id)
New-Item -ItemType Directory -Path $work -Force|Out-Null
$script:checks=0;$script:labels=@();$script:attempt=0
function Require([bool]$Value,[string]$Message) { $script:checks++;if (-not $Value) { throw $Message } }
function Run-Bundle([string]$Name,[string]$InputPath=$fixture) {
    & (Join-Path $root 'tools/New-ConsoleTestBundle.ps1') -InputDirectory $InputPath -BundleName $Name
}
function Reject([scriptblock]$Action) { $rejected=$false;try { &$Action|Out-Null }catch{$rejected=$true};Require $rejected 'Invalid bundle accepted.' }
function Mutate-Manifest([scriptblock]$Change) {
    $m=$originalText|ConvertFrom-Json
    &$Change $m
    [IO.File]::WriteAllText((Join-Path $fixture 'manifest.json'),($m|ConvertTo-Json -Depth 8),$utf8)
    $script:attempt++
    $name='test-'+$id+'-bad'+$script:attempt
    try { Reject { Run-Bundle $name };Require (-not(Test-Path (Join-Path $bundleRoot $name))) 'Failure published a bundle.' }
    finally { [IO.File]::WriteAllBytes((Join-Path $fixture 'manifest.json'),$originalManifest) }
}
$bundleRoot=Join-Path $root 'out/console-test-bundles'
$utf8=New-Object Text.UTF8Encoding($false)
try {
    if (-not $InputDirectory) {
        $InputDirectory=Join-Path $work 'source'
        $opts=@{};if($NasmPath){$opts.NasmPath=$NasmPath}
        & (Join-Path $root 'tools/Build.ps1') @opts -Console -OutputDirectory $InputDirectory
    }
    $InputDirectory=[IO.Path]::GetFullPath($InputDirectory)
    $fixture=Join-Path $work 'fixture'
    New-Item -ItemType Directory -Path $fixture|Out-Null
    $names=@('BOOTX64.EFI','joshua-uefi.img','joshua-m0.iso','JOSHUA-Console-Test.iso','boot.lst','manifest.json')
    $before=@{}
    foreach($name in $names) {
        $before[$name]=(Get-FileHash -LiteralPath (Join-Path $InputDirectory $name) -Algorithm SHA256).Hash
        Copy-Item -LiteralPath (Join-Path $InputDirectory $name) -Destination (Join-Path $fixture $name)
    }
    $originalManifest=[IO.File]::ReadAllBytes((Join-Path $fixture 'manifest.json'))
    $originalText=$utf8.GetString($originalManifest)
    $manifest=$originalText|ConvertFrom-Json
    foreach($suffix in @('one','two')) {
        $name='test-'+$id+'-'+$suffix;$script:labels+=$name
        Run-Bundle $name
    }
    $one=Join-Path $bundleRoot $script:labels[0];$two=Join-Path $bundleRoot $script:labels[1]
    $payloads=@('JOSHUA-Console-Test.iso','build-manifest.json','LICENSE.txt','TEST-INSTRUCTIONS.txt','TRIAL-TEMPLATE.json','bundle.json','SHA256SUMS.txt')
    $all=$payloads+@('JOSHUA-Console-Test.zip','JOSHUA-Console-Test.zip.sha256')
    Require ((@(Get-ChildItem -File $one|Select-Object -ExpandProperty Name|Sort-Object)-join '|') -ceq (@($all|Sort-Object)-join '|')) 'Unexpected final files.'
    foreach($name in $all) {
        Require ((Get-FileHash (Join-Path $one $name)).Hash -eq (Get-FileHash (Join-Path $two $name)).Hash) 'Identical inputs changed local bundle bytes.'
    }
    Require ((Get-FileHash (Join-Path $one $payloads[0])).Hash -eq $before['JOSHUA-Console-Test.iso']) 'Packaged ISO differs.'
    Require ((Get-FileHash (Join-Path $one $payloads[1])).Hash -eq $before['manifest.json']) 'Manifest was rewritten.'
    Require ((Get-FileHash (Join-Path $one 'LICENSE.txt')).Hash -eq (Get-FileHash (Join-Path $root 'LICENSE')).Hash) 'License differs.'
    $trial=Get-Content -Raw (Join-Path $one 'TRIAL-TEMPLATE.json')|ConvertFrom-Json
    Require ($trial.image.sha256 -eq $before['JOSHUA-Console-Test.iso'].ToLowerInvariant() -and $trial.buildManifestSha256 -eq $before['manifest.json'].ToLowerInvariant()) 'Trial bindings differ.'
    Require ($trial.boot -eq 'NotRun' -and $trial.nativeInput -eq 'NotRun' -and $trial.safeHalt -eq 'NotRun' -and $trial.qualification -eq 'NotEstablished' -and $trial.referenceId -eq 'UNASSIGNED' -and $null -eq $trial.trialNumber) 'Trial invented success.'
    Require ($trial.bootMethod -eq 'Unknown' -and $trial.bootType -eq 'Unknown' -and $trial.displayMode -eq 'Unknown' -and $trial.secureBootObserved -eq 'Unknown' -and $trial.photoEvidence -eq 'NotProvided') 'Trial invented observations.'
    $bundle=Get-Content -Raw (Join-Path $one 'bundle.json')|ConvertFrom-Json
    Require ($bundle.schemaVersion -eq 1 -and $bundle.profile -eq 'Console' -and $bundle.fixture -eq 'None' -and $bundle.qualification -eq 'NotEstablished' -and $bundle.files.Count -eq 5) 'Bundle claims wrong scope.'
    foreach($entry in $bundle.files) {
        Require ((Get-FileHash (Join-Path $one $entry.path)).Hash -eq $entry.sha256 -and (Get-Item (Join-Path $one $entry.path)).Length -eq $entry.bytes) 'Bundled metadata differs.'
    }
    foreach($line in Get-Content (Join-Path $one 'SHA256SUMS.txt')) {
        Require ($line -cmatch '^([0-9a-f]{64})  ([A-Za-z0-9.-]+)$') 'Malformed checksum line.'
        Require ((Get-FileHash (Join-Path $one $Matches[2])).Hash -eq $Matches[1]) 'Payload checksum differs.'
    }
    Require ((Get-Content -Raw (Join-Path $one 'JOSHUA-Console-Test.zip.sha256')).Trim() -ceq ((Get-FileHash (Join-Path $one 'JOSHUA-Console-Test.zip')).Hash.ToLowerInvariant()+'  JOSHUA-Console-Test.zip')) 'ZIP sidecar differs.'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $one 'JOSHUA-Console-Test.zip'))
    try {
        Require (($archive.Entries.FullName-join '|') -ceq ($payloads-join '|')) 'ZIP membership/order differs.'
        $extract=Join-Path $work 'extracted';New-Item -ItemType Directory -Path $extract|Out-Null
        foreach($entry in $archive.Entries) {
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $extract $entry.FullName))
            Require ((Get-FileHash (Join-Path $extract $entry.FullName)).Hash -eq (Get-FileHash (Join-Path $one $entry.FullName)).Hash) 'Extracted content differs.'
            Require ($entry.ExternalAttributes -eq 0 -and $entry.LastWriteTime.DateTime -eq [datetime]'1980-01-01T00:00:00') 'ZIP metadata differs.'
        }
    }finally{$archive.Dispose()}
    $zipBefore=(Get-FileHash (Join-Path $one 'JOSHUA-Console-Test.zip')).Hash
    Reject { Run-Bundle $script:labels[0] }
    Require ((Get-FileHash (Join-Path $one 'JOSHUA-Console-Test.zip')).Hash -eq $zipBefore) 'Prior bundle overwritten.'
    foreach($field in @('consoleTest','faultTest','timerTest','contextTest','userTest','elfTest','keyboardTest')) {
        Mutate-Manifest {param($m) $m.$field='Fixture'}
    }
    Mutate-Manifest {param($m) $m.stage=@()}
    Mutate-Manifest {param($m) $m.stage=@('M0/M1 boot prototype - unsigned, no installer')}
    Mutate-Manifest {param($m) $m.stage='Qualified release'}
    Mutate-Manifest {param($m) $m.assembler='NASM version 9.9.9'}
    Mutate-Manifest {param($m) $m.assemblerSha256='not-a-hash'}
    Mutate-Manifest {param($m) $m.console=$false}
    Mutate-Manifest {param($m) $m.console='true'}
    Mutate-Manifest {param($m) $m.console=[pscustomobject]@{Private='SECRET'}}
    Mutate-Manifest {param($m) $m.inputs[0].sha256='0'*64}
    Mutate-Manifest {param($m) $m.inputs[0].path='../private'}
    Mutate-Manifest {param($m) $m.inputs=@($m.inputs)+@($m.inputs[0])}
    Mutate-Manifest {param($m) $m.inputs=@($m.inputs|Select-Object -Skip 1)}
    Mutate-Manifest {param($m) $m.inputs[0].sha256=[pscustomobject]@{SerialNumber='SECRET'}}
    Mutate-Manifest {param($m) $m.artifacts=@($m.artifacts)+@($m.artifacts[0])}
    Mutate-Manifest {param($m) $m.artifacts[0].path='../private.iso'}
    Mutate-Manifest {param($m) $m.artifacts[0].bytes=0}
    Mutate-Manifest {param($m) $m.artifacts[0].bytes='86016'}
    Mutate-Manifest {param($m) $m.artifacts=@($m.artifacts|Select-Object -Skip 1)}
    Mutate-Manifest {param($m) $m|Add-Member -NotePropertyName unexpected -NotePropertyValue 'SECRET'}
    $duplicate=[regex]::Replace($originalText,'"console"\s*:\s*true\s*,','"console": false, "console": true,')
    Require ($duplicate -cne $originalText) 'Duplicate JSON mutation missed actual syntax.'
    [IO.File]::WriteAllText((Join-Path $fixture 'manifest.json'),$duplicate,$utf8)
    try {Reject {Run-Bundle ('test-'+$id+'-duplicate')}}finally{[IO.File]::WriteAllBytes((Join-Path $fixture 'manifest.json'),$originalManifest)}
    foreach($name in $names|Where-Object {$_ -ne 'manifest.json'}) {
        $path=Join-Path $fixture $name;$backup=$path+'.saved'
        Move-Item -LiteralPath $path -Destination $backup
        try {Reject {Run-Bundle ('test-'+$id+'-missing')}}finally{Move-Item -LiteralPath $backup -Destination $path}
        $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        try {$first=$stream.ReadByte();$stream.Position=0;$stream.WriteByte($first -bxor 1)}finally{$stream.Dispose()}
        try {Reject {Run-Bundle ('test-'+$id+'-tampered')}}finally{Copy-Item -LiteralPath (Join-Path $InputDirectory $name) -Destination $path -Force}
    }
    # Forge the corresponding declared hash too: the format validator must still reject.
    $efi=Join-Path $fixture 'BOOTX64.EFI';$bytes=[IO.File]::ReadAllBytes($efi);$bytes[0]=0;[IO.File]::WriteAllBytes($efi,$bytes)
    try {Mutate-Manifest {param($m) $m.artifacts[0].sha256=(Get-FileHash $efi).Hash}}finally{Copy-Item -LiteralPath (Join-Path $InputDirectory 'BOOTX64.EFI') -Destination $efi -Force}
    foreach($name in @('joshua-uefi.img','joshua-m0.iso')) {
        $path=Join-Path $fixture $name;$bytes=[IO.File]::ReadAllBytes($path)
        if($name -eq 'joshua-uefi.img'){$bytes[510]=0}else{$bytes[32769]=0}
        [IO.File]::WriteAllBytes($path,$bytes)
        if($name -eq 'joshua-m0.iso'){[IO.File]::WriteAllBytes((Join-Path $fixture 'JOSHUA-Console-Test.iso'),$bytes)}
        try {
            Mutate-Manifest {param($m)
                foreach($a in $m.artifacts){if($a.path -eq $name -or ($name -eq 'joshua-m0.iso' -and $a.path -eq 'JOSHUA-Console-Test.iso')){$a.sha256=(Get-FileHash (Join-Path $fixture $a.path)).Hash}}
            }
        }finally{
            Copy-Item -LiteralPath (Join-Path $InputDirectory $name) -Destination $path -Force
            if($name -eq 'joshua-m0.iso'){Copy-Item -LiteralPath (Join-Path $InputDirectory 'JOSHUA-Console-Test.iso') -Destination (Join-Path $fixture 'JOSHUA-Console-Test.iso') -Force}
        }
    }
    foreach($name in @('CON','NUL.txt','LPT1','trailing.','../escape')) {Reject {Run-Bundle $name}}
    Reject {Run-Bundle ('test-'+$id+'-network') '//server/share/private'}
    Reject {Run-Bundle ('test-'+$id+'-outside') $root}
    $link=Join-Path $work 'redirect';$linkType='SymbolicLink';if($env:OS -eq 'Windows_NT'){$linkType='Junction'}
    New-Item -ItemType $linkType -Path $link -Target $fixture|Out-Null
    try{Reject {Run-Bundle ('test-'+$id+'-redirect') $link}}finally{
        # Windows PowerShell 5.1 Remove-Item can fail on directory junctions.
        # Remove only the link entry, never recursively traverse the fixture.
        if($env:OS -eq 'Windows_NT'){[IO.Directory]::Delete($link)}else{Remove-Item -LiteralPath $link -Force}
    }
    Require (-not(Test-Path -LiteralPath $link)) 'Redirect fixture was not removed.'
    Require ((Test-Path -LiteralPath $fixture) -and (Get-FileHash (Join-Path $fixture 'BOOTX64.EFI')).Hash -eq $before['BOOTX64.EFI']) 'Redirect cleanup changed its target.'
    if($env:OS -ne 'Windows_NT') {
        $path=Join-Path $fixture 'BOOTX64.EFI';Move-Item -LiteralPath $path -Destination ($path+'.saved')
        New-Item -ItemType SymbolicLink -Path $path -Target ($path+'.saved')|Out-Null
        try{Reject {Run-Bundle ('test-'+$id+'-file-redirect')}}finally{Remove-Item -LiteralPath $path;Move-Item -LiteralPath ($path+'.saved') -Destination $path}
    }else{Write-Host 'Windows leaf-file symlink execution is not covered; parent junction rejection is exercised.'}
    foreach($name in $names) {Require ((Get-FileHash (Join-Path $InputDirectory $name)).Hash -eq $before[$name]) 'Source artifacts modified.'}
    Require (@(Get-ChildItem -Directory $bundleRoot -Filter '.staging-*').Count -eq 0) 'Staging residue remains.'
    Write-Host ('PASS: '+$script:checks+' Console bundle/profile/hash/archive/rejection checks; packaging is not a boot trial.')
}finally{
    foreach($name in $script:labels) { $path=Join-Path $bundleRoot $name;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Recurse -Force} }
    if(Test-Path -LiteralPath $work){Remove-Item -LiteralPath $work -Recurse -Force}
}
