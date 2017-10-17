import ctds

from .base import TestExternalDatabase

class TestCursorFetchOne(TestExternalDatabase):
    '''Unit tests related to the Cursor.fetchone() method.
    '''
    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.fetchone.__doc__,
            '''\
fetchone()

Fetch the next row of a query result set, returning a single sequence, or
:py:data:`None` when no more data is available.

:pep:`0249#fetchone`

:return: The next row or :py:data:`None`.
'''
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.fetchone()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.fetchone() did not fail as expected') # pragma: nocover

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            try:
                cursor.fetchone()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'connection closed')
            else:
                self.fail('.fetchone() did not fail as expected') # pragma: nocover

    def test_premature(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(ctds.InterfaceError, cursor.fetchone)

    def test_fetchone(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3);
                        SELECT * FROM @{0};
                        SELECT i * 2 FROM @{0};
                    '''.format(self.test_fetchone.__name__)
                )
                self.assertEqual(tuple(cursor.fetchone()), (1,))
                self.assertEqual(tuple(cursor.fetchone()), (2,))
                self.assertEqual(tuple(cursor.fetchone()), (3,))
                self.assertEqual(cursor.fetchone(), None)
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual(tuple(cursor.fetchone()), (2,))
                self.assertEqual(tuple(cursor.fetchone()), (4,))
                self.assertEqual(tuple(cursor.fetchone()), (6,))
                self.assertEqual(cursor.fetchone(), None)
                self.assertEqual(cursor.nextset(), None)
                self.assertRaises(ctds.InterfaceError, cursor.fetchone)

    def test_empty_resultset(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3);
                        SELECT i FROM @{0} WHERE i < 0;
                    '''.format(self.test_empty_resultset.__name__)
                )
                self.assertEqual(cursor.fetchone(), None)
                self.assertEqual(cursor.nextset(), None)

    def test_multiple_resultsets(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3);
                        SELECT i FROM @{0} WHERE i < 0;
                        SELECT i AS j FROM @{0} WHERE i > 2;
                        SELECT i AS k FROM @{0} WHERE i > 3;
                        SELECT i AS ii FROM @{0};
                    '''.format(self.test_multiple_resultsets.__name__)
                )
                self.assertEqual(cursor.fetchone(), None)
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual(tuple(cursor.fetchone()), (3,))
                self.assertEqual(cursor.fetchone(), None)
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual(cursor.fetchone(), None)
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual(tuple(cursor.fetchone()), (1,))
                self.assertEqual(tuple(cursor.fetchone()), (2,))
                self.assertEqual(tuple(cursor.fetchone()), (3,))
                self.assertEqual(cursor.nextset(), None)
