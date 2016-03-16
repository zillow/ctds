import ctds

from .base import TestExternalDatabase

class TestConnectionAutocommit(TestExternalDatabase):

    def test_closed(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        try:
            connection.autocommit = False
        except ctds.InterfaceError:
            pass
        else:
            self.fail('.autocommit did not fail as expected') # pragma: nocover

    def test_getset(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertEqual(connection.autocommit, True)
                cursor.execute('SELECT 1 FROM sys.objects')
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

                # Verify the IMPLICIT_TRANSACTIONS setting is OFF.
                cursor.execute('SELECT @@OPTIONS')
                self.assertFalse(self.IMPLICIT_TRANSACTIONS & cursor.fetchone()[0])

                connection.autocommit = False
                self.assertEqual(connection.autocommit, False)

                # Verify a transaction is created automatically.
                cursor.execute('SELECT 1 FROM sys.objects')
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(1, cursor.fetchone()[0])

                # Verify the IMPLICIT_TRANSACTIONS setting is ON.
                cursor.execute('SELECT @@OPTIONS')
                self.assertTrue(self.IMPLICIT_TRANSACTIONS & cursor.fetchone()[0])

                # The transaction should be committed when enabling autocommit.
                connection.autocommit = True
                self.assertEqual(connection.autocommit, True)

                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

                cursor.execute('SELECT 1 FROM sys.objects')
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

                # Verify the IMPLICIT_TRANSACTIONS setting is OFF.
                cursor.execute('SELECT @@OPTIONS')
                self.assertFalse(self.IMPLICIT_TRANSACTIONS & cursor.fetchone()[0])

    def test_typeerror(self):
        with self.connect() as connection:
            for case in (None, '', 0, 1, ' ', 'True', 'False', object()):
                try:
                    connection.autocommit = case
                except TypeError:
                    pass
                else:
                    self.fail('.autocommit did not fail as expected') # pragma: nocover
