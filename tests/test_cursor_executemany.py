from datetime import datetime
from decimal import Decimal
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import PY3, long_, unicode_

class TestCursorExecuteMany(TestExternalDatabase): # pylint: disable=too-many-public-methods
    '''Unit tests related to the Cursor.executemany() method.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.executemany.__doc__,
            '''\
executemany(sql, seq_of_parameters)

Prepare a database operation (query or command) and then execute it
against all parameter sequences or mappings found in the sequence
`seq_of_parameters`.

:pep:`0249#executemany`

:param str sql: The SQL statement to execute.
:param seq_of_parameters: An iterable of parameter sequences to bind.
:type seq_of_parameters: :ref:`typeiter <python:typeiter>`
'''
        )

    def test_missing_command(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.executemany)

    def test_missing_numeric_parameter(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for index, args in (
                        (0, [()]),
                        (1, [(1,)]),
                        (-1, [(1,)]),
                ):
                    self.assertRaises(IndexError, cursor.executemany, 'SELECT :{0} AS missing'.format(index), args)

    def test_invalid_numeric_format(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (':', ':a', ':ab12', ":'somestring'"):
                    try:
                        cursor.executemany('SELECT {0} AS missing'.format(case), ((1,),))
                    except ctds.InterfaceError as ex:
                        self.assertEqual(str(ex), 'invalid parameter marker')
                    else:
                        self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_invalid_numeric_parameter(self):
        with self.connect(paramstyle='numeric') as connection:
            with connection.cursor() as cursor:
                for case, ex, msg in (
                        (
                            (None),
                            TypeError,
                            "'NoneType' object is not iterable",
                        ),
                        (
                            ((None,), False),
                            TypeError,
                            'invalid parameter sequence item 1',
                        ),
                        (
                            ([None], (None, 1)),
                            ctds.InterfaceError,
                            'unexpected parameter count in sequence item 2'
                        ),
                        (
                            ([None], (x for x in (None,)), ()),
                            ctds.InterfaceError,
                            'unexpected parameter count in sequence item 3'
                        ),
                ):
                    try:
                        cursor.executemany('SELECT :0 AS missing', case)
                    except ex as actual:
                        self.assertEqual(str(actual), msg)
                    else:
                        self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_invalid_named_parameter(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                for case, ex, msg in (
                        (
                            ({'arg': None}, False),
                            TypeError,
                            'invalid parameter mapping item 1',
                        ),
                        (
                            ({'arg': None}, [None, 1]),
                            ctds.InterfaceError if PY3 else TypeError,
                            (
                                'unexpected parameter count in mapping item 2'
                                if PY3 else
                                'invalid parameter mapping item 1'
                            )
                        ),
                ):
                    try:
                        cursor.executemany('SELECT :arg AS missing', case)
                    except ex as actual:
                        self.assertEqual(str(actual), msg)
                    else:
                        self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_int_overflow(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(OverflowError, cursor.executemany, 'SELECT :0', ((long_(2**63),),))

    def test_sql_syntax_error(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.executemany('SELECT :0 FROM', ((None,),))
                except ctds.ProgrammingError as ex:
                    msg = "Incorrect syntax near 'FROM'."
                    self.assertEqual(str(ex), msg)
                    self.assertEqual(ex.severity, 15)
                    self.assertEqual(ex.os_error, None)
                    self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                    self.assertEqual(ex.last_message, {
                        'number': 102,
                        'state': 1,
                        'severity': 15,
                        'description': msg,
                        'proc': '',
                        'line': 1,
                    })
                else:
                    self.fail('.executemany() did not fail as expected') # pragma: nocover

                # The cursor should be usable after an exception.
                cursor.execute('SELECT @@VERSION')
                self.assertTrue(cursor.fetchall())

    def test_sql_raiseerror(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.executemany(
                        "RAISERROR (N'some custom error %s', 12, 111, 'hello!');",
                        (('hello1',),)
                    )
                except ctds.ProgrammingError as ex:
                    msg = "some custom error hello!"
                    self.assertEqual(str(ex), msg)
                    self.assertEqual(ex.severity, 12)
                    self.assertEqual(ex.os_error, None)
                    self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                    self.assertEqual(ex.last_message, {
                        'number': 50000,
                        'state': 111,
                        'severity': 12,
                        'description': msg,
                        'proc': '',
                        'line': 1,
                    })
                else:
                    self.fail('.executemany() did not fail as expected') # pragma: nocover

                # The cursor should be usable after an exception.
                cursor.execute('SELECT @@VERSION')
                self.assertTrue(cursor.fetchall())

    def test_sql_raiseerror_warning(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings(record=True) as warns:
                    cursor.executemany(
                        "RAISERROR (N'some custom non-severe error %s', 10, 111, :0);",
                        ((unicode_('hello!'),),)
                    )
                    msg = "some custom non-severe error hello!"
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        [msg] * len(warns)
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
                        cursor.executemany(
                            "RAISERROR (N'some custom non-severe error %s', 10, 111, :0);",
                            ((unicode_('hello!'),),)
                        )
                    except ctds.Warning as warn:
                        self.assertEqual('some custom non-severe error hello!', str(warn))
                    else:
                        self.fail('.executemany() did not fail as expected') # pragma: nocover

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_no_arguments(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.executemany, '''SELECT ':0';''')

    def test_escape_numeric(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for sqltype, constant, parameter in (
                        ('DATETIME', '2016-01-02 03:04:05', datetime(2016, 1, 2, 3, 4, 5)),
                        ('VARCHAR(100)', "'' foo '' :1 '' '':", "' foo ' :1 ' ':"),
                ):
                    cursor.executemany(
                        '''
                        SELECT
                            CONVERT({0}, '{1}') AS Constant,
                            :0 AS Parameter
                        '''.format(sqltype, constant),
                        ((parameter,),) * 2
                    )
                    for row in cursor.fetchall():
                        self.assertEqual(row['Constant'], row['Parameter'])

    def test_escape_named(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                for sqltype, constant, parameter in (
                        ('DATETIME', '2016-01-02 03:04:05', datetime(2016, 1, 2, 3, 4, 5)),
                        ('VARCHAR(100)', "'' foo '' :1 '' '':", "' foo ' :1 ' ':"),
                ):
                    cursor.executemany(
                        '''
                        SELECT
                            CONVERT({0}, '{1}') AS Constant,
                            :arg AS Parameter
                        '''.format(sqltype, constant),
                        ({'arg': parameter},) * 2
                    )
                    for row in cursor.fetchall():
                        self.assertEqual(row['Constant'], row['Parameter'])

    def test_format_numeric(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (
                    None,
                    -1234567890,
                    2 ** 45,
                    b'1234',
                    bytearray('1234', 'ascii'),
                    unicode_(
                        b'hello \'world\' ' + (b'\xe3\x83\x9b' if self.nchars_supported else b''),
                        encoding='utf-8'
                    ),
                    datetime(2001, 1, 1, 12, 13, 14, 150 * 1000),
                    Decimal('123.4567890'),
                    Decimal('1000000.4532')
                )
                query = cursor.executemany(
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
                    (args, args)
                )
                self.assertEqual(None, query)

                self.assertEqual(
                    tuple(cursor.fetchone()),
                    (
                        args[0],
                        args[1],
                        args[2],
                        args[3],
                        bytes(args[4]),
                        args[5],
                        args[5],
                        args[6],
                        args[7],
                        self.round_money(args[8]),
                    )
                )

    def test_format_named(self):
        with self.connect(paramstyle='named') as connection:
            with connection.cursor() as cursor:
                args = {
                    unicode_('none'): None,
                    'int': -1234567890,
                    unicode_('bigint'): 2 ** 45,
                    'bytes': ctds.Parameter(b'1234'),
                    'byte_array': bytearray('1234', 'ascii'),
                    'string': unicode_(
                        b'hello \'world\' ' + (b'\xe3\x83\x9b' if self.nchars_supported else b''),
                        encoding='utf-8'
                    ),
                    'datetime': datetime(2001, 1, 1, 12, 13, 14, 150 * 1000),
                    'decimal': Decimal('123.4567890'),
                    'money': Decimal('1000000.4532')
                }
                query = cursor.executemany(
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
                    (args, args)
                )
                self.assertEqual(None, query)

                while cursor.description:
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
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
                        ]
                    )
                    cursor.nextset()

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
                        cursor.executemany(
                            '''
                            SELECT :arg
                            ''',
                            (args, args)
                        )
                    except TypeError as ex:
                        self.assertEqual(str(ex), str(arg or ''))
                    else:
                        self.assertFalse(self.use_sp_executesql) # pragma: nocover

    def test_compute(self):
        # COMPUTE clauses are only supported in SQL Server 2005 & 2008
        version = self.sql_server_version()
        if version[0] > 10:
            version = '.'.join(str(part) for part in version)
            self.skipTest(
                'SQL Server {0} does not support compute columns'.format(version)
            )
        else: # pragma: nocover
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.executemany(
                        '''
                            DECLARE @{0} TABLE(s VARCHAR(10));
                            INSERT INTO @{0}(s) VALUES (:0),(:1),(:2);
                            SELECT s, s FROM @{0} COMPUTE COUNT(s);
                        '''.format(self.test_compute.__name__),
                        (('a', 'b', 'c'),)
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

                    cursor.executemany(
                        '''
                            DECLARE @{0}2 TABLE(i INT);
                            INSERT INTO @{0}2(i) VALUES (:0),(:1),(:2);
                            SELECT i, i, i FROM @{0}2 COMPUTE SUM(i), AVG(i);
                        '''.format(self.test_compute.__name__),
                        ((1, 2, 3),)
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
                cursor.executemany('SELECT @@VERSION')
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            try:
                cursor.executemany('SELECT @@VERSION')
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'connection closed')
            else:
                self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_sequences(self):
        with self.connect(autocommit=False) as connection:
            try:
                with connection.cursor() as cursor:
                    def iterables():
                        for sequence in (
                                (1, 2, 3),
                                [1, 2, 3],
                                range(1, 4)
                        ):
                            yield sequence

                    cursor.execute(
                        '''
                        CREATE TABLE {0} (Total INT)
                        '''.format(self.test_sequences.__name__)
                    )
                    cursor.executemany(
                        '''
                        INSERT INTO {0}(Total) VALUES (:0 + :1 + :2)
                        '''.format(self.test_sequences.__name__),
                        iterables()
                    )
                    cursor.execute('SELECT * FROM {0}'.format(self.test_sequences.__name__))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(sum(sequence),) for sequence in iterables()]
                    )
            finally:
                connection.rollback()

    def test_multiple(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.executemany('SELECT :0', ((None,),))
                self.assertEqual(
                    [tuple(row) for row in cursor.fetchall()],
                    [tuple((None,))]
                )
                cursor.executemany('SELECT :0', ((None,),))
                self.assertEqual(
                    [tuple(row) for row in cursor.fetchall()],
                    [tuple((None,))]
                )

    def test_execute_bad_query(self):
        try:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.executemany("BOGUS ''", ())
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
            self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_identity_insert(self):
        with self.connect(autocommit=False, paramstyle='named') as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0} (Ident INT IDENTITY(1,1));
                        '''.format(self.test_identity_insert.__name__)
                    )
                    cursor.execute(
                        '''
                        SET IDENTITY_INSERT {0} ON;
                        '''.format(self.test_identity_insert.__name__)
                    )
                    cursor.executemany(
                        '''
                        INSERT INTO {0}(Ident) VALUES (:ix);
                        '''.format(self.test_identity_insert.__name__),
                        ({'ix': ix} for ix in range(10))
                    )
                    cursor.execute(
                        '''
                        SET IDENTITY_INSERT {0} OFF;
                        '''.format(self.test_identity_insert.__name__)
                    )
            finally:
                connection.rollback()

    def test_variable_width_columns(self):
        with self.connect(autocommit=False, paramstyle='named') as connection:
            try:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        CREATE TABLE {0} (
                            [varchar] VARCHAR(MAX),
                            [nvarchar] NVARCHAR(MAX),
                            [varbinary] VARBINARY(MAX),
                            [int] BIGINT
                        );
                        '''.format(self.test_variable_width_columns.__name__)
                    )
                    cursor.executemany(
                        '''
                        INSERT INTO
                            {0}([varchar], [nvarchar], [varbinary], [int])
                        VALUES (
                            :varchar, :nvarchar, :varbinary, :int
                        );
                        '''.format(self.test_variable_width_columns.__name__),
                        (
                            {
                                'varchar': unicode_('v' * (ix + 1)),
                                'nvarchar': unicode_('n' * (ix + 1)),
                                'varbinary': b'b' * (ix + 1),
                                'int': 8 ** ix,
                            }
                            for ix in range(10)
                        )
                    )
                    cursor.execute(
                        '''
                        SELECT * FROM {0}
                        '''.format(self.test_variable_width_columns.__name__)
                    )

                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (
                                unicode_('v' * (ix + 1)),
                                unicode_('n' * (ix + 1)),
                                b'b' * (ix + 1),
                                8 ** ix,
                            )
                            for ix in range(10)
                        ]
                    )
            finally:
                connection.rollback()
