from .base import TestExternalDatabase

class TestCursorRowList(TestExternalDatabase):

    def test_rowlist(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                    DECLARE @{0} TABLE(i INT);
                        INSERT INTO @{0}(i) VALUES (1),(2),(3),(4),(5),(6);
                    SELECT * FROM @{0};
                    '''.format(self.test_rowlist.__name__)
                )
                rows = cursor.fetchall()

        # The rowlist should be accessible after closing the cursor
        # and connection.
        self.assertEqual(len(rows), 6)
        for index, row in enumerate(rows):
            # The row object should always be the same instance.
            self.assertEqual(id(row), id(rows[index]))
