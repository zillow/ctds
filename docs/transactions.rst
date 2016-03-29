Transactions
============

`cTDS` utilizes the `IMPLICIT_TRANSACTIONS`_ feature of `Microsoft SQL Server`_ for
transaction management. This approach was chosen given the similarities to the
`DB API 2.0 specification <https://www.python.org/dev/peps/pep-0249/>`_ . The
spec implies that transactions are implicitly created, but must be explicitly
committed, using the :py:meth:`ctds.Connection.commit()` method, or rolled back,
using the :py:meth:`ctds.Connection.rollback` method.

`cTDS` supports disabling of the use of `IMPLICIT_TRANSACTIONS`_ if desired. In
this case the :py:meth:`ctds.Connection.commit()` and
:py:meth:`ctds.Connection.rollback()` methods have no affect and transactions
are implicitly commited after each call to :py:meth:`ctds.Cursor.execute()`,
:py:meth:`ctds.Cursor.executemany()` or :py:meth:`ctds.Cursor.callproc()`.


Autocommit
^^^^^^^^^^

The `autocommit` parameter to :py:func:`ctds.connect()` and
:py:attr:`ctds.Connection.autocommit` controls the use of
`IMPLICIT_TRANSACTIONS`_. When :py:obj:`True`, transactions are implicitly
committed after an operation and calls to :py:meth:`ctds.Connection.commit()` are
unnecessary (`IMPLICIT_TRANSACTIONS`_ is set to **OFF**).


.. _IMPLICIT_TRANSACTIONS: http://www.freetds.org
.. _Microsoft SQL Server: http://www.microsoft.com/sqlserver/
