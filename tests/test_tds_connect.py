import ctds

from .base import TestExternalDatabase
from .compat import long_, PY3, unicode_

class TestTdsConnection(TestExternalDatabase):

    def test__doc__(self):
        self.assertEqual(
            ctds.connect.__doc__,
            # pylint: disable=line-too-long
            '''\
connect(server, port=1433, instance=None, user='', password='', database=None, \
appname='ctds', login_timeout=5, timeout=5, tds_version=None, autocommit=False, \
ansi_defaults=True, enable_bcp=True, paramstyle=None, read_only=False)

Connect to a database.

.. note::

    :py:meth:`ctds.Connection.close` should be called when the returned
    connection object is no longer required.

:pep:`0249#connect`

.. versionadded:: 1.6
    `paramstyle`

:param str server: The database server host.
:param int port: The database server port. This value is ignored if
    `instance` is provided.
:param str instance: An optional database instance to connect to.
:param str user: The database server username.
:param str password: The database server password.
:param str database: An optional database to initially connect to.
:param str appname: An optional application name to associate with
    the connection.
:param int login_timeout: An optional login timeout, in seconds.
:param int timeout: An optional timeout for database requests, in
    seconds.
:param str tds_version: The TDS protocol version to use. If
    :py:data:`None` is specified, the highest version supported by
    FreeTDS will be used.
:param bool autocommit: Autocommit transactions on the connection.
:param bool ansi_defaults: Set `ANSI_DEFAULTS` and related settings to
    mimic ODBC drivers.
:param bool enable_bcp: Enable bulk copy support on the connection. This
    is required for :py:meth:`.bulk_insert` to function.
:param str paramstyle: Override the default :py:data:`ctds.paramstyle` value for
    this connection. Supported values: `numeric`, `named`.
:param bool read_only: Indicate 'read-only' application intent.
:return: A new `Connection` object connected to the database.
:rtype: Connection
'''
        )

    def test_arguments(self):
        for kwargs in (
                {'port': ''},
                {'port': '1234'},
                {'port': None},
                {'instance': 123},
                {'user': 123},
                {'password': 123},
                {'database': 123},
                {'appname': 123},
                {'login_timeout': ''},
                {'login_timeout': '1234'},
                {'login_timeout': None},
                {'timeout': ''},
                {'timeout': '1234'},
                {'timeout': None},
                {'tds_version': 123},
        ):
            self.assertRaises(
                TypeError,
                ctds.connect,
                '127.0.0.1', # use an IP to avoid DNS lookup timeouts
                **kwargs
            )

    def test_typeerror(self):
        def string_case(name):
            cases = [
                (('hostname',), {name: 1234}),
                (('hostname',), {name: object()}),
            ]
            if PY3: # pragma: nocover
                cases.append((('hostname',), {name: b'1234'}))
            return cases
        def uint_case(name):
            return [
                (('hostname',), {name: '1234'}),
                (('hostname',), {name: unicode_('1234')}),
                (('hostname',), {name: b'1234'}),
                (('hostname',), {name: None}),
                (('hostname',), {name: object()}),
            ]
        def bool_case(name):
            return [
                (('hostname',), {name: 'False'}),
                (('hostname',), {name: 0}),
                (('hostname',), {name: 1}),
                (('hostname',), {name: None}),
            ]

        cases = (
            [
                ((None,), {}),
                ((1,), {},),
            ] +
            uint_case('port') +
            string_case('instance') +
            string_case('user') +
            string_case('password') +
            string_case('database') +
            string_case('appname') +
            uint_case('login_timeout') +
            uint_case('timeout') +
            string_case('tds_version') +
            bool_case('autocommit') +
            bool_case('ansi_defaults') +
            bool_case('enable_bcp') +
            string_case('paramstyle') +
            bool_case('read_only')
        )

        for args, kwargs in cases:
            try:
                connection = ctds.connect(*args, **kwargs)
                connection.close() # pragma: nocover
            except TypeError:
                pass
            else:
                self.fail('.connect() did not fail as expected') # pragma: nocover

    def test_tds_version(self):
        for tds_version in (
                '7',
                '7.13',
                '7.30'
        ):
            try:
                connection = ctds.connect('hostname', tds_version=tds_version)
                connection.close() # pragma: nocover
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'unsupported TDS version "{0}"'.format(tds_version))
            else:
                self.fail('.connect() did not fail as expected') # pragma: nocover

    def test_paramstyle(self):
        for paramstyle in (
                'qmark',
                'NUMERIC',
                'nAmed',
                'unknown'
        ):
            try:
                connection = ctds.connect('hostname', paramstyle=paramstyle)
                connection.close() # pragma: nocover
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'unsupported paramstyle "{0}"'.format(paramstyle))
            else:
                self.fail('.connect() did not fail as expected') # pragma: nocover

    def test_interfaceerror(self):
        for kwargs in (
                {'user': '*' * 256},
                {'password': '*' * 256},
                {'appname': '*' * 256}
        ):
            try:
                connection = ctds.connect('hostname', **kwargs)
                connection.close() # pragma: nocover
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), next(iter(kwargs.values())))
            else:
                self.fail('.connect() did not fail as expected') # pragma: nocover

    def test_error_unavailable(self):
        host = '127.0.0.1' # use an IP to avoid DNS lookup timeouts
        try:
            ctds.connect(
                host,
                login_timeout=1,
                tds_version='7.1',
                port=self.get_option('port', int) + 1000
            )
        except ctds.OperationalError as ex:
            # FreeTDS version 0.95+ adds a (<host>:<port) to this error.
            self.assertTrue(
                'Unable to connect: Adaptive Server is unavailable or does not exist' in str(ex)
            )
            self.assertEqual(ex.severity, 9)
            self.assertEqual(ex.db_error['number'], 20009)
            self.assertTrue(
                'Unable to connect: Adaptive Server is unavailable or does not exist' in ex.db_error['description']
            )
            # Sepcific errors vary by platform and FreeTDS version.
            self.assertTrue(isinstance(ex.os_error['description'], unicode_))
            self.assertTrue(isinstance(ex.os_error['number'], long_))

            self.assertEqual(ex.last_message, None)
        else:
            self.fail('.connect() did not fail as expected') # pragma: nocover

    def test_error_login(self):
        for username, password in (
                (self.get_option('user'), self.get_option('password') + 'invalid'),
                (self.get_option('user') + 'invalid', self.get_option('password')),
        ):
            try:
                ctds.connect(
                    self.get_option('server'),
                    port=self.get_option('port', type_=int),
                    instance=self.get_option('instance'),
                    user=username,
                    password=password,
                    tds_version='7.1'
                )
            except ctds.OperationalError as ex:
                msg = "Login failed for user '{0}'.".format(username)
                self.assertEqual(
                    str(ex),
                    msg
                )
                self.assertEqual(ex.severity, 9)
                self.assertEqual(ex.db_error['number'], 20002)

                # FreeTDS version 0.95+ adds a (<host>:<port) to this error.
                self.assertTrue(
                    'Adaptive Server connection failed' in ex.db_error['description']
                )
                self.assertEqual(ex.os_error, None)
                self.assertTrue(self.server_name_and_instance in ex.last_message.pop('server'))
                self.assertEqual(ex.last_message, {
                    'description': msg,
                    'line': 1,
                    'number': 18456,
                    'proc': '',
                    'severity': 14,
                    'state': 1
                })
            else:
                self.fail('.connect() did not fail as expected') # pragma: nocover

    def test_autocommit(self):
        with self.connect(autocommit=True) as connection:
            self.assertEqual(connection.autocommit, True)
            with connection.cursor() as cursor:
                cursor.execute('SELECT 1 FROM sys.objects')
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

                # Verify the IMPLICIT_TRANSACTIONS setting is OFF.
                cursor.execute('SELECT @@OPTIONS')
                self.assertFalse(self.IMPLICIT_TRANSACTIONS & cursor.fetchone()[0])

        with self.connect(autocommit=False) as connection:
            self.assertEqual(connection.autocommit, False)
            with connection.cursor() as cursor:
                cursor.execute('SELECT 1 FROM sys.objects')
                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(1, cursor.fetchone()[0])

                # Verify the IMPLICIT_TRANSACTIONS setting is ON.
                cursor.execute('SELECT @@OPTIONS')
                self.assertTrue(self.IMPLICIT_TRANSACTIONS & cursor.fetchone()[0])

                connection.rollback()

                cursor.execute('SELECT @@TRANCOUNT')
                self.assertEqual(0, cursor.fetchone()[0])

                # Verify the IMPLICIT_TRANSACTIONS setting is ON.
                cursor.execute('SELECT @@OPTIONS')
                self.assertTrue(self.IMPLICIT_TRANSACTIONS & cursor.fetchone()[0])

    def test_ansi_defaults(self):

        for autocommit in (True, False):
            with self.connect(ansi_defaults=True, autocommit=autocommit) as connection:
                with connection.cursor() as cursor:
                    cursor.execute('SELECT @@OPTIONS')
                    options = cursor.fetchone()[0]

                    self.assertEqual(bool(self.IMPLICIT_TRANSACTIONS & options), not autocommit)
                    self.assertTrue(self.CURSOR_CLOSE_ON_COMMIT & options)
                    self.assertTrue(self.ANSI_PADDING & options)
                    self.assertTrue(self.ANSI_NULLS & options)
                    self.assertTrue(self.ARITHABORT & options)
                    self.assertFalse(self.ARITHIGNORE & options)
                    self.assertTrue(self.QUOTED_IDENTIFIER & options)
                    self.assertTrue(self.ANSI_NULL_DFLT_ON & options)
                    self.assertFalse(self.ANSI_NULL_DFLT_OFF & options)

            with self.connect(ansi_defaults=False, autocommit=autocommit) as connection:
                with connection.cursor() as cursor:
                    cursor.execute('SELECT @@OPTIONS')
                    options = cursor.fetchone()[0]

                    self.assertEqual(bool(self.IMPLICIT_TRANSACTIONS & options), not autocommit)
                    self.assertFalse(self.CURSOR_CLOSE_ON_COMMIT & options)
                    self.assertFalse(self.ANSI_PADDING & options)
                    self.assertFalse(self.ANSI_NULLS & options)
                    self.assertFalse(self.ARITHABORT & options)
                    self.assertFalse(self.ARITHIGNORE & options)
                    self.assertFalse(self.QUOTED_IDENTIFIER & options)
                    self.assertFalse(self.ANSI_NULL_DFLT_ON & options)
                    self.assertFalse(self.ANSI_NULL_DFLT_OFF & options)

    def test_read_only(self):
        try:
            with self.connect(read_only=True):
                pass
        except NotImplementedError as ex:
            self.assertFalse(self.read_only_intent_supported)
        else:
            self.assertTrue(self.read_only_intent_supported)
