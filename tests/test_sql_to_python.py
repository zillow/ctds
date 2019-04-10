from datetime import date, datetime, time
from binascii import hexlify, unhexlify
from decimal import Decimal
import platform
import uuid

import ctds

from .base import TestExternalDatabase
from .compat import unichr_, unicode_


class TestSQLToPython(TestExternalDatabase): # pylint: disable=too-many-public-methods
    '''Unit tests related to SQL to Python type conversion.
    '''

    def setUp(self):
        TestExternalDatabase.setUp(self)

        self.connection = self.connect()
        self.cursor = self.connection.cursor()

    def tearDown(self):
        TestExternalDatabase.tearDown(self)

        self.cursor.close()
        self.connection.close()

    def test_notsupportederror(self):
        self.cursor.execute("SELECT sql_variant_property(1, 'BaseType')")
        try:
            self.cursor.fetchone()
        except ctds.NotSupportedError as ex:
            self.assertEqual(str(ex), 'unsupported type 98 for column ""')
        else:
            self.fail('.fetchone() did not fail as expected') # pragma: nocover

    def test_bit(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(BIT, NULL),
                CONVERT(BIT, 1),
                CONVERT(BIT, 0)
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (None, True, False)
        )

    def test_binary(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(BINARY(8), NULL),
                CONVERT(BINARY(8), 0xdeadbeef),

                CONVERT(VARBINARY(8), NULL),
                CONVERT(VARBINARY(16), 0xdeadbeef)
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                unhexlify('deadbeef00000000'),

                None,
                unhexlify('deadbeef'),
            )
        )

    def test_char(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(CHAR(4), NULL),
                CONVERT(CHAR(4), '1234'),

                CONVERT(VARCHAR(4), NULL),
                CONVERT(VARCHAR(4), '1234'),
                CONVERT(VARCHAR(4), ''),
                CONVERT(VARCHAR(4), '1'),
                REPLICATE(CONVERT(VARCHAR(MAX), 'x'), 8001),

                CONVERT(CHAR(1), CHAR(189))
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                unicode_('1234'),
                None,
                unicode_('1234'),
                unicode_(''),
                unicode_('1'),
                unicode_('x' * 8001),
                unicode_(b'\xc2\xbd', encoding='utf-8'),
            )
        )

    def test_nchar(self):
        non_ucs2_emoji = unichr_(127802) if self.UCS4_SUPPORTED else self.UNICODE_REPLACEMENT
        self.cursor.execute(
            '''
            SELECT
                CONVERT(NCHAR(4), NULL),
                CONVERT(NCHAR(4), '1234'),

                CONVERT(NVARCHAR(4), NULL),
                CONVERT(NVARCHAR(4), N'1234'),
                CONVERT(NVARCHAR(4), N''),
                CONVERT(NVARCHAR(4), N'1'),
                REPLICATE(CONVERT(NVARCHAR(MAX), N'x'), 8001),

                NCHAR(189),
                NCHAR(256),
                CONVERT(NVARCHAR(100), 0x{0})
            '''.format(hexlify(non_ucs2_emoji.encode('utf-16le')).decode('ascii'))
        )

        # Windows builds properly decode codepoints from the supplementary plane.
        # Possibly due to the iconv implementation??
        decoded = self.use_utf16 or platform.system() == 'Windows' or not self.UCS4_SUPPORTED
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                unicode_('1234'),
                None,
                unicode_('1234'),
                unicode_(''),
                unicode_('1'),
                unicode_('x' * 8001),
                unicode_(b'\xc2\xbd', encoding='utf-8'),
                unicode_(b'\xc4\x80', encoding='utf-8'),
                non_ucs2_emoji if decoded else unicode_('??')
            )
        )

    def test_text(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(TEXT, NULL),
                CONVERT(TEXT, N''),
                CONVERT(TEXT, N'1234')
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                unicode_(''),
                unicode_('1234')
            )
        )

    def test_ntext(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(NTEXT, NULL),
                CONVERT(NTEXT, N''),
                CONVERT(NTEXT, N'1234')
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                unicode_(''),
                unicode_('1234')
            )
        )

    def test_int(self):
        ints = (2 ** 8 - 1, 2 ** 15 - 1, 2 ** 31 - 1, 2 ** 63 - 1)
        self.cursor.execute(
            '''
            SELECT
                CONVERT(TINYINT, NULL),
                CONVERT(TINYINT, {0}),

                CONVERT(SMALLINT, NULL),
                CONVERT(SMALLINT, {1}),
                CONVERT(SMALLINT, -{1}),

                CONVERT(INT, NULL),
                CONVERT(INT, {2}),
                CONVERT(INT, -{2}),

                CONVERT(BIGINT, NULL),
                CONVERT(BIGINT, {3}),
                CONVERT(BIGINT, -{3})
            '''.format(*ints)
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None, ints[0],
                None, ints[1], -ints[1],
                None, ints[2], -ints[2],
                None, ints[3], -ints[3],
            )
        )

    def test_float(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(REAL, NULL),
                CONVERT(REAL, '-3.40E+38'),
                CONVERT(REAL, '-1.18E-38'),
                CONVERT(REAL, 0),
                CONVERT(REAL, '1.18E-38'),
                CONVERT(REAL, '3.40E+38'),

                CONVERT(FLOAT(24), NULL),
                CONVERT(FLOAT(24), '-3.40E+38'),
                CONVERT(FLOAT(24), '-1.18E-38'),
                CONVERT(FLOAT(24), 0),
                CONVERT(FLOAT(24), '1.18E-38'),
                CONVERT(FLOAT(24), '3.40E+38'),

                CONVERT(FLOAT(53), NULL),
                CONVERT(FLOAT(53), '-1.79E+308'),
                CONVERT(FLOAT(53), '-2.23E-308'),
                CONVERT(FLOAT(53), 0),
                CONVERT(FLOAT(53), '2.23E-308'),
                CONVERT(FLOAT(53), '1.79E+308')
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                -3.3999999521443642e+38,
                -1.179999945774631e-38,
                float(0),
                1.179999945774631e-38,
                3.3999999521443642e+38,

                None,
                -3.3999999521443642e+38,
                -1.179999945774631e-38,
                float(0),
                1.179999945774631e-38,
                3.3999999521443642e+38,

                None,
                -1.79e+308,
                -2.23e-308,
                float(0),
                2.23e-308,
                1.79e+308,
            )
        )

    def test_numeric(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(NUMERIC(5,3), NULL),
                CONVERT(NUMERIC(5,3), '12.345'),
                CONVERT(NUMERIC(5,3), '12.34567'),
                CONVERT(NUMERIC(5,3), '12.34543'),
                CONVERT(NUMERIC(5,3), 0),
                CONVERT(NUMERIC(5,3), 66),

                CONVERT(DECIMAL(5,3), NULL),
                CONVERT(DECIMAL(5,3), '12.345'),
                CONVERT(DECIMAL(5,3), '12.34567'),
                CONVERT(DECIMAL(5,3), '12.34543'),
                CONVERT(DECIMAL(5,3), 0),
                CONVERT(DECIMAL(5,3), 66)
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                Decimal('12.345'),
                Decimal('12.346'),
                Decimal('12.345'),
                Decimal('0.000'),
                Decimal('66.000'),

                None,
                Decimal('12.345'),
                Decimal('12.346'),
                Decimal('12.345'),
                Decimal('0.000'),
                Decimal('66.000'),
            )
        )

    def test_money(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(MONEY, NULL),
                CONVERT(MONEY, '-922,337,203,685,477.5808'),
                CONVERT(MONEY, '922,337,203,685,477.5807'),

                CONVERT(SMALLMONEY, NULL),
                CONVERT(SMALLMONEY, '-214,748.3648'),
                CONVERT(SMALLMONEY, '214,748.3647')
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                self.round_money(Decimal('-922337203685477.5808')),
                self.round_money(Decimal('922337203685477.5807')),

                None,
                # SMALLMONEY seems to be rounded properly by FreeTDS ...
                Decimal('-214748.3648'),
                Decimal('214748.3647'),
            )
        )

    def test_date(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(DATE, NULL),
                CONVERT(DATE, '0001-01-01'),
                CONVERT(DATE, '9999-12-31')
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                (
                    '0001-01-01'
                    if self.connection.tds_version < '7.3'
                    else date(1, 1, 1)
                ),
                (
                    '9999-12-31'
                    if self.connection.tds_version < '7.3'
                    else date(9999, 12, 31)
                ),
            )
        )

    def test_time(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(TIME, NULL),
                CONVERT(TIME, '01:02:03.01'),
                CONVERT(TIME, '23:59:59.99')
            '''
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                (
                    '01:02:03.0100000'
                    if self.connection.tds_version < '7.3'
                    else time(1, 2, 3, 10000)
                ),
                (
                    '23:59:59.9900000'
                    if self.connection.tds_version < '7.3'
                    else time(23, 59, 59, 990000)
                ),
            )
        )

    def test_datetime(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(DATETIME, NULL),
                CONVERT(DATETIME, :0),
                CONVERT(DATETIME, :1)
            ''',
            (
                datetime(1753, 1, 1),
                datetime(9999, 12, 31, 23, 59, 59, 997000)
            )
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                datetime(1753, 1, 1),
                datetime(9999, 12, 31, 23, 59, 59, 997000)
            )
        )

    def test_smalldatetime(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(SMALLDATETIME, NULL),
                CONVERT(SMALLDATETIME, :0),
                CONVERT(SMALLDATETIME, :1)
            ''',
            (
                datetime(1900, 1, 1),
                datetime(2076, 6, 6, 23, 59, 59)
            )
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                datetime(1900, 1, 1),
                # SMALLDATETIME only has minute accuracy.
                datetime(2076, 6, 7)
            )
        )

    def test_datetime2(self):
        self.cursor.execute(
            '''
            SELECT
                CONVERT(DATETIME2, NULL),
                CONVERT(DATETIME2, :0),
                CONVERT(DATETIME2, :1)
            ''',
            (
                datetime(1, 1, 1),
                # $future: fix rounding issues. DB lib doesn't expose a good way to access
                # the more precise DATETIME2 structure
                datetime(9999, 12, 31, 23, 59, 59, 997 * 1000)
            )
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                (
                    '2001-01-01 00:00:00.0000000'
                    if self.connection.tds_version < '7.3'
                    else datetime(2001, 1, 1)
                ),
                (
                    '9999-12-31 23:59:59.9966667'
                    if self.connection.tds_version < '7.3'
                    else (
                        datetime(9999, 12, 31, 23, 59, 59, 996666)
                        if self.tdstime_supported else
                        datetime(9999, 12, 31, 23, 59, 59, 997 * 1000)
                    )
                ),
            )
        )

    def test_guid(self):

        uuid1 = uuid.uuid4()
        uuid2 = uuid.uuid4()

        self.cursor.execute(
            '''
            SELECT
                CONVERT(UNIQUEIDENTIFIER, NULL),
                CONVERT(UNIQUEIDENTIFIER, :0),
                CONVERT(UNIQUEIDENTIFIER, :1)
            ''',
            (
                uuid1,
                uuid2
            )
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                uuid1,
                uuid2
            )
        )

    def test_xml(self):

        xml = '<foo><bar>1</bar><baz/></foo>'
        self.cursor.execute(
            '''
            SELECT
                CONVERT(XML, NULL),
                CONVERT(XML, :0)
            ''',
            (
                xml,
            )
        )
        self.assertEqual(
            tuple(self.cursor.fetchone()),
            (
                None,
                xml
            )
        )

    def test_unsupported(self):
        obj = object()
        try:
            self.cursor.execute('SELECT :0', (obj,))
        except ctds.InterfaceError as ex:
            self.assertEqual(
                str(ex),
                'could not implicitly convert Python type "{0}" to SQL'.format(type(obj))
            )
        else:
            self.fail('.execute() did not fail as expected') # pragma: nocover
