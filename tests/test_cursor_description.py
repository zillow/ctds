from datetime import date, datetime, time
from decimal import Decimal

import ctds

from .base import TestExternalDatabase
from .compat import long_, PY3, unicode_

class TestCursorDescription(TestExternalDatabase):
    '''Unit tests related to the Cursor.desciption attribute.
    '''
    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.description.__doc__,
            '''\
A description of the current result set columns.
The description is a sequence of tuples, one tuple per column in the
result set. The tuple describes the column data as follows:

+---------------+--------------------------------------+
| name          | The name of the column, if provided. |
+---------------+--------------------------------------+
| type_code     | The specific type of the column.     |
+---------------+--------------------------------------+
| display_size  | The SQL type size of the column.     |
+---------------+--------------------------------------+
| internal_size | The client size of the column.       |
+---------------+--------------------------------------+
| precision     | The precision of NUMERIC and DECIMAL |
|               | columns.                             |
+---------------+--------------------------------------+
| scale         | The scale of NUMERIC and DECIMAL     |
|               | columns.                             |
+---------------+--------------------------------------+
| null_ok       | Whether the column allows NULL.      |
+---------------+--------------------------------------+

.. note::
    In Python 3+, this is a :py:class:`tuple` of
    :py:func:`collections.namedtuple` objects whose members are defined
    above.

:pep:`0249#description`

:return:
    A sequence of tuples or :py:data:`None` if no results are
    available.
:rtype: tuple(tuple(str, int, int, int, int, int, bool))
'''
        )

    def test_before_results(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertEqual(cursor.description, None)

    def test_no_results(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute("PRINT('no results here')")
                self.assertEqual(cursor.description, None)

    def test_types(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (
                    None,
                    -1234567890,
                    2 ** 45,
                    b'1234',
                    bytearray('1234', 'ascii'),
                    unicode_(
                        b'hello \'world\' ' + (b'\xc4\x80' if self.nchars_supported else b''),
                        encoding='utf-8'
                    ),
                    datetime(2001, 1, 1, 12, 13, 14, 150 * 1000),
                    date(2010, 2, 14),
                    time(11, 12, 13, 140 * 1000),
                    Decimal('123.4567890'),
                    Decimal('1000000.0001')
                )

                cursor.execute(
                    '''
                    SELECT
                        :0 AS none,
                        :1 AS int,
                        CONVERT(BIGINT, :2) AS bigint,
                        :3 AS bytes,
                        CONVERT(BINARY(10), :3) AS binary10,
                        CONVERT(VARBINARY(10), :3) AS varbinary10,
                        :4 AS bytearray,
                        :5 AS string,
                        CONVERT(CHAR(10), :5) AS char10,
                        CONVERT(VARCHAR(10), :5) AS varchar10,
                        CONVERT(NVARCHAR(MAX), :5) AS nvarchar,
                        CONVERT(DATETIME, :6) AS datetime,
                        CONVERT(DATE, :7) AS date,
                        CONVERT(TIME, :8) as time,
                        :9 AS decimal,
                        CONVERT(MONEY, :10) AS money
                    ''',
                    args
                )

                self.assertEqual(
                    cursor.description,
                    (
                        (unicode_('none'), long_(ctds.CHAR), long_(4), long_(4), long_(0), long_(0), True),
                        (unicode_('int'), long_(ctds.INT), long_(4), long_(4), long_(0), long_(0), True),
                        (unicode_('bigint'), long_(ctds.BIGINT), long_(8), long_(8), long_(0), long_(0), True),
                        (unicode_('bytes'), long_(ctds.BINARY), long_(4), long_(4), long_(0), long_(0), True),
                        (unicode_('binary10'), long_(ctds.BINARY), long_(10), long_(10), long_(0), long_(0), True),
                        (unicode_('varbinary10'), long_(ctds.BINARY), long_(10), long_(10), long_(0), long_(0), True),
                        (unicode_('bytearray'), long_(ctds.BINARY), long_(4), long_(4), long_(0), long_(0), True),
                        (
                            unicode_('string'),
                            long_(ctds.CHAR),
                            long_((len(args[5])) * 4),
                            long_((len(args[5])) * 4),
                            long_(0),
                            long_(0),
                            self.use_sp_executesql
                        ),
                        (unicode_('char10'), long_(ctds.CHAR), long_(40), long_(40), long_(0), long_(0), True),
                        (unicode_('varchar10'), long_(ctds.CHAR), long_(40), long_(40), long_(0), long_(0), True),
                        (
                            unicode_('nvarchar'),
                            long_(ctds.TEXT) if connection.tds_version < '7.3' else long_(ctds.CHAR),
                            long_(2**31 - 1),
                            long_(2**31 - 1),
                            long_(0),
                            long_(0),
                            True
                        ),
                        (unicode_('datetime'), long_(ctds.DATETIME), long_(8), long_(8), long_(0), long_(0), True),
                        (
                            unicode_('date'),
                            long_(ctds.CHAR) if connection.tds_version < '7.3' else long_(ctds.DATE),
                            long_(40) if connection.tds_version < '7.3' else long_(16),
                            long_(40) if connection.tds_version < '7.3' else long_(16),
                            long_(0),
                            long_(0),
                            True
                        ),
                        (
                            unicode_('time'),
                            long_(ctds.CHAR) if connection.tds_version < '7.3' else long_(ctds.TIME),
                            long_(64) if connection.tds_version < '7.3' else long_(16),
                            long_(64) if connection.tds_version < '7.3' else long_(16),
                            long_(0) if connection.tds_version < '7.3' else long_(7),
                            long_(0) if connection.tds_version < '7.3' else long_(7),
                            True
                        ),
                        (unicode_('decimal'), long_(ctds.DECIMAL), long_(17), long_(17), long_(10), long_(7), True),
                        (unicode_('money'), long_(ctds.MONEY), long_(8), long_(8), long_(0), long_(0), True)
                    )
                )
                self.assertIs(cursor.description, cursor.description) # description is cached
                if PY3:
                    self.assertEqual(cursor.description[0].name, 'none')
                    self.assertEqual(cursor.description[0].type_code, ctds.CHAR)
                    self.assertEqual(cursor.description[0].display_size, 4)
                    self.assertEqual(cursor.description[0].internal_size, 4)
                    self.assertEqual(cursor.description[0].scale, 0)
                    self.assertEqual(cursor.description[0].precision, 0)
                    self.assertEqual(cursor.description[0].null_ok, True)
                else: # pragma: nocover
                    pass

    def test_multiple_descriptions(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            try:
                descriptions = []

                cursor.execute("SELECT CONVERT(BIT, 1) AS [bit]")
                descriptions.append(cursor.description)

                cursor.execute("SELECT CONVERT(INT, 123) AS [int]")
                descriptions.append(cursor.description)

                with self.stored_procedure(
                    cursor,
                    'CallProcDescription',
                    """
                    AS BEGIN
                        SELECT CONVERT(DECIMAL(5,2), 1.75) AS [decimal];
                        SELECT CONVERT(MONEY, 1.99) AS [money];
                    END
                    """
                ):
                    cursor.callproc('CallProcDescription', {})
                    descriptions.append(cursor.description)
                    cursor.nextset()
                    descriptions.append(cursor.description)

                self.assertEqual(descriptions, [
                    (('bit', ctds.BIT, 1, 1, 0, 0, True),),
                    (('int', ctds.INT, 4, 4, 0, 0, True),),
                    (('decimal', ctds.DECIMAL, 17, 17, 5, 2, True),),
                    (('money', ctds.MONEY, 8, 8, 0, 0, True),),
                ])
            finally:
                cursor.close()

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            try:
                cursor.execute("SELECT CONVERT(BIT, 1) AS [bit]")
                description = cursor.description
                self.assertIsNotNone(description)
                self.assertEqual(description, (
                    ('bit', ctds.BIT, 1, 1, 0, 0, True),
                ))
            finally:
                cursor.close()

            # cursor is closed
            self.assertEqual(cursor.description, None)
            self.assertEqual(description, (
                ('bit', ctds.BIT, 1, 1, 0, 0, True),
            ))

        # connection is closed
        self.assertEqual(cursor.description, None)
        self.assertEqual(description, (
            ('bit', ctds.BIT, 1, 1, 0, 0, True),
        ))
