from datetime import datetime
from decimal import Decimal
import uuid
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import PY27, PY3, unichr_, unicode_

class TestCursorCallproc(TestExternalDatabase): # pylint: disable=too-many-public-methods
    '''Unit tests related to the Cursor.callproc() method.
    '''

    @staticmethod
    def stored_procedure(cursor, sproc, body, args=()):
        class StoredProcedureContext(object): # pylint: disable=too-few-public-methods
            def __enter__(self):
                cursor.execute(
                    '''
                    IF EXISTS (
                        SELECT 1
                        FROM sys.objects
                        WHERE object_id = OBJECT_ID(N'{0}')
                    )
                    BEGIN
                        DROP PROCEDURE {0};
                    END
                    '''.format(sproc)
                )
                cursor.execute('CREATE PROCEDURE {0}\n{1}'.format(sproc, body), args)

            def __exit__(self, exc_type, exc_value, traceback):
                cursor.execute('DROP PROCEDURE {0}'.format(sproc))

        return StoredProcedureContext()

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.callproc.__doc__,
            '''\
callproc(sproc, parameters)

Call a stored database procedure with the given name. The sequence of
parameters must contain one entry for each argument that the procedure
expects. The result of the call is returned as modified copy of the input
sequence. Input parameters are left untouched. Output and input/output
parameters are replaced with output values.

.. warning:: Due to `FreeTDS` implementation details, stored procedures
    with both output parameters and resultsets are not supported.

.. warning:: Currently `FreeTDS` does not support passing empty string
    parameters. Empty strings are converted to `NULL` values internally
    before being transmitted to the database.

:pep:`0249#callproc`

:param str sproc: The name of the stored procedure to execute.
:param parameters: Parameters to pass to the stored procedure.
    Parameters passed in a :py:class:`dict` must map from the parameter
    name to value and start with the **@** character. Parameters passed
    in a :py:class:`tuple` are passed in the tuple order.
:type parameters: dict or tuple
:return: The input `parameters` with any output parameters replaced with
    the output values.
:rtype: dict or tuple
'''
        )

    def test_closed_connection(self):
        connection = self.connect()
        with connection.cursor() as cursor:
            connection.close()
            try:
                cursor.callproc('myproc', ())
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'connection closed')
            else:
                self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_invalid_arguments(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertRaises(TypeError, cursor.callproc)
                self.assertRaises(TypeError, cursor.callproc, 'sproc')
                for case in ('1', None, 1, object()):
                    self.assertRaises(TypeError, cursor.callproc, 'sproc', case)

    def test_unknown_stored_procedure(self):
        with self.connect() as connection:
            sproc = 'this_is_an_unknown_sproc'
            with connection.cursor() as cursor:
                try:
                    cursor.callproc(sproc, ())
                except ctds.ProgrammingError as ex:
                    self.assertEqual(str(ex), 'Could not find stored procedure \'{0}\'.'.format(sproc))
                else:
                    self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_sql_exception(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_sql_exception.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                    AS
                        RAISERROR (N'some custom error %s', 12, 111, 'hello!');
                    '''
                    ):
                    try:
                        cursor.callproc(sproc, ())
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
                            'proc': sproc,
                            'line': 4,
                        })
                    else:
                        self.fail('.callproc() did not fail as expected') # pragma: nocover

                # The cursor should be usable after an exception.
                cursor.execute('SELECT @@VERSION')
                self.assertTrue(cursor.fetchall())

    def test_sql_raiseerror_warning(self):
        # The error is simply reported as a warning because another statement follows
        # and completes sucessfully.
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_sql_raiseerror_warning.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                    AS
                        RAISERROR (N'some custom error %s', 16, -1, 'hello!');
                        SELECT 1 As SomeResult
                    '''
                    ):
                    with warnings.catch_warnings(record=True) as warns:
                        cursor.callproc(sproc, ())
                        msg = unicode_('some custom error hello!')
                        self.assertEqual(len(warns), 1)
                        self.assertEqual(
                            [str(warn.message) for warn in warns],
                            [msg] * len(warns)
                        )
                        self.assertEqual(
                            [warn.category for warn in warns],
                            [ctds.Warning] * len(warns)
                        )

                    expected = [
                        {
                            'description': msg,
                            'line': 4,
                            'number': 50000,
                            'proc': sproc,
                            'severity': 16,
                            'state': 1,
                        },
                    ]

                    with warnings.catch_warnings(record=True):
                        for index, message in enumerate(connection.messages):
                            self.assertTrue(self.server_name_and_instance in message.pop('server'))
                            self.assertEqual(expected[index], message)

                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [
                            (1,)
                        ]
                    )

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_warning_as_error(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_warning_as_error.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                    AS
                        RAISERROR (N'some custom error %s', 16, -1, 'hello!');
                        SELECT 1 As SomeResult
                    '''
                    ):
                    with warnings.catch_warnings():
                        warnings.simplefilter('error', ctds.Warning)
                        try:
                            cursor.callproc(sproc, ())
                        except ctds.Warning as warn:
                            self.assertEqual('some custom error hello!', str(warn))
                        else:
                            self.fail('.callproc() did not fail as expected') # pragma: nocover

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_sql_print(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_sql_print.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pMsg VARCHAR(MAX)
                    AS
                        PRINT(@pMsg);
                    '''
                    ):
                    msg = unicode_('hello world!')
                    with warnings.catch_warnings(record=True) as warns:
                        cursor.callproc(sproc, (msg,))
                        self.assertEqual(len(warns), 0)

                    expected = [
                        {
                            'description': msg,
                            'line': 5,
                            'number': 0,
                            'proc': sproc,
                            'severity': 0,
                            'state': 1,
                        },
                    ]

                    with warnings.catch_warnings(record=True):
                        for index, message in enumerate(connection.messages):
                            self.assertTrue(self.server_name_and_instance in message.pop('server'))
                            self.assertEqual(expected[index], message)

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_dataerror(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_dataerror.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pInt TINYINT OUTPUT
                    AS
                        SELECT @pInt = @pInt * 255;
                    '''
                    ):
                    try:
                        cursor.callproc(sproc, (256,))
                    except ctds.DataError as ex:
                        msg = "Error converting data type smallint to tinyint."
                        self.assertEqual(str(ex), msg)
                        self.assertEqual(ex.severity, 16)
                        self.assertEqual(ex.os_error, None)
                        self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                        self.assertEqual(ex.last_message, {
                            'number': 8114,
                            'state': 5,
                            'severity': 16,
                            'description': msg,
                            'proc': sproc,
                            'line': 0,
                        })
                    else:
                        self.fail('.callproc() did not fail as expected') # pragma: nocover

                # The cursor should be usable after a warning.
                with warnings.catch_warnings(record=True) as warns:
                    cursor.execute('SELECT @@VERSION')
                    self.assertTrue(cursor.fetchall())
                    self.assertEqual(len(warns), 0)

    def test_sql_conversion_warning(self):
        with self.connect(ansi_defaults=False) as connection:
            with connection.cursor() as cursor:
                sproc = self.test_sql_conversion_warning.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pInt TINYINT OUTPUT,
                        @pBigInt BIGINT OUTPUT
                    AS
                        SELECT @pInt = @pInt * 255;
                        SELECT @pBigInt = @pBigInt - 1000;
                    '''
                    ):
                    with warnings.catch_warnings(record=True) as warns:
                        inputs = (
                            ctds.Parameter(10, output=True),
                            ctds.Parameter(1234, output=True),
                        )
                        outputs = cursor.callproc(sproc, inputs)
                        self.assertEqual(outputs, (None, inputs[1].value - 1000))
                        self.assertEqual(len(warns), 1)
                        msg = 'Arithmetic overflow occurred.'
                        self.assertEqual(
                            [str(warn.message) for warn in warns],
                            [msg] * len(warns)
                        )
                        self.assertEqual(
                            [warn.category for warn in warns],
                            [ctds.Warning] * len(warns)
                        )

    def test_sql_conversion_warning_as_error(self): # pylint: disable=invalid-name
        with self.connect(ansi_defaults=False) as connection:
            with connection.cursor() as cursor:
                sproc = self.test_sql_conversion_warning_as_error.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pInt TINYINT OUTPUT,
                        @pBigInt BIGINT OUTPUT
                    AS
                        SELECT @pInt = @pInt * 255;
                        SELECT @pBigInt = @pBigInt - 1000;
                    '''
                    ):
                    with warnings.catch_warnings():
                        warnings.simplefilter('error', ctds.Warning)
                        inputs = (
                            ctds.Parameter(10, output=True),
                            ctds.Parameter(1234, output=True),
                        )
                        try:
                            cursor.callproc(sproc, inputs)
                        except ctds.Warning as warn:
                            self.assertEqual('Arithmetic overflow occurred.', str(warn))
                        else:
                            self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_sql_conversion_error(self):
        with self.connect(ansi_defaults=True) as connection:
            with connection.cursor() as cursor:
                sproc = self.test_sql_conversion_warning.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pInt TINYINT OUTPUT,
                        @pBigInt BIGINT OUTPUT
                    AS
                        SELECT @pInt = @pInt * 255;
                        SELECT @pBigInt = @pBigInt - 1000;
                    '''
                    ):
                    inputs = (
                        ctds.Parameter(10, output=True),
                        ctds.Parameter(1234, output=True),
                    )
                    try:
                        cursor.callproc(sproc, inputs)
                    except ctds.ProgrammingError as ex:
                        self.assertEqual(
                            str(ex),
                            'Arithmetic overflow error for data type tinyint, value = {0}.'.format(
                                inputs[0].value * 255
                            )
                        )

    def test_output_with_resultset(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_output_with_resultset.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pOut INT OUTPUT
                    AS
                        SET @pOut = 5;
                        SELECT 10;
                    '''
                    ):
                    with warnings.catch_warnings(record=True) as warns:
                        cursor.callproc(sproc, (ctds.Parameter(ctds.SqlInt(0), output=True),))
                        cursor.callproc(sproc, {'@pOut': ctds.Parameter(ctds.SqlInt(0), output=True)})
                        self.assertEqual(len(warns), 2)
                        self.assertEqual(
                            [str(warn.message) for warn in warns],
                            ['output parameters are not supported with result sets'] * len(warns)
                        )
                        self.assertEqual(
                            [warn.category for warn in warns],
                            [Warning] * len(warns)
                        )

    def test_output_with_resultset_as_error(self): # pylint: disable=invalid-name
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_output_with_resultset_as_error.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pOut INT OUTPUT
                    AS
                        SET @pOut = 5;
                        SELECT 10;
                    '''
                    ):
                    with warnings.catch_warnings():
                        warnings.simplefilter('error')
                        try:
                            cursor.callproc(sproc, (ctds.Parameter(ctds.SqlInt(0), output=True),))
                        except Warning as warn:
                            self.assertEqual('output parameters are not supported with result sets', str(warn))
                        else:
                            self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_dict_invalid_name(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_dict_invalid_name.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pParameter BIGINT
                    AS
                        SELECT @pParameter;
                    '''
                ):
                    for case in ('pParameter', 1, '1', None, ''):
                        try:
                            cursor.callproc(sproc, {case: 1234})
                        except ctds.InterfaceError as ex:
                            self.assertEqual(
                                str(ex),
                                'invalid parameter name "{0}"'.format(repr(case))
                            )
                        else:
                            self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_dict(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_dict.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pBigInt BIGINT,
                        @pVarChar VARCHAR(32),
                        @pVarBinary VARBINARY(32),
                        @pDateTime DATETIME,
                        @pDecimal DECIMAL(32, 20)
                    AS
                        SELECT
                            @pDecimal AS Decimal,
                            @pVarBinary AS VarBinary,
                            @pVarChar AS VarChar,
                            @pBigInt AS BigInt,
                            @pDateTime AS DateTime;
                    '''
                    ):
                    types = [str]
                    if not PY3: # pragma: nocover
                        types.append(unicode_)
                    for type_ in types:
                        inputs = {
                            type_('@pDecimal'): Decimal('5.31234123'),
                            type_('@pBigInt'): 3453456908,
                            type_('@pVarBinary'): b'1234567890',
                            type_('@pVarChar'): 'this is some string',
                            type_('@pDateTime'): datetime(2011, 11, 5, 12, 12, 12)
                        }
                        outputs = cursor.callproc(sproc, inputs)
                        self.assertEqual(inputs, outputs)
                        for key in inputs:
                            # (N)(VAR)CHAR parameters are translated to UCS-2 and therefore
                            # the id() values will never match.
                            if key != '@pVarChar':
                                self.assertEqual(id(outputs[key]), id(inputs[key]))
                            self.assertEqual(outputs[key], inputs[key])

                        self.assertEqual(
                            [tuple(row) for row in cursor.fetchall()],
                            [
                                (
                                    inputs[type_('@pDecimal')],
                                    inputs[type_('@pVarBinary')],
                                    inputs[type_('@pVarChar')],
                                    inputs[type_('@pBigInt')],
                                    inputs[type_('@pDateTime')]
                                )
                            ]
                        )

    def test_dict_outputs(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_dict.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pBigInt BIGINT OUTPUT,
                        @pVarChar VARCHAR(32) OUTPUT,
                        @pVarBinary VARBINARY(32) OUTPUT,
                        @pFloat FLOAT OUTPUT,
                        @pDateTime DATETIME,
                        @pDateTimeOut DATETIME OUTPUT
                    AS
                        SET @pBigInt = @pBigInt * 2;
                        SET @pVarBinary = CONVERT(VARBINARY(32), @pVarChar);
                        SET @pVarChar = CONVERT(VARCHAR(32), @pDateTime, 120);
                        SET @pFloat = @pFloat * 2.1;
                        SET @pDateTimeOut = CONVERT(DATETIME, '2017-01-01 01:02:03');
                    '''
                    ):
                    types = [str]
                    if not PY3: # pragma: nocover
                        types.append(unicode_)
                    for type_ in types:
                        inputs = {
                            type_('@pBigInt'): ctds.Parameter(12345, output=True),
                            type_('@pVarChar'): ctds.Parameter(
                                unicode_('hello world, how\'s it going! '),
                                output=True
                            ),
                            type_('@pVarBinary'): ctds.Parameter(ctds.SqlVarBinary(None, size=32), output=True),
                            type_('@pFloat'): ctds.Parameter(1.23, output=True),
                            type_('@pDateTime'): datetime(2011, 11, 5, 12, 12, 12),
                            type_('@pDateTimeOut'): ctds.Parameter(datetime.utcnow(), output=True),
                        }
                        outputs = cursor.callproc(sproc, inputs)
                        self.assertNotEqual(id(outputs[type_('@pBigInt')]), id(inputs[type_('@pBigInt')]))
                        self.assertNotEqual(id(outputs[type_('@pVarChar')]), id(inputs[type_('@pVarChar')]))
                        self.assertNotEqual(id(outputs[type_('@pVarBinary')]), id(inputs[type_('@pVarBinary')]))
                        self.assertEqual(id(outputs[type_('@pDateTime')]), id(inputs[type_('@pDateTime')]))

                        self.assertEqual(
                            outputs[type_('@pBigInt')],
                            inputs[type_('@pBigInt')].value * 2
                        )
                        self.assertEqual(
                            outputs[type_('@pVarChar')],
                            inputs[type_('@pDateTime')].strftime('%Y-%m-%d %H:%M:%S')
                        )
                        self.assertEqual(
                            outputs[type_('@pVarBinary')],
                            bytes(inputs[type_('@pVarChar')].value.encode('utf-8'))
                        )
                        self.assertEqual(
                            outputs[type_('@pFloat')],
                            inputs[type_('@pFloat')].value * 2.1
                        )
                        self.assertEqual(
                            outputs[type_('@pDateTimeOut')],
                            datetime(2017, 1, 1, 1, 2, 3)
                        )

    def test_binary(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_binary.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pBinary BINARY(8),
                        @pVarBinary VARBINARY(16),

                        @pBinaryNone BINARY(1),
                        @pVarBinaryNone VARBINARY(1)
                    AS
                        SELECT
                            CONVERT(VARCHAR(32), @pBinary, 1),
                            CONVERT(VARCHAR(32), @pVarBinary, 1),

                            @pBinaryNone,
                            @pVarBinaryNone
                    '''
                    ):
                    inputs = (
                        b'\x01\x02\x03\x04',
                        bytearray(b'\xde\xad\xbe\xef\xde\xad\xbe\xef'),
                        ctds.SqlVarBinary(None),
                        ctds.SqlVarBinary(None)
                    )
                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))
                    self.assertEqual(id(outputs[1]), id(inputs[1]))

                    self.assertEqual(tuple(cursor.fetchone()), (
                        ('0x0102030400000000', '0xDEADBEEFDEADBEEF', None, None)
                    ))

                    input_dict = {
                        '@pBinaryNone': ctds.SqlVarBinary(None),
                        '@pVarBinary': bytearray(b'\xde\xad\xbe\xef\xde\xad\xbe\xef'),
                        '@pBinary': b'\x01\x02\x03\x04',
                        '@pVarBinaryNone': ctds.SqlVarBinary(None)
                    }
                    outputs = cursor.callproc(sproc, input_dict)
                    self.assertEqual(id(outputs['@pBinary']), id(input_dict['@pBinary']))
                    self.assertEqual(id(outputs['@pVarBinary']), id(input_dict['@pVarBinary']))

                    self.assertEqual(tuple(cursor.fetchone()), (
                        ('0x0102030400000000', '0xDEADBEEFDEADBEEF', None, None)
                    ))

    def test_binary_output(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_binary_output.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pIn VARBINARY(8),
                        @pOut VARBINARY(16) OUTPUT
                    AS
                        SET @pOut = @pIn + @pIn;
                    '''
                    ):
                    inputs = (
                        b'\x01\x02\x03\x04',
                        ctds.Parameter(ctds.SqlVarBinary(None, size=16), output=True)
                    )
                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))
                    self.assertEqual(outputs[1], inputs[0] + inputs[0])
                    self.assertRaises(ctds.InterfaceError, cursor.fetchone)

                    # Note: When the output parameter size is less than the actual data,
                    # SQL Server truncates the data.
                    inputs = (
                        b'\x01\x02\x03\x04',
                        ctds.Parameter(
                            ctds.SqlVarBinary(None, size=2),
                            output=True
                        )
                    )
                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))
                    self.assertEqual(outputs[1], inputs[0][:2])
                    self.assertRaises(ctds.InterfaceError, cursor.fetchone)

    def test_binary_inputoutput(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_binary_inputoutput.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pInOut VARBINARY(16) OUTPUT
                    AS
                        SET @pInOut = @pInOut + @pInOut;
                    '''
                    ):
                    value = b'\x01\x02\x03\x04'
                    inputs = (
                        ctds.Parameter(
                            ctds.SqlVarBinary(value, size=16),
                            output=True
                        ),
                    )
                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(outputs, (value + value,))
                    self.assertRaises(ctds.InterfaceError, cursor.fetchone)

    def test_int(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_int.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pTinyInt TINYINT,
                        @pSmallInt SMALLINT,
                        @pInt INT,
                        @pBigInt BIGINT,

                        @pTinyIntOut TINYINT OUTPUT,
                        @pSmallIntOut SMALLINT OUTPUT,
                        @pIntOut INT OUTPUT,
                        @pBigIntOut BIGINT OUTPUT
                    AS
                        SELECT @pTinyIntOut = @pTinyInt + 1;
                        SELECT @pSmallIntOut = -1 * @pSmallInt;
                        SELECT @pIntOut = -1 * @pInt;
                        SELECT @pBigIntOut = -1 * @pBigInt;
                    '''
                    ):
                    inputs = (
                        1,
                        2 ** 8,
                        2 ** 15,
                        2 ** 31,
                        ctds.Parameter(ctds.SqlTinyInt(None), output=True),
                        ctds.Parameter(ctds.SqlSmallInt(None), output=True),
                        ctds.Parameter(ctds.SqlInt(None), output=True),
                        ctds.Parameter(ctds.SqlBigInt(None), output=True),
                    )
                    outputs = cursor.callproc(sproc, inputs)
                    self.assertRaises(ctds.InterfaceError, cursor.fetchone)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))
                    self.assertEqual(id(outputs[1]), id(inputs[1]))
                    self.assertEqual(id(outputs[2]), id(inputs[2]))
                    self.assertEqual(id(outputs[3]), id(inputs[3]))

                    self.assertEqual(
                        outputs[4:],
                        (
                            outputs[0] + 1,
                            outputs[1] * -1,
                            outputs[2] * -1,
                            outputs[3] * -1,
                        )
                    )

    def test_float(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_float.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pReal REAL,
                        @pFloat24 FLOAT(24),
                        @pFloat53 FLOAT(53),

                        @pRealOut REAL OUTPUT,
                        @pFloat24Out FLOAT(24) OUTPUT,
                        @pFloat53Out FLOAT(53) OUTPUT
                    AS
                        SET @pRealOut = @pReal * 2;
                        SET @pFloat24Out = @pFloat24 * 2;
                        SET @pFloat53Out = @pFloat53 * 2;

                        SELECT
                            @pReal AS Real,
                            @pFloat24 AS Float24,
                            @pFloat53 AS Float53;
                    '''
                    ):
                    inputs = (
                        12345.6787109375,
                        1234567936.0,
                        453423423423445352345.23424123,
                        0, 0, 0
                    )
                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))
                    self.assertEqual(id(outputs[1]), id(inputs[1]))
                    self.assertEqual(id(outputs[2]), id(inputs[2]))

                    self.assertEqual(tuple(cursor.fetchone()), inputs[0:3])
                    self.assertEqual(cursor.fetchone(), None)

    def test_datetime(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_datetime.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pDatetime DATETIME,
                        @pDatetimeOut DATETIME OUTPUT
                    AS
                        SET @pDatetimeOut = '{0}';

                        SELECT
                            @pDatetime AS Datetime;
                    '''.format(datetime(1999, 12, 31, 23, 59, 59).strftime('%Y-%m-%d %H:%M:%S'))
                    ):
                    inputs = (
                        datetime(2001, 1, 1, 1, 1, 1, 123000),
                        datetime(1984, 1, 1, 1, 1, 1),
                    )

                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))

                    self.assertEqual(tuple(cursor.fetchone()), inputs[0:1])
                    self.assertEqual(cursor.fetchone(), None)

    def test_smalldatetime(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_datetime.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pSmallDatetime SMALLDATETIME,
                        @pSmallDatetimeOut SMALLDATETIME OUTPUT
                    AS
                        SET @pSmallDatetimeOut = '{0}';
                    '''.format(datetime(1999, 12, 31, 23, 59, 59).strftime('%Y-%m-%d %H:%M:%S'))
                    ):
                    inputs = (
                        datetime(2001, 1, 1, 1, 1, 1, 123456),
                        datetime(1984, 1, 1, 1, 1, 1),
                    )

                    outputs = cursor.callproc(sproc, inputs)
                    self.assertEqual(id(outputs[0]), id(inputs[0]))
                    self.assertRaises(ctds.InterfaceError, cursor.fetchone)

    def test_decimal(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_decimal.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pDecimal DECIMAL(7,3),
                        @pDecimalOut VARCHAR(MAX) OUTPUT
                    AS
                        SET @pDecimalOut = CONVERT(VARCHAR(MAX), @pDecimal);
                    '''
                    ):
                    for value, string, truncation in (
                            (1000.0, '1000.000', False),
                            (-1000.0, '-1000.000', False),
                            (9999.999, '9999.999', True),
                            (-9999.999, '-9999.999', True),
                            (.999, '0.999', True),
                            (-.999, '-0.999', True),
                            (.001, '0.001', True),
                            (-.001, '-0.001', True),
                            (0, '0.000', False),
                    ):
                        inputs = (
                            Decimal(value if PY27 else str(value)),
                            ctds.Parameter(unicode_('*' * 256), output=True),
                        )

                        with warnings.catch_warnings(record=True) as warns:
                            outputs = cursor.callproc(sproc, inputs)
                            if truncation and PY27:
                                self.assertEqual(len(warns), 1)
                                msg = "Decimal('{0}') exceeds SQL DECIMAL precision; truncating".format(inputs[0])
                                self.assertEqual(
                                    [str(warn.message) for warn in warns],
                                    [msg] * len(warns)
                                )
                                self.assertEqual(warns[0].category, Warning)
                            else:
                                self.assertEqual(len(warns), 0)

                            self.assertEqual(id(outputs[0]), id(inputs[0]))
                            self.assertEqual(string, outputs[1])
                            self.assertRaises(ctds.InterfaceError, cursor.fetchone)

    def test_decimal_output(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_decimal_output.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pString VARCHAR(MAX),
                        @pDecimalOut DECIMAL(15,5) OUTPUT
                    AS
                        SET @pDecimalOut = CONVERT(DECIMAL(15,5), @pString);
                    '''
                    ):
                    for value in (
                            '1230.456',
                            '123456789.1234',
                            '123',
                            '1234567890.123456789123456789',
                            None
                    ):
                        inputs = (
                            value,
                            ctds.Parameter(ctds.SqlDecimal(value, precision=15, scale=5), output=True),
                        )

                        outputs = cursor.callproc(sproc, inputs)
                        self.assertEqual(id(outputs[0]), id(inputs[0]))
                        if outputs[0] is not None:
                            self.assertEqual(
                                Decimal(outputs[0]).quantize(Decimal('.00001')),
                                outputs[1]
                            )
                        else:
                            self.assertEqual(None, outputs[1])

    def test_decimal_outofrange(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_decimal_outofrange.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pDecimal DECIMAL(7,3),
                        @pDecimalOut VARCHAR(MAX) OUTPUT
                    AS
                        SET @pDecimalOut = CONVERT(VARCHAR(MAX), @pDecimal);
                    '''
                    ):
                    for value in (
                            10 ** 38,
                            -10 ** 38,
                    ):
                        inputs = (
                            Decimal(value),
                            ctds.Parameter(unicode_('*' * 256), output=True),
                        )
                        try:
                            cursor.callproc(sproc, inputs)
                        except ctds.DataError as ex:
                            self.assertEqual(
                                str(ex),
                                "Decimal('{0}') out of range".format(value)
                            )
                        else:
                            self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_decimal_internalerror(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_decimal_internalerror.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pDecimal DECIMAL(7,3)
                    AS
                        SELECT CONVERT(VARCHAR(MAX), @pDecimal);
                    '''
                    ):
                    inputs = (Decimal('NaN'),)
                    try:
                        cursor.callproc(sproc, inputs)
                    except ctds.InterfaceError as ex:
                        self.assertEqual(
                            str(ex),
                            'failed to convert {0}'.format(repr(inputs[0]))
                        )
                    else:
                        self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_varchar(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_varchar.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar VARCHAR(256),
                        @pVarCharOut VARCHAR(256) OUTPUT
                    AS
                        SET @pVarCharOut = @pVarChar;
                    '''
                    ):

                    format_ = (
                        unichr_(191) + unicode_(' 8 ') +
                        unichr_(247) + unicode_(' 2 = 4 ? {0} {1}')
                    )
                    snowman = unichr_(9731)

                    # Python must be built with UCS4 support to test the large codepoints.
                    if self.UCS4_SUPPORTED:
                        catface = unichr_(128568)
                    else:
                        catface = self.UNICODE_REPLACEMENT

                    # Older versions of SQL server don't support passing codepoints outside
                    # of the server's code page. SQL Server defaults to latin-1, so assume
                    # non-latin-1 codepoints won't be supported.
                    if not self.use_sp_executesql: # pragma: nocover
                        catface = unicode_('?')
                        snowman = unicode_('?')

                    inputs = (
                        format_.format(snowman, catface),
                        ctds.Parameter(ctds.SqlVarChar(None, size=256), output=True),
                    )

                    # If the connection supports UTF-16, unicode codepoints outside of the UCS-2
                    # range are supported and not replaced by ctds.
                    if self.use_utf16:
                        outputs = cursor.callproc(sproc, inputs)
                        # SQL server will improperly convert the values to single-byte, which corrupts them.
                        # FreeTDS will replace them on reads with a '?'.
                        # The catface is a multi-byte UTF-16 character, and therefore is replaced by two '?'
                        # in UCS-4 builds, and one '?' in UCS-2 builds of python.
                        self.assertEqual(
                            format_.format(unicode_('?'), unicode_('??' if self.UCS4_SUPPORTED else '?')),
                            outputs[1]
                        )
                    else: # pragma: nocover
                        # The catface is not representable in UCS-2, and therefore is replaced.
                        with warnings.catch_warnings(record=True) as warns:
                            outputs = cursor.callproc(sproc, inputs)
                            if ord(catface) > 2**16:
                                self.assertEqual(len(warns), 1)
                                msg = 'Unicode codepoint U+{0:08X} is not representable in UCS-2; replaced with U+{1:04X}'.format( # pylint: disable=line-too-long
                                    ord(catface),
                                    ord(self.UNICODE_REPLACEMENT)
                                )
                                self.assertEqual(
                                    [str(warn.message) for warn in warns],
                                    [msg] * len(warns)
                                )
                                self.assertEqual(
                                    [warn.category for warn in warns],
                                    [UnicodeWarning] * len(warns)
                                )
                            else:
                                self.assertEqual(len(warns), 0) # pragma: nocover

                        # SQL server will improperly convert the values to single-byte, which corrupts them.
                        # FreeTDS will replace them on reads with a '?'.
                        self.assertEqual(
                            format_.format(unicode_('?'), unicode_('?')),
                            outputs[1]
                        )

                    self.assertEqual(id(inputs[0]), id(outputs[0]))
                    self.assertNotEqual(id(inputs[1]), id(outputs[1]))

    def test_ucs2_warning_as_error(self):
        if not self.use_utf16 and self.use_sp_executesql and self.UCS4_SUPPORTED: # pragma: nocover
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    sproc = self.test_ucs2_warning_as_error.__name__
                    with self.stored_procedure(
                        cursor,
                        sproc,
                        '''
                            @pVarChar VARCHAR(256),
                            @pVarCharOut VARCHAR(256) OUTPUT
                        AS
                            SET @pVarCharOut = @pVarChar;
                        '''
                        ):

                        format_ = (
                            unichr_(191) + unicode_(' 8 ') +
                            unichr_(247) + unicode_(' 2 = 4 ? {0}')
                        )
                        catface = unichr_(128568)

                        inputs = (
                            format_.format(catface),
                            ctds.Parameter(ctds.SqlVarChar(None, size=256), output=True),
                        )

                        with warnings.catch_warnings():
                            warnings.simplefilter('error', UnicodeWarning)
                            try:
                                cursor.callproc(sproc, inputs)
                            except UnicodeWarning as warn:
                                msg = 'Unicode codepoint U+{0:08X} is not representable in UCS-2; replaced with U+{1:04X}'.format( # pylint: disable=line-too-long
                                    ord(catface),
                                    ord(self.UNICODE_REPLACEMENT)
                                )
                                self.assertEqual(msg, str(warn))
                            else:
                                self.fail('.callproc() did not fail as expected') # pragma: nocover

    def test_varchar_bytes(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_varchar_bytes.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar VARCHAR(MAX)
                    AS
                        SELECT LEN(@pVarChar), @pVarChar;
                    '''
                    ):
                    inputs = (b'some bytes for a VARCHAR parameter',)
                    cursor.callproc(sproc, inputs)
                    row = cursor.fetchone()
                    self.assertEqual(len(inputs[0]), row[0])
                    self.assertEqual(unicode_(inputs[0], encoding='ascii'), row[1])

    def test_varchar_max(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_varchar_max.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar VARCHAR(MAX)
                    AS
                        SELECT LEN(@pVarChar), @pVarChar;
                    '''
                    ):
                    lengths = [
                        1, 2, 3, 3999, 4000
                    ]
                    # FreeTDS 0.92.x doesn't properly handle VARCHAR types > 4000 characters.
                    if self.freetds_version[:2] != (0, 92): # pragma: nocover
                        lengths.extend([
                            4001, 7999, 8000, 8001
                        ])
                    for length in lengths:
                        inputs = (unicode_('*') * length,)
                        cursor.callproc(sproc, inputs)
                        row = cursor.fetchone()
                        self.assertEqual(len(inputs[0]), row[0])
                        self.assertEqual(inputs[0], row[1])

    def test_varchar_null(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_varchar_null.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar VARCHAR(256),
                        @pIsNullOut BIT OUTPUT
                    AS
                        IF @pVarChar IS NULL
                            BEGIN
                                SET @pIsNullOut = 1;
                            END
                        ELSE
                            BEGIN
                                SET @pIsNullOut = 0;
                            END
                    '''
                    ):

                    for value in (None, unicode_(''), unicode_('0'), unicode_('one')):
                        inputs = (
                            value,
                            ctds.Parameter(True, output=True),
                        )
                        outputs = cursor.callproc(sproc, inputs)

                        # $future: fix this once supported by FreeTDS
                        # Currently FreeTDS (really the db-lib API) will
                        # turn the empty string to NULL
                        self.assertEqual(outputs[1], inputs[0] is None or inputs[0] == '')

    def test_varchar_empty(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_varchar_null.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar VARCHAR(256)
                    AS
                        SELECT @pVarChar, LEN(@pVarChar) AS Length
                    '''
                    ):

                    for value in (None, unicode_(''), unicode_('0'), unicode_('one')):
                        inputs = (
                            value,
                        )
                        outputs = cursor.callproc(sproc, inputs)
                        self.assertEqual(
                            tuple(cursor.fetchone()),
                            (None, None) if value in (None, unicode_('')) else (value, len(value))
                        )

    def test_nvarchar(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_nvarchar.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar NVARCHAR(256),
                        @pVarCharOut NVARCHAR(256) OUTPUT
                    AS
                        SET @pVarCharOut = @pVarChar;
                    '''
                    ):

                    format_ = (
                        unichr_(191) + unicode_(' 8 ') +
                        unichr_(247) + unicode_(' 2 = 4 ? {0} {1} {2} test')
                    )
                    snowman = unichr_(9731)

                    # Python must be built with UCS4 support to test the large codepoints.
                    if self.UCS4_SUPPORTED:
                        catface = unichr_(128568)
                        flower = unichr_(127802)
                    else:
                        catface = self.UNICODE_REPLACEMENT
                        flower = self.UNICODE_REPLACEMENT

                    # Older versions of SQL server don't support passing codepoints outside
                    # of the server's code page. SQL Server defaults to latin-1, so assume
                    # non-latin-1 codepoints won't be supported.
                    if not self.use_sp_executesql: # pragma: nocover
                        catface = unicode_('?')
                        snowman = unicode_('?')
                        flower = unicode_('?')

                    inputs = (
                        format_.format(snowman, catface, flower),
                        ctds.Parameter(ctds.SqlVarChar(None, size=256), output=True),
                    )

                    # If the connection supports UTF-16, unicode codepoints outside of the UCS-2
                    # range are supported and not replaced by ctds.
                    if self.use_utf16:
                        outputs = cursor.callproc(sproc, inputs)
                        self.assertEqual(inputs[0], outputs[1])
                    else: # pragma: nocover
                        # The catface is not representable in UCS-2, and therefore is replaced.
                        with warnings.catch_warnings(record=True) as warns:
                            outputs = cursor.callproc(sproc, inputs)
                            if ord(catface) > 2**16:
                                self.assertEqual(len(warns), 2)
                                msg = unicode_('Unicode codepoint U+{0:08X} is not representable in UCS-2; replaced with U+{1:04X}') # pylint: disable=line-too-long
                                self.assertEqual(
                                    [str(warn.message) for warn in warns],
                                    [msg.format(ord(char), ord(self.UNICODE_REPLACEMENT)) for char in (catface, flower)]
                                )
                                self.assertEqual(
                                    [warn.category for warn in warns],
                                    [UnicodeWarning] * len(warns)
                                )
                            else:
                                self.assertEqual(len(warns), 0) # pragma: nocover

                        self.assertEqual(
                            format_.format(
                                snowman,
                                self.UNICODE_REPLACEMENT if self.use_sp_executesql else unicode_('?'),
                                self.UNICODE_REPLACEMENT if self.use_sp_executesql else unicode_('?')
                            ),
                            outputs[1]
                        )

                    self.assertEqual(id(inputs[0]), id(outputs[0]))
                    self.assertNotEqual(id(inputs[1]), id(outputs[1]))

    def test_nvarchar_max(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_nvarchar_max.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pVarChar NVARCHAR(MAX)
                    AS
                        SELECT LEN(@pVarChar), @pVarChar;
                    '''
                    ):
                    lengths = [
                        1, 2, 3, 3999, 4000
                    ]
                    # FreeTDS 0.92.x doesn't properly handle NVARCHAR types > 4000 characters.
                    if self.freetds_version[:2] != (0, 92): # pragma: nocover
                        lengths.extend([
                            4001, 7999, 8000, 8001
                        ])

                    for length in lengths:
                        inputs = (
                            unicode_(b'\xe3\x83\x9b' if self.nchars_supported else b'&', encoding='utf-8') * length,
                        )
                        cursor.callproc(sproc, inputs)
                        row = cursor.fetchone()
                        self.assertEqual(len(inputs[0]), row[0])
                        self.assertEqual(inputs[0], row[1])

    def test_nvarchar_null(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_nvarchar_null.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pNVarChar NVARCHAR(256),
                        @pIsNullOut BIT OUTPUT
                    AS
                        IF @pNVarChar IS NULL
                            BEGIN
                                SET @pIsNullOut = 1;
                            END
                        ELSE
                            BEGIN
                                SET @pIsNullOut = 0;
                            END
                    '''
                    ):

                    for value in (None, unicode_(''), unicode_('0'), unicode_('one')):
                        inputs = (
                            value,
                            ctds.Parameter(True, output=True),
                        )
                        outputs = cursor.callproc(sproc, inputs)
                        self.assertEqual(outputs[1], inputs[0] in (None, unicode_('')))

    def test_guid(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                sproc = self.test_guid.__name__
                with self.stored_procedure(
                    cursor,
                    sproc,
                    '''
                        @pIn UNIQUEIDENTIFIER,
                        @pOut UNIQUEIDENTIFIER OUTPUT
                    AS
                        SET @pOut = @pIn;
                    '''
                    ):
                    for value in (
                            None,
                            uuid.uuid1(),
                    ):
                        inputs = (
                            value,
                            ctds.Parameter(uuid.uuid1(), output=True)
                        )
                        outputs = cursor.callproc(sproc, inputs)

                        # FreeTDS doesn't support passing raw GUIDs, so they are convereted to
                        # VARCHAR. On output, the type is also VARCHAR.
                        expected = uuid.UUID('{{{0}}}'.format(outputs[1])) if value is not None else None
                        self.assertEqual(value, expected)
