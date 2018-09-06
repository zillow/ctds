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

& "$env:PYTHON\Scripts\codecov" --disable gcov -f "coverage.xml" "cobertura.xml"
if ($LastExitCode -ne 0) { exit $LastExitCode }
