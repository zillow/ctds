#!/bin/sh -e

if [ -n "$VERBOSE" ]; then VERBOSITY="-v"; fi

check-manifest $VERBOSITY

python setup.py sdist

# Run twine to validate setup.py metadata. Fail unless passed without warnings.
twine check dist/* | tee /dev/stderr | grep -E '^Checking (distribution )?dist/ctds-[[:digit:]]+\.[[:digit:]]+\.[[:digit:]]+\.tar\.gz: P(ASSED|assed)$'
