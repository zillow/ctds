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

# Install test dependencies using pip.
& "$env:PYTHON\Scripts\pip" install -v `
        --no-warn-script-location `
        --no-cache-dir `
        --disable-pip-version-check `
    -e .[tests]

& "$env:PYTHON\Scripts\coverage" run --branch --source 'ctds' setup.py test
if ($LastExitCode -ne 0) { exit $LastExitCode }

& "$env:PYTHON\Scripts\coverage" report -m --skip-covered
if ($LastExitCode -ne 0) { exit $LastExitCode }

& "$env:PYTHON\Scripts\coverage" xml
if ($LastExitCode -ne 0) { exit $LastExitCode }
