from decimal import Decimal
import os
import platform
import re
import socket
import sys
import unittest

import ctds

from .compat import configparser, PY27, unichr_, unicode_


class TestExternalDatabase(unittest.TestCase):

    maxDiff = None

    UCS4_SUPPORTED = sys.maxunicode > 0xFFFF

    UNICODE_REPLACEMENT = unichr_(65533)

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
        for section_ in (section, 'DEFAULT'): # pragma: no branch
            if self.parser.has_option(section_, key):
                return type_(self.parser.get(section_, key))
        return None # pragma: no cover

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
        # If running against localhost and the docker HOSTNAME var is set, use that.
        if server.lower() == 'localhost':
            if 'HOSTNAME' in os.environ: # pragma: nobranch
                server = os.environ['HOSTNAME'] # pragma: nocover
            elif platform.system() == 'Windows':
                server = socket.gethostname()
        instance = self.get_option('instance')
        if instance is not None: # pragma: nocover
            server += '\\' + instance
        return server

    def connect(self, **kwargs):
        '''Connect to the database using parameters defined in the config.
        '''

        kwargs_ = dict( # pylint: disable=consider-using-dict-comprehension
            [
                (key, self.get_option(key, type_))
                for key, type_ in (
                    ('appname', str),
                    ('autocommit', bool),
                    ('database', str),
                    ('instance', str),
                    ('login_timeout', int),
                    ('password', str),
                    ('port', int),
                    ('tds_version', str),
                    ('timeout', int),
                    ('user', str),
                )
            ]
        )

        kwargs_.update(kwargs)
        kwargs_.setdefault('appname', 'egg.tds.unittest')

        return ctds.connect(
            self.get_option('server'),
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
        matches = re.match(
            r'^freetds v(?P<major>[\d]+)\.(?P<minor>[\d]+)(?:\.(?P<patch>[\d]+))?$',
            ctds.freetds_version
        )
        assert matches is not None
        return (
            int(matches.group('major')),
            int(matches.group('minor') or 0),
            int(matches.group('patch') or 0),
        )

    @property
    def use_sp_executesql(self):
        return self.freetds_version >= (0, 95, 0)

    @property
    def nchars_supported(self):
        return self.use_sp_executesql

    @property
    def read_only_intent_supported(self):
        return self.freetds_version >= (1, 0, 74)

    @property
    def ntlmv2_supported(self):
        return self.freetds_version >= (1, 0, 0)

    @property
    def use_utf16(self):
        return self.freetds_version >= (1, 0, 0)

    @property
    def have_valid_rowcount(self):
        # FreeTDS 1.1+ properly returns rowcount, even when calling sp_executesql.
        return self.freetds_version >= (1, 1, 0) or not self.use_sp_executesql

    @property
    def tdstime_supported(self):
        return self.freetds_version >= (0, 95, 0)

    @property
    def tdsdatetime2_supported(self):
        return self.freetds_version >= (0, 95, 0)

    @property
    def bcp_empty_string_supported(self):
        return self.freetds_version >= (0, 95, 0)

    # Older versions of FreeTDS improperly round the money to the nearest hundredth.
    def round_money(self, money):
        if self.freetds_version > (0, 92, 405): # pragma: nobranch
            return money
        return money.quantize(Decimal('.01')) # pragma: nocover

    @staticmethod
    def nvarchar_width(value):
        assert isinstance(value, unicode_)
        return sum(1 if ord(ch) < 0xFFFF else 2 for ch in value)
