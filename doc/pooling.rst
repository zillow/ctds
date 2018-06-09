Connection Pooling
==================

Many applications will desire some form of connection pooling for improved
performance. As of version 1.2, *cTDS* does provide a simple connection pool
implementation: :py:class:`ctds.pool.ConnectionPool`. It can also be used with
3rd party implementation, such as `antipool <http://furius.ca/antiorm/>`_.

.. note::

    Whatever connection pooling solution is used, it is important to
    remember that :py:class:`ctds.Connection` and :py:class:`ctds.Cursor`
    objects must **not** be shared across threads.


ctds.pool Example
-----------------

.. code-block:: python

    import ctds
    import pprint

    config = {
        'server': 'my-host',
        'database': 'MyDefaultDatabase',
        'user': 'my-username',
        'password': 'my-password',
        'appname': 'ctds-doc-pooling-example',
        'timeout': 5,
        'login_timeout': 5,
        'autocommit': True
    }

    pool = ctds.pool.ConnectionPool(
        ctds,
        config
    )

    with pool.connection() as connection:
        with connection.cursor() as cursor:
            try:
                cursor.execute('SELECT @@VERSION;')
                rows = cursor.fetchall()
                print([c.name for c in cursor.description])
                pprint.pprint([tuple(row) for row in rows])
            except ctds.Error as ex:
                print(ex)

    # Explicitly cleanup the connection pool.
    pool.finalize()


antipool Example
----------------

Using `antipool <http://furius.ca/antiorm/>`_ is fairly straightforward.

.. warning::

    Never set the`disable_rollback` option. Allowing the connection to be
    rolled back on release is the only way to to discard broken/invalid
    connections.


.. code-block:: python

    import antipool
    import ctds
    import pprint

    config = {
        'server': 'my-host',
        'database': 'MyDefaultDatabase',
        'user': 'my-username',
        'password': 'my-password',
        'appname': 'ctds-doc-pooling-example',
        'timeout': 5,
        'login_timeout': 5,
        'autocommit': True
    }

    pool = antipool.ConnectionPool(
        ctds,
        options={
            # Don't have the need for read-only connections.
            'disable_ro': True,
            # Never disable rollback
            'disable_rollback': False
        },
        **config
    )

    connection = pool.connection()
    try:
        with connection.cursor() as cursor:
            try:
                cursor.execute('SELECT @@VERSION;')
                rows = cursor.fetchall()
                print([c.name for c in cursor.description])
                pprint.pprint([tuple(row) for row in rows])
            except ctds.Error as ex:
                print(ex)
    finally:
        connection.release()

    # Explicitly cleanup the connection pool.
    pool.finalize()
    
Microsoft Azure Connections
--------------------------

With microsofot azure you need to make sure the TDS Version 7.3 is being used. Failure to do so will result in connection failure with an error message similar to `Adaptive connection failed.`

To set the TDS version to 7.3, add the `tds_version` paramter to the `config` object.

.. code-block:: python

    import antipool
    import ctds
    import pprint

    config = {
        ...
        'tds_version': '7.3'
    }
