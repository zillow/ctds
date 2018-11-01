#!/bin/sh -e

if [ -z "$TRAVIS" ]; then
    echo "$0 should only be run in the Travis-ci environment."
    exit 1
fi

if [ "$TRAVIS_OS_NAME" != "osx" ]; then
    # Update to the latest version of docker.
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
    sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
    sudo apt-get update
    sudo apt-get -y install docker-ce

    # Launch SQL Server
    make start-sqlserver
else
    # Upgrade Travis CI's Python2.7 to Python3.
    python3 -m venv ctds-venv
    source ctds-venv/bin/activate
fi
