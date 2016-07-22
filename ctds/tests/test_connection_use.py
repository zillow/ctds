import ctds

from .base import TestExternalDatabase
from .compat import PY3, unicode_

class TestConnectionUse(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.use.__doc__,
            '''\
use(database)

Set the current database.

:param str database: The database.
:rtype: None
'''
        )

    def test_use(self):
        with self.connect() as connection:
            for database in ('master', unicode_('master')):
                connection.use(database)
                self.assertEqual(connection.database, unicode_(database))

        self.assertEqual(connection.database, None)

    def test_invalid(self):
        with self.connect() as connection:
            for database in (None, 1234):
                self.assertRaises(TypeError, connection.use, database)
            if PY3:
                self.assertRaises(TypeError, connection.use, b'1234')
            else: # pragma: nocover
                pass

    def test_closed(self):
        connection = self.connect()
        connection.close()
        self.assertRaises(ctds.InterfaceError, connection.use, 'master')

    def test_unknown_database(self):
        database = '.master'
        with self.connect() as connection:
            current = connection.database
            try:
                connection.use(database)
            except ctds.DatabaseError as ex:
                msg = "Database '{0}' does not exist. Make sure that the name is entered correctly.".format(database)
                self.assertEqual(str(ex), msg)
            else:
                self.fail('.use() did not fail as expected') # pragma: nocover
            self.assertEqual(connection.database, current)
