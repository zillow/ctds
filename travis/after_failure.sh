#!/bin/sh -e

if [ -z "$TRAVIS" ]; then
    echo "$0 should only be run in the Travis-ci environment."
    exit 1
fi

# On failure, try dumping the SQL server to aid in debugging.
if [ "$TRAVIS_OS_NAME" != "osx" ]; then
    docker logs ctds-unittest-sqlserver
fi
