Calling Stored Procedures
=========================

*cTDS* implements the :py:meth:`~ctds.Cursor.callproc` method for calling stored
procedures, as defined by :pep:`0249#callproc`.
Stored procedure parameters may be passed as either a `tuple` or `dict`.

Passing `tuple` parameters
--------------------------

Arguments passed to :py:meth:`~ctds.Cursor.callproc` in a `tuple` will be passed
to the stored procedure in the tuple order. The returned results will be a
`tuple` object, with output parameters replaced.

.. code-block:: python

    outputs = cursor.callproc('MyStoredProcedure', (1, 3, 2, 4))

    # ... is roughly equivalent to the SQL statement:
    # EXEC MyStoredProcedure 1, 3, 2, 4

    assert isinstance(outputs, tuple)


.. warning::

    Currently `FreeTDS` does not support passing empty string parameters. Empty strings
    are converted to `NULL` values internally before being transmitted to the database.


Passing `dict` parameters
-------------------------

Arguments passed to :py:meth:`~ctds.Cursor.callproc` in a `dict` will be passed
to the stored procedure using the `dict` keys for the argument names and `dict`
values for the argument values. Order does not matter in this usage. The
returned results will be a `dict` object, with output parameters replaced.

.. code-block:: python

    outputs = cursor.callproc(
        'MyStoredProcedure',
        {
            '@arg1': 1,
            '@arg3': 2,
            '@arg4': 4,
            '@arg2': 3,
        }
    )

    # ... is roughly equivalent to the SQL statement:
    # EXEC MyStoredProcedure @arg1=1, @arg3=2, @arg4=4, @arg2=3

    assert isinstance(outputs, dict)


.. note::

    All parameter names **must** begin with the **@** character when using this
    form of :py:meth:`~ctds.Cursor.callproc`.


Output Parameters
-----------------

:pep:`0249#callproc` does not define a way to specify a stored procedure parameter
as an `output` parameter. To accomodate output parameters, the
:py:class:`ctds.Parameter` is provided to wrap a parameter and indicate it as
an output parameter. Output parameter values are available in the result returned
from :py:meth:`~ctds.Cursor.callproc`.

.. code-block:: python

    outputs = cursor.callproc(
        'MyStoredProcedureWithOutputs',
        {
            # This is not necessary for input parameters.
            '@input': ctds.Parameter(1, output=False),

            # Input/Output parameters must be specified as output
            '@inputOutput': ctds.Parameter(2, output=True),

            # The Parameter class is also available on Cursor.
            '@output': cursor.Parameter(4, output=True)
        }
    )

    # Do something with the output parameters.
    print(outputs[1], outputs[2])


By default, the output parameter's type is inferred from the Python value
passed to it when created. This can be explicitly specified using a
:doc:`type wrapper class <types>`. Additionally, the buffer for recieving
the output parameter is allocated based on the size of the value passed to
:py:meth:`ctds.Parameter`. Again using an explicit
:doc:`type wrapper class <types>` is useful for indicating how large the
parameter should be. For example, to specify a large `VARCHAR` output
parameter:

.. code-block:: python

    outputs = cursor.callproc(
        'MyStoredProcedureWithLargeVariableOutput',
        (cursor.Parameter(cursor.SqlVarChar(None, size=4000)),)
    )
