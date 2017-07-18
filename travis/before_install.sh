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
else
    # Travis-ci doesn't support Python on OS X. Install it manually for now.
    brew update
    brew install freetds
    brew install python3
    virtualenv venv -p python3
    source venv/bin/activate
fi
