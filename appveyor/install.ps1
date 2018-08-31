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
    Set-AppveyorBuildVariable 'APPVEYOR_SAVE_CACHE_ON_ERROR' 'True'
}
else
{
    Add-AppveyorMessage -Message "Using cached version of FreeTDS." -Category Information
}

# Download OpenCppCoverage to the cached dir.
$OpenCppCoverageInstaller = "$env:BUILD_INSTALL_PREFIX\OpenCppCoverageSetup.exe"
if (-not (Test-Path -Path $OpenCppCoverageInstaller))
{
    $Url = "https://github.com/OpenCppCoverage/OpenCppCoverage/releases/download/release-0.9.6.1/OpenCppCoverageSetup-x64-0.9.6.1.exe"
    Add-AppveyorMessage -Message "Downloading OpenCppCodeCoverage installer ..." -Category Information
    (New-Object System.Net.WebClient).DownloadFile($Url, $OpenCppCoverageInstaller)
}
else
{
    Add-AppveyorMessage -Message "Using cached version of OpenCppCoverage installer ..." -Category Information
}

# Run the OpenCppCodeCoverage installer.
Add-AppveyorMessage -Message "Installing OpenCppCodeCoverage ..." -Category Information
& "$OpenCppCoverageInstaller" /SILENT
if ($LastExitCode -ne 0) { exit $LastExitCode }

# Upgrade pip.
& "$env:PYTHON\python.exe" -m pip install `
        --no-warn-script-location `
        --no-cache-dir `
        --disable-pip-version-check `
        --upgrade `
    pip
if ($LastExitCode -ne 0) { exit $LastExitCode }

# Install testing dependencies.
& "$env:PYTHON\Scripts\pip" install `
        --no-warn-script-location `
        --no-cache-dir `
        --disable-pip-version-check `
    check-manifest codecov coverage
if ($LastExitCode -ne 0) { exit $LastExitCode }
