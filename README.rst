cTDS
====

.. include-documentation-begin-marker

.. image:: https://travis-ci.org/zillow/ctds.svg?branch=master
        :target: https://travis-ci.org/zillow/ctds

.. image:: http://img.shields.io/pypi/dm/ctds.svg
        :target: https://pypi.python.org/pypi/ctds/

.. image:: http://img.shields.io/pypi/v/ctds.svg
        :target: https://pypi.python.org/pypi/ctds/

`cTDS` is a full Python `DB API-2.0 <https://www.python.org/dev/peps/pep-0249>`_-compliant
SQL Server database library for `Linux` and `Mac OS X` supporting both Python 2
and Python 3.

Features
--------

* Supports `Microsoft SQL Server <http://www.microsoft.com/sqlserver/>`_ 2008 and up.
* Complete `DB API-2.0 <https://www.python.org/dev/peps/pep-0249>`_ support.
* Python 2.6, Python 2.7, Python 3.3, Python 3.4, and Python 3.5 support.
* Bulk insert (bcp) support.
* Written entirely in C.

.. include-documentation-end-marker


Documentation
-------------

The full documentation for `cTDS` can be found
`here <http://pythonhosted.org/ctds/>`_.

Generate documentation using the following:

.. code-block::

    make doc
    # Generated to build/docs/html


Development
-----------

To setup a development environment, some system packages are required. On Debian
systems these can be installed using:

.. code-block::

    make setup


This will also install the supported Python versions for testing.

Testing
-------

cTDS tests require an actual SQL Server instance to run. `SQL Server Express`_
is a free, readily available variant should a paid version not be available. The
included **test-setup.sql** script will create the necessary database, login,
etc. required for running the tests.

.. _`SQL Server Express`: https://www.microsoft.com/en-us/server-cloud/products/sql-server-editions/sql-server-express.aspx

Additionally, the hostname for the SQL Server instance to use for the tests must
be configured in the **ctds/tests/database.ini** configuration file.

To run the tests against the default version of Python and FreeTDS, use:

.. code-block::

    make test


To run the tests against an arbitrary version of Python and FreeTDS:

.. code-block::

    make test_X.Y-Z.ZZ.ZZ


To run tests against all versions of Python and FreeTDS:

.. code-block::

    make check
