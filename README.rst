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
* Python 2.6, Python 2.7, Python 3.3, Python 3.4, Python 3.5, and Python 3.6 support.
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
    # Generated to ./.gh-pages

Documentation is hosted on `GitHub Pages <https://pages.github.com/>`_.
As such, the source code for the documentation pages must be committed
to the master branch in order to update the live documentation.


Development
-----------

Local development and testing is supported linux-based systems running
`Docker`_. Docker containers are used for both running a local instance
of `SQL Server on Linux`_ and creating containers for each combination
of Python and FreeTDS version supported.

.. _`Docker`: https://www.docker.com/
.. _`SQL Server on Linux`_: https://hub.docker.com/r/microsoft/mssql-server-linux/


Testing
-------

To run the tests against the default version of Python and FreeTDS, use:

.. code-block::

    make test


To run the tests against an arbitrary version of Python and FreeTDS:

.. code-block::

    # Python X.Y & FreeTDS Z.ZZ.ZZ
    make test_X.Y_Z.ZZ.ZZ


To run tests against all supported versions of Python and FreeTDS:

.. code-block::

    make check
