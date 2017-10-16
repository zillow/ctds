import ctds

from .base import TestExternalDatabase

class TestCursorEnter(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.__enter__.__doc__,
            '''\
__enter__()

Enter the cursor's runtime context. On exit, the cursor is
closed automatically.

:return: The cursor object.
:rtype: ctds.Cursor
'''
        )
