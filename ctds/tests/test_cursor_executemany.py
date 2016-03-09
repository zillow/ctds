from datetime import datetime
from decimal import Decimal
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import long_, unicode_

class TestCursorExecuteMany(TestExternalDatabase):
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
:param iterable seq_of_parameters: An iterable of parameter sequences to bind.
'''
        )

    def test_missing_command(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.executemany)

    def test_missing_parameter(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(IndexError, cursor.executemany, 'SELECT :1 AS missing', ((1,),))

    def test_invalid_format(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for case in (':', ':a', ':ab12'):
                    try:
                        cursor.executemany('SELECT {0} AS missing'.format(case), ((1,),))
                    except ctds.InterfaceError as ex:
                        self.assertEqual(str(ex), 'invalid parameter marker')
                    else:
                        self.fail('.executemany() did not fail as expected') # pragma: nocover

    def test_invalid_parameter(self):
        with self.connect() as connection:
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

                    self.assertEqual(warns[0].category, ctds.Warning)

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_no_arguments(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.executemany, '''SELECT ':0';''')

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
                    Decimal('1000000.0001')
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
                        args[8],
                    )
                )

    def test_compute(self):
        # COMPUTE clauses are only supported in SQL Server 2005 & 2008
        if self.sql_server_version()[0] < 11: # pragma: nocover
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
                    cursor.executemany('BOGUS :0', ((None,),))
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