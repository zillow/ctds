from datetime import datetime
from decimal import Decimal
import re
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import long_, unicode_

class TestCursorExecute(TestExternalDatabase):
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

    def test_missing_parameter(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(IndexError, cursor.execute, 'SELECT :1 AS missing', (1,))

    def test_invalid_format(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (':', ':a', ':ab12'):
                    try:
                        cursor.execute('SELECT {0} AS missing'.format(case), (1,))
                    except ctds.InterfaceError as ex:
                        self.assertEqual(str(ex), 'invalid parameter marker')
                    else:
                        self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_int_overflow(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(OverflowError, cursor.execute, 'SELECT :0', (long_(2**63),))

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
                    self.assertEqual(ex.last_message, {
                        'number': 102,
                        'state': 1,
                        'severity': 15,
                        'description': msg,
                        'proc': '',
                        'line': 1,
                    })
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
                    self.assertEqual(ex.last_message, {
                        'number': 50000,
                        'state': 111,
                        'severity': 12,
                        'description': msg,
                        'proc': '',
                        'line': 2,
                    })
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
                        '''
                    )
                    msg = "some custom non-severe error hello!"
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        [msg] * len(warns)
                    )

                    self.assertEqual(warns[0].category, ctds.Warning)

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
                except ctds.ProgrammingError as ex:
                    regex = r'''Violation of PRIMARY KEY constraint 'PK__[^']+'. ''' \
                            r'''Cannot insert duplicate key in object 'dbo\.@tTable'. ''' \
                            r'''The duplicate key value is \(1\).'''
                    self.assertTrue(re.match(regex, str(ex)))
                    self.assertEqual(ex.severity, 14)
                    self.assertEqual(ex.os_error, None)
                    self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                    self.assertTrue(re.match(regex, ex.last_message.pop('description')))
                    self.assertEqual(ex.last_message, {
                        'number': 2627,
                        'state': 1,
                        'severity': 14,
                        'proc': '',
                        'line': 7,
                    })
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

    def test_no_arguments(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    SELECT ':0';
                    '''
                )
                self.assertEqual(tuple(cursor.fetchone()), (':0',))

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

    def test_format(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                args = (
                    None,
                    -1234567890,
                    2 ** 45,
                    b'1234',
                    bytearray('1234', 'ascii'),
                    unicode_('hello \'world\' \u0153'), # pylint: disable=anomalous-unicode-escape-in-string
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
                        args[3],
                        bytes(args[4]),
                        args[5],
                        args[5],
                        args[6],
                        args[7],
                        self.round_money(args[8]),
                    )
                )

    def test_escape(self):
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
            self.fail('DatabaseError expected') # pragma: nocover
