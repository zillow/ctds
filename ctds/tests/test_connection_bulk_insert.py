import datetime
from decimal import Decimal

import ctds

from .base import TestExternalDatabase
from .compat import unicode_


class TestConnectionBulkInsert(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.bulk_insert.__doc__,
            '''\
bulk_insert(table, rows, batch_size=None, tablock=False)

Bulk insert rows into a given table.
This method utilizes the `BULK INSERT` functionality of SQL Server
to efficiently insert large amounts of data into a table. By default,
rows are not validated until all rows have been processed.

An optional batch size may be specified to validate the inserted rows
after `batch_size` rows have been copied to server.

:param str table: The table in which to insert the rows.
:param rows: An iterable of data rows. Data rows are Python `sequence`
    objects. Each item in the data row is inserted into the table in
    sequential order.
:type rows: :ref:`typeiter <python:typeiter>`
:param int batch_size: An optional batch size.
:param bool tablock: Should the `TABLOCK` hint be passed.
:return: The number of rows saved to the table.
:rtype: int
'''
        )

    def test_typeerror(self):
        cases = (
            ((None, ()), {}),
            ((True, ()), {}),
            ((1234, ()), {}),
            ((object(), ()), {}),

            (('table', None), {}),
            (('table', True), {}),
            (('table', 1234), {}),
            (('table', object()), {}),

            (('table', object()), {'batch_size': '1234'}),
            (('table', object()), {'batch_size': True}),
            (('table', object()), {'batch_size': object()}),
        )

        with self.connect() as connection:
            for args, kwargs in cases:
                try:
                    connection.bulk_insert(*args, **kwargs)
                except TypeError:
                    pass
                else:
                    self.fail('.bulk_insert() did not fail as expected') # pragma: nocover

    def test_overflowerror(self):
        with self.connect() as connection:
            try:
                connection.bulk_insert('table', (), batch_size=2 ** 64)
            except OverflowError:
                pass
            else:
                self.fail('.bulk_insert() did not fail as expected') # pragma: nocover

    def test_ctds_notsupported(self):
        with self.connect(enable_bcp=False) as connection:
            try:
                connection.bulk_insert('table', ())
            except ctds.NotSupportedError:
                pass
            else:
                self.fail('.bulk_insert() did not fail as expected') # pragma: nocover

    def test_closed(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        self.assertRaises(ctds.InterfaceError, connection.bulk_insert, 'temp', (1, 2))

    def test_insert(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey  INT NOT NULL PRIMARY KEY,
                            Date        DATETIME,
                            String      VARCHAR(1000),
                            Bytes       VARBINARY(1000),
                            Decimal     DECIMAL(7,3)
                        )
                        '''.format(self.test_insert.__name__)
                    )

                rows = 100
                inserted = connection.bulk_insert(
                    self.test_insert.__name__,
                    (
                        (
                            ix,
                            datetime.datetime(2000 + ix, 1, 1) if ix < 1000 else None,
                            unicode_('this is row {0}'.format(ix)),
                            bytes(ix),
                            Decimal(str(ix + .125))
                        )
                        for ix in range(0, rows)
                    )
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT COUNT(1) FROM {0}'.format(self.test_insert.__name__))
                    self.assertEqual(cursor.fetchone()[0], rows)

            finally:
                connection.rollback()

    def test_insert_batch(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey INT NOT NULL PRIMARY KEY,
                            Date       DATETIME,
                            String     VARCHAR(1000),
                            Bytes      VARBINARY(1000),
                            Decimal    DECIMAL(7,3)
                        )
                        '''.format(self.test_insert.__name__)
                    )

                rows = 100
                inserted = connection.bulk_insert(
                    self.test_insert.__name__,
                    (
                        (
                            ix,
                            datetime.datetime(2000 + ix, 1, 1) if ix < 1000 else None,
                            unicode_('this is row {0}'.format(ix)),
                            bytes(ix),
                            Decimal(str(ix + .125))
                        )
                        for ix in range(0, rows)
                    ),
                    batch_size=100
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT COUNT(1) FROM {0}'.format(self.test_insert.__name__))
                    self.assertEqual(cursor.fetchone()[0], rows)

            finally:
                connection.rollback()

    def test_insert_tablock(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey INT NOT NULL PRIMARY KEY,
                            Date       DATETIME,
                            String     VARCHAR(1000),
                            Bytes      VARBINARY(1000),
                            Decimal    DECIMAL(7,3)
                        )
                        '''.format(self.test_insert.__name__)
                    )

                rows = 100
                inserted = connection.bulk_insert(
                    self.test_insert.__name__,
                    (
                        (
                            ix,
                            datetime.datetime(2000 + ix, 1, 1) if ix < 1000 else None,
                            unicode_('this is row {0}'.format(ix)),
                            bytes(ix),
                            Decimal(str(ix + .125))
                        )
                        for ix in range(0, rows)
                    ),
                    tablock=True
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT COUNT(1) FROM {0}'.format(self.test_insert.__name__))
                    self.assertEqual(cursor.fetchone()[0], rows)

            finally:
                connection.rollback()
