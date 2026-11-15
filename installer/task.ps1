[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Install', 'Uninstall')]
    [string] $Mode,
    [string] $Processor
)

$ErrorActionPreference = 'Stop'
$taskName = 'WormHole Processor'

if ($Mode -eq 'Uninstall') {
    $existing = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if ($null -ne $existing) {
        Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
    }
    exit 0
}

if (-not (Test-Path -LiteralPath $Processor -PathType Leaf)) {
    throw "Processor executable not found: $Processor"
}

$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$action = New-ScheduledTaskAction -Execute $Processor -WorkingDirectory (Split-Path -Parent $Processor)
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet `
    -RestartCount 3 `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -MultipleInstances IgnoreNew `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries

Register-ScheduledTask `
    -TaskName $taskName `
    -Description 'WormHole background audio processor' `
    -Action $action `
    -Trigger $trigger `
    -Principal $principal `
    -Settings $settings `
    -Force | Out-Null
