import datetime
from decimal import Decimal

import ctds

from .base import TestExternalDatabase
from .compat import int_, long_, unichr_, unicode_, PY3

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlBigInt({0!r})'.format(value)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlBinary({0!r}, size={1})'.format(value, wrapper.size)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

SQL CHAR type wrapper. The value's UTF-8-encoded length must be <= 8000.

:param object value: The value to wrap or `None`.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        b'1234',
                        b'*' * 8000,
                        unicode_('1234'),
                        unicode_('*') * 8000,
                        unicode_(b'\xc2\xa9', encoding='utf-8') * 4000,
                        None
                ):
                    wrapper = ctds.SqlChar(value)
                    self.assertEqual(id(value), id(wrapper.value))
                    encoded = value.encode('utf-8') if isinstance(value, unicode_) else value
                    self.assertEqual(wrapper.size, len(encoded) if encoded is not None else 1)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(row.Type, 'char' if value is not None else None)
                    expected = encoded.decode('utf-8') if encoded is not None else encoded

                    # Ignore any padding added by the database with `rstip()`.
                    self.assertEqual(row.Value.rstrip() if row.Value is not None else row.Value, expected)

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlChar({0!r}, size={1})'.format(value, wrapper.size)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

    def test_typeerror(self):
        for value in (
                object(),
                int_(1234),
                long_(1234),
        ):
            self.assertRaises(TypeError, ctds.SqlChar, value)

    def test_valueerror(self):
        for value in (
                b'*' * 8001,
                unicode_('*') * 8001,
                unicode_(b'\xc2\xa9', encoding='utf-8') * 4000 + unicode_('?'),
        ):
            self.assertRaises(ValueError, ctds.SqlChar, value)


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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlDate({0!r})'.format(value)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlInt({0!r})'.format(value)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlSmallInt({0!r})'.format(value)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlTinyInt({0!r})'.format(value)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlVarBinary({0!r}, size={1})'.format(value, wrapper.size)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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

.. note:: Byte strings are passed through unchanged to the database.

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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlVarChar({0!r}, size={1})'.format(value, wrapper.size)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

    def test_size(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                # The parameter_type method does not work with NVARCHAR(MAX) and
                # will fail with "Operand type clash: varchar(max) is incompatible with sql_variant"
                # Therefore, limit input sizes to 8000 or less.
                for value, size in (
                        (b'1234', 14),
                        (b'1234', 1),
                        (unicode_('*'), 5000),
                        (unicode_('*' * 5000), 5000),
                        (unicode_('*' * 8000), 8000),
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


class TestSqlNVarChar(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.SqlNVarChar.__doc__,
            '''\
SqlNVarChar(value, size=None)

SQL NVARCHAR type wrapper.

.. versionadded:: 1.1

:param object value: The value to wrap or `None`.
:param int size: An optional size override. This value will be used for
    the output parameter buffer size. It can also be used to truncate the
    input parameter.
'''
        )

    def test_wrapper(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                inputs = [
                    None,
                    unicode_(b'*', encoding='utf-8'),
                    unicode_(b'*', encoding='utf-8') * 4000,
                ]
                if self.nchars_supported: # pragma: nobranch
                    inputs.extend([
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8'),
                        unicode_(b' \xe3\x83\x9b ', encoding='utf-8'),
                        unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 4000,
                    ])
                    if self.use_utf16: # pragma: nobranch
                        inputs.extend([
                            unichr_(127802),
                            unichr_(127802) * 2000,
                        ])
                for value in inputs:
                    wrapper = ctds.SqlNVarChar(value)
                    self.assertEqual(
                        wrapper.size,
                        self.nvarchar_width(value) if value is not None else 1
                    )
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(
                        row.Type,
                        '{0}varchar'.format('n' if self.nchars_supported else '') if value is not None else None
                    )
                    self.assertEqual(row.Value, value)
                    self.assertEqual(
                        row.MaxLength,
                        (self.nvarchar_width(value) * (1 + int(self.nchars_supported))) if value is not None else None
                    )

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlNVarChar({0!r}, size={1})'.format(
                            value if PY3 else (value.encode('utf-8') if value is not None else value),
                            wrapper.size
                        )
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

    def test_size(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                # The parameter_type method does not work with NVARCHAR(MAX) and
                # will fail with "Operand type clash: nvarchar(max) is incompatible with sql_variant"
                # Therefore, limit input sizes to 4000 or less.
                inputs = [
                    (None, 14),
                    (unicode_(b'*', encoding='utf-8'), 4000),
                    (unicode_(b'*', encoding='utf-8') * 4000, 4000),
                ]
                if self.nchars_supported: # pragma: nobranch
                    inputs.extend([
                        (unicode_(b'\xe3\x83\x9b', encoding='utf-8'), 4000),
                        (unicode_(b'\xe3\x83\x9b', encoding='utf-8') * 4000, 4000),
                    ])
                for value, size in inputs:
                    wrapper = ctds.SqlNVarChar(value, size=size)
                    self.assertEqual(wrapper.size, size)
                    row = self.parameter_type(cursor, wrapper)
                    self.assertEqual(
                        row.Type,
                        '{0}varchar'.format('n' if self.nchars_supported else '') if value is not None else None
                    )
                    if value is not None:
                        self.assertEqual(len(row.Value), min(size, len(value)))
                    self.assertEqual(row.Value, value[:size] if value is not None else None)
                    self.assertEqual(
                        row.MaxLength,
                        size * (1 + int(self.nchars_supported)) if value is not None else None
                    )

                # Manually check NVARCHAR(MAX) types due to sql_variant limitation.
                value = unicode_(b'\xe3\x83\x9b' if self.nchars_supported else b'&', encoding='utf-8') * 4001
                wrapper = ctds.SqlNVarChar(value)
                cursor.execute('SELECT DATALENGTH(:0)', (wrapper,))
                row = cursor.fetchone()
                self.assertEqual(row[0], len(value) * (1 + int(self.nchars_supported)))

    def test_typeerror(self):
        for value in (
                object(),
                int_(1234),
                long_(1234),
                b'1234',
        ):
            self.assertRaises(TypeError, ctds.SqlNVarChar, value)

    def test_size_typeerror(self):
        for size in (
                None,
                object(),
                '1234'
        ):
            self.assertRaises(TypeError, ctds.SqlNVarChar, None, size)


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

                    self.assertEqual(
                        repr(wrapper),
                        'tds.SqlDecimal({0!r})'.format(value)
                    )
                    self.assertEqual(repr(wrapper), str(wrapper))

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
