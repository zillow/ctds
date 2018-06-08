from datetime import datetime
from decimal import Decimal
import re
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import PY3, long_, unicode_

class TestCursorExecute(TestExternalDatabase): # pylint: disable=too-many-public-methods
    '''Unit tests related to the Cursor.execute() method.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.execute.__doc__,
            '''\
execute(sql, parameters=None)

Prepare and execute a database operation.
Parameters may be provided as sequence and will be bound to variables
specified in the SQL statement. Parameter notation is specified by
:py:const:`ctds.paramstyle`.

:pep:`0249#execute`

:param str sql: The SQL statement to execute.
:param tuple parameters: Optional variables to bind.
'''
        )

    def test_missing_command(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.execute)

    def test_missing_numeric_parameter(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for index, args in (
                        (1, (1,)),
                        (-1, (1,)),
                ):
                    self.assertRaises(
                        IndexError,
                        cursor.execute,
                        'SELECT :{0} AS missing'.format(index),
                        args
                    )

    def test_missing_named_parameter(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                for args in (
                        ({'name': 'value'}),
                        ({'1': 'value'}),
                ):
                    try:
                        cursor.execute('SELECT :missing AS missing', args)
                    except LookupError as ex:
                        self.assertEqual(
                            str(ex),
                            'unknown named parameter "missing"'
                        )
                    else:
                        self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_invalid_numeric_format(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (':', ':::', ':a', ':ab12', ":'somestring'"):
                    try:
                        cursor.execute('SELECT {0}'.format(case), (1,))
                    except ctds.InterfaceError as ex:
                        self.assertEqual(str(ex), 'invalid parameter marker')
                    else:
                        self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_invalid_named_parameter(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                for case, ex, msg in (
                        (
                            False,
                            TypeError,
                            "'bool' object is not iterable",
                        ),
                        (
                            [None],
                            TypeError,
                            (
                                'invalid parameter mapping item 0'
                            )
                        ),
                        (
                            object(),
                            TypeError,
                            (
                                "'object' object is not iterable"
                            )
                        ),
                ):
                    try:
                        cursor.executemany('SELECT :arg AS missing', case)
                    except ex as actual:
                        self.assertEqual(str(actual), msg)
                    else:
                        self.fail('.execute() did not fail as expected') # pragma: nocover


    def test_int_overflow(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(
                    OverflowError,
                    cursor.execute,
                    'SELECT :0',
                    (long_(2**63),)
                )

    def test_sql_syntax_error(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.execute('SELECT * FROM')
                except ctds.ProgrammingError as ex:
                    msg = "Incorrect syntax near 'FROM'."
                    self.assertEqual(str(ex), msg)
                    self.assertEqual(ex.severity, 15)
                    self.assertEqual(ex.os_error, None)
                    self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                    self.assertEqual(
                        ex.last_message,
                        {
                            'number': 102,
                            'state': 1,
                            'severity': 15,
                            'description': msg,
                            'proc': '',
                            'line': 1,
                        }
                    )
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

                # The cursor should be usable after an exception.
                cursor.execute('SELECT @@VERSION')
                self.assertTrue(cursor.fetchall())

    def test_sql_raiserror(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.execute(
                        '''
                        RAISERROR (N'some custom error %s', 12, 111, 'hello!');
                        '''
                    )
                except ctds.ProgrammingError as ex:
                    msg = "some custom error hello!"
                    self.assertEqual(str(ex), msg)
                    self.assertEqual(ex.severity, 12)
                    self.assertEqual(ex.os_error, None)
                    self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                    self.assertEqual(
                        ex.last_message,
                        {
                            'number': 50000,
                            'state': 111,
                            'severity': 12,
                            'description': msg,
                            'proc': '',
                            'line': 2,
                        }
                    )
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

                # The cursor should be usable after an exception.
                cursor.execute('SELECT @@VERSION')
                self.assertTrue(cursor.fetchall())

    def test_sql_raiseerror_warning(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute(
                        '''
                        RAISERROR (N'some custom non-severe error %s', 10, 111, 'hello!');
                        RAISERROR (N'some custom non-severe error %s', 10, 222, 'world');
                        '''
                    )
                fmt = unicode_('some custom non-severe error %s')
                self.assertEqual(len(warns), 2)
                self.assertEqual(
                    [str(warn.message) for warn in warns],
                    [fmt % args for args in (unicode_('hello!'), unicode_('world'))]
                )
                self.assertEqual(
                    [warn.category for warn in warns],
                    [ctds.Warning] * len(warns)
                )

                self.assertEqual(warns[0].category, ctds.Warning)

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')

                self.assertTrue(cursor.fetchall())
                self.assertEqual(len(warns), 0)

    def test_sql_warning_as_error(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings():
                    warnings.simplefilter('error', ctds.Warning)
                    try:
                        cursor.execute(
                            '''
                            RAISERROR (N'some custom non-severe error %s', 10, 111, 'hello!');
                            '''
                        )
                    except ctds.Warning as warn:
                        self.assertEqual('some custom non-severe error hello!', str(warn))
                    else:
                        self.fail('.execute() did not fail as expected') # pragma: nocover

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_statement_terminated(self):
        # The purpose of this test is to cause multiple info messages to be returned
        # to the client. The following SQL statement should cause two messages to be
        # returned, the second of which is the useless "The statement has been terminated.".
        with self.connect() as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.execute(
                        '''
                        DECLARE @tTable TABLE (i int PRIMARY KEY);
                        INSERT INTO
                            @tTable
                        VALUES
                            (1);
                        INSERT INTO
                            @tTable
                        VALUES
                            (1);
                        '''
                    )
                except ctds.IntegrityError as ex:
                    regex = r'''Violation of PRIMARY KEY constraint 'PK__[^']+'. ''' \
                            r'''Cannot insert duplicate key in object 'dbo\.@tTable'. ''' \
                            r'''The duplicate key value is \(1\).'''
                    self.assertTrue(re.match(regex, str(ex)))
                    self.assertEqual(ex.severity, 14)
                    self.assertEqual(ex.os_error, None)
                    self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                    self.assertTrue(re.match(regex, ex.last_message.pop('description')))
                    self.assertEqual(
                        ex.last_message,
                        {
                            'number': 2627,
                            'state': 1,
                            'severity': 14,
                            'proc': '',
                            'line': 7,
                        }
                    )
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_no_arguments(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    /* parameter markers, such as :1, should be ignored */
                    SELECT ':0';
                    '''
                )
                self.assertEqual(tuple(cursor.fetchone()), (':0',))

    def test_escape_paramstyle(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for sqltype, constant, parameter in (
                        ('DATETIME', '2016-01-02 03:04:05', datetime(2016, 1, 2, 3, 4, 5)),
                        (
                            'VARCHAR(100)',
                            unicode_(b"'' foo '' :1 '' '':", encoding='utf-8'),
                            unicode_(b"' foo ' :1 ' ':", encoding='utf-8')
                        ),
                        (
                            'VARCHAR(100)',
                            unicode_(b" ", encoding='utf-8'),
                            unicode_(b" ", encoding='utf-8')
                        ),
                        (
                            'VARCHAR(100)',
                            unicode_(b"'' ''", encoding='utf-8'),
                            unicode_(b"' '", encoding='utf-8')
                        ),
                ):
                    cursor.execute(
                        '''
                        SELECT
                            CONVERT({0}, '{1}') AS Constant,
                            :0 AS Parameter
                        '''.format(sqltype, constant),
                        (parameter,)
                    )
                    row = cursor.fetchone()
                    self.assertEqual(row['Constant'], row['Parameter'])

    def test_typeerror(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for parameter in (
                        1,
                        True,
                        None,
                ):
                    try:
                        cursor.execute('SELECT :0', parameter)
                    except TypeError as ex:
                        self.assertEqual(str(ex), str(parameter or ''))
                    else:
                        self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_format_numeric(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (
                    None,
                    -1234567890,
                    2 ** 45,
                    ctds.Parameter(b'1234'),
                    bytearray('1234', 'ascii'),
                    unicode_(
                        b'hello \'world\' ' + (b'\xe3\x83\x9b' if self.nchars_supported else b''),
                        encoding='utf-8'
                    ),
                    datetime(2001, 1, 1, 12, 13, 14, 150 * 1000),
                    Decimal('123.4567890'),
                    Decimal('1000000.4532')
                )

                query = cursor.execute(
                    '''
                    SELECT
                        :0 AS none,
                        :1 AS int,
                        CONVERT(BIGINT, :2) AS bigint,
                        :3 AS bytes,
                        :4 AS bytearray,
                        :5 AS string,
                        :5 AS string_again,
                        CONVERT(DATETIME, :6) AS datetime,
                        :7 AS decimal,
                        CONVERT(MONEY, :8) AS money
                    ''',
                    args
                )
                self.assertEqual(None, query)

                self.assertEqual(
                    tuple(cursor.fetchone()),
                    (
                        args[0],
                        args[1],
                        args[2],
                        args[3].value,
                        bytes(args[4]),
                        args[5],
                        args[5],
                        args[6],
                        args[7],
                        self.round_money(args[8]),
                    )
                )

    def test_format_numeric_empty(self):
        with self.connect(paramstyle='numeric') as connection:
            with connection.cursor() as cursor:
                query = cursor.execute(
                    '''
                    SELECT
                        'test'
                    ''',
                    ()
                )
                self.assertEqual(None, query)

                self.assertEqual(
                    tuple(cursor.fetchone()),
                    (
                        unicode_('test'),
                    )
                )

    def test_format_named(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                args = {
                    unicode_('none'): None,
                    'int': -1234567890,
                    'bigint': 2 ** 45,
                    'bytes': ctds.Parameter(b'1234'),
                    unicode_('byte_array'): bytearray('1234', 'ascii'),
                    'string': unicode_(
                        b'hello \'world\' ' + (b'\xe3\x83\x9b' if self.nchars_supported else b''),
                        encoding='utf-8'
                    ),
                    'datetime': datetime(2001, 1, 1, 12, 13, 14, 150 * 1000),
                    'decimal': Decimal('123.4567890'),
                    'money': Decimal('1000000.4532')
                }
                query = cursor.execute(
                    '''
                    SELECT
                        :none AS none,
                        :int AS int,
                        CONVERT(BIGINT, :bigint) AS bigint,
                        :bytes AS bytes,
                        :byte_array AS bytearray,
                        :string AS string,
                        :string AS string_again,
                        CONVERT(DATETIME, :datetime) AS datetime,
                        :decimal AS decimal,
                        CONVERT(MONEY, :money) AS money
                    ''',
                    args
                )
                self.assertEqual(None, query)

                self.assertEqual(
                    tuple(cursor.fetchone()),
                    (
                        args['none'],
                        args['int'],
                        args['bigint'],
                        args['bytes'].value,
                        bytes(args['byte_array']),
                        args['string'],
                        args['string'],
                        args['datetime'],
                        args['decimal'],
                        self.round_money(args['money']),
                    )
                )

    def test_format_named_empty(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                query = cursor.execute(
                    '''
                    SELECT
                        'test'
                    ''',
                    {}
                )
                self.assertEqual(None, query)

                self.assertEqual(
                    tuple(cursor.fetchone()),
                    (
                        unicode_('test'),
                    )
                )

    def test_format_named_special(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                args = {
                    unicode_('stmt'): 'the stmt parameter',
                    unicode_('params'): 'the params parameter',

                }
                cursor.execute(
                    '''
                    SELECT
                        :stmt,
                        :params
                    ''',
                    args
                )

                self.assertEqual(
                    tuple(cursor.fetchone()),
                    (args['stmt'], args['params'])
                )

    def test_format_named_invalid_name(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                for arg in (
                    None,
                    object(),
                    1,
                    Decimal('1.0'),

                ):
                    args = {
                        arg: 'value',
                        'arg': 'value'
                    }
                    try:
                        cursor.execute(
                            '''
                            SELECT :arg
                            ''',
                            args
                        )
                    except TypeError as ex:
                        self.assertTrue(self.use_sp_executesql)
                        self.assertEqual(str(ex), str(arg or ''))
                    except LookupError as ex:
                        self.assertFalse(self.use_sp_executesql)
                        self.assertEqual(str(ex), str(arg or ''))
                    else:
                        self.assertFalse(self.use_sp_executesql) # pragma: nocover

    def test_escape_numeric(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = ("''; SELECT @@VERSION;",)
                cursor.execute(
                    '''
                    SELECT :0
                    ''',
                    args
                )
                self.assertEqual(
                    [tuple(row) for row in cursor.fetchall()],
                    [
                        args
                    ]
                )
                self.assertEqual(cursor.nextset(), None)

    def test_escape_named(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                args = {'arg': "''; SELECT @@VERSION;"}
                cursor.execute(
                    '''
                    SELECT :arg
                    ''',
                    args
                )
                self.assertEqual(
                    [tuple(row) for row in cursor.fetchall()],
                    [
                        (args['arg'],)
                    ]
                )
                self.assertEqual(cursor.nextset(), None)

    def test_empty_string(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                args = {'arg': unicode_('')}
                cursor.execute(
                    '''
                    SELECT :arg
                    ''',
                    args
                )
                self.assertEqual(
                    [tuple(row) for row in cursor.fetchall()],
                    [
                        (None if self.use_sp_executesql else unicode_(''),)
                    ]
                )
                self.assertEqual(cursor.nextset(), None)

    def test_long_statement(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    /* {0} */
                    SELECT 1;
                    '''.format('?' * 8000)
                )

    def test_compute(self):
        # COMPUTE clauses are only supported in SQL Server 2005 & 2008
        if self.sql_server_version()[0] < 11: # pragma: nocover
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                            DECLARE @test_compute2 TABLE(s VARCHAR(10));
                            INSERT INTO @test_compute2(s) VALUES ('a'),('b'),('c');
                            SELECT s, s FROM @test_compute2 COMPUTE COUNT(s);
                        '''
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            ('a', 'a'),
                            ('b', 'b'),
                            ('c', 'c'),
                            (3, None)
                        ]
                    )
                    self.assertEqual(cursor.nextset(), None)

                    cursor.execute(
                        '''
                            DECLARE @test_compute TABLE(i INT);
                            INSERT INTO @test_compute(i) VALUES (1),(2),(3);
                            SELECT i, i, i FROM @test_compute COMPUTE SUM(i), AVG(i);
                        '''
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (1, 1, 1),
                            (2, 2, 2),
                            (3, 3, 3),
                            (6, 2, None)
                        ]
                    )
                    self.assertEqual(cursor.nextset(), None)

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.execute('SELECT @@VERSION')
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            try:
                cursor.execute('SELECT @@VERSION')
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'connection closed')
            else:
                self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_multiple(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute('SELECT @@VERSION')
                cursor.execute('SELECT @@VERSION')

    def test_execute_bad_query(self):
        try:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute('BOGUS')
        except ctds.ProgrammingError as ex:
            self.assertEqual(str(ex), "Could not find stored procedure 'BOGUS'.")
            self.assertEqual(ex.severity, 16)
            # Don't vaildate the DB error number. It is not consistent across FreeTDS versions.
            self.assertTrue(isinstance(ex.db_error['number'], long_))
            self.assertEqual(
                ex.db_error['description'],
                'General SQL Server error: Check messages from the SQL Server',
            )
            self.assertEqual(ex.os_error, None)
            self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
            self.assertEqual(ex.last_message, {
                'number': 2812,
                'state': 62,
                'severity': 16,
                'description': "Could not find stored procedure 'BOGUS'.",
                'proc': '',
                'line': 1,
            })
        else:
            self.fail('ProgrammingError expected') # pragma: nocover

    def test_error_mapping(self):
        for query, exception, severity, number in (
            (
                '''
                DROP TABLE DoesntExist;
                ''',
                ctds.ProgrammingError,
                11,
                3701
            ),
            (
                '''
                DECLARE @tTable TABLE (NotNullColumn INT NOT NULL);
                INSERT INTO @tTable VALUES (NULL);
                ''',
                ctds.IntegrityError,
                16,
                515
            ),
            (
                '''
                DECLARE @var TINYINT = 255;
                SET @var = @var + 1;
                ''',
                ctds.DataError,
                16,
                220
            ),
            (
                '''
                DECLARE @tTable TABLE (VarCharColumn VARCHAR(1) NOT NULL);
                INSERT INTO @tTable VALUES ('12');
                ''',
                ctds.DataError,
                16,
                8152
            ),
            (
                '''
                DECLARE @var FLOAT = CONVERT(FLOAT, 0x00);
                ''',
                ctds.DataError,
                16,
                529
            ),
            (
                '''
                DBCC CHECKDB ('master') WITH NO_INFOMSGS;
                ''',
                ctds.DatabaseError,
                14,
                7983
            ),
        ):
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    try:
                        cursor.execute(query)
                    except exception as ex:
                        self.assertEqual(ex.last_message['severity'], severity)
                        self.assertEqual(ex.last_message['number'], number)
                    else:
                        self.fail('query "{0}" did not fail'.format(query)) # pragma: nocover
