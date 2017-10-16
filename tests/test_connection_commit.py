import ctds

from .base import TestExternalDatabase

class TestConnectionCommit(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.commit.__doc__,
            '''\
commit()

Commit any pending transaction to the database.

:pep:`0249#commit`
'''
        )

    def test_closed(self):
        connection = self.connect()
        connection.close()
        try:
            connection.commit()
        except ctds.InterfaceError as ex:
            self.assertEqual(str(ex), 'connection closed')
        else:
            self.fail('.commit() did not fail as expected ') # pragma: nocover

    def test_not_in_transaction(self):
        with self.connect() as connection:
            self.assertEqual(connection.commit(), None)

    def test_commit(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    name = self.test_commit.__name__
                    cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                    cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))
                connection.commit()
                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(name))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(1,), (2,), (3,)]
                    )
            finally:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                            IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                                DROP TABLE dbo.{0};
                        '''.format(name)
                    )
                connection.commit()
