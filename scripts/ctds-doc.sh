#!/bin/sh -e

pip install --no-cache-dir -v -e .

python -m sphinx -n -a -E doc docs
