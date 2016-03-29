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


Batch Size
^^^^^^^^^^

By default, :py:meth:`ctds.Connection.bulk_insert()` will push all data to the
database before it is actually validated against the table's schema. If any of
the data is invalid, the entire `BULK INSERT`_ operation would fail. The
`batch_size` parameter of :py:meth:`ctds.Connection.bulk_insert()` can be used
to control how many rows should be copied before validating them.

.. _BULK INSERT: https://msdn.microsoft.com/en-us/library/ms188365.aspx
