#!/bin/bash

if [ -z "$TRAVIS" ]; then
    echo "$0 should only be run in the Travis-ci environment."
    exit 1
fi


if [ "$TRAVIS_OS_NAME" != "osx" ]; then
    bash <(curl -s https://codecov.io/bash)
fi
