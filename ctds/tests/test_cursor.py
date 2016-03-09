import ctds

from .base import TestExternalDatabase

class TestCursor(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.__doc__,
            '''\
A database cursor used to manage the context of a fetch operation.

:pep:`0249#cursor-objects`
'''
        )

    def test_typeerror(self):
        self.assertRaises(TypeError, ctds.Cursor)
