from decimal import Decimal
import re
import os
import unittest

import ctds

from .compat import configparser, PY27, sysconfig

class TestExternalDatabase(unittest.TestCase):

    maxDiff = None

    UCS4_SUPPORTED = sysconfig.get_config_vars().get('Py_UNICODE_SIZE', 4) > 2

    # T-SQL option values. These are the bit mask values for options in @@OPTIONS.
    IMPLICIT_TRANSACTIONS = 2
    CURSOR_CLOSE_ON_COMMIT = 4
    ANSI_PADDING = 16
    ANSI_NULLS = 32
    ARITHABORT = 64
    ARITHIGNORE = 128
    QUOTED_IDENTIFIER = 256
    ANSI_NULL_DFLT_ON = 1024
    ANSI_NULL_DFLT_OFF = 2048

    def setUp(self):
        unittest.TestCase.setUp(self)

        # Load external database settings from file.
        self.parser = configparser.ConfigParser(**({'allow_no_value': True} if PY27 else {}))
        config = os.path.join(os.path.dirname(__file__), 'database.ini')
        self.parser.read([config])

    def get_option(self, key, type_=str):
        section = self.__class__.__name__
        for section_ in (section, 'DEFAULT'): # pragma: nocover
            if self.parser.has_option(section_, key):
                return type_(self.parser.get(section_, key))

    @staticmethod
    def parameter_type(cursor, value):
        cursor.execute(
            '''
            SELECT
                CONVERT(VARCHAR, SQL_VARIANT_PROPERTY(:0, 'BaseType')) AS Type,
                CONVERT(INT, SQL_VARIANT_PROPERTY(:0, 'Precision')) AS Precision,
                CONVERT(INT, SQL_VARIANT_PROPERTY(:0, 'Scale')) AS Scale,
                CONVERT(INT, SQL_VARIANT_PROPERTY(:0, 'MaxLength')) AS MaxLength,
                :0 AS Value
            ''',
            (value,)
        )
        return cursor.fetchone()

    def assert_type(self, type_, values):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in values:
                    row = self.parameter_type(cursor, value)
                    self.assertEqual(type_, row.Type)
                    self.assertEqual(value, row.Value)

    @property
    def server_name_and_instance(self):
        server = self.get_option('server')
        instance = self.get_option('instance')
        if instance is not None: # pragma: nocover
            server += '\\' + instance
        return server.upper()

    def connect(self, **kwargs):
        '''Connect to the database using parameters defined in the config.
        '''

        kwargs_ = dict(
            [
                (key, self.get_option(key, type_))
                for key, type_ in (
                    ('autocommit', bool),
                    ('database', str),
                    ('instance', str),
                    ('login_timeout', int),
                    ('password', str),
                    ('tds_version', str),
                    ('timeout', int),
                    ('user', str),
                )
            ]
        )

        kwargs_.update(kwargs)

        return ctds.connect(
            self.get_option('server'),
            appname='egg.tds.unittest',
            **kwargs_
        )

    def sql_server_version(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute('SELECT CONVERT(VARCHAR(20), SERVERPROPERTY(\'ProductVersion\'))')
                version = cursor.fetchone()[0]

        return tuple(int(part) for part in version.split('.'))

    @property
    def freetds_version(self):
        matches = re.match(r'freetds v(\d+)\.(\d+)\.(\d+)', ctds.freetds_version)
        if matches:
            return tuple(int(g) for g in matches.groups())

    @property
    def use_sp_executesql(self):
        return self.freetds_version >= (0, 92, 405)

    # Older versions of FreeTDS improperly round the money to the nearest hundredth.
    def round_money(self, money):
        if self.freetds_version > (0, 92, 405):
            return money
        else:
            return money.quantize(Decimal('.01'))
