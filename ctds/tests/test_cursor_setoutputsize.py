import ctds

from .base import TestExternalDatabase

class TestCursorSetOutputSize(TestExternalDatabase):
    '''Unit tests related to the Cursor.setoutputsize() method.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.setoutputsize.__doc__,
            '''\
setoutputsize()

This method has no affect.

:pep:`0249#setoutputsize`
'''
        )

    def test_setoutputsize(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertEqual(cursor.setoutputsize(100, 1), None)
                self.assertEqual(cursor.setoutputsize(100, None), None)
                self.assertEqual(cursor.setoutputsize(100), None)
