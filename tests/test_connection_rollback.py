import ctds

from .base import TestExternalDatabase

class TestConnectionRollback(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.rollback.__doc__,
            '''\
rollback()

Rollback any pending transaction to the database.

:pep:`0249#rollback`
'''
        )

    def test_closed(self):
        connection = self.connect()
        connection.close()
        try:
            connection.rollback()
        except ctds.InterfaceError as ex:
            self.assertEqual(str(ex), 'connection closed')
        else:
            self.fail('.rollback() did not fail as expected ') # pragma: nocover

    def test_not_in_transaction(self):
        with self.connect() as connection:
            self.assertEqual(connection.rollback(), None)

    def test_rollback(self):
        with self.connect(autocommit=False) as connection:
            with connection.cursor() as cursor:
                cursor.execute('CREATE TABLE {0} (i INT, f FLOAT)'.format(self.test_rollback.__name__))
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(1, cursor.fetchone()[0])

                connection.rollback()

                connection.autocommit = True
                cursor.execute(
                    '''
                        SELECT 1
                        FROM sys.objects
                        WHERE object_id = OBJECT_ID(N'{0}')
                    '''.format(self.test_rollback.__name__)
                )

                self.assertEqual(list(cursor.fetchall()), [])

                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

    def test_rollback_autocommit(self):
        with self.connect(timeout=1) as connection:
            with connection.cursor() as cursor:
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

                connection.rollback()

                self.assertRaises(
                    ctds.DatabaseError,
                    cursor.execute,
                    "WAITFOR DELAY '00:00:02';SELECT @@VERSION"
                )

                try:
                    connection.rollback()
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
