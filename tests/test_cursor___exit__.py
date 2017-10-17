import ctds

from .base import TestExternalDatabase

class TestCursorExit(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.__exit__.__doc__,
            '''\
__exit__(exc_type, exc_val, exc_tb)

Exit the cursor's runtime context, closing the cursor.

:param type exc_type: The exception type, if an exception
    is raised in the context, otherwise :py:data:`None`.
:param Exception exc_val: The exception value, if an exception
    is raised in the context, otherwise :py:data:`None`.
:param object exc_tb: The exception traceback, if an exception
    is raised in the context, otherwise :py:data:`None`.

:rtype: None
'''
        )
