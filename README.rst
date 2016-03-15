cTDS
====

.. image:: https://travis-ci.org/zillow/ctds.svg?branch=master
        :target: https://travis-ci.org/zillow/ctds

`cTDS` is a full Python `DB API-2.0`_-compliant SQL Server database library for `Linux` and
`Mac OS X`.

The full documentation for `cTDS` can be found `here`_.

.. _`DB API-2.0`: https://www.python.org/dev/peps/pep-0249
.. _`here`: http://pythonhosted.org/ctds/


Dependencies
------------

`cTDS` depends on the following:

* `FreeTDS`_

.. _`FreeTDS`: http://freetds.org


Documentation
-------------

Generate documentation using the following:

.. code-block::

    make doc
    # Generated to build/doc/


Development
-----------

To setup a development environment, some system packages are required. On Debian systems
these can be installed using:

.. code-block::

    make setup


This will also install the supported Python versions for testing.

Testing
-------

cTDS tests require an actual SQL Server instance to run. `SQL Server Express`_ is a free, readily
available variant should a paid version not be available. The included **test-setup.sql** script
will create the necessary database, login, etc. required for running the tests.

.. _`SQL Server Express`: https://www.microsoft.com/en-us/server-cloud/products/sql-server-editions/sql-server-express.aspx

Additionally, the hostname for the SQL Server instance to use for the tests must be configured
in the **ctds/tests/database.ini** configuration file.

To run the tests against the default version of Python, use:

.. code-block::

    make test


To run the tests against an arbitrary version of Python:

.. code-block::

    make test_X.Y


To run tests against all versions of Python:

.. code-block::

    make check


The tests are only run against one version of FreeTDS (*0.95.87*) by default. However, they can
easily be run against any version of FreeTDS using the following.

.. code-block::

    make test FREETDS_VERSION=0.92.408

This will download the specified version of FreeTDS, compile it, and then run the tests.
