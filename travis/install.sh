#!/bin/sh -e

if [ -z "$TRAVIS" ]; then
    echo "$0 should only be run in the Travis-ci environment."
    exit 1
fi

# Travis-ci doesn't support docker support on OS X. Just verify ctds builds on OS X.
if [ "$TRAVIS_OS_NAME" = "osx" ]; then
    pip install .
fi
