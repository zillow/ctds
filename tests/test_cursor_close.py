import ctds

from .base import TestExternalDatabase

class TestCursorClose(TestExternalDatabase):
    '''Unit tests related to the Cursor.close() method.
    '''

    def test___doc__(self):
        self.assertEqual(
            ctds.Cursor.close.__doc__,
            '''\
close()

Close the cursor.

:pep:`0249#Cursor.close`
'''
        )


    def test_multiple(self):
        with self.connect() as connection:
            cursor = connection.cursor()
            cursor.close()
            try:
                cursor.close()
            except ctds.InterfaceError as ex:
                self.assertEqual(str(ex), 'cursor closed')
            else:
                self.fail('.close() did not fail as expected') # pragma: nocover
