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
A list of any informational messages received from the last executed SQL command.
For example, this will include messages produced by the T-SQL `PRINT` statement.

.. versionadded:: 1.4

:rtype: list
:return: None if the connection is closed.
'''
        )

    def test_read(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    PRINT(NCHAR(191) + N' carpe diem!');
                    PRINT(N'Hello World!');
                    '''
                )

            expected = [
                unicode_(b'\xc2\xbf carpe diem!', encoding='utf-8'),
                unicode_('Hello World!'),
            ]
            for index, message in enumerate(connection.messages):
                self.assertTrue(self.server_name_and_instance in message.pop('server'))
                self.assertEqual(expected.pop(0), message.pop('description'))
                self.assertEqual(
                    message,
                    {
                        'line': 2 + index,
                        'number': 0,
                        'proc': '',
                        'severity': 0,
                        'state': 1,
                    }
                )
            with connection.cursor() as cursor:
                cursor.execute('''PRINT(N'Some other message')''')
                self.assertEqual(
                    [
                        unicode_('Some other message')
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
                connection.last_message = 9
            except AttributeError:
                pass
            else:
                self.fail('.last_message did not fail as expected') # pragma: nocover

        try:
            connection.last_message = None
        except AttributeError:
            pass
        else:
            self.fail('.last_message did not fail as expected') # pragma: nocover
