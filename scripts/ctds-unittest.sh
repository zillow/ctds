#!/bin/sh -e

if [ -z "$VERBOSE" ]; then VERBOSITY="-q"; else VERBOSITY="-v"; fi

# Install using setuptools directly so the local setup.cfg is used.
CTDS_STRICT=1 python /usr/src/ctds/setup.py $VERBOSITY install

if [ -n "$TEST" ]; then
    ARGS="-s $TEST"
fi

# Install test dependencies using pip.
pip install \
    --no-cache-dir \
    --disable-pip-version-check \
    $VERBOSITY \
    /usr/src/ctds/[tests]

python setup.py test -v $ARGS
