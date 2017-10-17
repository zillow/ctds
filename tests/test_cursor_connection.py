import warnings

import ctds

from .base import TestExternalDatabase

class TestCursorConnection(TestExternalDatabase):
    '''Unit tests related to the Cursor.connection attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.connection.__doc__,
            '''\
A reference to the Connection object on which the cursor was created.

:pep:`0249#id28`

:rtype: ctds.Connection
'''
        )

    def test_closed(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            with warnings.catch_warnings(record=True) as warns:
                try:
                    cursor.connection
                except ctds.InterfaceError:
                    pass
                else:
                    self.fail('.connection did not fail as expected') # pragma: nocover

                self.assertEqual(len(warns), 1)
                self.assertEqual(
                    [str(warn.message) for warn in warns],
                    ['DB-API extension cursor.connection used'] * len(warns)
                )

    def test_connection(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual(connection, cursor.connection)
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.connection used'] * len(warns)
                    )

                    # cursor.connection is read-only
                    try:
                        cursor.connection = None
                    except AttributeError:
                        pass
                    else:
                        self.fail('.connection did not fail as expected') # pragma: nocover

    def test_warning_as_error(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings():
                    warnings.simplefilter('error')
                    try:
                        _ = cursor.connection
                    except Warning as warn:
                        self.assertEqual('DB-API extension cursor.connection used', str(warn))
                    else:
                        self.fail('.connection did not fail as expected') # pragma: nocover
