Connection Pooling
==================

Many applications will desire some form of connection pooling for improved
performance. *cTDS* does not provide connection pooling itself, but can be
used in a 3rd party implementation, such as `antipool <http://furius.ca/antiorm/>`_.

Whatever connection pooling solution is used, it is important to remember that
`:py:class:ctds.Connection` and `:py:class:ctds.Cursor` objects must **not** be
shared across threads.


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

    config = {
        'server': 'my-host',
        'database': 'MyDefaultDatabase',
        'user': 'my-username',
        'password": 'my-password',
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
