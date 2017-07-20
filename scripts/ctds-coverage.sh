#!/bin/sh -e

pip install --no-cache-dir -v -e . -e .[tests]

mkdir coverage

# Outputs data to ".coverage"
coverage run --branch --source 'ctds' setup.py test
cp .coverage coverage

# There should be one build directory of object files,
# e.g. build/temp.linux-x86_64-3.6/ctds
for OBJDIR in build/*/ctds; do :; done;

# Outputs data to "<source-file>.gcov"
gcov -o $OBJDIR ctds/*.c
cp *.gcov coverage
