# Exercise the actual QEMU checker function with a read-only physical-memory model.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'tools/Test-Qemu.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count -ne 0) { throw 'QEMU checker parse failed.' }
$function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-OwnedLeaf' }, $true)
if (-not $function) { throw 'Missing actual stack walker.' }
Invoke-Expression $function.Extent.Text
$ownedCr3 = [uint64]0x1000
$address = [uint64]0x12345000
$entries = @{}
$slots = @()
$checks = 0
function Read-PhysicalQword([uint64]$Address) {
    if (-not $entries.ContainsKey($Address)) { throw 'Unexpected out-of-model read.' }
    return $entries[$Address]
}
function Reset-Walk {
    $entries.Clear()
    $script:slots = @()
    $level = 1
    foreach ($shift in @(39,30,21,12)) {
        $slot = [uint64](($level * 4096) + ((($address -shr $shift) -band 511) * 8))
        $script:slots += $slot
        $value = [uint64](($level + 1) * 4096 + 3)
        if ($shift -eq 12) { $value = [Convert]::ToUInt64('8000000012345003',16) }
        $entries[$slot] = $value
        $level++
    }
}
Reset-Walk
if ((Get-OwnedLeaf $address) -ne [Convert]::ToUInt64('8000000012345003',16)) { throw 'Valid stack rejected.' }
$checks++
foreach ($level in 0..3) {
    foreach ($mutation in @('ReadOnly','User','Absent')) {
        Reset-Walk
        if ($mutation -eq 'Absent') {
            $entries[$slots[$level]] = [uint64]0
            if ((Get-OwnedLeaf $address) -ne 0) { throw 'Absent translation accepted.' }
        }
        else {
            if ($mutation -eq 'ReadOnly') { $entries[$slots[$level]] = $entries[$slots[$level]] -band [Convert]::ToUInt64('FFFFFFFFFFFFFFFD',16) }
            else { $entries[$slots[$level]] = $entries[$slots[$level]] -bor [uint64]4 }
            $rejected = $false
            try { Get-OwnedLeaf $address | Out-Null }
            catch {
                if ($_.Exception.Message -notmatch 'Read-only kernel stack|User-accessible kernel stack') { throw }
                $rejected = $true
            }
            if (-not $rejected) { throw ('Unsafe translation accepted at level ' + $level) }
        }
        $checks++
    }
}
foreach ($level in @(1,2)) {
    Reset-Walk
    $entries[$slots[$level]] = [Convert]::ToUInt64('8000000000000083',16)
    if ((Get-OwnedLeaf $address) -ne $entries[$slots[$level]]) { throw 'Large leaf rejected.' }
    $checks++
}
Write-Host ('PASS: ' + $checks + ' actual QEMU stack-walker permission/presence fixtures.')
