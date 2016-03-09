import ctds

from .base import TestExternalDatabase
from .compat import int_, long_

class TestConnectionTimeout(TestExternalDatabase):
    '''Unit tests related to the Connection.timeout attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.timeout.__doc__,
            '''\
The connection timeout.
'''
        )

    def test_closed(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        try:
            self.assertEqual(connection.timeout, None)
            connection.timeout = 123
        except ctds.InterfaceError:
            pass
        except ctds.NotSupportedError: # pragma: nocover
            pass
        else:
            self.fail('.timeout did not fail as expected') # pragma: nocover

    def test_getset(self):
        with self.connect(timeout=1) as connection:
            try:
                self.assertEqual(connection.timeout, 1)
                for timeout in (
                        int_(123),
                        long_(123)
                ):
                    connection.timeout = timeout
                    self.assertEqual(connection.timeout, timeout)
            except ctds.NotSupportedError: # pragma: nocover
                pass

    def test_timeout(self):
        with self.connect(timeout=1) as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.execute("WAITFOR DELAY '00:00:01.1';SELECT @@VERSION")
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

        with self.connect(timeout=1) as connection:
            with connection.cursor() as cursor:
                try:
                    connection.timeout = 2
                    cursor.execute("WAITFOR DELAY '00:00:01.1';SELECT @@VERSION")
                except ctds.NotSupportedError: # pragma: nocover
                    pass

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
