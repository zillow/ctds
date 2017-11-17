import re
import unittest
import pkg_resources

import ctds

from .compat import PY27, StandardError_

class TestTds(unittest.TestCase):

    maxDiff = None

    def test_version(self):
        self.assertEqual(
            tuple(int(item) for item in ctds.__version__.split('.')),
            ctds.version_info
        )

        self.assertTrue(pkg_resources.get_distribution('ctds').version)

    def test_apilevel(self):
        self.assertEqual(ctds.apilevel, '2.0')

    def test_threadsafety(self):
        self.assertEqual(ctds.threadsafety, 1)

    def test_paramstyle(self):
        self.assertEqual(ctds.paramstyle, 'numeric')

    def test_freetds_version(self):
        version = ctds.freetds_version
        self.assertTrue(isinstance(version, str))
        self.assertTrue(
            re.match(
                r'^freetds v(?P<major>[\d]+)\.(?P<minor>[\d]+)(?:\.(?P<patch>[\d]+))?$',
                version
            )
        )

    def test_exceptions(self):
        self.assertTrue(issubclass(ctds.Warning, StandardError_))
        self.assertEqual(
            ctds.Warning.__doc__,
            '''\
Warning

:pep:`0249#warning`

Exception raised for important warnings like data truncations while
inserting, etc.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.Error, StandardError_))
        self.assertEqual(
            ctds.Error.__doc__,
            '''\
Error

:pep:`0249#error`

Exception that is the base class of all other error exceptions. You
can use this to catch all errors with one single except statement.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.InterfaceError, ctds.Error))
        self.assertEqual(
            ctds.InterfaceError.__doc__,
            '''\
InterfaceError

:pep:`0249#interfaceerror`

Exception raised for errors that are related to the database interface
rather than the database itself.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.DatabaseError, ctds.Error))
        self.assertEqual(
            ctds.DatabaseError.__doc__,
            '''\
DatabaseError

:pep:`0249#databaseerror`

Exception raised for errors that are related to the database.

The exception contains the following properties:

.. py:attribute:: db_error

    A :py:class:`dict` containing information relating to an
    error returned by the database.

    .. code-block:: python

        {
            'number': 123,
            'description': 'An error description'
        }

.. py:attribute:: os_error

    A :py:class:`dict` containing information relating to an
    error caused by a database connection issue.
    This will be :py:data:`None` if the error was not caused by a
    connection issue.

    .. code-block:: python

        {
            'number': 123,
            'description': 'An error description'
        }

.. py:attribute:: last_message

    A :py:class:`dict` containing more detailed information about.
    the error returned from the database.
    This may be :py:data:`None` if the error does not include more
    information from the database, e.g. a connection error.

    .. code-block:: python

        {
            'number': 123,
            'state': 0,
            'severity': 16,
            'description': 'An error description'
            'server': 'database-hostname'
            'proc': 'procedure_name'
            'line': 34
        }
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.DataError, ctds.DatabaseError))
        self.assertEqual(
            ctds.DataError.__doc__,
            '''\
DataError

:pep:`0249#dataerror`

Exception raised for errors that are due to problems with the
processed data like division by zero, numeric value out of range,
etc.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.OperationalError, ctds.DatabaseError))
        self.assertEqual(
            ctds.OperationalError.__doc__,
            '''\
OperationalError

:pep:`0249#operationalerror`

Exception raised for errors that are related to the database's
operation and not necessarily under the control of the programmer,
e.g. an unexpected disconnect occurs, the data source name is not
found, a transaction could not be processed, a memory allocation
error occurred during processing, etc.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.IntegrityError, ctds.DatabaseError))
        self.assertEqual(
            ctds.IntegrityError.__doc__,
            '''\
IntegrityError

:pep:`0249#integrityerror`

Exception raised when the relational integrity of the database is
affected, e.g. a foreign key check fails.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.InternalError, ctds.DatabaseError))
        self.assertEqual(
            ctds.InternalError.__doc__,
            '''\
InternalError

:pep:`0249#internalerror`

Exception raised when the database encounters an internal error,
e.g. the cursor is not valid anymore, the transaction is out of
sync, etc.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.ProgrammingError, ctds.DatabaseError))
        self.assertEqual(
            ctds.ProgrammingError.__doc__,
            '''\
ProgrammingError

:pep:`0249#programmingerror`

Exception raised for programming errors, e.g. table not found or
already exists, syntax error in the SQL statement, wrong number of
parameters specified, etc.
''' if PY27 else None
        )

        self.assertTrue(issubclass(ctds.NotSupportedError, ctds.DatabaseError))
        self.assertEqual(
            ctds.NotSupportedError.__doc__,
            '''\
NotSupportedError

:pep:`0249#notsupportederror`

Exception raised in case a method or database API was used which is
not supported by the database, e.g. calling
:py:func:`~ctds.Connection.rollback()` on a connection that does not
support transactions or has transactions turned off.
''' if PY27 else None
        )
