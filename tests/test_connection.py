import ctds

from .base import TestExternalDatabase

class TestConnection(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.__doc__,
            '''\
A connection to the database server.

:pep:`0249#connection-objects`
'''
        )

    def test_typeerror(self):
        self.assertRaises(TypeError, ctds.Connection)

    def test_timeout(self):
        with self.connect(timeout=1) as connection:
            with connection.cursor() as cursor:
                try:
                    cursor.execute("WAITFOR DELAY '00:00:02';SELECT @@VERSION")
                except ctds.DatabaseError as ex:
                    msg = 'Adaptive Server connection timed out'
                    self.assertEqual(str(ex), msg)
                    self.assertEqual(ex.severity, 6)
                    self.assertEqual(ex.db_error, {
                        'number': 20003,
                        'description': msg,
                    })
                    self.assertEqual(ex.os_error, None)
                    self.assertEqual(ex.last_message, None)
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover

                # Subsequent operations should fail.
                try:
                    cursor.execute("SELECT 1")
                except ctds.DatabaseError as ex:
                    pass
                else:
                    self.fail('.execute() did not fail as expected') # pragma: nocover
