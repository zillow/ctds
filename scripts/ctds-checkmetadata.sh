#!/bin/sh -e

# Python 3.3's check implementation fails incorrectly when validating rst.
if [ "False" = `python -c 'import sys; sys.stdout.write(str(sys.version_info[:2] == (3, 3)))'` ]; then
    ARGS=--restructuredtext
fi
python setup.py check -v --strict --metadata $ARGS

check-manifest -v
