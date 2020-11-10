from .base import TestExternalDatabase
from .compat import long_, unicode_

class TestCursorRow(TestExternalDatabase):

    def test_subscript(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (1, 'two', 'three', 4)
                cursor.execute(
                    '''
                    SELECT
                        :0 AS Col1,
                        :1 AS Col2,
                        :2 AS Col3,
                        :3 AS Col4
                    ''',
                    args
                )
                rows = cursor.fetchall()

        self.assertEqual(len(rows), 1)

        row = rows[0]
        self.assertTrue('Col1' in row)
        self.assertEqual(row['Col1'], args[0])
        self.assertEqual(row[unicode_('Col1')], args[0])

        self.assertTrue('Col2' in row)
        self.assertEqual(row['Col2'], args[1])
        self.assertEqual(row[unicode_('Col2')], args[1])

        self.assertTrue('Col3' in row)
        self.assertEqual(row['Col3'], args[2])
        self.assertEqual(row[unicode_('Col3')], args[2])

        self.assertTrue('Col4' in row)
        self.assertEqual(row['Col4'], args[3])
        self.assertEqual(row[unicode_('Col4')], args[3])

        self.assertFalse(1 in row)
        for key in ('unknown', None, object()):
            self.assertFalse(key in row)
            try:
                row[key]
            except KeyError:
                pass
            else:
                self.fail('row[] lookup did not fail as expected') # pragma: nocover

    def test_index(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (1, 'two', 'three', 4)
                cursor.execute(
                    '''
                    SELECT
                        :0 AS Col1,
                        :1 AS Col2,
                        :2 AS Col3,
                        :3 AS Col4
                    ''',
                    args
                )
                rows = cursor.fetchall()

        self.assertEqual(len(rows), 1)

        row = rows[0]

        self.assertEqual(len(args), len(row))
        for index, value in enumerate(row):
            self.assertEqual(value, args[index])
            self.assertEqual(row[index], args[index])
            self.assertEqual(row[long_(index)], args[index])

            for case, ex in (
                    (-1, IndexError),
                    (4, IndexError),
                    (len(row), IndexError),
                    (2 ** 32, IndexError),
                    (2 ** 64, OverflowError),
            ):
                try:
                    row[case]
                except ex:
                    pass
                else:
                    self.fail('row[{0}] lookup did not fail as expected'.format(case)) # pragma: nocover

    def test_attr(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (1, 'two', 'three', 4)
                cursor.execute(
                    '''
                    SELECT
                        :0 AS Col1,
                        :1 AS Col2,
                        :2 AS Col3,
                        :3 AS Col4
                    ''',
                    args
                )
                rows = cursor.fetchall()

        self.assertEqual(len(rows), 1)

        row = rows[0]

        self.assertTrue(hasattr(row, 'Col1'))
        self.assertEqual(row.Col1, args[0])
        self.assertTrue(hasattr(row, 'Col2'))
        self.assertEqual(row.Col2, args[1])
        self.assertEqual(row.Col3, args[2])
        self.assertTrue(hasattr(row, 'Col3'))
        self.assertEqual(row.Col4, args[3])
        self.assertTrue(hasattr(row, 'Col4'))

        try:
            row.unknown
        except AttributeError:
            pass
        else:
            self.fail('row.attr lookup did not fail as expected') # pragma: nocover

    def test_description(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (1, 'two', 'three', 4)
                cursor.execute(
                    '''
                    SELECT
                        :0 AS Col1,
                        :1 AS Col2,
                        :2 AS Col3,
                        :3 AS Col4
                    ''',
                    args
                )
                description = cursor.description
                row = cursor.fetchone()

        self.assertTrue(row.description is description)

    def test_dict(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (1, 'two', 'three', 4)
                cursor.execute(
                    '''
                    SELECT
                        :0 AS Col1,
                        'unnamed',
                        :1 AS Col2,
                        :2 AS Col3,
                        'another unnamed',
                        :3 AS Col4
                    ''',
                    args
                )
                rows = cursor.fetchall()

        row = rows[0]
        self.assertEqual(row.dict(), {
            'Col1': 1,
            1: 'unnamed',
            'Col2': 'two',
            'Col3': 'three',
            4: 'another unnamed',
            'Col4': 4,
        })
