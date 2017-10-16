import ctds

from .base import TestExternalDatabase

class TestConnectionClose(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.close.__doc__,
            '''\
close()

Close the connection now. Pending transactions will be rolled back.
Subsequent calls to this object or any :py:class:`ctds.Cursor` objects it
created will raise :py:exc:`ctds.InterfaceError`.

:pep:`0249#Connection.close`
'''
        )

    def test_close(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        self.assertRaises(ctds.InterfaceError, connection.close)
