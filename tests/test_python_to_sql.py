import datetime
import decimal
import uuid

from .base import TestExternalDatabase
from .compat import long_, PY3, unichr_, unicode_

class TestPythonToSQL(TestExternalDatabase):
    '''Unit tests related to Python to SQL type conversion.
    '''

    def test_bit(self):
        self.assert_type('bit', (True, False))

    def test_tinyint(self):
        values = (0, 123, 255)
        self.assert_type('tinyint', values)
        if not PY3: # pragma: nocover
            self.assert_type('tinyint', (long_(value) for value in values))

    def test_smallint(self):
        values = (-2**15, -255, -1, 256, 2**15 - 1)
        self.assert_type('smallint', values)
        if not PY3: # pragma: nocover
            self.assert_type('smallint', (long_(value) for value in values))

    def test_int(self):
        values = (-2**31, -2**16, 2**16, 2**31 - 1)
        self.assert_type('int', values)
        if not PY3: # pragma: nocover
            self.assert_type('int', (long_(value) for value in values))

    def test_bigint(self):
        self.assert_type('bigint', (long_(-2**63), long_(-2**31 - 1), long_(2**31), long_(2**63 - 1)))

    def test_bigint_overflow(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (-2**63 - 1, 2**63):
                    self.assertRaises(OverflowError, self.parameter_type, cursor, value)

    def test_none(self):
        self.assert_type(None, (None,))

    def test_decimal(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value, precision, scale in  (
                        (decimal.Decimal(0), 1, 0),
                        (decimal.Decimal('1.1'), 2, 1),
                        (decimal.Decimal('0.1'), 2, 1),
                        (decimal.Decimal('-1.234567'), 7, 6),
                        (decimal.Decimal('1234567.890123'), 13, 6),
                        (decimal.Decimal('-1234567.890123'), 13, 6),
                        (decimal.Decimal('4.01E+8'), 9, 0),
                        (decimal.Decimal('-1.54E+11'), 12, 0),
                        (decimal.Decimal('0.004354'), 7, 6),
                        (decimal.Decimal('900.0'), 4, 1),
                        (decimal.Decimal('54.234246451650'), 14, 12),
                        (
                            decimal.Decimal('.{0}'.format('1' * decimal.getcontext().prec)),
                            decimal.getcontext().prec + 1,
                            decimal.getcontext().prec
                        ),
                ):
                    row = self.parameter_type(cursor, value)
                    self.assertEqual('decimal', row.Type)

                    self.assertEqual(row.Precision, precision, repr(value))
                    self.assertEqual(row.Scale, scale, repr(value))
                    self.assertEqual(row.Value, value)

    def test_date(self):
        values = (
            datetime.date(2001, 1, 1),
            datetime.date(2001, 12, 31),
        )
        # Dates are always converted to datetime for compatibility with older FreeTDS versions.
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in values:
                    row = self.parameter_type(cursor, value)
                    self.assertEqual('datetime', row.Type)
                    self.assertEqual(row.Precision, 23)
                    self.assertEqual(row.Scale, 3)
                    self.assertEqual(
                        datetime.datetime(value.year, value.month, value.day),
                        row.Value
                    )

    def test_time(self):
        values = (
            datetime.time(0, 0, 0),
            datetime.time(12, 13, 14, 123456),
            datetime.time(23, 59, 59, 997000),
        )
        # Times are always converted to datetime for compatibility with older FreeTDS versions.
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in values:
                    row = self.parameter_type(cursor, value)
                    self.assertEqual(
                        'time' if self.tdstime_supported else 'datetime',
                        row.Type
                    )
                    self.assertEqual(
                        16 if self.tdstime_supported else 23,
                        row.Precision
                    )
                    self.assertEqual(
                        7 if self.tdstime_supported else 3,
                        row.Scale
                    )
                    self.assertEqual(
                        datetime.time(
                            value.hour,
                            value.minute,
                            value.second,
                            value.microsecond,
                        )
                        if self.tdstime_supported else
                        datetime.datetime(
                            1900, 1, 1,
                            value.hour,
                            value.minute,
                            value.second,
                            # TDSTIME support is required for microsecond resolution
                            (value.microsecond // 1000) * 1000,
                        ),
                        row.Value
                    )

    def test_datetime(self):
        values = (
            datetime.datetime(1753, 1, 1, 0, 0),
            datetime.datetime(9999, 12, 31, 23, 59, 59, 997 * 1000),
        )
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in values:
                    row = self.parameter_type(cursor, value)
                    self.assertEqual('datetime', row.Type)
                    self.assertEqual(row.Precision, 23)
                    self.assertEqual(row.Scale, 3)
                    self.assertEqual(value, row.Value)

    def test_float(self):
        self.assert_type('float', (0.0, -1.1234, 12345.67890))

    def test_string(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                values = [
                    unicode_('*'),
                    unicode_('*' * 3999),
                    unicode_('*' * 4000),
                    unicode_('*' * 4001),
                    unicode_('*' * 7999),
                    unicode_('*' * 8000),
                    unicode_('*' * 8001),
                    b'this a string, but as bytes',
                    unicode_(b'quand une dr\xc3\xb4le', encoding='utf-8'),
                ]
                if self.nchars_supported: # pragma: nobranch
                    values.extend([
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 4001,
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 4000,
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 3999,
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 8001,
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 8000,
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 7999,
                    ])
                    if self.use_utf16 and self.UCS4_SUPPORTED: # pragma: nobranch
                        values.extend([
                            unichr_(127802) * 3999,
                            unichr_(127802) * 4000,
                            unichr_(127802) * 4001,
                        ])
                for value in values:
                    cursor.execute('SELECT :0 AS Value', (value,))
                    row = cursor.fetchone()
                    self.assertEqual(len(value), len(row.Value))
                    self.assertEqual(value, row.Value)

    def test_binary(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                values = [
                    b'*',
                    b'*' * 8001,
                    b'*' * 8000,
                ]
                for value in values:
                    cursor.execute('SELECT :0 AS Value', (value,))
                    row = cursor.fetchone()
                    self.assertEqual(len(value), len(row.Value))
                    self.assertEqual(value, row.Value)

    def test_uuid(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                values = [
                    uuid.uuid1()
                ]
                for value in values:
                    cursor.execute('SELECT :0 AS Value', (value,))
                    row = cursor.fetchone()
                    # FreeTDS doesn't support sending raw GUID values, so they are sent as CHAR.
                    self.assertTrue(isinstance(row.Value, unicode_))
                    self.assertEqual(unicode_(value), row.Value)
