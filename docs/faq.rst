Frequently Asked Questions
==========================

Why can't I pass an empty string to `callproc`?
-----------------------------------------------

The definition of the `dblib` API implemented by `FreeTDS` does
not define a way to specify a **VARCHAR** with length *0*. This
is a known deficiency of the `dblib` API. String parameters with
length *0* are interpreted as `NULL` by the `dblib` API.


Why don't :py:class:`str` values map to **VARCHAR** in Python 2?
----------------------------------------------------------------

In Python 2 :py:class:`bytes` and :py:class:`str` are equivalent. As such it is
difficult for `ctds` to infer a proper *SQL* type from the Python type.
:py:class:`unicode` should be used in Python 2 when a **CHAR**-type parameter
is desired. Alternatively, :py:class:`ctds.SqlVarChar` can be used to
explicitly define the desired *SQL* type.


Why doesn't `RAISERROR` raise a Python exception?
-------------------------------------------------

A Python exception is raised only if the last SQL operation resulted in an
error. For example, the following will not raise a
:py:class:`ctds.ProgrammingError` exception because the last statement does not
result in an error.

.. code-block:: SQL

    RAISERROR (N'some custom error', 16, -1);

    /* This statement does not fail, hence a Python exception is not raised. */
    SELECT 1 AS Column1;

The error `some custom error` is reported as a Python :py:class:`Warning`.
