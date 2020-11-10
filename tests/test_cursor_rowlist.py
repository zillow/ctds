import ctds

from .base import TestExternalDatabase

class TestCursorRowList(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.RowList.__doc__,
            '''\
A :ref:`sequence <python:sequence>` object which buffers result set rows
in a lightweight manner. Python objects wrapping the columnar data are
only created when the data is actually accessed.
'''
        )

    def test_indexerror(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3),(4),(5),(6);
                    SELECT * FROM @{0};
                    '''.format(self.test_indexerror.__name__)
                )
                rows = cursor.fetchall()

        for index in (6, 7):
            try:
                rows[index]
            except IndexError as ex:
                self.assertEqual('index is out of range', str(ex))
            else:
                self.fail('IndexError was not raised for index {0}'.format(index)) # pragma: nocover

    def test_rowlist(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3),(4),(5),(6);
                    SELECT * FROM @{0};
                    '''.format(self.test_rowlist.__name__)
                )
                description = cursor.description
                rows = cursor.fetchall()

        # The rowlist should be accessible after closing the cursor
        # and connection.
        self.assertTrue(isinstance(rows, ctds.RowList))
        self.assertEqual(len(rows), 6)
        self.assertTrue(rows.description is description)
        for index, row in enumerate(rows):
            # The row object should always be the same instance.
            self.assertTrue(isinstance(row, ctds.Row))
            self.assertEqual(id(row), id(rows[index]))
