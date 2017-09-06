import ctds

from .base import TestExternalDatabase

class TestCursorNextset(TestExternalDatabase):
    '''Unit tests related to the Cursor.nextset() method.
    '''
    def test___doc__(self):
        doc = '''\
nextset()

Skip to the next available set, discarding any remaining rows from the
current set.

:pep:`0249#nextset`

:return: True if there was another result set or None if not.
'''
        self.assertEqual(
            ctds.Cursor.nextset.__doc__,
            doc
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.nextset()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.nextset() did not fail as expected') # pragma: nocover

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            try:
                cursor.nextset()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'connection closed')
            else:
                self.fail('.nextset() did not fail as expected') # pragma: nocover

    def test_partial_read(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    DECLARE @tTable TABLE (i int);
                    INSERT INTO
                        @tTable
                    VALUES
                        (1),
                        (2),
                        (3),
                        (4);
                    SELECT * FROM @tTable;
                    SELECT 'resultset 2';
                    '''
                )

                self.assertTrue(cursor.nextset())
                self.assertEqual(
                    [tuple(row) for row in cursor.fetchall()],
                    [('resultset 2',)]
                )
