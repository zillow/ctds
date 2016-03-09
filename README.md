cTDS
====

*cTDS* is a full Python [DB API-2.0](https://www.python.org/dev/peps/pep-0249)-compliant
SQL Server database library.

Documentation
=============

Generate documentation using the following:

    make doc
    # Generated to build/doc/


Development
===========

To setup a development environment, some system packages are required. On Debian systems
these can be installed using the following:

    make setup


This will also install the supported Python versions for testing.

Testing
=======

cTDS tests require an actual SQL Server instance to run.
[SQL Server Express](https://www.microsoft.com/en-us/server-cloud/products/sql-server-editions/sql-server-express.aspx)
is a free, readily available variant should a paid version not be available. The included
_test-setup.sql_ script will create the necessary database, login, etc. required for running
the tests.

Additionally, the hostname for the SQL Server instance to use for the tests must be configured
in the *ctds/tests/database.ini* configuration file.

To run the tests against the default version of Python, use:

    make test


To run the tests against an arbitrary version of Python:

    make test_X.Y


To run tests against all versions of Python:

    make check


The tests are only run against one version of FreeTDS (0.95.87) by default. However, they can
easily be run against any version of FreeTDS using the following. This will download the
specified version of FreeTDS, compile it and then run the tests.

    make test FREETDS_VERSION=0.92.408

