import warnings

import ctds

from .base import TestExternalDatabase
from .compat import unicode_

class TestConnectionMessages(TestExternalDatabase):
    '''Unit tests related to the Connection.messages attribute.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.messages.__doc__,
            '''\
A list of any informational messages received from the last
:py:meth:`ctds.Cursor.execute`, :py:meth:`ctds.Cursor.executemany`, or
:py:meth:`ctds.Cursor.callproc` call.
For example, this will include messages produced by the T-SQL `PRINT` and
`RAISERROR` statements. Messages are preserved until the next call to any
of the above methods.

.. versionadded:: 1.4

:rtype: list
:return: None if the connection is closed.
'''
        )

    def test_execute(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                with warnings.catch_warnings(record=True):
                    cursor.execute(
                        '''
                        PRINT(NCHAR(191) + N' carpe diem!');
                        EXEC sp_executesql N'RAISERROR (''message from RAISERROR'', 0, 99) WITH NOWAIT';
                        PRINT(N'Hello World!');
                        RAISERROR ('more severe message from RAISERROR', 3, 16) WITH NOWAIT;
                        '''
                    )

            expected = [
                {
                    'description': unicode_(b'more severe message from RAISERROR', encoding='utf-8'),
                    'line': 5,
                    'number': 50000,
                    'proc': '',
                    'severity': 3,
                    'state': 16,
                },
                {
                    'description': unicode_(b'\xc2\xbf carpe diem!', encoding='utf-8'),
                    'line': 2,
                    'number': 0,
                    'proc': '',
                    'severity': 0,
                    'state': 1,
                },
                {
                    'description': unicode_(b'message from RAISERROR', encoding='utf-8'),
                    'line': 3,
                    'number': 50000,
                    'proc': '',
                    'severity': 0,
                    'state': 99,
                },
                {
                    'description': unicode_(b'Hello World!', encoding='utf-8'),
                    'line': 4,
                    'number': 0,
                    'proc': '',
                    'severity': 0,
                    'state': 1,
                },
            ]
            for index, message in enumerate(connection.messages):
                self.assertTrue(self.server_name_and_instance in message.pop('server'))
                self.assertEqual(expected[index], message)

            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    PRINT(N'Some other message')
                    PRINT(N'and yet another one')
                    '''
                )
                self.assertEqual(
                    [
                        unicode_('Some other message'),
                        unicode_('and yet another one')
                    ],
                    [
                        message.pop('description')
                        for message in connection.messages
                    ]
                )

        self.assertIsNone(connection.messages, None)

    def test_write(self):
        with self.connect() as connection:
            try:
                connection.messages = 9
            except AttributeError:
                pass
            else:
                self.fail('.messages did not fail as expected') # pragma: nocover

        try:
            connection.messages = None
        except AttributeError:
            pass
        else:
            self.fail('.messages did not fail as expected') # pragma: nocover
