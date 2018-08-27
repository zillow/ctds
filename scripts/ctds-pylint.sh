#!/bin/sh -e

if [ -z "$VERBOSE" ]; then VERBOSITY="-q"; else VERBOSITY="-v"; fi

pip install \
    --no-cache-dir \
    --disable-pip-version-check \
    $VERBOSITY \
    -e .

pylint setup.py src tests
