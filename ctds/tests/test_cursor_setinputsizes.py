import ctds

from .base import TestExternalDatabase

class TestCursorSetInputSizes(TestExternalDatabase):
    '''Unit tests related to the Cursor.setinputsizes() method.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.setinputsizes.__doc__,
            '''\
setinputsizes()

This method has no affect.

:pep:`0249#setinputsizes`
'''
        )

    def test_setinputsizes(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                self.assertEqual(cursor.setinputsizes(), None)
                self.assertEqual(cursor.setinputsizes(1), None)
                self.assertEqual(cursor.setinputsizes('1'), None)
                self.assertEqual(cursor.setinputsizes(object()), None)
                self.assertEqual(cursor.setinputsizes({}), None)
                self.assertEqual(cursor.setinputsizes(()), None)
