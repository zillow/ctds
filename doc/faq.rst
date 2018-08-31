Frequently Asked Questions
==========================

Why can't I pass an empty string to :py:meth:`ctds.Cursor.callproc`?
--------------------------------------------------------------------

The definition of the `dblib` API implemented by `FreeTDS` does
not define a way to specify a **(N)VARCHAR** with length *0*. This
is a known deficiency of the `dblib` API. String parameters with
length *0* are interpreted as `NULL` by the `dblib` API.


Why don't :py:class:`str` values map to **(N)VARCHAR** in Python 2?
-------------------------------------------------------------------

In Python 2 :py:class:`bytes` and :py:class:`str` are equivalent. As such, it
is difficult for `cTDS` to infer a proper *SQL* type from the Python type.
:py:func:`unicode` should be used in Python 2 when a **(N)CHAR**-type
parameter is desired. Alternatively, :py:class:`ctds.SqlVarChar` and
:py:class:`ctds.SqlNVarChar` can be used to explicitly define the desired
*SQL* type.


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

The error `some custom error` is reported as a :py:obj:`ctds.Warning`.

In `cTDS` v1.3.0 and later, this warning can be turned into an exception using
the :py:mod:`warnings` module.

.. code-block:: python

    import warnings
    import ctds

    warnings.simplefilter('error', ctds.Warning)

    with ctds.connect() as connection:
        with connection.cursor() as cursor:
            # The following will raise a `ctds.Warning` exception.
            cursor.execute(
                "RAISERROR (N'this will become a python exception', 16, -1);"
            )


What does the `Unicode codepoint U+1F4A9 is not representable...` warning mean?
-------------------------------------------------------------------------------

Until `FreeTDS`_ **1.00**, the default encoding used on the connection to
the database was *UCS-2*. FreeTDS requires all text data be encodable in the
connection's encoding. Therefore `cTDS` would replace non *UCS-2* characters in
strings and generate a warning before sending the data to the database. Once
support was added for configuring the connection to use *UTF-16* in `FreeTDS`_
**1.00**, this behavior was no longer necessary.

Upgrading the version of `FreeTDS`_ will resolve this warning and unicode
codepoints outside the *UCS-2* range will no longer be replaced.

.. note::

   `FreeTDS`_ **0.95** does support using *UTF-16* on connections, however
   the only way to configure it is via *freetds.conf*. The option is disabled
   by default, and there is no way to determine if *UTF-16* is enabled for a
   connection. Because of these limitations, `cTDS` cannot reliably determine
   if the connection will support *UTF-16* and assumes it does not.


.. _FreeTDS: http://www.freetds.org
