import warnings

import ctds

from .base import TestExternalDatabase
from .compat import long_, unicode_

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
For example, this will include messages produced by the T-SQL `PRINT`
and `RAISERROR` statements. Messages are preserved until the next call
to any of the above methods. :py:data:`None` is returned if the
connection is closed.

:pep:`0249#connection-messages`

.. versionadded:: 1.4

:rtype: list(dict)
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
                    'line': long_(5),
                    'number': long_(50000),
                    'proc': unicode_(''),
                    'severity': long_(3),
                    'state': long_(16),
                },
                {
                    'description': unicode_(b'\xc2\xbf carpe diem!', encoding='utf-8'),
                    'line': long_(2),
                    'number': long_(0),
                    'proc': unicode_(''),
                    'severity': long_(0),
                    'state': long_(1),
                },
                {
                    'description': unicode_(b'message from RAISERROR', encoding='utf-8'),
                    'line': long_(1),
                    'number': long_(50000),
                    'proc': unicode_(''),
                    'severity': long_(0),
                    'state': long_(99),
                },
                {
                    'description': unicode_(b'Hello World!', encoding='utf-8'),
                    'line': long_(4),
                    'number': long_(0),
                    'proc': unicode_(''),
                    'severity': long_(0),
                    'state': long_(1),
                },
            ]
            with warnings.catch_warnings(record=True) as warns:
                messages = [msg.copy() for msg in connection.messages]

            self.assertEqual(len(warns), 1)
            self.assertEqual(
                [str(warn.message) for warn in warns],
                ['DB-API extension connection.messages used'] * len(warns)
            )
            self.assertEqual(
                [warn.category for warn in warns],
                [Warning] * len(warns)
            )

            for message in messages:
                self.assertTrue(self.server_name_and_instance in message.pop('server'))
            self.assertEqual(expected, messages)

            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    PRINT(N'Some other message')
                    PRINT(N'and yet another one')
                    '''
                )

            with warnings.catch_warnings(record=True):
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

        with warnings.catch_warnings(record=True):
            self.assertEqual(connection.messages, None)

    def test_warning_as_error(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    PRINT(NCHAR(191) + N' carpe diem!');
                    '''
                )

            with warnings.catch_warnings():
                warnings.simplefilter('error')
                try:
                    getattr(connection, 'messages')
                except Warning as warn:
                    self.assertEqual('DB-API extension connection.messages used', str(warn))
                else:
                    self.fail('.messages did not fail as expected') # pragma: nocover

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
