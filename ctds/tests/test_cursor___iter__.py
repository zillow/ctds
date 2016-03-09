import warnings

from .base import TestExternalDatabase

class TestCursorNext(TestExternalDatabase):
    '''Unit tests related to the Cursor.__iter__() member.
    '''

    def test_next(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                cursor.execute(
                    '''
                        DECLARE @test_fetchone TABLE(i INT);
                        INSERT INTO @test_fetchone(i) VALUES (1),(2),(3);
                        SELECT * FROM @test_fetchone;
                        SELECT i * 2 FROM @test_fetchone;
                    '''
                )
                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual([tuple(row) for row in cursor], [(1,), (2,), (3,)])
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.__iter__() used'] * len(warns)
                    )

                self.assertEqual(cursor.nextset(), True)

                with warnings.catch_warnings(record=True) as warns:
                    self.assertEqual([tuple(row) for row in cursor], [(2,), (4,), (6,)])
                    self.assertEqual(len(warns), 1)
                    self.assertEqual(
                        [str(warn.message) for warn in warns],
                        ['DB-API extension cursor.__iter__() used'] * len(warns)
                    )

                self.assertEqual(cursor.nextset(), None)
