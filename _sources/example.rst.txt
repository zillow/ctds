Basic Example
=============

All interactions with *cTDS* first require a connection. Connections
are created using the :func:`ctds.connect` method. All
:py:class:`ctds.Connection` objects can be used as context managers
in a `with <https://www.python.org/dev/peps/pep-0343/>`_ statement.

In order to actually perform queries or called stored procedures,
a :py:class:`ctds.Cursor` is required. This can be acquired using the
:py:func:`ctds.Connection.cursor` method. Like the :py:class:`ctds.Connection`
object, the :py:class:`ctds.Cursor` object can also be used as a context
manager.

.. note::

    Due to the design of the *TDS* protocol, :py:class:`ctds.Cursor` objects
    cannot have multiple query results active at once. Only the results from
    the last query are available from the :py:class:`ctds.Cursor`.


An example of a simple query follows:

.. code-block:: python

    import ctds

    connection = ctds.connect(
        'my-database-host',
        user='username',
        password='password'
    )
    with connection:
        with connection.cursor() as cursor:
            cursor.execute('SELECT @@VERSION AS Version')
            row = cursor.fetchone()

    # Columns can be accessed by index, name, or attribute.
    assert row[0] == row['Version']
    print(row.Version)
