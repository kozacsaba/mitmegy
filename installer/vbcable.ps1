[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Install', 'Uninstall')]
    [string] $Mode,
    [Parameter(Mandatory)]
    [string] $ZipFile,
    [Parameter(Mandatory)]
    [string] $OwnerMarker,
    [string] $RestartMarker
)

$ErrorActionPreference = 'Stop'

# A restart marker applies only to a *new* installation, not an upgrade.
if ($Mode -eq 'Install' -and $RestartMarker) {
    Remove-Item -LiteralPath $RestartMarker -Force -ErrorAction SilentlyContinue
}

function Test-VBCablePresent {
    # Match the driver, not the user-editable name of its playback endpoint.
    $devices = @(Get-PnpDevice -Class MEDIA -PresentOnly -ErrorAction SilentlyContinue)
    return @($devices | Where-Object {
        $_.InstanceId -like '*VBAUDIOVACWDM*' -or
        $_.FriendlyName -like '*VB-Audio Virtual Cable*'
    }).Count -gt 0
}

if ($Mode -eq 'Install' -and (Test-VBCablePresent)) {
    Write-Output 'VB-CABLE is already present; leaving it untouched.'
    exit 0
}

if ($Mode -eq 'Uninstall' -and -not (Test-Path -LiteralPath $OwnerMarker)) {
    Write-Output 'VB-CABLE was not installed by this application; skipping removal.'
    exit 0
}

$extractDir = Join-Path $env:TEMP ([System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $extractDir -Force | Out-Null
try {
    Expand-Archive -LiteralPath $ZipFile -DestinationPath $extractDir
    $setup = Get-ChildItem -LiteralPath $extractDir -Filter 'VBCABLE_Setup_x64.exe' -Recurse -File |
        Select-Object -First 1
    if (-not $setup) { throw 'VBCABLE_Setup_x64.exe is missing from the archived package.' }

    # These switches are used by community integrations; confirm with Pack46 locally.
    $switches = if ($Mode -eq 'Install') { '-i -h' } else { '-u -h' }
    $process = Start-Process -FilePath $setup.FullName -WorkingDirectory $setup.DirectoryName `
        -ArgumentList $switches -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "VB-CABLE setup failed with exit code $($process.ExitCode)."
    }

    if ($Mode -eq 'Install') {
        # Preserve ownership information across app upgrades.
        New-Item -ItemType File -Path $OwnerMarker -Force | Out-Null
        if ($RestartMarker) { New-Item -ItemType File -Path $RestartMarker -Force | Out-Null }
    } else {
        Remove-Item -LiteralPath $OwnerMarker -Force -ErrorAction SilentlyContinue
    }
}
finally {
    Remove-Item -LiteralPath $extractDir -Recurse -Force -ErrorAction SilentlyContinue
}
