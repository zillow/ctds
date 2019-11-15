:mod: `ctds`

ctds
====

.. automodule:: ctds
    :members:
        connect,
        Date,
        Time,
        Timestamp,
        DateFromTicks,
        TimeFromTicks,
        TimestampFromTicks,
        Binary

    .. py:data:: apilevel

        :pep:`0249#apilevel`

    .. py:data:: paramstyle

        :pep:`0249#paramstyle`

    .. py:data:: threadsafety

        :pep:`0249#threadsafety`

    .. py:data:: freetds_version

        The version of `FreeTDS <https://www.freetds.org>`_ in use at runtime.

    .. py:data:: version_info

        The *cTDS* version, as a **(MAJOR, MINOR, PATCH)** tuple.

    .. autoexception:: Error
    .. autoexception:: InterfaceError
    .. autoexception:: DatabaseError
    .. autoexception:: DataError
    .. autoexception:: OperationalError
    .. autoexception:: IntegrityError
    .. autoexception:: InternalError
    .. autoexception:: ProgrammingError
    .. autoexception:: NotSupportedError

    .. autoexception:: Warning
