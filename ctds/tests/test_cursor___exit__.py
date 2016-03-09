import ctds

from .base import TestExternalDatabase

class TestCursorExit(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.__exit__.__doc__,
            '''\
__exit__(exc_type, exc_val, exc_tb)

Exit the cursor's runtime context, closing the cursor.

:param type exc_type: The exeception type, if an exception
    is raised in the context, otherwise `None`.
:param Exception exc_val: The exeception value, if an exception
    is raised in the context, otherwise `None`.
:param object exc_tb: The exeception traceback, if an exception
    is raised in the context, otherwise `None`.

:rtype: None
'''
        )
