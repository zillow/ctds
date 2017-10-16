$ErrorActionPreference = "Stop"

if ($env:APPVEYOR -ne "True")
{
    Write-Output "$PSCommandPath should only be run in the Appveyor-ci environment."
    exit 1
}

if (-not (Test-Path -Path $env:BUILD_INSTALL_PREFIX))
{
    # Install freetds (and dependencies).
    Add-AppveyorMessage -Message "Building FreeTDS ..." -Category Information
    & "$PSScriptRoot\..\windows\freetds-install.ps1"

    if ($LastExitCode -ne 0) { exit $LastExitCode }

    Add-AppveyorMessage -Message "FreeTDS build completed." -Category Information

    # Allow FreeTDS to be cached even on failure.
    $env:APPVEYOR_SAVE_CACHE_ON_ERROR = "True"
}
else
{
    Add-AppveyorMessage -Message "Using cached version of FreeTDS." -Category Information
}

& "$env:PYTHON\Scripts\pip" install check-manifest codecov coverage

if ($LastExitCode -ne 0) { exit $LastExitCode }
