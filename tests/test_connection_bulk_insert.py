import datetime
from decimal import Decimal
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import PY35, unicode_


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
    Version 1.9 supports passing rows as `:py:class:`dict`. Keys must map
    to column names and must exist for all non-NULL columns.
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
                self.assertRaises(TypeError, connection.bulk_insert, *args, **kwargs)

    def test_overflowerror(self):
        with self.connect() as connection:
            self.assertRaises(OverflowError, connection.bulk_insert, 'table', (), batch_size=2 ** 64)

    def test_ctds_notsupported(self):
        with self.connect(enable_bcp=False) as connection:
            self.assertRaises(ctds.NotSupportedError, connection.bulk_insert, 'table', ())

    def test_closed(self):
        connection = self.connect()
        self.assertEqual(connection.close(), None)

        self.assertRaises(ctds.InterfaceError, connection.bulk_insert, 'temp', (1, 2))

    def test_string(self):
        parameter = unicode_(b'what DB encoding is used? \xc2\xbd', encoding='utf-8')
        for index, param in enumerate(
                (
                    parameter,
                    ctds.Parameter(parameter),
                )
        ):
            with self.connect(autocommit=False) as connection:
                try:
                    with connection.cursor() as cursor:
                        cursor.execute(
                            '''
                            CREATE TABLE {0}
                            (
                                String VARCHAR(1000) COLLATE SQL_Latin1_General_CP1_CI_AS
                            )
                            '''.format(self.test_string.__name__)
                        )

                    with warnings.catch_warnings(record=True) as warns:
                        connection.bulk_insert(
                            self.test_string.__name__,
                            [
                                (param,)
                            ]
                        )

                    # Python 3.4 and lower has an issue recording the same warning multiple times.
                    # See http://bugs.python.org/issue4180.
                    if index == 0 or PY35: # pragma: nobranch
                        self.assertEqual(len(warns), 1)
                        self.assertEqual(
                            [str(warn.message) for warn in warns],
                            [
                                '''\
Direct bulk insert of a Python str object may result in unexpected character \
encoding. It is recommended to explicitly encode Python str values for bulk \
insert.\
'''
                            ] * len(warns)
                        )
                        self.assertEqual(
                            [warn.category for warn in warns],
                            [Warning] * len(warns)
                        )

                    with connection.cursor() as cursor:
                        cursor.execute('SELECT * FROM {0}'.format(self.test_string.__name__))
                        self.assertEqual(
                            [tuple(row) for row in cursor.fetchall()],
                            [
                                (parameter.encode('utf-8').decode('latin-1'),)
                            ]
                        )

                finally:
                    connection.rollback()

    def test_string_warning_as_error(self):
        parameter = unicode_(b'what DB encoding is used? \xc2\xbd', encoding='utf-8')
        try:
            with self.connect(autocommit=False) as connection:
                try:
                    with connection.cursor() as cursor:
                        cursor.execute(
                            '''
                            CREATE TABLE {0}
                            (
                                String VARCHAR(1000) COLLATE SQL_Latin1_General_CP1_CI_AS
                            )
                            '''.format(self.test_string_warning_as_error.__name__)
                        )

                    with warnings.catch_warnings():
                        warnings.simplefilter('error')
                        try:
                            connection.bulk_insert(
                                self.test_string_warning_as_error.__name__,
                                [
                                    (parameter,)
                                ]
                            )
                        except Warning as warn:
                            self.assertEqual(
                                '''\
Direct bulk insert of a Python str object may result in unexpected character \
encoding. It is recommended to explicitly encode Python str values for bulk \
insert.\
''',
                                str(warn)
                            )
                        else:
                            self.fail('.bulk_insert() did not fail as expected') # pragma: nocover
                finally:
                    connection.rollback()
        except ctds.DatabaseError: # pragma: nocover
            # Avoid sporadic "Unexpected EOF from the server"
            # when running against SQL Server on linux.
            pass

    def test_insert(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey INT NOT NULL PRIMARY KEY,
                            Date       DATETIME,
                            /*
                                FreeTDS' bulk insert implementation doesn't seem to work
                                properly with *VARCHAR(MAX) columns.
                            */
                            String     VARCHAR(1000) COLLATE SQL_Latin1_General_CP1_CI_AS,
                            Unicode    NVARCHAR(100),
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
                            datetime.datetime(2000 + ix, 1, 1) if ix % 2 else None,
                            ctds.SqlVarChar(
                                unicode_(b'this is row {0} \xc2\xbd', encoding='utf-8').format(ix).encode('latin-1')
                            ),
                            ctds.SqlVarChar((unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 100).encode('utf-16le')),
                            bytes(ix + 1),
                            Decimal(str(ix + .125))
                        )
                        for ix in range(0, rows)
                    )
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(self.test_insert.__name__))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (
                                ix,
                                datetime.datetime(2000 + ix, 1, 1) if ix % 2 else None,
                                unicode_(b'this is row {0} \xc2\xbd', encoding='utf-8').format(ix),
                                unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 100,
                                bytes(ix + 1),
                                Decimal(str(ix + .125))
                            )
                            for ix in range(0, rows)
                        ]
                    )

            finally:
                connection.rollback()

    def test_insert_dict(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey INT NOT NULL PRIMARY KEY,
                            Date       DATETIME NULL,
                            /*
                                FreeTDS' bulk insert implementation doesn't seem to work
                                properly with *VARCHAR(MAX) columns.
                            */
                            String     VARCHAR(1000) COLLATE SQL_Latin1_General_CP1_CI_AS NOT NULL,
                            Unicode    NVARCHAR(100) NULL,
                            Bytes      VARBINARY(1000) NULL,
                            Decimal    DECIMAL(7,3) NOT NULL
                        )
                        '''.format(self.test_insert_dict.__name__)
                    )

                rows = 100
                inserted = connection.bulk_insert(
                    self.test_insert_dict.__name__,
                    (
                        {
                            'Bytes': bytes(ix + 1),
                            'Date': datetime.datetime(2000 + ix, 1, 1) if ix % 2 else None,
                            'Decimal': Decimal(str(ix + .125)),
                            'PrimaryKey': ix,
                            'String': ctds.SqlVarChar(
                                unicode_(b'this is row {0} \xc2\xbd', encoding='utf-8').format(ix).encode('latin-1')
                            ),
                            'Unicode': ctds.SqlVarChar(
                                (unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 100).encode('utf-16le')
                            ),
                        }
                        for ix in range(0, rows)
                    )
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(self.test_insert_dict.__name__))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (
                                ix,
                                datetime.datetime(2000 + ix, 1, 1) if ix % 2 else None,
                                unicode_(b'this is row {0} \xc2\xbd', encoding='utf-8').format(ix),
                                unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 100,
                                bytes(ix + 1),
                                Decimal(str(ix + .125))
                            )
                            for ix in range(0, rows)
                        ]
                    )

            finally:
                connection.rollback()

    def test_insert_dict_nullable(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey     INT NOT NULL PRIMARY KEY,
                            NullableInt    INT NULL,
                            NullableBinary VARBINARY(2) NULL
                        )
                        '''.format(self.test_insert_dict_nullable.__name__)
                    )

                rows = 100
                inserted = connection.bulk_insert(
                    self.test_insert_dict_nullable.__name__,
                    (
                        {
                            'NullableInt': ix * 1000,
                            'PrimaryKey': ix,
                            'NullableBinary': unicode_(ix).encode('utf-8')
                        }
                        if ix % 2 else
                        {
                            'PrimaryKey': ix,
                        }
                        for ix in range(0, rows)
                    )
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(self.test_insert_dict_nullable.__name__))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (
                                ix,
                                ix * 1000 if ix % 2 else None,
                                unicode_(ix).encode('utf-8') if ix % 2 else None
                            )
                            for ix in range(0, rows)
                        ]
                    )

            finally:
                connection.rollback()

    def test_insert_dict_keyerror(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey INT NOT NULL PRIMARY KEY
                        )
                        '''.format(self.test_insert_dict_keyerror.__name__)
                    )

                rows = 100

                self.assertRaises(
                    KeyError,
                    connection.bulk_insert,
                    self.test_insert_dict_keyerror.__name__,
                    (
                        {
                            'PrimaryKey': ix,
                        }
                        if ix == 0 else
                        {
                            'NotPrimaryKey': ix,
                        }
                        for ix in range(0, rows)
                    )
                )

                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(self.test_insert_dict_keyerror.__name__))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (0,)
                        ]
                    )

            finally:
                connection.rollback()

    def test_insert_nothing(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            PrimaryKey INT NOT NULL PRIMARY KEY,
                        )
                        '''.format(self.test_insert_tablock.__name__)
                    )

                inserted = connection.bulk_insert(
                    self.test_insert_tablock.__name__,
                    ()
                )
                self.assertEqual(inserted, 0)

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
                            PrimaryKey  INT NOT NULL PRIMARY KEY,
                            Date        DATETIME,
                            String      VARCHAR(1000),
                            Unicode     NVARCHAR(1000),
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
                            ctds.SqlVarChar(
                                unicode_(b'this is row {0} \xc2\xbd', encoding='utf-8').format(ix).encode('latin-1')
                            ),
                            ctds.SqlVarChar((unicode_(b'\xe3\x83\x9b', encoding='utf-8')).encode('utf-16le')),
                            bytes(ix + 1),
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
                            Unicode    NVARCHAR(1000),
                            Bytes      VARBINARY(1000),
                            Decimal    DECIMAL(7,3)
                        )
                        '''.format(self.test_insert_batch.__name__)
                    )

                rows = 100
                inserted = connection.bulk_insert(
                    self.test_insert_batch.__name__,
                    (
                        (
                            ix,
                            datetime.datetime(2000 + ix, 1, 1) if ix < 1000 else None,
                            ctds.SqlVarChar(unicode_(b'this is row {0}', encoding='utf-8').format(ix)),
                            ctds.SqlVarChar((unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 100).encode('utf-16le')),
                            bytes(ix + 1),
                            Decimal(str(ix + .125))
                        )
                        for ix in range(0, rows)
                    ),
                    batch_size=10
                )

                self.assertEqual(inserted, rows)

                with connection.cursor() as cursor:
                    cursor.execute('SELECT COUNT(1) FROM {0}'.format(self.test_insert_batch.__name__))
                    self.assertEqual(cursor.fetchone()[0], rows)

            finally:
                connection.rollback()

    def test_insert_invalid_encoding(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            String VARCHAR(1000) COLLATE SQL_Latin1_General_CP1_CI_AS
                        )
                        '''.format(self.test_insert_invalid_encoding.__name__)
                    )

                connection.bulk_insert(
                    self.test_insert_invalid_encoding.__name__,
                    (
                        (
                            ctds.SqlVarChar(unicode_(b'\xc2\xbd', encoding='utf-8').encode('utf-8')),
                        ),
                    )
                )

                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        SELECT
                            String,
                            CONVERT(VARBINARY(1000), String) AS Bytes
                        FROM
                            {0}
                        '''.format(self.test_insert_invalid_encoding.__name__)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (unicode_(b'\xc2\xbd', encoding='latin-1'), b'\xc2\xbd',)
                        ]
                    )

            finally:
                connection.rollback()

    def test_insert_empty_string(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0}
                        (
                            String VARCHAR(1000) COLLATE SQL_Latin1_General_CP1_CI_AS
                        )
                        '''.format(self.test_insert_empty_string.__name__)
                    )

                with warnings.catch_warnings(record=True) as warns:
                    connection.bulk_insert(
                        self.test_insert_empty_string.__name__,
                        (
                            (
                                ctds.SqlVarChar(unicode_(b'\xc2\xbd', encoding='utf-8').encode('latin-1')),
                            ),
                            (
                                ctds.SqlVarChar(unicode_('').encode('latin-1')),
                            ),
                        )
                    )
                if self.bcp_empty_string_supported:
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        [
                            '''\
"" converted to NULL for compatibility with FreeTDS. \
Please update to a recent version of FreeTDS. \
'''
                        ] * len(warns)
                    )

                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        SELECT
                            String,
                            CONVERT(VARBINARY(1000), String) AS Bytes
                        FROM
                            {0}
                        '''.format(self.test_insert_empty_string.__name__)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (unicode_(b'\xc2\xbd', encoding='utf-8'), b'\xbd',),
                            (unicode_('') if self.bcp_empty_string_supported else None, None,),
                        ]
                    )

            finally:
                connection.rollback()
