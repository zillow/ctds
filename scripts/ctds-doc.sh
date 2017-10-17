#!/bin/sh -e

OUTPUTDIR=${1:-docs}

pip install --no-cache-dir -v -e .

python -m sphinx -n -a -E -W doc "$OUTPUTDIR"
