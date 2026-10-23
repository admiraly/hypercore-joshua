param([ValidateSet('None','BadEditor','BadFacts','Saturation','UnexpectedTimer','PendingTimer','DrainExhaust')][string]$Fixture='None',[switch]$Features,[string]$OutputDirectory,[string]$QemuPath,[string]$FirmwareDirectory,[ValidateSet('Iso','Fat')][string]$BootMedia='Iso',[string]$CpuModel='qemu64',[ValidateRange(60,300)][int]$TimeoutSeconds=120)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $root $(if ($Fixture -eq 'None') { 'out/console' } else { 'out/console-test-'+$Fixture.ToLowerInvariant() }) }
if (-not $QemuPath) { $QemuPath=Join-Path $root '.tools/qemu/qemu-system-x86_64.exe' }
$QemuPath=(Resolve-Path -LiteralPath $QemuPath).Path
if (-not $FirmwareDirectory) { $FirmwareDirectory=Join-Path (Split-Path $QemuPath -Parent) 'share' }
$pythonName='python3'
if ($env:OS -eq 'Windows_NT') { $pythonName='python' }
$python=Get-Command $pythonName -ErrorAction Stop
$extra=@('--fixture',$Fixture)
if ($Features) { $extra += '--features' }
& $python.Source (Join-Path $root 'tests/test_console_runtime.py') ([IO.Path]::GetFullPath($OutputDirectory)) --qemu $QemuPath --firmware ([IO.Path]::GetFullPath($FirmwareDirectory)) --boot-media $BootMedia --cpu $CpuModel --timeout $TimeoutSeconds @extra
if ($LASTEXITCODE -ne 0) { throw 'Actual console smoke validation failed.' }
