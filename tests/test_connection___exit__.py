import ctds

from .base import TestExternalDatabase

class TestConnectionExit(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Connection.__exit__.__doc__,
            '''\
__exit__(exc_type, exc_val, exc_tb)

Exit the connection's runtime context, closing the connection.
If no error occurred, any pending transaction will be committed
prior to closing the connection. If an error occurred, the transaction
will be implicitly rolled back when the connection is closed.

:param type exc_type: The exception type, if an exception
    is raised in the context, otherwise :py:data:`None`.
:param Exception exc_val: The exception value, if an exception
    is raised in the context, otherwise :py:data:`None`.
:param object exc_tb: The exception traceback, if an exception
    is raised in the context, otherwise :py:data:`None`.

:returns: :py:data:`None`
'''
        )

    def test_typeerror(self):
        connection = self.connect()
        self.assertRaises(TypeError, connection.__exit__, 1, 2)
        connection.close()

    def test_commit(self):
        name = self.test_commit.__name__
        try:
            # Create a table and add values. On exit from the context, it should
            # be committed.
            with self.connect(autocommit=False) as connection:
                with connection.cursor() as cursor:
                    cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                    cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))

            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(name))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(1,), (2,), (3,)]
                    )

        finally:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                        DROP TABLE dbo.{0};
                        '''.format(name)
                    )

    def test_rollback_autocommit(self):
        name = self.test_rollback_autocommit.__name__
        try:
            try:
                with self.connect(autocommit=True) as connection:
                    with connection.cursor() as cursor:
                        cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                        cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))
                    raise RuntimeError('error')

            except RuntimeError:
                pass
            else:
                self.fail('RuntimeError was not raised as expected') # pragma: nocover

            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute('SELECT * FROM {0}'.format(name))
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(1,), (2,), (3,)]
                    )

        finally:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                        DROP TABLE dbo.{0};
                        '''.format(name)
                    )

    def test_rollback_python_error(self):
        name = self.test_rollback_python_error.__name__
        try:
            # Create a table and add values. On exit from the context, it should
            # be rolled back.
            try:
                with self.connect(autocommit=False) as connection:
                    with connection.cursor() as cursor:
                        cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                        cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))
                    # Raise an error, causing the transaction to rollback.
                    raise RuntimeError('something bad')
            except RuntimeError:
                pass
            else:
                self.fail('RuntimeError was not raised as expected') # pragma: nocover

            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        SELECT OBJECT_ID('dbo.{0}', 'U')
                        '''.format(name)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(None,)]
                    )

        finally:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                        DROP TABLE dbo.{0};
                        '''.format(name)
                    )

    def test_rollback_timeout(self):
        name = self.test_rollback_timeout.__name__
        try:
            # Create a table and add values. On exit from the context, it should
            # be rolled back.
            try:
                with self.connect(autocommit=False, timeout=1) as connection:
                    with connection.cursor() as cursor:
                        cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                        cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))
                        cursor.execute("WAITFOR DELAY '00:00:02';SELECT @@VERSION")
            except ctds.DatabaseError as ex:
                self.assertEqual(str(ex), 'Adaptive Server connection timed out')
            else:
                self.fail('ctds.DatabaseError was not raised as expected') # pragma: nocover

            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        SELECT OBJECT_ID('dbo.{0}', 'U')
                        '''.format(name)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(None,)]
                    )

        finally:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                        DROP TABLE dbo.{0};
                        '''.format(name)
                    )

    def test_rollback_syntax_error(self):
        name = self.test_rollback_syntax_error.__name__
        try:
            # Create a table and add values. On exit from the context, it should
            # be rolled back.
            try:
                with self.connect(autocommit=False) as connection:
                    with connection.cursor() as cursor:
                        cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                        cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))
                        cursor.execute('unknown')
            except ctds.ProgrammingError as ex:
                self.assertEqual(str(ex), "Could not find stored procedure 'unknown'.")
            else:
                self.fail('ctds.ProgrammingError was not raised as expected') # pragma: nocover

            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        SELECT OBJECT_ID('dbo.{0}', 'U')
                        '''.format(name)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(None,)]
                    )

        finally:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                        DROP TABLE dbo.{0};
                        '''.format(name)
                    )

    def test_rollback_database_error(self):
        name = self.test_rollback_database_error.__name__
        try:
            # Create a table and add values. On exit from the context, it should
            # be rolled back.
            try:
                with self.connect(autocommit=False) as connection:
                    with connection.cursor() as cursor:
                        cursor.execute('CREATE TABLE {0} (ints INT)'.format(name))
                        cursor.execute('INSERT INTO {0} VALUES (1),(2),(3)'.format(name))
                        cursor.execute("RAISERROR (N'Some Error', 16, -1)")
            except ctds.ProgrammingError as ex:
                self.assertEqual(str(ex), 'Some Error')
            else:
                self.fail('ctds.ProgrammingError was not raised as expected') # pragma: nocover

            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        SELECT OBJECT_ID('dbo.{0}', 'U')
                        '''.format(name)
                    )
                    self.assertEqual(
                        [tuple(row) for row in cursor.fetchall()],
                        [(None,)]
                    )

        finally:
            with self.connect() as connection:
                with connection.cursor() as cursor:
                    cursor.execute(
                        '''
                        IF OBJECT_ID('dbo.{0}', 'U') IS NOT NULL
                        DROP TABLE dbo.{0};
                        '''.format(name)
                    )
