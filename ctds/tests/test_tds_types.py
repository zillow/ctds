from datetime import date, datetime
from decimal import Decimal

import ctds

from .base import TestExternalDatabase
from .compat import unicode_

class TestCursorTypes(TestExternalDatabase):
    '''Unit tests related to the SQL type wrappers.
    '''

    def test_varbinary(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (None, b' ', b'1234567890'):
                    for size in (None, 1, 3, 500):
                        kwargs = {}
                        if size is not None:
                            kwargs['size'] = size
                            expected_size = size
                        else:
                            expected_size = 1 if value is None else max(1, len(value))

                        varbinary = ctds.SqlVarBinary(value, **kwargs)
                        self.assertEqual(id(varbinary.value), id(value))
                        self.assertEqual(varbinary.size, expected_size)
                        self.assertEqual(varbinary.tdstype, ctds.VARBINARY)

                        cursor.execute(
                            '''
                            SELECT :0
                            ''',
                            (varbinary,)
                        )
                        self.assertEqual(
                            [tuple(row) for row in cursor.fetchall()],
                            [
                                (value if value is None else value[0:expected_size],)
                            ]
                        )

    def test_varchar(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        unicode_(''),
                        None,
                        unicode_(' '),
                        unicode_('one'),
                        unicode_(b'hola \xc2\xa9', encoding='utf-8')
                ):
                    for size in (None, 1, 3, 500):
                        kwargs = {}
                        if size is not None:
                            kwargs['size'] = size
                            expected_size = size
                        else:
                            expected_size = 1 if value is None else max(1, len(value.encode('utf-8')))

                        varchar = ctds.SqlVarChar(value, **kwargs)
                        self.assertEqual(id(varchar.value), id(value))
                        self.assertEqual(varchar.size, expected_size)
                        self.assertEqual(varchar.tdstype, ctds.VARCHAR)

                        cursor.execute(
                            '''
                            SELECT :0
                            ''',
                            (varchar,)
                        )

                        # TODO: fix this once supported by FreeTDS
                        # Currently FreeTDS (really the db-lib API) will
                        # turn empty string to NULL
                        if value == '' and self.use_sp_executesql:
                            value = None
                        self.assertEqual(
                            [tuple(row) for row in cursor.fetchall()],
                            [
                                (value if value is None else value[0:expected_size],)
                            ]
                        )

    def test_nvarchar(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                inputs = [
                    None,
                    unicode_(''),
                    unicode_(' '),
                    unicode_('one'),
                ]
                if self.nchars_supported:
                    inputs.extend([
                        unicode_(b'hola \xc5\x93 \xe3\x83\x9b', encoding='utf-8'),
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 4000
                    ])
                for value in inputs:
                    for size in (None, 1, 3, 500):
                        kwargs = {}
                        if size is not None:
                            kwargs['size'] = size
                            expected_size = size
                        else:
                            expected_size = 1 if value is None else max(1, len(value))

                        nvarchar = ctds.SqlNVarChar(value, **kwargs)
                        self.assertEqual(nvarchar.size, expected_size)
                        self.assertEqual(nvarchar.tdstype, ctds.NVARCHAR if self.nchars_supported else ctds.VARCHAR)

                        cursor.execute(
                            '''
                            SELECT :0
                            ''',
                            (nvarchar,)
                        )

                        # TODO: fix this once supported by FreeTDS
                        # Currently FreeTDS (really the db-lib API) will
                        # turn empty string to NULL
                        if value == '' and self.use_sp_executesql:
                            value = None
                        self.assertEqual(
                            [tuple(row) for row in cursor.fetchall()],
                            [
                                (value if value is None else value[0:expected_size],)
                            ]
                        )

    def test_tinyint(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (123, None):
                    tinyint = ctds.SqlTinyInt(case)
                    self.assertEqual(id(tinyint.value), id(case))
                    self.assertEqual(tinyint.size, -1)
                    self.assertEqual(tinyint.tdstype, ctds.TINYINT)

                    cursor.execute(
                        '''
                        SELECT :0
                        ''',
                        (tinyint,)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (case,)
                        ]
                    )

    def test_tinyint_overflow(self):
        for case in (256, -1, -256):
            self.assertRaises(OverflowError, ctds.SqlTinyInt, case)

    def test_smallint(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (123, None):
                    smallint = ctds.SqlSmallInt(case)
                    self.assertEqual(id(smallint.value), id(case))
                    self.assertEqual(smallint.size, -1)
                    self.assertEqual(smallint.tdstype, ctds.SMALLINT)

                    cursor.execute(
                        '''
                        SELECT :0
                        ''',
                        (smallint,)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (case,)
                        ]
                    )

    def test_smallint_overflow(self):
        for case in (2**16, -2**16):
            self.assertRaises(OverflowError, ctds.SqlSmallInt, case)

    def test_int(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (123, None):
                    int_ = ctds.SqlInt(case)
                    self.assertEqual(id(int_.value), id(case))
                    self.assertEqual(int_.size, -1)
                    self.assertEqual(int_.tdstype, ctds.INT)

                    cursor.execute(
                        '''
                        SELECT :0
                        ''',
                        (int_,)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (case,)
                        ]
                    )

    def test_int_overflow(self):
        for case in (2**32, -2**32):
            self.assertRaises(OverflowError, ctds.SqlInt, case)

    def test_bigint(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (
                        0,
                        -1,
                        255,
                        256,
                        -255,
                        2**15 - 1,
                        -2 ** 15 + 1,
                        2**63 - 1,
                        -2**63 + 1,
                        None
                ):
                    bigint = ctds.SqlBigInt(case)
                    self.assertEqual(id(bigint.value), id(case))
                    self.assertEqual(bigint.size, -1)
                    self.assertEqual(bigint.tdstype, ctds.BIGINT)

                    cursor.execute(
                        '''
                        SELECT :0
                        ''',
                        (bigint,)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (case,)
                        ]
                    )

    def test_bigint_overflow(self):
        for case in (2**64, -2**64):
            self.assertRaises(OverflowError, ctds.SqlBigInt, case)

    def test_binary(self):
        python = b'1234567890'
        binary = ctds.SqlBinary(python)
        self.assertEqual(id(binary.value), id(python))
        self.assertEqual(binary.size, len(python))
        self.assertEqual(binary.tdstype, ctds.BINARY)

        # The BINARY mimimum size is 1
        python = b''
        binary = ctds.SqlBinary(python)
        self.assertEqual(id(binary.value), id(python))
        self.assertEqual(binary.size, 1)
        self.assertEqual(binary.tdstype, ctds.BINARY)

        binary = ctds.SqlBinary(None)
        self.assertEqual(binary.value, None)
        self.assertEqual(binary.size, 1)
        self.assertEqual(binary.tdstype, ctds.BINARY)

    def test_date(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case, expected in (
                        (date(2001, 11, 5), datetime(2001, 11, 5)),
                        (None, None),
                ):
                    date_ = ctds.SqlDate(case)
                    self.assertEqual(id(date_.value), id(case))
                    self.assertEqual(date_.size, -1)
                    self.assertEqual(date_.tdstype, ctds.DATE)

                    cursor.execute(
                        '''
                        SELECT :0
                        ''',
                        (date_,)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(expected,)]
                    )

    def test_date_typeerror(self):
        for case in (
                unicode_(''),
                ''.encode('ascii'),
                object(),
                1,
                1.1,
                True
        ):
            self.assertRaises(TypeError, ctds.SqlDate, case)

    def test_decimal(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value, kwargs, expected in (
                        (1.234, {'scale': 1}, Decimal('1.2')),
                        (1234.123456789, {'scale': 4}, Decimal('1234.1234')),
                        (None, {}, None),
                ):
                    decimal = ctds.SqlDecimal(value, **kwargs)
                    self.assertEqual(id(decimal.value), id(value))
                    self.assertEqual(decimal.size, -1)
                    self.assertEqual(decimal.tdstype, ctds.DECIMAL)

                    cursor.execute(
                        '''
                        SELECT :0, CONVERT(VARCHAR(50), :0)
                        ''',
                        (decimal,)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(expected, None if expected is None else str(expected))]
                    )
