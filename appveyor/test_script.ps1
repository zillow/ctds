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

& "$env:PYTHON\Scripts\check-manifest" -v
if ($LastExitCode -ne 0) { exit $LastExitCode }

# Add FreeTDS library dir to PATH so shared libs will be found.
$env:PATH += ";$env:BUILD_INSTALL_PREFIX\lib"

# The computer's hostname is returned in messages from SQL Server.
$env:HOSTNAME = "$env:COMPUTERNAME"

& "$env:PYTHON\python.exe" -c 'import ctds; print(ctds.freetds_version)'

& "$env:ProgramFiles\OpenCppCoverage\OpenCppCoverage.exe" `
    --export_type=cobertura:cobertura.xml --optimized_build `
    --sources "$env:APPVEYOR_BUILD_FOLDER\src" `
    --modules "$env:APPVEYOR_BUILD_FOLDER" -- `
    "$env:PYTHON\python.exe" -m coverage run --branch --source 'ctds' setup.py test
if ($LastExitCode -ne 0) { exit $LastExitCode }

& "$env:PYTHON\Scripts\coverage" report -m --skip-covered
if ($LastExitCode -ne 0) { exit $LastExitCode }

& "$env:PYTHON\Scripts\coverage" xml
if ($LastExitCode -ne 0) { exit $LastExitCode }
