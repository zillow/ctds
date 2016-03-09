import warnings

import ctds

from .base import TestExternalDatabase
from .compat import PY3, PY34

class TestCursorNext(TestExternalDatabase):
    '''Unit tests related to the Cursor.next() method.
    '''
    def test___doc__(self):
        if PY3:
            doc = '''\
next()

Return the next row from the currently executing SQL statement using the
same semantics as :py:meth:`.fetchone`. A :py:exc:`StopIteration` exception
is raised when the result set is exhausted.

:pep:`0249#next`
'''
        else: # pragma: nocover
            doc = 'x.next() -> the next value, or raise StopIteration'
            self.assertEqual(
                ctds.Cursor.next.__doc__,
                doc
            )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            with warnings.catch_warnings(record=True) as warns:
                try:
                    cursor.next()
                except ctds.InterfaceError as ex:
                    self.assertEqual(str(ex), 'cursor closed')
                else:
                    self.fail('.next() did not fail as expected') # pragma: nocover
            if PY3:
                self.assertEqual(len(warns), 1)
                self.assertEqual(
                    str(warns[0].message),
                    'DB-API extension cursor.next() used'
                )
            else: # pragma: nocover
                self.assertEqual(len(warns), 0)

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            with warnings.catch_warnings(record=True) as warns:
                try:
                    cursor.next()
                except ctds.InterfaceError as ex:
                    self.assertEqual(str(ex), 'connection closed')
                else:
                    self.fail('.next() did not fail as expected') # pragma: nocover
                if PY3:
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        str(warns[0].message),
                        'DB-API extension cursor.next() used'
                    )
                else: # pragma: nocover
                    self.assertEqual(len(warns), 0)

    def test_premature(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings(record=True) as warns:
                    self.assertRaises(ctds.InterfaceError, cursor.next)
                if PY3:
                    if PY34:
                        self.assertEqual(len(warns), 1)
                        self.assertEqual(
                            str(warns[0].message),
                            'DB-API extension cursor.next() used'
                        )
                else: # pragma: nocover
                    pass

    def test_next(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @test_next TABLE(i INT);
                        INSERT INTO @test_next(i) VALUES (1),(2),(3);
                        SELECT * FROM @test_next;
                        SELECT i * 2 FROM @test_next;
                    '''
                )
                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual(tuple(cursor.next()), (1,))
                    self.assertEqual(tuple(cursor.next()), (2,))
                    self.assertEqual(tuple(cursor.next()), (3,))
                    self.assertRaises(StopIteration, cursor.next)
                if PY3:
                    self.assertEqual(len(warns), 4)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.next() used'] * len(warns)
                    )
                else: # pragma: nocover
                    self.assertEqual(len(warns), 0)

                self.assertEqual(cursor.nextset(), True)

                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual(tuple(cursor.next()), (2,))
                    self.assertEqual(tuple(cursor.next()), (4,))
                    self.assertEqual(tuple(cursor.next()), (6,))
                    self.assertRaises(StopIteration, cursor.next)
                if PY3:
                    self.assertEqual(len(warns), 4 if PY34 else 3)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.next() used'] * len(warns)
                    )
                else: # pragma: nocover
                    self.assertEqual(len(warns), 0)

                self.assertEqual(cursor.nextset(), None)
