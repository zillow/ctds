Getting Started
===============

Installing
----------

*cTDS* requires `FreeTDS <http://www.freetds.org>`_. Additionally the development
headers for the version of *Python* you wish to compile against are also required.

Debian-based Systems
^^^^^^^^^^^^^^^^^^^^

Both **FreeTDS** and the **Python** development headers can easily be installed using
the system pacakge manager on Debian-based systems, such as Ubuntu.

.. code-block:: bash

    sudo apt-get install freetds-dev python-dev


However, it is recommended to use the latest stable version of
`FreeTDS <http://www.freetds.org>`_, if possible. It can be easily built from
the latest stable source into a
`virutualenv <http://virtualenv.readthedocs.org/en/latest/userguide.html>`_
using the following:

.. code-block:: bash

    virtualenv ctds-venv && cd ctds-venv
    wget 'ftp://ftp.freetds.org/pub/freetds/stable/freetds-patched.tar.gz'
    tar -xzf freetds-patched.tar.gz
    pushd freetds-*
    ./configure --prefix "$(dirname $(pwd))" && make && make install
    popd


cTDS
^^^^

*cTDS* can be installed easily using `pip <https://pip.pypa.io/en/stable/>`_.

.. code-block:: bash

    pip install ctds

If using a non-system version of `FreeTDS <http://www.freetds.org>`_, use the
following to specify which `include` and `library` directories to compile and
link *cTDS* against.

.. code-block:: bash

    # Assuming . is the root of the virtualenv.
    # Note: In order to load the locally built version of the
    # FreeTDS libraries either the working directory must be
    # the same as when ctds was installed or LD_LIBRARY_PATH
    # must be set correctly.
    pip install --global-option=build_ext \
        --global-option="--include-dirs=$(pwd)/include" \
        --global-option=build_ext \
        --global-option="--library-dirs=$(pwd)/lib" \
        --global-option=build_ext --global-option="--rpath=./lib" \
        ctds
