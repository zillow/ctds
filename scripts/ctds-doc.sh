#!/bin/sh -e

if [ -z "$VERBOSE" ]; then VERBOSITY="-q"; else VERBOSITY="-v"; fi

OUTPUTDIR=${1:-docs}

pip install \
    --no-cache-dir \
    --disable-pip-version-check \
    $VERBOSITY \
    -e .

python -m sphinx -n -a -E -W doc "$OUTPUTDIR"
