import ctds

from .base import TestExternalDatabase
from .compat import int_, long_

class TestConnectionSpid(TestExternalDatabase):
    '''Unit tests related to the Connection.spid attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.spid.__doc__,
            '''\
Retrieve the SQL Server Session Process ID (SPID) for the connection.

:return: None if the connection is closed.
'''
        )


    def test_read(self):
        with self.connect() as connection:
            self.assertTrue(isinstance(connection.spid, (int_, long_)))

        self.assertEqual(connection.spid, None)

    def test_write(self):
        with self.connect() as connection:
            try:
                connection.spid = 9
            except AttributeError:
                pass
            else:
                self.fail('.spid did not fail as expected') # pragma: nocover

        try:
            connection.spid = None
        except AttributeError:
            pass
        else:
            self.fail('.spid did not fail as expected') # pragma: nocover
