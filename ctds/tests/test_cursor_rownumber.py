import warnings

import ctds

from .base import TestExternalDatabase
from .compat import long_

class TestCursorRowNumber(TestExternalDatabase):
    '''Unit tests related to the Cursor.rownumber attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.rownumber.__doc__,
            '''\
The current 0-based index of the cursor in the result set or None if the
index cannot be determined.

:pep:`0249#rownumber`

:return: None if no results are available.
:rtype: int
'''
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            with warnings.catch_warnings(record=True) as warns:
                self.assertEqual(cursor.rownumber, None)
                self.assertEqual(len(warns), 1)
                self.assertEqual(
                    [str(warn.message) for warn in warns],
                    ['DB-API extension cursor.rownumber used'] * len(warns)
                )

    def test_premature(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual(cursor.rownumber, None)
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.rownumber used'] * len(warns)
                    )

    def test_rownumber(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @test_rownumber TABLE(i INT);
                        INSERT INTO @test_rownumber(i) VALUES (1),(2),(3),(1),(2),(3);
                        SELECT * FROM @test_rownumber
                    '''
                )
                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual(cursor.rownumber, long_(0))
                    cursor.fetchone()
                    self.assertEqual(cursor.rownumber, long_(1))
                    cursor.fetchmany(size=2)
                    self.assertEqual(cursor.rownumber, long_(3))
                    cursor.fetchall()
                    self.assertEqual(cursor.rownumber, long_(6))

                    cursor.nextset()
                    self.assertEqual(cursor.rownumber, None)

                    self.assertEqual(len(warns), 5)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.rownumber used'] * len(warns)
                    )

                cursor.execute(
                    '''
                        DECLARE @test_rownumber TABLE(i INT);
                        INSERT INTO @test_rownumber(i) VALUES (1),(2),(3),(1),(2),(3);
                        SELECT * FROM @test_rownumber
                    '''
                )
                with warnings.catch_warnings(record=True) as warns:
                    cursor.fetchmany(size=2)
                    self.assertEqual(cursor.rownumber, long_(2))
