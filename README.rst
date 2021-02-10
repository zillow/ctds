cTDS
====

.. include-documentation-begin-marker

.. image:: https://github.com/zillow/ctds/workflows/CI/CD/badge.svg?branch=master
        :target: https://github.com/zillow/ctds/actions

.. image:: https://ci.appveyor.com/api/projects/status/voa33r7qdnxh6wwp/branch/master?svg=true
        :target: https://ci.appveyor.com/project/joshuahlang/ctds/branch/master

.. image:: http://img.shields.io/pypi/v/ctds.svg
        :target: https://pypi.python.org/pypi/ctds/

.. image:: https://codecov.io/gh/zillow/ctds/branch/master/graph/badge.svg
        :target: https://codecov.io/gh/zillow/ctds


`cTDS` is a full Python `DB API-2.0`_-compliant
SQL Server database library for `Linux`, `Windows`, and `Mac OS X` supporting
both Python 2 and Python 3.

The full documentation for `cTDS` can be found
`here <https://zillow.github.io/ctds/>`_.

Features
--------

* Supports `Microsoft SQL Server <http://www.microsoft.com/sqlserver/>`_ 2008 and up.
* Complete `DB API-2.0`_ support.
* Python 2.6, Python 2.7, Python 3.3, Python 3.4, Python 3.5, Python 3.6, Python 3.7, Python 3.8, and Python 3.9 support.
* Bulk insert (bcp) support.
* Written entirely in C.

Dependencies
------------

* `FreeTDS`_
* `Python`_

.. _`FreeTDS`: https://www.freetds.org/
.. _`Python`: https://www.python.org/
.. _`DB API-2.0`: https://www.python.org/dev/peps/pep-0249

.. include-documentation-end-marker

See `installation instructions <https://zillow.github.io/ctds/install.html>`_
for more information on installing `FreeTDS`_.

Releasing
---------

Publishing new versions of the egg and documentation is automated using a
`Github Actions <https://docs.github.com/en/actions/>`_ workflow.
Official releases are marked using git
`tags <https://git-scm.com/book/en/v2/Git-Basics-Tagging>`_. Pushing the
tag to the git remote will trigger the automated deployment. E.g.

.. code-block:: console

    git tag -a v1.2.3 -m 'v1.2.3'
    git push --tags


Documentation
-------------

Generate documentation using the following:

.. code-block:: console

    tox -e docs
    # Generated to build/docs/

Documentation is hosted on `GitHub Pages <https://pages.github.com/>`_.
As such, the source code for the documentation pages must be committed
to the `gh-pages <https://github.com/zillow/ctds/tree/gh-pages>`_ branch in
order to update the live documentation.


Development
-----------

Local development and testing is supported on Linux-based systems running
`tox`_ and `Docker`_. Docker containers are used for running a local instance
of `SQL Server on Linux`_. Only `Docker`_ and `tox`_ are required for running
tests locally on Linux or OS X systems. `pyenv`_ is recommended for managing
multiple local versions of Python. By default all tests are run against
the system version of `FreeTDS`_. `GNU Make`_ targets are provided to make
compiling specific `FreeTDS`_ versions locally for testing purposes. For
example:

.. code-block:: console

    # Run tests against FreeTDS version 1.1.24
    make test-1.1.24


Development and testing will require an instance of `SQL Server on Linux`_
running for validation. A script, **./scripts/ensure-sqlserver.sh**, is provided
to start a `Docker`_ container running the database and create the login used
by the tests.

.. code-block:: console

    # Start a docker-based SQL Server instance.
    # The default tox targets will do this automatically for you.
    make start-sqlserver

    # Run tests as needed ...

    # Stop the docker-base SQL Server instance.
    make stop-sqlserver


Testing
-------

Testing is designed to be relatively seamless using `Docker`_ containers
and `SQL Server on Linux`_. The `pytest`_ framework is used for running
the automated tests.

To run the tests against the system version of `FreeTDS`_ and `Python`_,
use:

.. code-block:: console

    tox


`GNU make`_ targets are provided for convenience and to provide a standard
method for building and installing the various versions of `FreeTDS`_ used
in testing. Most targets are wrappers around `tox`_ or replicate some
behavior in the CI/CD automation.

To run the tests against an arbitrary version of `FreeTDS`_:

.. code-block:: console

    # Python X.Y & FreeTDS Z.ZZ.ZZ
    make test_X.Y_Z.ZZ.ZZ


To run tests against all supported versions of `FreeTDS`_ and `Python`_
and additional linting and metadata checks:

.. code-block:: console

    make check


Valgrind
--------
`valgrind`_ is utilized to ensure memory is managed properly and to detect
defects such as memory leaks, buffer overruns, etc. Because `valgrind`_
requires Python is compiled with specific flags, a `Docker`_ file is provided
to `compile Python`_ as necessary to run the test suite under `valgrind`_.

To run test test suite under `valgrind`_:

.. code-block:: console

    make valgrind


.. _`Docker`: https://www.docker.com/
.. _`compile Python`: https://pythonextensionpatterns.readthedocs.io/en/latest/debugging/valgrind.html
.. _`SQL Server on Linux`: https://hub.docker.com/r/microsoft/mssql-server-linux/
.. _`GNU make`: https://www.gnu.org/software/make/
.. _`pyenv`: https://github.com/pyenv/pyenv
.. _`pytest`: https://docs.pytest.org/en/stable/
.. _`tox`: https://tox.readthedocs.io/en/latest/
.. _`valgrind`: https://valgrind.org/
