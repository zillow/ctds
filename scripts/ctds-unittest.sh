#!/bin/sh -e

pip install --no-cache-dir -v -e . -e .[tests]

if [ -n "$TEST" ]; then
    ARGS="-s $TEST"
fi
python setup.py test $ARGS
