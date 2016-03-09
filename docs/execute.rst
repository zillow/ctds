Executing SQL Statements
========================

*cTDS* implements both the :py:meth:`~ctds.Cursor.execute` and
:py:meth:`~ctds.Cursor.executemany` methods for executing SQL statements.
Both are implemented using the `sp_executesql` SQL Server stored procedure.
This allows optimizations when running batches using
:py:meth:`~ctds.Cursor.executemany`.

.. code-block:: python

    cursor.execute('SELECT * FROM MyTable')


Passing Parameters
------------------

Parameters may be passed to the :py:meth:`~ctds.Cursor.execute` and
:py:meth:`~ctds.Cursor.executemany` methods using the **numeric** parameter
style as defined in :pep:`0249#paramstyle`.

.. code-block:: python

    cursor.execute(
        'SELECT * FROM MyTable WHERE Id = :0 AND OtherId = :1',
        (1234, 5678)
    )

    cursor.executemany(
        '''
        INSERT (Id, OtherId, Name, Birthday) INTO MyTable
        VALUES (:0, :1, :2, :3)
        ''',
        (
            (1, 2, 'John Doe', datetime.date(2001, 1, 1)),
            (2000, 22, 'Jane Doe', datetime.date(1974, 12, 11)),
        )
    )

Parameter Types
^^^^^^^^^^^^^^^

Parameter SQL types are inferred from the Python object type. If desired,
the SQL type can be explicitly specified using a
:doc:`type wrapper class <types>`. For example, this may be necessary when
passing `None` for a `BINARY` column.

.. code-block:: python

    cursor.execute(
        '''
        INSERT (Id, BinaryValue) INTO MyTable
        VALUES (:0, :1)
        ''',
        (
            (1, cursor.SqlBinary(None)),
        )
    )


Limitations
-----------

Due to the implementation of :py:meth:`~ctds.Cursor.execute` and
:py:meth:`~ctds.Cursor.executemany`, any SQL code which defines parameters
cannot be used with execute parameters. For example, the following is **not**
supported:

.. code-block:: python

    # Parameters passed from python are not supported with SQL '@'
    # parameters.
    cursor.execute(
        '''
        CREATE PROCEDURE Increment
            @value INT OUTPUT
        AS
            SET @value = @value + :0;
        ''',
        (1,)
    )


.. warning::

    Currently `FreeTDS` does not support passing empty string parameters. Empty strings
    are converted to `NULL` values internally before being transmitted to the database.
