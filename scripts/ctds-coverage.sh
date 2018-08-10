#!/bin/sh -e

if [ -z "$VERBOSE" ]; then VERBOSITY="-q"; else VERBOSITY="-v"; fi

# Install using setuptools directly so the local setup.cfg is used.
CTDS_STRICT=1 python setup.py $VERBOSITY install

mkdir coverage

# Install test dependencies using pip.
pip install \
    --no-cache-dir \
    --disable-pip-version-check \
    $VERBOSITY \
    /usr/src/ctds/[tests]

# Outputs data to ".coverage"
coverage run --branch --source 'ctds' setup.py test
coverage report -m --skip-covered

coverage xml
cp coverage.xml coverage

# There should be one build directory of object files,
# e.g. build/temp.linux-x86_64-3.6/src/ctds
for OBJDIR in build/*/src/ctds; do :; done;

# Outputs data to "<source-file>.gcov"
gcov -b -o $OBJDIR src/ctds/*.c
cp *.gcov coverage
