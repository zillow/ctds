import ctds

from .base import TestExternalDatabase
from .compat import long_

class TestCursorSpid(TestExternalDatabase):
    '''Unit tests related to the Cursor.spid property.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.spid.__doc__,
            '''\
Retrieve the SQL Server Session Process ID (SPID) for the connection or
:py:data:`None` if the connection is closed.

:rtype: int
'''
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.spid
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.spid did not fail as expected') # pragma: nocover

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            self.assertEqual(cursor.spid, None)

    def test_read(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertTrue(isinstance(cursor.spid, long_))
                cursor.execute('SELECT @@SPID;')
                spid = cursor.fetchone()[0]

                if self.freetds_version >= (1, 0, 0): # pragma: nocover
                    self.assertEqual(cursor.spid, spid)

    def test_write(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.spid = 9
                except AttributeError:
                    pass
                else:
                    self.fail('.spid did not fail as expected') # pragma: nocover

                try:
                    cursor.spid = None
                except AttributeError:
                    pass
                else:
                    self.fail('.spid did not fail as expected') # pragma: nocover
