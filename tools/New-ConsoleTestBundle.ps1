param([string]$InputDirectory,[ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$')][string]$BundleName)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
$outRoot=[IO.Path]::GetFullPath((Join-Path $root 'out'))
$comparison=[StringComparison]::Ordinal
if ($env:OS -eq 'Windows_NT') { $comparison=[StringComparison]::OrdinalIgnoreCase }
function Local-Path([string]$Path,[bool]$UnderOut=$false) {
    if ($Path -match '^[\\/]{2}' -or $Path -match '::') { throw 'Local filesystem paths required.' }
    $full=[IO.Path]::GetFullPath($Path)
    if ($UnderOut -and -not $full.StartsWith($outRoot+[IO.Path]::DirectorySeparatorChar,$comparison)) { throw 'Path must remain below ignored out.' }
    if ($env:OS -eq 'Windows_NT') {
        $drive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot($full))
        if ($drive.DriveType -notin @([IO.DriveType]::Fixed,[IO.DriveType]::Removable)) { throw 'Local drive required.' }
    }
    $chain=@();$cursor=$full
    while ($cursor) {
        $chain+=,$cursor
        $parent=Split-Path $cursor -Parent
        if ($parent -eq $cursor) { break };$cursor=$parent
    }
    [array]::Reverse($chain)
    foreach ($component in $chain) {
        $leaf=[IO.Path]::GetFileName($component)
        if ($leaf -match '[. ]$' -or $leaf -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'Unsupported path alias.' }
        if (Test-Path -LiteralPath $component) {
            $item=Get-Item -LiteralPath $component -Force -ErrorAction Stop
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Redirected paths are unsupported.' }
        }
    }
    return $full
}
function File-Record([string]$Path) {
    $path=Local-Path $Path
    $item=Get-Item -LiteralPath $path -Force -ErrorAction Stop
    if ($item -isnot [IO.FileInfo]) { throw 'Regular file required.' }
    $stream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $size=$stream.Length;$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $stream.Dispose();$sha.Dispose() }
    return [pscustomobject]@{bytes=$size;sha256=$hash}
}
function Keys($Object,[string[]]$Names) {
    if ($null -eq $Object -or ((@($Object.PSObject.Properties.Name|Sort-Object)-join '|') -cne (@($Names|Sort-Object)-join '|'))) { throw 'Unexpected manifest fields.' }
}
function Hash-Leaf($Value) { if ($Value -isnot [string] -or $Value -cnotmatch '^[A-Fa-f0-9]{64}$') { throw 'Invalid hash field.' } }
function Compact-Json([string]$Value) {
    $builder=New-Object Text.StringBuilder;$quoted=$false;$escaped=$false
    foreach ($character in $Value.ToCharArray()) {
        if ($quoted) {
            [void]$builder.Append($character)
            if ($escaped) { $escaped=$false }
            elseif ($character -eq [char]92) { $escaped=$true }
            elseif ($character -eq [char]34) { $quoted=$false }
        } elseif ($character -eq [char]34) { $quoted=$true;[void]$builder.Append($character) }
        elseif ($character -notin @([char]32,[char]9,[char]10,[char]13)) { [void]$builder.Append($character) }
    }
    return $builder.ToString()
}
function Source-Path([string]$Name) {
    if ($Name -notmatch '^(?:boot|kernel|sdk|tools)/[A-Za-z0-9_./-]+$' -or @($Name.Split('/')|Where-Object { $_ -in @('','..','.') }).Count) { throw 'Invalid source path.' }
    $path=Local-Path (Join-Path $root $Name)
    if (-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,$comparison)) { throw 'Source escaped repository.' }
    return $path
}
if (-not $InputDirectory) { $InputDirectory=Join-Path $outRoot 'console' }
$inputPath=Local-Path $InputDirectory $true
if ((Get-Item -LiteralPath $inputPath -Force -ErrorAction Stop) -isnot [IO.DirectoryInfo]) { throw 'Input directory required.' }
if (-not $BundleName) { $BundleName='console-'+[datetime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8) }
if ($BundleName -match '[. ]$' -or $BundleName -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'Unsupported bundle label.' }
$bundleRoot=Local-Path (Join-Path $outRoot 'console-test-bundles') $true
$destination=Local-Path (Join-Path $bundleRoot $BundleName) $true
if (Test-Path -LiteralPath $destination) { throw 'Bundle already exists; preserve prior outputs.' }
$manifestPath=Local-Path (Join-Path $inputPath 'manifest.json') $true
$manifestInfo=File-Record $manifestPath
if ($manifestInfo.bytes -le 0 -or $manifestInfo.bytes -gt 1048576) { throw 'Invalid manifest size.' }
$manifestBytes=[IO.File]::ReadAllBytes($manifestPath)
$utf8=New-Object Text.UTF8Encoding($false,$true)
$manifestText=$utf8.GetString($manifestBytes)
$manifest=$manifestText|ConvertFrom-Json
if ((Compact-Json $manifestText) -cne (Compact-Json ($manifest|ConvertTo-Json -Depth 8))) { throw 'Manifest must use unambiguous Build-shaped JSON.' }
Keys $manifest @('stage','console','consoleTest','faultTest','timerTest','contextTest','userTest','elfTest','keyboardTest','assembler','assemblerSha256','inputs','artifacts')
if ($manifest.console -isnot [bool] -or -not $manifest.console -or $manifest.stage -isnot [string] -or $manifest.stage -cne 'M0/M1 boot prototype - unsigned, no installer') { throw 'Normal Console profile required.' }
foreach ($field in @('consoleTest','faultTest','timerTest','contextTest','userTest','elfTest','keyboardTest')) {
    if ($manifest.$field -isnot [string] -or $manifest.$field -cne 'None') { throw 'Fixture images cannot become normal Console bundles.' }
}
if ($manifest.assembler -isnot [string] -or $manifest.assembler -cnotmatch '^NASM version 2\.16\.03(?: [A-Za-z0-9 .()+_-]{0,120})?$') { throw 'Pinned assembler metadata required.' }
Hash-Leaf $manifest.assemblerSha256
if ($manifest.inputs -isnot [array] -or $manifest.artifacts -isnot [array]) { throw 'Manifest arrays required.' }
$expectedSources=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
$pending=New-Object 'Collections.Generic.Stack[string]'
$pending.Push('boot/uefi.asm')
while ($pending.Count) {
    $name=$pending.Pop()
    if (-not $expectedSources.Add($name)) { continue }
    $source=Source-Path $name
    foreach ($match in [regex]::Matches([IO.File]::ReadAllText($source),"%include\s+'([^']+)'")) { $pending.Push($match.Groups[1].Value) }
}
[void]$expectedSources.Add('tools/Build.ps1');[void]$expectedSources.Add('tools/New-BootImage.ps1')
$provided=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
foreach ($entry in $manifest.inputs) {
    Keys $entry @('path','sha256');Hash-Leaf $entry.sha256
    if ($entry.path -isnot [string] -or -not $expectedSources.Contains($entry.path) -or -not $provided.Add($entry.path)) { throw 'Unknown/duplicate source input.' }
}
if (-not $expectedSources.SetEquals($provided)) { throw 'Incomplete source registry.' }
foreach ($entry in $manifest.inputs) { if ((File-Record (Source-Path $entry.path)).sha256 -ine $entry.sha256) { throw 'Source hash differs from build.' } }
$artifactNames=@('BOOTX64.EFI','joshua-uefi.img','joshua-m0.iso','JOSHUA-Console-Test.iso','boot.lst')
$artifacts=New-Object 'Collections.Generic.Dictionary[string,object]' ([StringComparer]::Ordinal)
foreach ($entry in $manifest.artifacts) {
    Keys $entry @('path','bytes','sha256');Hash-Leaf $entry.sha256
    if ($entry.path -isnot [string] -or $entry.path -cnotin $artifactNames -or $artifacts.ContainsKey($entry.path)) { throw 'Unknown/duplicate artifact.' }
    if (($entry.bytes -isnot [int] -and $entry.bytes -isnot [long]) -or $entry.bytes -le 0 -or $entry.bytes -gt 67108864) { throw 'Invalid artifact size.' }
    $artifacts.Add($entry.path,$entry)
}
if ($artifacts.Count -ne $artifactNames.Count) { throw 'Missing build artifact.' }
foreach ($name in $artifactNames) {
    $actual=File-Record (Local-Path (Join-Path $inputPath $name) $true)
    if ($actual.bytes -ne $artifacts[$name].bytes -or $actual.sha256 -ine $artifacts[$name].sha256) { throw 'Build artifact hash/size mismatch.' }
}
$iso=$artifacts['JOSHUA-Console-Test.iso']
if ($iso.bytes -ne $artifacts['joshua-m0.iso'].bytes -or $iso.sha256 -ine $artifacts['joshua-m0.iso'].sha256) { throw 'Named ISO differs from boot media.' }
& (Join-Path $root 'tests/Test-Pe.ps1') -EfiPath (Join-Path $inputPath 'BOOTX64.EFI')
& (Join-Path $root 'tests/Test-BootImage.ps1') -OutputDirectory $inputPath -EfiPath (Join-Path $inputPath 'BOOTX64.EFI')
$licensePath=Local-Path (Join-Path $root 'LICENSE')
$licenseInfo=File-Record $licensePath
$instructions=@'
JOSHUA / HYPERCORE - UNSIGNED DEVELOPMENT CONSOLE TEST

This is a non-installing BSP diagnostic preview, not a qualified release.
The image contains no filesystem/network/AI runtime or installer. Piper is separate.
Packaging checks bytes and static formats; it does not prove boot or hardware success.

VERIFY: check SHA256SUMS.txt before use (Linux: sha256sum -c SHA256SUMS.txt;
PowerShell: Get-FileHash -Algorithm SHA256 for each named file and compare).
The ZIP checksum is beside the archive; extract first, then verify the payloads.
Hashes identify bytes, not their authenticity. Trust the source/build separately.
The original build manifest describes validated EFI/FAT/listing files that are not
included in this lean ISO bundle. bundle.json lists the five bundled payloads.

QEMU TEST (QEMU and matching OVMF firmware are external prerequisites):
Copy OVMF_VARS.fd to a regular local file named test-vars.fd before the session.
Use UEFI firmware without Secure Boot enforcement for this unsigned VM image.
Example, replacing OVMF_CODE.fd with the matching local firmware path:
qemu-system-x86_64 -machine q35 -accel tcg -cpu qemu64 -smp 2 -m 512 -vga std -nic none -no-reboot -no-shutdown -boot order=d -drive if=pflash,format=raw,unit=0,readonly=on,file=OVMF_CODE.fd -drive if=pflash,format=raw,unit=1,file=test-vars.fd -drive media=cdrom,readonly=on,format=raw,file=JOSHUA-Console-Test.iso
Attach no internal/physical disk. test-vars.fd is a disposable VM file, not host firmware.
The image is read-only CD-ROM media; this bundle runs no command automatically.

After the firmware countdown, the JOSHUA prompt accepts help and about; try status,
mem --json, ps --json and devices --json. Test Left/Right/Home/End/Delete/Backspace,
Up/Down history and Ctrl-C. Lines are bounded to 127 characters. clear resets the
transcript. halt retires keyboard/interrupt ownership, shows HALTED and stops;
close the emulator afterward. A hang, reset or black screen is a failed test.

REFERENCE TRIALS: copy TRIAL-TEMPLATE.json for every actual run. Preserve its
image/manifest binding, record method/tool version, display and observed Secure
Boot state, boot/input/halt outcomes and a redacted photo reference. Retain failed
trials too. Never treat NotRun/Unknown as success. Do not change physical firmware,
enroll keys, install or erase a disk through this workflow. Reference hardware
compatibility and maintainer release decisions remain open. No inventory or logs
are collected/uploaded; redact serials, service tags and private identifiers before sharing.

LICENSE: original JOSHUA source is MIT licensed; see LICENSE.txt. QEMU/OVMF are
not bundled and retain their own licenses. No tools, firmware or voice weights are here.
'@
$payloads=@('JOSHUA-Console-Test.iso','build-manifest.json','LICENSE.txt','TEST-INSTRUCTIONS.txt','TRIAL-TEMPLATE.json')
New-Item -ItemType Directory -Force -Path $bundleRoot|Out-Null
$staging=Join-Path $bundleRoot ('.staging-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging|Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $inputPath 'JOSHUA-Console-Test.iso') -Destination (Join-Path $staging $payloads[0])
    [IO.File]::WriteAllBytes((Join-Path $staging $payloads[1]),$manifestBytes)
    Copy-Item -LiteralPath $licensePath -Destination (Join-Path $staging $payloads[2])
    [IO.File]::WriteAllText((Join-Path $staging $payloads[3]),$instructions.Replace("`r`n","`n")+"`n",$utf8)
    $trial=[ordered]@{schemaVersion=1;referenceId='UNASSIGNED';image=[ordered]@{path=$payloads[0];bytes=$iso.bytes;sha256=$iso.sha256.ToLowerInvariant()};buildManifestSha256=$manifestInfo.sha256;trialNumber=$null;bootMethod='Unknown';bootType='Unknown';displayMode='Unknown';secureBootObserved='Unknown';boot='NotRun';nativeInput='NotRun';safeHalt='NotRun';photoEvidence='NotProvided';qualification='NotEstablished'}
    [IO.File]::WriteAllText((Join-Path $staging $payloads[4]),($trial|ConvertTo-Json -Depth 6).Replace("`r`n","`n")+"`n",$utf8)
    foreach ($pair in @(@($payloads[0],$iso),@($payloads[1],$manifestInfo),@($payloads[2],$licenseInfo))) {
        $copied=File-Record (Join-Path $staging $pair[0])
        if ($copied.sha256 -ine $pair[1].sha256 -or $copied.bytes -ne $pair[1].bytes) { throw 'Copied payload differs from preflight.' }
    }
    $files=@($payloads|ForEach-Object { $record=File-Record (Join-Path $staging $_);[ordered]@{path=$_;bytes=$record.bytes;sha256=$record.sha256} })
    $bundle=[ordered]@{schemaVersion=1;profile='Console';fixture='None';qualification='NotEstablished';files=$files}
    [IO.File]::WriteAllText((Join-Path $staging 'bundle.json'),($bundle|ConvertTo-Json -Depth 6).Replace("`r`n","`n")+"`n",$utf8)
    $sumNames=$payloads+@('bundle.json')
    $sums=@($sumNames|ForEach-Object { (File-Record (Join-Path $staging $_)).sha256+'  '+$_ })
    [IO.File]::WriteAllText((Join-Path $staging 'SHA256SUMS.txt'),($sums-join "`n")+"`n",$utf8)
    $zipNames=$sumNames+@('SHA256SUMS.txt')
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zipPath=Join-Path $staging 'JOSHUA-Console-Test.zip'
    $zipStream=$null;$archive=$null
    try {
        $zipStream=[IO.File]::Open($zipPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
        $archive=New-Object IO.Compression.ZipArchive($zipStream,[IO.Compression.ZipArchiveMode]::Create,$false)
        foreach ($name in $zipNames) {
            $entry=$archive.CreateEntry($name,[IO.Compression.CompressionLevel]::NoCompression)
            $entry.LastWriteTime=[DateTimeOffset]::Parse('1980-01-01T00:00:00+00:00',[Globalization.CultureInfo]::InvariantCulture)
            $entry.ExternalAttributes=0
            $from=$null;$to=$null
            try { $from=[IO.File]::OpenRead((Join-Path $staging $name));$to=$entry.Open();$from.CopyTo($to) }
            finally { if ($from) { $from.Dispose() };if ($to) { $to.Dispose() } }
        }
    } finally {
        try { if ($archive) { $archive.Dispose() } }
        finally { if ($zipStream) { $zipStream.Dispose() } }
    }
    $archive=[IO.Compression.ZipFile]::OpenRead($zipPath)
    try {
        if (($archive.Entries.FullName-join '|') -cne ($zipNames-join '|')) { throw 'Unexpected ZIP membership/order.' }
        foreach ($entry in $archive.Entries) {
            $expected=File-Record (Join-Path $staging $entry.FullName)
            $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
            try { $hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
            finally { $stream.Dispose();$sha.Dispose() }
            if ($entry.Length -ne $expected.bytes -or $hash -cne $expected.sha256 -or $entry.ExternalAttributes -ne 0 -or $entry.LastWriteTime.DateTime -ne [datetime]'1980-01-01T00:00:00') { throw 'ZIP readback differs from payload.' }
        }
    } finally { $archive.Dispose() }
    [IO.File]::WriteAllText(($zipPath+'.sha256'),(File-Record $zipPath).sha256+'  JOSHUA-Console-Test.zip'+"`n",$utf8)
    [IO.Directory]::Move($staging,$destination)
} finally { if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force } }
Write-Host ('Created unqualified Console test bundle: '+$destination)
