Bulk Insert
===========

`cTDS` supports `BULK INSERT`_ for effeciently inserting large amounts of data
into a table using :py:meth:`ctds.Connection.bulk_insert()`.

Example
^^^^^^^

A bulk insert is done by providing an :ref:`iterator <python:typeiter>` of
rows to insert and the name of the table to insert the rows into. The iterator
should return a sequence containing the values for each column in the table.

.. code-block:: python

    import ctds
    with ctds.connect('host') as connection:
        connection.bulk_insert(
            'MyExampleTable',
            # A generator of the rows.
            (
                # The row values can be any python sequence type
                (i, 'hello world {0}'.format(i))
                for i in range(0, 100)
            )
        )

        # Version 1.9 supports passing dict rows.
        connection.bulk_insert(
            'MyExampleTable',
            # A generator of the rows.
            (
                {
                    'IntColumn': i,
                    'TextColumn': 'hello world {0}'.format(i)
                }
                for i in range(0, 100)
            )
        )


Inserting from a CSV File
^^^^^^^^^^^^^^^^^^^^^^^^^

This example illustrates how to import data from a *CSV* file.

.. code-block:: python

    import ctds
    import csv

    with open('BulkInsertExample.csv', 'rb') as csvfile:
        csvreader = csv.reader(csvfile, delimiter=',')
        with ctds.connect('host') as connection:
            connection.bulk_insert(
                'BulkInsertExample',
                iter(csvreader)
            )

    # ctds 1.9 supports passing rows as dict objects, mapping column name
    # to value. This is useful if the table contains NULLable columns
    # not present in the CSV file. 
    with open('BulkInsertExample.csv', 'rb') as csvfile:
        csvreader = csv.DictReader(csvfile, delimiter=',')
        with ctds.connect('host') as connection:
            connection.bulk_insert(
                'BulkInsertExample',
                iter(csvreader)
            )



Batch Size
^^^^^^^^^^

By default, :py:meth:`ctds.Connection.bulk_insert()` will push all data to the
database before it is actually validated against the table's schema. If any of
the data is invalid, the entire `BULK INSERT`_ operation would fail. The
`batch_size` parameter of :py:meth:`ctds.Connection.bulk_insert()` can be used
to control how many rows should be copied before validating them.


Text Columns
^^^^^^^^^^^^

Data specified for bulk insertion into text columns (e.g. **VARCHAR**,
**NVARCHAR**, **TEXT**) is not encoded on the client in any way by FreeTDS.
Because of this behavior it is possible to insert textual data with an invalid
encoding and cause the column data to become corrupted.

To prevent this, it is recommended the caller is explicitly wrap the the object
with either :py:class:`ctds.SqlVarChar` (for **CHAR**, **VARCHAR** or **TEXT**
columns) or :py:class:`ctds.SqlNVarChar` (for **NCHAR**, **NVARCHAR** or
**NTEXT** columns). For non-Unicode columns, the value should be first encoded
to column's encoding (e.g. `latin-1`). By default :py:class:`ctds.SqlVarChar`
will encode :py:class:`str` objects to `utf-8`, which is likely incorrect for
most SQL Server configurations.

.. code-block:: python

    import ctds
    with ctds.connect('host') as connection:
        connection.bulk_insert(
            #
            # Assumes a table with the following schema:
            #
            # CREATE TABLE MyExampleTableWithVarChar (
            #     Latin1Column VARCHAR(100) COLLATE
            #         SQL_Latin1_General_CP1_CI_AS,
            #     UnicodeColumn NVARCHAR(100)
            # )
            #

            'MyExampleTableWithVarChar',
            [
                (
                    # Note the value passed to SqlVarChar is first encoded to
                    # match the server's encoding.
                    ctds.SqlVarChar(
                        b'a string with latin-1 -> \xc2\xbd'.decode(
                            'utf-8'
                        ).encode('latin-1')
                    ),
                    ctds.SqlVarChar(
                        b'a string with Unicode -> \xe3\x83\x9b'.decode(
                            'utf-8'
                        ).encode('utf-16le')
                    ),
                )
            ]
        )


.. _BULK INSERT: https://msdn.microsoft.com/en-us/library/ms188365.aspx
