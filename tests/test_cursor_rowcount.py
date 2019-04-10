import ctds

from .base import TestExternalDatabase
from .compat import long_

class TestCursorRowCount(TestExternalDatabase):
    '''Unit tests related to the Cursor.rowcount attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.rowcount.__doc__,
            '''\
The number of rows that the last :py:meth:`.execute` produced or affected.

.. note::

    This value is unreliable when :py:meth:`.execute` is called with parameters
    and using a version of FreeTDS prior to 1.1.

:pep:`0249#rowcount`

:rtype: int
'''
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.rowcount
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.rowcount() did not fail as expected') # pragma: nocover

    def test_premature(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertEqual(cursor.rowcount, long_(-1))

    def test_rowcount(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @test_rowcount TABLE(i INT);
                        INSERT INTO @test_rowcount(i) VALUES (1),(2),(3);
                    '''
                )
                # When sp_executesql is not used, rowcount is always valid.
                self.assertEqual(cursor.rowcount, 3)

                cursor.execute(
                    '''
                        DECLARE @test_rowcount TABLE(i INT);
                        INSERT INTO @test_rowcount(i) VALUES (:0),(:1),(:2);
                    ''',
                    (1, 2, 3)
                )
                self.assertEqual(
                    cursor.rowcount,
                    3 if self.have_valid_rowcount else -1
                )
