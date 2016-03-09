#ifndef __TYPE_H__
#define __TYPE_H__

#include <Python.h>

#include <sybdb.h>

#define DECIMAL_MAX_PRECISION 38

enum TdsType {
    TDSUNKNOWN = -1,

    TDSCHAR = SYBCHAR,
#define TDSCHAR TDSCHAR
    TDSVARCHAR = SYBVARCHAR,
#define TDSVARCHAR TDSVARCHAR

    TDSTEXT = SYBTEXT,
#define TDSTEXT TDSTEXT

/*
    MS SQL Unicode types are converted to UTF-8 on the client
    by FreeTDS.

    TDSNVARCHAR = SYBNVARCHAR,
#define TDSNVARCHAR TDSNVARCHAR
    TDSNTEXT = SYBNTEXT,
#define TDSNTEXT TDSNTEXT
*/

    TDSBIT = SYBBIT,
#define TDSBIT TDSBIT
    TDSBITN = SYBBITN,
#define TDSBITN TDSBITN
    TDSINTN = SYBINTN,
#define TDSINTN TDSINTN
    TDSTINYINT = SYBINT1,
#define TDSTINYINT TDSTINYINT
    TDSSMALLINT = SYBINT2,
#define TDSSMALLINT TDSSMALLINT
    TDSINT = SYBINT4,
#define TDSINT TDSINT
    TDSBIGINT = SYBINT8,
#define TDSBIGINT TDSBIGINT

    TDSFLOAT = SYBFLT8,
#define TDSFLOAT TDSFLOAT
    TDSFLOATN = SYBFLTN,
#define TDSFLOATN TDSFLOATN
    TDSREAL = SYBREAL,
#define TDSREAL TDSREAL

    TDSDATETIME = SYBDATETIME,
#define TDSDATETIME TDSDATETIME
    TDSSMALLDATETIME = SYBDATETIME4,
#define TDSSMALLDATETIME TDSSMALLDATETIME
    TDSDATETIMEN = SYBDATETIMN,
#define TDSDATETIMEN TDSDATETIMEN
#ifdef SYBMSDATE
    TDSDATE = SYBMSDATE,
#else
    TDSDATE = 40,
#endif
#define TDSDATE TDSDATE
#ifdef SYBMSTIME
    TDSTIME = SYBMSTIME,
#else
    TDSTIME = 41,
#endif
#define TDSTIME TDSTIME
#ifdef SYBMSDATETIME2
    TDSDATETIME2 = SYBMSDATETIME2,
#else
    TDSDATETIME2 = 42,
#endif
#define TDSDATETIME2 TDSDATETIME2

    TDSIMAGE =  SYBIMAGE,
#define TDSIMAGE TDSIMAGE

    TDSSMALLMONEY = SYBMONEY4,
#define TDSSMALLMONEY TDSSMALLMONEY
    TDSMONEY = SYBMONEY,
#define TDSMONEY TDSMONEY
    TDSMONEYN = SYBMONEYN,
#define TDSMONEYN TDSMONEYN
    TDSNUMERIC = SYBNUMERIC,
#define TDSNUMERIC TDSNUMERIC
    TDSDECIMAL = SYBDECIMAL,
#define TDSDECIMAL TDSDECIMAL

    TDSBINARY = SYBBINARY,
#define TDSBINARY TDSBINARY
    TDSVARBINARY = SYBVARBINARY,
#define TDSVARBINARY TDSVARBINARY

    TDSGUID = 36,
#define TDSGUID TDSGUID

    TDSXML = 241,
#define TDSXML TDSXML

    TDSVOID = SYBVOID,
#define TDSVOID TDSVOID
};

/*
    A Python wrapper around a Python object, which also includes some metadata
    to better serialize the Python object to a SQL type/value.

    This struct is meant to be a base class for implementing different SQL
    type wrappers.
*/
struct SqlType
{
    PyObject_HEAD

    /* The wrapped Python object. */
    PyObject* value;

    /* The precise TDS type to serialize `value` to. */
    enum TdsType tdstype;

    /*
        The SQL type size. For fixed-length types, `size` == -1.
        For variable-length types, `size` >= `ndata`.
    */
    int size;

    /*
        A reference to the raw bytes to send. This may point to data owned
        by `value` or to another memory location in part of this struct.
        For NULL values, this is NULL.
    */
    const void* data;

    /*
        The length of `data`, in bytes.
        For NULL values, this is 0.
    */
    size_t ndata;
};

int SqlTypes_init(void);

int SqlType_Check(PyObject* o);

#define DECLARE_SQL_TYPE(_type) \
    PyTypeObject* Sql ## _type ## Type_init(void); \
    PyObject* Sql ## _type ## _create(PyObject* self, PyObject* args, PyObject* kwargs);

DECLARE_SQL_TYPE(TinyInt);
DECLARE_SQL_TYPE(SmallInt);
DECLARE_SQL_TYPE(Int);
DECLARE_SQL_TYPE(BigInt);

DECLARE_SQL_TYPE(Binary);
DECLARE_SQL_TYPE(VarBinary);

DECLARE_SQL_TYPE(Char);
DECLARE_SQL_TYPE(VarChar);

DECLARE_SQL_TYPE(Date);

DECLARE_SQL_TYPE(Decimal);

typedef PyObject* (*sql_topython)(enum TdsType tdstype, const void* data, size_t ndata);

sql_topython sql_topython_lookup(enum TdsType tdstype);

/**
    Translate a unicode Python object another which can be represented
    in UCS2. Unicode codepoints which do not exist in UCS2 are replaced
    with the unicode replacement character.

    @note This returns a new reference.

    @param o [in] The unicode object to translate to UCS2.

    @return The translated unicode string.
*/
PyObject* translate_to_ucs2(PyObject* o);

/**
    Convert a Python datetime, date or time to a DBDATETIME.

    @note: This will silently discard the microsecond precision for Python's
    `time` and `datetime` objects.

    @param o [in] The date, datetime or time object.
    @param dbdatetime [out] The converted value.

    @retval 0 on success, -1 on error.
*/
int datetime_to_sql(PyObject* o, DBDATETIME* dbdatetime);

#endif /* ifndef __TYPE_H__ */
