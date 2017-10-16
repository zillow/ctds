import ctds

from .base import TestExternalDatabase

class TestCursorFetchMany(TestExternalDatabase):
    '''Unit tests related to the Cursor.fetchmany() method.
    '''
    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.fetchmany.__doc__,
            '''\
fetchmany(size=self.arraysize)

Fetch the next set of rows of a query result, returning a sequence of
sequences. An empty sequence is returned when no more rows are available.

:pep:`0249#fetchmany`

:return: A sequence of result rows.
'''
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.fetchmany()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.fetchmany() did not fail as expected') # pragma: nocover

    def test_closed_connection(self): # pylint: disable=invalid-name
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            try:
                cursor.fetchmany()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'connection closed')
            else:
                self.fail('.fetchmany() did not fail as expected') # pragma: nocover

    def test_invalid_size(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.fetchmany, size='123')

    def test_premature(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(ctds.InterfaceError, cursor.fetchmany)

    def test_fetchmany(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3);
                        SELECT * FROM @{0};
                        SELECT i * 2 FROM @{0};
                    '''.format(self.test_fetchmany.__name__)
                )
                self.assertEqual([tuple(row) for row in cursor.fetchmany()], [(1,)])
                self.assertEqual([tuple(row) for row in cursor.fetchmany()], [(2,)])
                self.assertEqual([tuple(row) for row in cursor.fetchmany()], [(3,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual([tuple(row) for row in cursor.fetchmany()], [(2,)])
                self.assertEqual([tuple(row) for row in cursor.fetchmany()], [(4,)])
                self.assertEqual([tuple(row) for row in cursor.fetchmany()], [(6,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), None)
                self.assertRaises(ctds.InterfaceError, cursor.fetchmany)

                cursor.arraysize = 3
                cursor.execute(
                    '''
                        DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3);
                        SELECT * FROM @{0};
                        SELECT i * 2 FROM @{0};
                    '''.format(self.test_fetchmany.__name__)
                )
                self.assertEqual([tuple(row) for row in cursor.fetchmany(3)], [(1,), (2,), (3,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual([tuple(row) for row in cursor.fetchmany(3)], [(2,), (4,), (6,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), None)
                self.assertRaises(ctds.InterfaceError, cursor.fetchmany)

    def test_size(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3);
                        SELECT * FROM @{0};
                        SELECT i * 2 FROM @{0};
                    '''.format(self.test_size.__name__)
                )
                self.assertEqual([tuple(row) for row in cursor.fetchmany(3)], [(1,), (2,), (3,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual([tuple(row) for row in cursor.fetchmany(3)], [(2,), (4,), (6,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), None)
                self.assertRaises(ctds.InterfaceError, cursor.fetchmany)

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
                self.assertEqual(list(cursor.fetchmany()), [])
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
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual([tuple(row) for row in cursor.fetchmany(3)], [(3,)])
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual(list(cursor.fetchmany()), [])
                self.assertEqual(cursor.nextset(), True)
                self.assertEqual([tuple(row) for row in cursor.fetchmany(3)], [(1,), (2,), (3,)])
                self.assertEqual(cursor.nextset(), None)
