import datetime
from decimal import Decimal
import uuid

from .base import TestExternalDatabase
from .compat import long_, PY3, unicode_

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
                for value in  (
                        Decimal(0),
                        Decimal('1.1'),
                        Decimal('-1.234567'),
                        Decimal('1234567.890123'),
                        Decimal('-1234567.890123'),
                ):
                    row = self.parameter_type(cursor, value)
                    self.assertEqual('decimal', row.Type)
                    parts = str(abs(value)).split('.')
                    scale = len(parts[1]) if len(parts) > 1 else 0
                    precision = len(parts[0]) + scale

                    self.assertEqual(row.Precision, precision)
                    self.assertEqual(row.Scale, scale)
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
            datetime.time(12, 13, 14, 123000),
            datetime.time(23, 59, 59, 997000),
        )
        # Times are always converted to datetime for compatibility with older FreeTDS versions.
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in values:
                    row = self.parameter_type(cursor, value)
                    self.assertEqual('datetime', row.Type)
                    self.assertEqual(row.Precision, 23)
                    self.assertEqual(row.Scale, 3)
                    self.assertEqual(
                        datetime.datetime(
                            1900, 1, 1,
                            value.hour,
                            value.minute,
                            value.second,
                            value.microsecond,
                        ),
                        row.Value
                    )

    def test_float(self):
        self.assert_type('float', (0.0, -1.1234, 12345.67890))

    def test_string(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                values = [
                    unicode_('*'),
                    unicode_('*' * 8001),
                    unicode_('*' * 8000),
                ]
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
