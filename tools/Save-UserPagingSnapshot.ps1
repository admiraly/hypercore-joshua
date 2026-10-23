# Host-only QMP collection. Invoke-Qmp is provided by the validated boot harness.
function Save-UserPagingSnapshot([string]$Directory, [string]$Media, [string]$PythonPath) {
    $seedText = & $PythonPath (Join-Path $PSScriptRoot '../tests/test_user_paging.py') $Directory --boot-media $Media --seed
    if ($LASTEXITCODE -ne 0) { throw 'Cannot derive compiled user metadata addresses.' }
    $seed = ($seedText | Out-String) | ConvertFrom-Json
    $pages = [ordered]@{}
    function Get-SnapshotPage([uint64]$Address) {
        if ($Address -eq 0 -or $Address % 4096 -ne 0 -or $Address -gt [uint64]0x000ffffffffff000) { throw 'Invalid physical snapshot page.' }
        $key = $Address.ToString('x')
        if (-not $pages.Contains($key)) {
            $dump = Invoke-Qmp 'human-monitor-command' @{ 'command-line' = ('xp /512gx 0x{0:x}' -f $Address) }
            $values = @()
            foreach ($line in ($dump -split "`n")) {
                if ($line.Contains(':')) {
                    $tail = $line.Substring($line.IndexOf(':') + 1)
                    foreach ($value in [regex]::Matches($tail, '0x([0-9a-fA-F]{16})')) {
                        $values += [Convert]::ToUInt64($value.Groups[1].Value, 16)
                    }
                }
            }
            if ($values.Count -ne 512) { throw 'QMP physical page dump is incomplete.' }
            $pages[$key] = $values
        }
        return ,$pages[$key]
    }
    function Get-SnapshotQword([uint64]$Address) {
        if ($Address % 8) { throw 'Unaligned metadata qword.' }
        $page = Get-SnapshotPage ($Address - ($Address % 4096))
        return $page[($Address % 4096) / 8]
    }
    function Save-SnapshotRange([uint64]$Address, [uint64]$Bytes) {
        for ($page = $Address - ($Address % 4096); $page -lt $Address + $Bytes; $page += 4096) {
            Get-SnapshotPage $page | Out-Null
        }
    }
    Get-SnapshotPage $seed.kernelRoot | Out-Null
    Save-SnapshotRange $seed.keyboardController 56
    Save-SnapshotRange $seed.keyboardAuthority 48
    Save-SnapshotRange $seed.keyboardDecoder 216
    Save-SnapshotRange $seed.elfPlan 64
    Save-SnapshotRange $seed.context 144
    Save-SnapshotRange $seed.builder 120
    Save-SnapshotRange $seed.processes (2 * 144)
    Save-SnapshotRange $seed.allocator 56
    $recordPointer = Get-SnapshotQword ($seed.allocator + 16)
    $recordCount = Get-SnapshotQword ($seed.allocator + 24)
    if ($recordPointer -ne $seed.records -or $recordCount -eq 0 -or $recordCount -gt 4096) { throw 'Invalid live frame ledger.' }
    Save-SnapshotRange $recordPointer ($recordCount * 24)
    function Save-KernelAliasPath([uint64]$Physical) {
        $walk = [uint64]$seed.kernelRoot
        foreach ($shift in @(39,30,21,12)) {
            $entries = Get-SnapshotPage $walk
            $leaf = $entries[($Physical -shr $shift) -band 511]
            if (($leaf -band 1) -eq 0) { throw 'Owned identity alias absent.' }
            if ($shift -eq 12 -or (($shift -eq 30 -or $shift -eq 21) -and ($leaf -band 128) -ne 0)) { break }
            $walk = $leaf -band [uint64]0x000ffffffffff000
        }
    }
    foreach ($record in 0..($recordCount - 1)) {
        $address = $recordPointer + $record * 24
        if ((Get-SnapshotQword ($address + 16)) -eq 0x50414745) {
            $physical = Get-SnapshotQword $address
            $count = Get-SnapshotQword ($address + 8)
            if ($count -notin @(1,64)) { throw 'Unsupported paging chunk count.' }
            for ($offset = 0; $offset -lt $count; $offset++) { Save-KernelAliasPath ($physical + $offset * 4096) }
        }
    }
    foreach ($root in $seed.roots) {
        $pml4 = Get-SnapshotPage $root
        $entry = $pml4[128]
        foreach ($level in @(1,2,3)) {
            if (($entry -band 1) -eq 0) { throw 'Private user path absent.' }
            $table = Get-SnapshotPage ($entry -band [uint64]0x000ffffffffff000)
            $entry = $table[0]
        }
        # table is the actual private PT. Capture immutable code and mutable data.
        foreach ($slot in @(0,1)) { Get-SnapshotPage ($table[$slot] -band [uint64]0x000ffffffffff000) | Out-Null }
        foreach ($slot in @(0,1,3,4,5,6)) {
            $physical = $table[$slot] -band [uint64]0x000ffffffffff000
            Save-KernelAliasPath $physical
        }
    }
    $snapshot = [ordered]@{ seed = $seed; pages = $pages }
    [IO.File]::WriteAllText((Join-Path $Directory ('qemu-' + $Media + '-user-paging.json')), ($snapshot | ConvertTo-Json -Depth 8), (New-Object Text.UTF8Encoding($false)))
}
