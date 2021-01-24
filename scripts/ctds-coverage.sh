#!/bin/sh -e

PYTHON_VERSION=$(python -c 'import sys; print(".".join(map(str, sys.version_info)))')
FREETDS_VERSION=$(python -c 'import ctds; print(ctds.freetds_version.replace(" ", "-"))')
COVERAGEDIR="build/coverage/python-$PYTHON_VERSION/$FREETDS_VERSION"
mkdir -p $COVERAGEDIR

pytest \
        --cov=src \
        --no-cov-on-fail \
        --cov-fail-under=100 \
        --cov-branch \
        --cov-report=xml:$COVERAGEDIR/coverage.xml \
        --cov-report=term-missing \
    tests/ "$@"

coverage erase

# There should be one build directory of object files,
# e.g. build/temp.linux-x86_64-3.6/src/ctds
for OBJDIR in build/*/src/ctds; do :; done;

# Outputs data to "<source-file>.gcov"
gcov -b -o $OBJDIR src/ctds/*.c
mv *.gcov $COVERAGEDIR
