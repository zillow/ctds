#!/bin/sh -e

if [ -e '/usr/local/bin/python3' ]; then
    PYTHON=python3
    PIP=pip3
else
    PYTHON=python
    PIP=pip
fi

/usr/local/bin/$PYTHON -m ensurepip

# Install using setuptools directly so the local setup.cfg is used.
CTDS_STRICT=1 /usr/local/bin/$PYTHON setup.py -v install

if [ -n "$TEST" ]; then
    ARGS="-s $TEST"
fi

# Install test dependencies using pip.
/usr/local/bin/$PIP install --no-cache-dir -v .[tests]

valgrind \
    --tool=memcheck \
    --vgdb=no \
    --child-silent-after-fork=yes \
    --track-origins=yes \
    --leak-check=full \
    --show-leak-kinds=definite,indirect \
    --errors-for-leak-kinds=definite,indirect \
    --trace-children=yes \
    --error-exitcode=1 \
    --suppressions=$(dirname $(readlink -f "$0"))/../misc/valgrind-python.supp \
    /usr/local/bin/$PYTHON setup.py test -v $ARGS
