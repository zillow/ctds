import ctds

from .base import TestExternalDatabase
from .compat import int_, long_

class TestConnectionTimeout(TestExternalDatabase):
    '''Unit tests related to the Connection.timeout attribute.
    '''

    @property
    def supports_timeout_set(self):
        return self.freetds_version >= (1, 0, 0)

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.timeout.__doc__,
            '''\
The connection timeout, in seconds, or :py:data:`None` if the connection
is closed.

.. note:: Setting the timeout requires FreeTDS version 1.00 or later.

:raises ctds.NotSupportedError: `cTDS` was compiled against a version of
    FreeTDS which does not support setting the timeout on a connection.

:rtype: int
'''
        )

    def test_closed(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        try:
            self.assertEqual(connection.timeout, None)
            connection.timeout = 123
        except ctds.InterfaceError:
            # An interface error is expected if the connection is closed.
            pass
        else:
            self.fail('.timeout did not fail as expected') # pragma: nocover

    def test_getset(self):
        with self.connect(timeout=123) as connection:
            self.assertEqual(connection.timeout, 123)
            if self.supports_timeout_set:
                for timeout in (
                        int_(321),
                        long_(321)
                ):
                    connection.timeout = timeout
                    self.assertEqual(connection.timeout, timeout)
            else: # pragma: nocover
                try:
                    connection.timeout = 2
                except ctds.NotSupportedError as ex:
                    self.assertEqual(str(ex), 'FreeTDS does not support the DBSETTIME option')
                else:
                    self.fail('.timeout did not fail as expected') # pragma: nocover

    def test_timeout(self):
        with self.connect(timeout=1) as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.execute("WAITFOR DELAY '00:00:02';SELECT @@VERSION")
                except ctds.DatabaseError as ex:
                    self.assertEqual(str(ex), 'Adaptive Server connection timed out')
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

                try:
                    cursor.execute('SELECT @@VERSION')
                except ctds.DatabaseError as ex:
                    self.assertTrue(
                        str(ex) in (
                            'DBPROCESS is dead or not enabled',
                            # Older versions of FreeTDS return a timeout error.
                            'Adaptive Server connection timed out'
                        )
                    )
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

                if self.supports_timeout_set:
                    try:
                        connection.timeout = 3
                    except ctds.DatabaseError as ex:
                        self.assertEqual(str(ex), 'DBPROCESS is dead or not enabled')
                    else:
                        self.fail('.timeout did not fail as expected') # pragma: nocover
                else:
                    pass # pragma: nocover

        if self.supports_timeout_set:
            with self.connect(timeout=1) as connection:
                with connection.cursor() as cursor:
                    connection.timeout = 2
                    cursor.execute("WAITFOR DELAY '00:00:01';SELECT @@VERSION")
        else:
            pass # pragma: nocover

    def test_typeerror(self):
        with self.connect() as connection:
            for case in (None, '', ' ', 'True', 'False', object()):
                try:
                    connection.timeout = case
                except TypeError:
                    pass
                else:
                    self.fail('.timeout did not fail as expected') # pragma: nocover

    def test_valueerror(self):
        with self.connect() as connection:
            for case in (2 ** 31, -1):
                try:
                    connection.timeout = case
                except ValueError:
                    pass
                else:
                    self.fail('.timeout did not fail as expected') # pragma: nocover
