cTDS
====

.. include-documentation-begin-marker

.. image:: https://travis-ci.org/zillow/ctds.svg?branch=master
        :target: https://travis-ci.org/zillow/ctds

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
* Python 2.6, Python 2.7, Python 3.3, Python 3.4, Python 3.5, Python 3.6, Python 3.7, and Python 3.8 support.
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

Publishing new versions of the egg and documentation is automated using
`travis-ci <https://docs.travis-ci.com/user/deployment/>`_ deployment.
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

    make doc
    # Generated to ./.gh-pages

Documentation is hosted on `GitHub Pages <https://pages.github.com/>`_.
As such, the source code for the documentation pages must be committed
to the `gh-pages <https://github.com/zillow/ctds/tree/gh-pages>`_ branch in
order to update the live documentation.


Development
-----------

Local development and testing is supported on Linux-based systems running
`Docker`_. Docker containers are used for both running a local instance
of `SQL Server on Linux`_ and creating containers for each combination
of Python and `FreeTDS`_ version supported. Only `Docker`_ and `GNU make`_
are required for running tests locally on Linux or OS X systems.

If desired, local development *can* be done by installing **ctds** against the
system versions of `FreeTDS`_ and `Python`_. Additionally there is a
`virtualenv` target which will download and compile a recent version of
`FreeTDS`_ and then install **ctds** into a *virtualenv* using the local
version of `FreeTDS`_.

.. code-block:: console

    # Install as a "develop" egg
    pip install -e .

    # Install tests.
    pip install -e .[tests]

    # Run tests (requires SQL Server running)
    python setup.py test


However, given the various supported combinations of `FreeTDS`_ and `Python`_,
it is easier to create a separate `Docker`_ container for each. The matrix
of `FreeTDS`_ and `Python`_ is driven using `GNU make`_.


Development and testing will require an instance of
`SQL Server on Linux`_ running for validation. A script is provided to
start a `Docker`_ container running the database and create the login
used by the tests.

.. code-block:: console

    # Start a docker-based SQL Server instance.
    make start-sqlserver

    # Run tests as needed ...

    # Stop the docker-base SQL Server instance.
    make stop-sqlserver


Testing
-------

Testing is designed to be relatively seamless using `Docker`_ containers
and `SQL Server on Linux`_. All *test* targets will ensure a running
database instance docker container exists and is accessible prior to running.

To run the tests against the most recent versions of `FreeTDS`_ and `Python`_,
use:

.. code-block:: console

    make test


To run the tests against an arbitrary version of `FreeTDS`_ and `Python`_:

.. code-block:: console

    # Python X.Y & FreeTDS Z.ZZ.ZZ
    make test_X.Y_Z.ZZ.ZZ


To run tests against all supported versions of `FreeTDS`_ and `Python`_
and additional linting and metadata checks:

.. code-block:: console

    make check


.. _`Docker`: https://www.docker.com/
.. _`SQL Server on Linux`: https://hub.docker.com/r/microsoft/mssql-server-linux/
.. _`GNU make`: https://www.gnu.org/software/make/
