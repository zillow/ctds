import ctds

from .base import TestExternalDatabase
from .compat import int_, long_, PY3

class TestCursorArraysize(TestExternalDatabase):
    '''Unit tests related to the Cursor.arraysize property.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.arraysize.__doc__,
            '''\
The number of rows to fetch at a time with :py:meth:`.fetchmany`.

:pep:`0249#arraysize`
'''
        )

    def test_getset(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for arraysize in (
                        int_(3),
                        long_(3),
                ):
                    cursor.arraysize = arraysize
                    self.assertEqual(cursor.arraysize, arraysize)

    def test_invalid(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for arraysize in (
                        None,
                        True,
                        False,
                        '',
                        b'1234'
                ):
                    try:
                        cursor.arraysize = arraysize
                    except TypeError:
                        self.assertEqual(cursor.arraysize, 1)
                    else:
                        self.fail('.arraysize did not fail as expected') # pragma: nocover
                if PY3:
                    try:
                        cursor.arraysize = -1
                    except OverflowError:
                        self.assertEqual(cursor.arraysize, 1)
                    else:
                        self.fail('.arraysize did not fail as expected') # pragma: nocover
                else: # pragma: nocover
                    pass

