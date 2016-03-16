'''
*cTDS* is a pure C implementation of the `Python DB API 2.0 <https://www.python.org/dev/peps/pep-0249>`_
specification for Microsoft SQL Server.
'''
import logging

# pylint: disable=no-name-in-module,redefined-builtin
from _tds import (
    apilevel,
    connect,
    paramstyle,
    threadsafety,

    Date,
    Time,
    Timestamp,
    DateFromTicks,
    TimeFromTicks,
    TimestampFromTicks,
    Binary,

    # DB API 2.0 Exceptions
    Warning,
    Error,
    InterfaceError,
    DatabaseError,
    DataError,
    OperationalError,
    IntegrityError,
    InternalError,
    ProgrammingError,
    NotSupportedError,

    Parameter,

    # Types
    TDSCHAR as CHAR,
    TDSVARCHAR as VARCHAR,
    TDSTEXT as TEXT,
    TDSBIT as BIT,
    TDSTINYINT as TINYINT,
    TDSSMALLINT as SMALLINT,
    TDSINT as INT,
    TDSBIGINT as BIGINT,
    TDSFLOAT as FLOAT,
    TDSREAL as REAL,
    TDSDATETIME as DATETIME,
    TDSSMALLDATETIME as SMALLDATETIME,
    TDSDATE as DATE,
    TDSTIME as TIME,
    TDSDATETIME2 as DATETIME2,
    TDSIMAGE as IMAGE,
    TDSSMALLMONEY as SMALLMONEY,
    TDSMONEY as MONEY,
    TDSMONEYN as MONEYN,
    TDSNUMERIC as NUMERIC,
    TDSDECIMAL as DECIMAL,
    TDSBINARY as BINARY,
    TDSVARBINARY as VARBINARY,
    TDSGUID as GUID,
    TDSVOID as VOID,

    freetds_version,
    version_info,

    Connection,
    Cursor,

    SqlBigInt,
    SqlBinary,
    SqlChar,
    SqlDate,
    SqlDecimal,
    SqlInt,
    SqlSmallInt,
    SqlTinyInt,
    SqlVarBinary,
    SqlVarChar,
)


class NullHandler(logging.Handler):
    def emit(self, record):
        pass # pragma: nocover

    def handle(self, record):
        pass # pragma: nocover

    def createLock(self):
        self.lock = None


# Configure a NullHandler for library logging.
# See https://docs.python.org/2/howto/logging.html#library-config.
logging.getLogger(__name__).addHandler(NullHandler())

del NullHandler


# Define the library version, in major.minor.patch format.
__version__ = '.'.join(str(item) for item in version_info) # pylint: disable=invalid-name
