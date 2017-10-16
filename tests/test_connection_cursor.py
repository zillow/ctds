import ctds

from .base import TestExternalDatabase

class TestConnectionCursor(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.cursor.__doc__,
            '''\
cursor()

Return a new :py:class:`ctds.Cursor` object using the connection.

.. note::

    :py:meth:`ctds.Cursor.close` should be called when the returned
    cursor is no longer required.

.. warning::

    Only one :py:class:`ctds.Cursor` object should be used per
    connection. The last command executed on any cursor associated
    with a connection will overwrite any previous results from all
    other cursors.

:pep:`0249#cursor`

:return: A new Cursor object.
:rtype: ctds.Cursor
'''
        )

    def test_closed(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        self.assertRaises(ctds.InterfaceError, connection.cursor)
