$ErrorActionPreference = "Stop"

if ($env:APPVEYOR -ne "True")
{
    Write-Output "$PSCommandPath should only be run in the Appveyor-ci environment."
    exit 1
}

if (-not (Test-Path env:PYTHON))
{
    Write-Output "PYTHON environment variable not set."
    exit 1
}

# Set include and lib paths.
$env:CTDS_INCLUDE_DIRS = "$env:BUILD_INSTALL_PREFIX\include"
$env:CTDS_LIBRARY_DIRS= "$env:BUILD_INSTALL_PREFIX\lib"

$env:CTDS_STRICT = 1
$env:CTDS_COVER = 1

& "$PSScriptRoot\build.cmd" "$env:PYTHON\python.exe" setup.py install

if ($LastExitCode -ne 0) { exit $LastExitCode }
