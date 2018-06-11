import ctds

from .base import TestExternalDatabase
from .compat import unicode_

class TestConnectionTDSVersion(TestExternalDatabase):
    '''Unit tests related to the Connection.tds_version attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.tds_version.__doc__,
            '''\
The TDS version in use for the connection or :py:data:`None` if the
connection is closed.

:rtype: str
'''
        )

    def test_read(self):
        supports73 = False
        try:
            with self.connect(tds_version='7.3'):
                supports73 = True
        except ctds.InterfaceError: # pragma: nocover
            pass

        versions = [
            ('7.0', '7.0'),
            ('7.1', '7.1'),

            # FreeTDS versions without 7.3 support always downgrade 7.2 to 7.1.
            ('7.2', '7.2' if supports73 else '7.1'),
        ]
        if supports73: # pragma: nobranch
            versions.append(('7.3', '7.3'))

        for tds_version, expected in versions:
            with self.connect(tds_version=tds_version) as connection:
                self.assertTrue(isinstance(connection.tds_version, unicode_))
                self.assertEqual(connection.tds_version, expected)

        self.assertEqual(connection.tds_version, None)

    def test_write(self):
        with self.connect() as connection:
            try:
                connection.tds_version = 9
            except AttributeError:
                pass
            else:
                self.fail('.tds_version did not fail as expected') # pragma: nocover

        try:
            connection.tds_version = None
        except AttributeError:
            pass
        else:
            self.fail('.tds_version did not fail as expected') # pragma: nocover
