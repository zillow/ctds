#!/bin/sh -e

# Install using setuptools directly so the local setup.cfg is used.
CTDS_STRICT=1 python setup.py -v install

if [ -n "$TEST" ]; then
    ARGS="-s $TEST"
fi

# Install test dependencies using pip.
pip install --no-cache-dir -v .[tests]

python setup.py test -v $ARGS
