import datetime
from decimal import Decimal

import ctds

from .base import TestExternalDatabase
from .compat import int_, long_, unicode_

class TestSqlBigInt(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlBigInt.__doc__,
            '''\
SqlBigInt(value)

SQL BIGINT type wrapper.

:param int value: The integer value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        int_(1234),
                        long_(1234),
                        2 ** 63 - 1,
                        -2 ** 63 + 1,
                        None
                ):
                    wrapper = ctds.SqlBigInt(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'bigint' if value is not None else None)
                    self.assertEqual(row.Value, wrapper.value)

    def test_overflowerror(self):
        self.assertRaises(OverflowError, ctds.SqlBigInt, 2 ** 63)

    def test_typeerror(self):
        for value in (
                object(),
                unicode_('1234'),
                b'123',
        ):
            self.assertRaises(TypeError, ctds.SqlBigInt, value)


class TestSqlBinary(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlBinary.__doc__,
            '''\
SqlBinary(value)

SQL BINARY type wrapper.

:param object value: The value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        b'1234',
                        unicode_('1234'),
                        None
                ):
                    wrapper = ctds.SqlBinary(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, len(value) if value is not None else 1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'binary' if value is not None else None)
                    expected = wrapper.value.encode() if isinstance(value, unicode_) else wrapper.value
                    self.assertEqual(row.Value, expected)

    def test_typeerror(self):
        for value in (
                object(),
                int_(1234),
                long_(1234),
        ):
            self.assertRaises(TypeError, ctds.SqlBinary, value)


class TestSqlChar(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlChar.__doc__,
            '''\
SqlChar(value)

SQL CHAR type wrapper.

:param object value: The value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        b'1234',
                        unicode_('1234'),
                        None
                ):
                    wrapper = ctds.SqlChar(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, len(value) if value is not None else 1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'char' if value is not None else None)
                    expected = wrapper.value.decode() if isinstance(value, bytes) else wrapper.value
                    self.assertEqual(row.Value, expected)

    def test_typeerror(self):
        for value in (
                object(),
                int_(1234),
                long_(1234),
        ):
            self.assertRaises(TypeError, ctds.SqlChar, value)


class TestSqlDate(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlDate.__doc__,
            '''\
SqlDate(value)

SQL DATE type wrapper.

:param datetime.date value: The date value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        datetime.date(2001, 2, 3),
                        datetime.datetime(2001, 2, 3),
                        None
                ):
                    wrapper = ctds.SqlDate(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    # Dates are always converted to datetime for compatibility with older FreeTDS versions.
                    self.assertEqual(row.Type, 'datetime' if value is not None else None)
                    self.assertEqual(
                        row.Value,
                        datetime.datetime(value.year, value.month, value.day) if value is not None else None
                    )

    def test_typeerror(self):
        for value in (
                object(),
                unicode_('1234'),
                b'123',
                int_(1234),
                long_(1234),
        ):
            self.assertRaises(TypeError, ctds.SqlDate, value)


class TestSqlInt(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlInt.__doc__,
            '''\
SqlInt(value)

SQL INT type wrapper.

:param int value: The integer value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        int_(1234),
                        long_(1234),
                        2 ** 31 - 1,
                        -2 ** 31 + 1,
                        None
                ):
                    wrapper = ctds.SqlInt(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'int' if value is not None else None)
                    self.assertEqual(row.Value, value)

    def test_overflowerror(self):
        self.assertRaises(OverflowError, ctds.SqlInt, 2 ** 31)

    def test_typeerror(self):
        for value in (
                object(),
                unicode_('1234'),
                b'123',
        ):
            self.assertRaises(TypeError, ctds.SqlInt, value)


class TestSqlSmallInt(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlSmallInt.__doc__,
            '''\
SqlSmallInt(value)

SQL SMALLINT type wrapper.

:param int value: The integer value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        int_(1234),
                        long_(1234),
                        2 ** 15 - 1,
                        -2 ** 15 + 1,
                        None
                ):
                    wrapper = ctds.SqlSmallInt(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'smallint' if value is not None else None)
                    self.assertEqual(row.Value, value)

    def test_overflowerror(self):
        self.assertRaises(OverflowError, ctds.SqlSmallInt, 2 ** 15)

    def test_typeerror(self):
        for value in (
                object(),
                unicode_('1234'),
                b'123',
        ):
            self.assertRaises(TypeError, ctds.SqlSmallInt, value)

class TestSqlTinyInt(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlTinyInt.__doc__,
            '''\
SqlTinyInt(value)

SQL TINYINT type wrapper.

:param int value: The integer value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        int_(123),
                        long_(123),
                        2 ** 8 - 1,
                        None
                ):
                    wrapper = ctds.SqlTinyInt(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'tinyint' if value is not None else None)
                    self.assertEqual(row.Value, value)

    def test_overflowerror(self):
        self.assertRaises(OverflowError, ctds.SqlTinyInt, 2 ** 8)

    def test_typeerror(self):
        for value in (
                object(),
                unicode_('1234'),
                b'123',
        ):
            self.assertRaises(TypeError, ctds.SqlTinyInt, value)


class TestSqlVarBinary(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlVarBinary.__doc__,
            '''\
SqlVarBinary(value, size=None)

SQL VARBINARY type wrapper.

:param object value: The value to wrap or `None`.
:param int size: An optional size override. This value will be used for
    the output parameter buffer size. It can also be used to truncate the
    input parameter.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        b'1234',
                        unicode_('1234'),
                        None
                ):
                    wrapper = ctds.SqlVarBinary(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, len(value) if value is not None else 1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'varbinary' if value is not None else None)
                    expected = wrapper.value.encode() if isinstance(value, unicode_) else wrapper.value
                    self.assertEqual(row.Value, expected)
                    self.assertEqual(row.MaxLength, len(value) if value is not None else None)

    def test_size(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value, size in (
                        (b'1234', 14),
                        (b'1234', 1),
                        (b'*', 5000),
                        (b'*' * 5000, 5000),
                        (None, 14),
                ):
                    wrapper = ctds.SqlVarBinary(value, size=size)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, size)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'varbinary' if value is not None else None)
                    expected = wrapper.value.encode() if isinstance(value, unicode_) else wrapper.value
                    self.assertEqual(row.Value, expected[:size] if value is not None else None)
                    self.assertEqual(row.MaxLength, size if value is not None else None)

    def test_typeerror(self):
        for value in (
                object(),
                int_(1234),
                long_(1234),
        ):
            self.assertRaises(TypeError, ctds.SqlVarBinary, value)

    def test_size_typeerror(self):
        for size in (
                None,
                object(),
                '1234'
        ):
            self.assertRaises(TypeError, ctds.SqlVarBinary, None, size)


class TestSqlVarChar(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlVarChar.__doc__,
            '''\
SqlVarChar(value, size=None)

SQL VARCHAR type wrapper.

:param object value: The value to wrap or `None`.
:param int size: An optional size override. This value will be used for
    the output parameter buffer size. It can also be used to truncate the
    input parameter.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        b'1234',
                        unicode_('1234'),
                        None
                ):
                    wrapper = ctds.SqlVarChar(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, len(value) if value is not None else 1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'varchar' if value is not None else None)
                    expected = wrapper.value.decode() if isinstance(value, bytes) else wrapper.value
                    self.assertEqual(row.Value, expected)
                    self.assertEqual(row.MaxLength, len(value) if value is not None else None)

    def test_size(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value, size in (
                        (b'1234', 14),
                        (b'1234', 1),
                        (unicode_('*'), 5000),
                        (unicode_('*' * 5000), 5000),
                        (None, 14),
                ):
                    wrapper = ctds.SqlVarChar(value, size=size)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, size)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'varchar' if value is not None else None)
                    expected = wrapper.value.decode() if isinstance(value, bytes) else wrapper.value
                    if value is not None:
                        self.assertEqual(len(row.Value), min(size, len(value)))
                    self.assertEqual(row.Value, expected[:size] if value is not None else None)
                    self.assertEqual(row.MaxLength, size if value is not None else None)

    def test_typeerror(self):
        for value in (
                object(),
                int_(1234),
                long_(1234),
        ):
            self.assertRaises(TypeError, ctds.SqlVarChar, value)

    def test_size_typeerror(self):
        for size in (
                None,
                object(),
                '1234'
        ):
            self.assertRaises(TypeError, ctds.SqlVarChar, None, size)

class TestSqlDecimal(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlDecimal.__doc__,
            '''\
SqlDecimal(value, precision=18, scale=0)

SQL DECIMAL type wrapper.

:param object value: The value to wrap or `None`.
:param int precision: The maximum number of total digits stored.
    This must be between 1 and 38.
:param int scale: The maximum number of digits stored to the right
    of the decimal point. 0 <= `scale` <= `precision`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        '12.34',
                        int_(1234),
                        long_(1234),
                        unicode_('1234'),
                        Decimal('12.34'),
                        12.34,
                        None
                ):
                    wrapper = ctds.SqlDecimal(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'decimal' if value is not None else None)
                    self.assertEqual(
                        row.Value,
                        # scale defaults to 0, which will truncate any fractional part.
                        Decimal(str(round(float(value), 0))) if value is not None else None
                    )
                    self.assertEqual(row.Precision, 18 if value is not None else None)
                    self.assertEqual(row.Scale, 0 if value is not None else None)

    def test_precision(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value, precision in (
                        (1, 1),
                        (1234, 4),
                        ('12.34', 4),
                        (12.34, 4),
                        ('123.4567', 14),
                        (123.4567, 14),
                        ('1234567890.123456789', 38),
                        (1234567890.123456789, 38),
                ):
                    wrapper = ctds.SqlDecimal(value, precision=precision)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'decimal' if value is not None else None)
                    self.assertEqual(row.Precision, precision)
                    self.assertEqual(row.Scale, 0)
                    self.assertEqual(
                        row.Value,
                        # scale defaults to 0, which will truncate any fractional part.
                        Decimal(str(round(float(value), 0)))
                    )
                    self.assertEqual(row.Precision, precision)
                    self.assertEqual(row.Scale, 0)

    def test_scale(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value, scale in (
                        (1234, 0),
                        ('12.34', 2),
                        (12.34, 2),
                        ('.1234567', 15),
                        (.1234567, 15),
                ):
                    wrapper = ctds.SqlDecimal(value, precision=38, scale=scale)
                    self.assertEqual(id(value), id(wrapper.value))
                    self.assertEqual(wrapper.size, -1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'decimal')
                    self.assertEqual(row.Precision, 38)
                    self.assertEqual(row.Scale, scale)
                    self.assertEqual(row.Value, Decimal(str(round(float(value), scale))))
                    self.assertEqual(row.Precision, 38)
                    self.assertEqual(row.Scale, scale)

    def test_precision_typeerror(self):
        for precision in (
                object(),
                None,
                '1234'
        ):
            self.assertRaises(TypeError, ctds.SqlDecimal, 12.34, precision)

    def test_precision_valueerror(self):
        for precision in (
                0,
                39
        ):
            try:
                ctds.SqlDecimal(None, precision=precision)
            except ValueError as ex:
                self.assertEqual(str(ex), 'invalid precision: {0}'.format(precision))
            else:
                self.fail('ctds.SqlDecimal did not fail as expected') # pragma: nocover

    def test_scale_typeerror(self):
        for scale in (
                object(),
                None,
                '1234'
        ):
            self.assertRaises(TypeError, ctds.SqlDecimal, 12.34, scale)

    def test_scale_valueerror(self):
        for precision, scale in (
                (38, -1),
                (10, 11),
        ):
            try:
                ctds.SqlDecimal(None, precision=precision, scale=scale)
            except ValueError as ex:
                self.assertEqual(str(ex), 'invalid scale: {0}'.format(scale))
            else:
                self.fail('ctds.SqlDecimal did not fail as expected') # pragma: nocover
