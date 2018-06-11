import ctds

from .base import TestExternalDatabase
from .compat import unicode_

class TestConnectionDatabase(TestExternalDatabase):
    '''Unit tests related to the Connection.database attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.database.__doc__,
            '''\
The current database or :py:data:`None` if the connection is closed.

:rtype: str
'''
        )

    def test_database(self):
        with self.connect() as connection:
            # Versions prior to 0.95 had issues with database names > 30 characters.
            if self.freetds_version[:2] > (0, 92):
                database = self.get_option('database')
            else: # pragma: nocover
                database = self.get_option('database')[:30]
            self.assertEqual(connection.database, database)

            for database in ('master', unicode_('master')):
                connection.database = database
                self.assertEqual(connection.database, unicode_(database))

        self.assertEqual(connection.database, None)

    def test_closed(self):
        connection = self.connect()
        connection.close()
        try:
            connection.database = 'master'
        except ctds.InterfaceError:
            self.assertEqual(connection.database, None)
        else:
            self.fail('.database did not fail as expected') # pragma: nocover

    def test_unknown_database(self):
        database = '.master'
        with self.connect() as connection:
            current = connection.database
            try:
                connection.database = database
            except ctds.DatabaseError as ex:
                msg = "Database '{0}' does not exist. Make sure that the name is entered correctly.".format(database)
                self.assertEqual(str(ex), msg)
            else:
                self.fail('.database did not fail as expected') # pragma: nocover
            self.assertEqual(connection.database, current)

    def test_typeerror(self):
        with self.connect() as connection:
            for case in (None, 1234, object()):
                try:
                    connection.database = case
                except TypeError:
                    pass
                else:
                    self.fail('.database did not fail as expected') # pragma: nocover
