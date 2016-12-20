#include "include/push_warnings.h"
#include <Python.h>
#include "structmember.h"
#include "include/pop_warnings.h"

#include <stddef.h>
#include <float.h>
#include <limits.h>

#include "include/connection.h"
#include "include/macros.h"
#include "include/parameter.h"
#include "include/pyutils.h"
#include "include/tds.h"
#include "include/type.h"

#ifdef __GNUC__
/* Ignore "ISO C90 does not support 'long long' [-Werror=long-long]". */
#  pragma GCC diagnostic ignored "-Wlong-long"

#  ifndef __clang__
/* Ignore "ISO C90 does not support the 'll' gnu_printf length modifier [-Werror=format=]" */
#    pragma GCC diagnostic ignored "-Wformat="
#  endif /* ifndef __clang__ */
#endif /* ifdef __GNUC__ */


struct Parameter
{
    PyObject_HEAD

    /*
        Note: If the first member changes, the creation code must be updated to
        properly initialize members to zero.
    */

    /* The underlying Python object this parameter wraps. */
    PyObject* value;

    /*
        The underlying Python source object for the parameter's data.
        This is only set when the source data has been translated in some way
        from the original object referred to by `value`. This reference
        merely exists to ensure any data owned by the `source` object can
        be safely passed and used by the DB-lib APIs.
    */
    PyObject* source;

    /*
        The type of this parameter. This may be inferred from the Python type
        or explicitly specified via one of the Sql* types.
    */
    enum TdsType tdstype;

    /*
        The size of this parameter. For fixed length values, this is -1. For
        NULL values, this is 0.
    */
    DBINT tdstypesize;

    /*
        A buffer containing the input data for dblib to read from.
    */
    union {
        BYTE byte;
        DBINT dbint;
        DBBIGINT dbbigint;
        DBDECIMAL dbdecimal;
        DBDATETIME dbdatetime;
        DBFLT8 dbflt8;
    } buffer;

    const void* input;

    /* The size of `input`, in bytes. */
    size_t ninput;

    /*
        A buffer for the output parameter data. If this value is NULL, the
        parameter is not an output parameter.
    */
    void* output;

    /* The size of `output`, in bytes. */
    size_t noutput;
};

static void Parameter_dealloc(PyObject* self)
{
    struct Parameter* parameter = (struct Parameter*)self;
    Py_XDECREF(parameter->value);
    Py_XDECREF(parameter->source);
    tds_mem_free(parameter->output);
    PyObject_Del(self);
}

static PyObject* Parameter_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    PyObject* value;
    static char* s_kwlist[] =
    {
        "value",
        "output",
        NULL
    };
    PyObject* output = Py_False;
    if (!PyArg_ParseTupleAndKeywords(args,
                                     kwargs, "O|O!",
                                     s_kwlist,
                                     &value,
                                     &PyBool_Type,
                                     &output))
    {
        return NULL;
    }

    return (PyObject*)Parameter_create(value, (bool)(Py_True == output));
    UNUSED(type);
}

static PyMemberDef s_Parameter_members[] = {
    /* name, type, offset, flags, doc */
    { (char*)"value",   T_OBJECT, offsetof(struct Parameter, value), READONLY, NULL },
    { NULL,             0,        0,                                 0,        NULL }
};

static const char s_tds_Parameter_doc[] =
    "Parameter(value, output=False)\n"
    "\n"
    "Explicitly define a parameter for :py:meth:`.callproc`,\n"
    ":py:meth:`.execute`, or :py:meth:`.executemany`. This is necessary\n"
    "to indicate whether a parameter is *SQL* `OUTPUT` or `INPUT/OUTPUT`\n"
    "parameter.\n"
    "\n"
    ":param object value: The parameter's value.\n"
    ":param bool output: Is the parameter an output parameter.\n";

PyTypeObject ParameterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    STRINGIFY(tds) ".Parameter",  /* tp_name */
    sizeof(struct Parameter),     /* tp_basicsize */
    0,                            /* tp_itemsize */
    Parameter_dealloc,            /* tp_dealloc */
    NULL,                         /* tp_print */
    NULL,                         /* tp_getattr */
    NULL,                         /* tp_setattr */
    NULL,                         /* tp_reserved */
    NULL,                         /* tp_repr */
    NULL,                         /* tp_as_number */
    NULL,                         /* tp_as_sequence */
    NULL,                         /* tp_as_mapping */
    NULL,                         /* tp_hash */
    NULL,                         /* tp_call */
    NULL,                         /* tp_str */
    NULL,                         /* tp_getattro */
    NULL,                         /* tp_setattro */
    NULL,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,           /* tp_flags */
    s_tds_Parameter_doc,          /* tp_doc */
    NULL,                         /* tp_traverse */
    NULL,                         /* tp_clear */
    NULL,                         /* tp_richcompare */
    0,                            /* tp_weaklistoffset */
    NULL,                         /* tp_iter */
    NULL,                         /* tp_iternext */
    NULL,                         /* tp_methods */
    s_Parameter_members,          /* tp_members */
    NULL,                         /* tp_getset */
    NULL,                         /* tp_base */
    NULL,                         /* tp_dict */
    NULL,                         /* tp_descr_get */
    NULL,                         /* tp_descr_set */
    0,                            /* tp_dictoffset */
    NULL,                         /* tp_init */
    NULL,                         /* tp_alloc */
    Parameter_new,                /* tp_new */
    NULL,                         /* tp_free */
    NULL,                         /* tp_is_gc */
    NULL,                         /* tp_bases */
    NULL,                         /* tp_mro */
    NULL,                         /* tp_cache */
    NULL,                         /* tp_subclasses */
    NULL,                         /* tp_weaklist */
    NULL,                         /* tp_del */
    0,                            /* tp_version_tag */
#if PY_VERSION_HEX >= 0x03040000
    NULL,                         /* tp_finalize */
#endif /* if PY_VERSION_HEX >= 0x03040000 */

};

/*
    Bind a python object to a parameter for use in a SQL statement.

    An appropriate Python error is set on failure.

    @param parameter [in] The parameter to bind the Python object to.
    @param value [in] The Python object to bind.

    @return -1 on error, 0 on success.
*/
static int Parameter_bind(struct Parameter* parameter, PyObject* value)
{
    assert(!parameter->value);

    parameter->value = value;
    Py_INCREF(parameter->value);

    if (SqlType_Check(value))
    {
        /* Explicitly specified a SQL type. */
        struct SqlType* sqltype = (struct SqlType*)value;
        parameter->input = sqltype->data;
        parameter->ninput = sqltype->ndata;

        switch (sqltype->tdstype)
        {
            case TDSDATE:
            {
                /* FreeTDS 0.9.15 doesn't support TDSDATE properly. Fallback to DATETIME. */
                parameter->tdstype = TDSDATETIME;
                break;
            }
            default:
            {
                parameter->tdstype = sqltype->tdstype;
                break;
            }
        }

        parameter->tdstypesize = (DBINT)sqltype->size;
    }
    else
    {
        /* Infer the SQL type from the Python type. */

        /* Default to a fixed size TDS type. */
        parameter->tdstypesize = -1;

        do
        {
            if (PyUnicode_Check(value))
            {
                Py_ssize_t nchars; /* number of unicode characters in the string */

                char* utf8bytes;

                Py_XDECREF(parameter->source);
                parameter->source = encode_for_dblib(value, &utf8bytes, &parameter->ninput);
                if (!parameter->source)
                {
                    break;
                }
                parameter->input = (void*)utf8bytes;

#if PY_VERSION_HEX >= 0x03030000
                nchars = PyUnicode_GET_LENGTH(value);
#else
                nchars = PyUnicode_GET_SIZE(value);
#endif

                /*
                    FreeTDS does not support passing *VARCHAR(MAX) types.
                    Use the *TEXT types instead.
                */

                /*
                    FreeTDS doesn't have good support for NCHAR types prior
                    to 0.95. Fallback to VARCHAR with somewhat crippled
                    functionality.
                */
#if CTDS_USE_NCHARS != 0
                parameter->tdstype = (nchars > TDS_NCHAR_MAX_SIZE) ? TDSNTEXT : TDSNVARCHAR;
#else /* if CTDS_USE_NCHARS != 0 */
                parameter->tdstype = (nchars > TDS_CHAR_MAX_SIZE) ? TDSTEXT : TDSVARCHAR;
#endif /* else if CTDS_USE_NCHARS != 0 */
                parameter->tdstypesize = (DBINT)nchars;
            }
            /* Check for bools prior to integers, which are treated as a boolean type by Python. */
            else if (PyBool_Check(value))
            {
                parameter->buffer.byte = (BYTE)(Py_True == value);

                parameter->input = (void*)&parameter->buffer;
                parameter->ninput = sizeof(parameter->buffer.byte);

                parameter->tdstype = TDSBITN;
                parameter->tdstypesize = (DBINT)parameter->ninput;
            }
            else if (
#if PY_MAJOR_VERSION < 3
                     PyInt_Check(value) ||
#endif /* if PY_MAJOR_VERSION < 3 */
                     PyLong_Check(value)
                     )
            {
                PY_LONG_LONG ll = 0;
#if PY_MAJOR_VERSION < 3
                if (PyInt_Check(value))
                {
                    ll = PyInt_AsLong(value);
                }
                else
#endif /* if PY_MAJOR_VERSION < 3 */
                {
                    /* This will raise an expected OverflowError on overflow. */
                    ll = PyLong_AsLongLong(value);
                }

                if (PyErr_Occurred())
                {
                    break;
                }

                if (INT32_MIN <= ll && ll <= INT32_MAX)
                {
                    parameter->buffer.dbint = (DBINT)ll;
                    parameter->ninput = sizeof(parameter->buffer.dbint);

                    /* TINYINT is unsigned. */
                    if (0 <= ll && ll <= 255)
                    {
                        parameter->tdstype = TDSTINYINT;
                    }
                    else if (INT16_MIN <= ll && ll <= INT16_MAX)
                    {
                        parameter->tdstype = TDSSMALLINT;
                    }
                    else
                    {
                       parameter->tdstype = TDSINT;
                    }
                }
                else
                {
                    parameter->buffer.dbbigint = (DBBIGINT)ll;
                    parameter->ninput = sizeof(parameter->buffer.dbbigint);

                    parameter->tdstype = TDSBIGINT;
                }

                parameter->input = (void*)&parameter->buffer;
            }
            else if (PyBytes_Check(value) || PyByteArray_Check(value))
            {
                if (PyBytes_Check(value))
                {
                    char* data;
                    Py_ssize_t size;
                    (void)PyBytes_AsStringAndSize(value, &data, &size);
                    parameter->input = (void*)data;
                    parameter->ninput = (size_t)size;
                }
                else
                {
                    parameter->input = (void*)PyByteArray_AS_STRING(value);
                    parameter->ninput = (size_t)PyByteArray_GET_SIZE(value);
                }

                /*
                    FreeTDS 0.95.74 does not support passing VARBINARY types larger
                    than 8000 characters. Use the IMAGE type instead.
                */
                parameter->tdstype = (parameter->ninput > 8000) ? TDSIMAGE : TDSVARBINARY;

                /* Non-NULL values must have a non-zero length.  */
                parameter->tdstypesize = (DBINT)parameter->ninput;
            }
            else if (PyFloat_Check(value))
            {
                parameter->buffer.dbflt8 = (DBFLT8)PyFloat_AS_DOUBLE(value);

                parameter->input = (void*)&parameter->buffer;
                parameter->ninput = sizeof(parameter->buffer.dbflt8);

                parameter->tdstype = TDSFLOAT;
            }
            else if (PyDecimal_Check(value))
            {
                /* Convert the Decimal to a string, then convert. */
                PyObject* ostr = PyObject_Str(value);
                if (ostr)
                {
                    DBTYPEINFO dbtypeinfo;
                    Py_ssize_t nutf8;
#if PY_MAJOR_VERSION < 3
                    const char* str = PyString_AS_STRING(ostr);
                    nutf8 = PyString_GET_SIZE(ostr);
#else /* if PY_MAJOR_VERSION < 3 */
                    const char* str = PyUnicode_AsUTF8AndSize(ostr, &nutf8);
#endif /* else if PY_MAJOR_VERSION < 3 */

                    do
                    {
                        DBINT size;

                        /*
                            Determine the precision and scale based on the integer and
                            fractional part lengths.
                        */
                        const char* point = strchr(str, '.');
                        bool negative = ('-' == *str);
                        size_t integer =
                            (size_t)((point) ? (point - str) : nutf8) -
                            /* Ignore negative sign. */
                            ((negative) ? 1 : 0);
                        size_t fractional = ((size_t)nutf8 - ((point) ? 1 : 0) - ((negative) ? 1 : 0)) - integer;

                        if (integer > DECIMAL_MAX_PRECISION)
                        {
                            PyErr_Format(PyExc_tds_DataError, "Decimal('%s') out of range", str);
                            break;
                        }
                        if (DECIMAL_MAX_PRECISION < (integer + fractional))
                        {
                            static const char s_fmt[] =
                                "Decimal('%s') exceeds SQL DECIMAL precision; truncating";
                            char buffer[ARRAYSIZE(s_fmt) + 100];
                            (void)snprintf(buffer, ARRAYSIZE(buffer), s_fmt, str);
                            buffer[ARRAYSIZE(buffer) - 1] = '\0';
                            (void)PyErr_Warn(PyExc_tds_Warning, buffer);

                            dbtypeinfo.precision = DECIMAL_MAX_PRECISION;
                        }
                        else
                        {
                            dbtypeinfo.precision = (DBINT)(integer + fractional);
                        }

                        dbtypeinfo.scale = (DBINT)MIN(fractional, DECIMAL_MAX_PRECISION - integer);

                        size = dbconvert_ps(NULL,
                                            TDSCHAR,
                                            (BYTE*)str,
                                            (DBINT)nutf8,
                                            TDSDECIMAL,
                                            (BYTE*)&parameter->buffer.dbdecimal,
                                            sizeof(parameter->buffer.dbdecimal),
                                            &dbtypeinfo);
                        if (-1 == size)
                        {
                            PyErr_Format(PyExc_tds_InternalError, "failed to convert Decimal('%s')", str);
                            break;
                        }
                        else
                        {
                            parameter->ninput = (size_t)size;
                            parameter->tdstype = TDSDECIMAL;
                            parameter->input = (void*)&parameter->buffer;
                        }
                    } while (0);
                    Py_DECREF(ostr);

                    if (PyErr_Occurred()) break;
                }
            }
            else if (PyDate_Check_(value) || PyTime_Check_(value))
            {
                if (-1 == datetime_to_sql(value, &parameter->buffer.dbdatetime))
                {
                    PyErr_Format(PyExc_tds_InternalError, "failed to convert datetime");
                    break;
                }

                parameter->ninput = sizeof(parameter->buffer.dbdatetime);
                parameter->tdstype = TDSDATETIME;
                parameter->input = (void*)&parameter->buffer;
            }
            else if (PyUuid_Check(value))
            {
                /*
                    FreeTDS doesn't support passing the raw bytes of the GUID, so pass
                    it as CHAR.
                */
                PyObject* uuidstr = PyObject_Str(value);
                if (uuidstr)
                {
#if PY_MAJOR_VERSION >= 3
                    Py_ssize_t size;
#endif /* if PY_MAJOR_VERSION >= 3 */
                    Py_XDECREF(parameter->source);
                    parameter->source = uuidstr; /* claim reference */
#if PY_MAJOR_VERSION < 3
                    parameter->input = (void*)PyString_AS_STRING(uuidstr);
                    parameter->ninput = (size_t)PyString_GET_SIZE(uuidstr);
#else /* if PY_MAJOR_VERSION < 3 */
                    parameter->input = (void*)PyUnicode_AsUTF8AndSize(uuidstr, &size);
                    parameter->ninput = (size_t)size;
#endif /* else if PY_MAJOR_VERSION < 3 */

                    assert(16 == parameter->ninput);

                    parameter->tdstype = TDSCHAR;
                    parameter->tdstypesize = (DBINT)parameter->ninput;
                }
            }
            else if (Py_None == value)
            {
                /*
                    Default to VARCHAR for an untyped None value.
                    Note: This will *not* work with BINARY types.

                    Ideally this would be the NULL type (0x1f), but it isn't supported by FreeTDS.
                */
                parameter->input = NULL;
                parameter->ninput = 0;

                parameter->tdstype = TDSVARCHAR;
                parameter->tdstypesize = (DBINT)parameter->ninput;
            }
            else
            {
                /* An unsupported type. */
                PyObject* typestr = PyObject_Str((PyObject*)value->ob_type);
                if (typestr)
                {
                    PyErr_Format(PyExc_tds_InterfaceError,
                                 "could not implicitly convert Python type \"%s\" to SQL",
#if PY_MAJOR_VERSION < 3
                                 PyString_AS_STRING(typestr)
#else /* if PY_MAJOR_VERSION < 3 */
                                 PyUnicode_AsUTF8(typestr)
#endif /* else if PY_MAJOR_VERSION < 3 */
                                 );
                    Py_DECREF(typestr);
                }
            }
        } while (0);
    }

    return (PyErr_Occurred()) ? -1 : 0;
}

struct Parameter* Parameter_create(PyObject* value, bool output)
{
    struct Parameter* parameter = PyObject_New(struct Parameter, &ParameterType);
    if (NULL != parameter)
    {
        memset((((char*)parameter) + offsetof(struct Parameter, value)),
               0,
               (sizeof(struct Parameter) - offsetof(struct Parameter, value)));
        if (0 == Parameter_bind(parameter, value))
        {
            if (output)
            {
                /*
                    Use the type size for variable-length parameters.
                    Determine the fixed size from the type.
                */
                bool fixed = (-1 == parameter->tdstypesize);
                if (fixed)
                {
                    switch (parameter->tdstype)
                    {
                        case TDSBIT:
                        case TDSBITN:
                        case TDSINTN:
                        case TDSTINYINT:
                        case TDSSMALLINT:
                        case TDSINT:
                        {
                            parameter->noutput = sizeof(DBINT);
                            break;
                        }
                        case TDSBIGINT:
                        {
                            parameter->noutput = sizeof(DBBIGINT);
                            break;
                        }
                        case TDSFLOAT:
                        case TDSFLOATN:
                        case TDSREAL:
                        {
                            parameter->noutput = sizeof(DBFLT8);
                            break;
                        }
                        case TDSDATETIME:
                        case TDSSMALLDATETIME:
                        case TDSDATETIMEN:
                        case TDSDATE:
                        case TDSTIME:
                        case TDSDATETIME2:
                        {
                            parameter->noutput = sizeof(DBDATETIME);
                            break;
                        }
                        case TDSSMALLMONEY:
                        case TDSMONEY:
                        case TDSMONEYN:
                        case TDSNUMERIC:
                        case TDSDECIMAL:
                        {
                            parameter->noutput = sizeof(DBDECIMAL);
                            break;
                        }
                        case TDSGUID:
                        {
                            parameter->noutput = 16;
                            break;
                        }
                        default:
                        {
                            parameter->noutput = 0;
                            break;
                        }
                    }
                }
                else
                {
                    parameter->noutput = (size_t)parameter->tdstypesize;
                }

                parameter->output = tds_mem_malloc(parameter->noutput);
                if (!parameter->output)
                {
                    PyErr_NoMemory();
                }

                memset(parameter->output, 0, parameter->noutput);

                memcpy(parameter->output,
                       parameter->input,
                       MIN(parameter->ninput, parameter->noutput));
            }
        }

        if (PyErr_Occurred())
        {
            Py_DECREF(parameter);
            parameter = NULL;
        }
    }
    else
    {
        PyErr_NoMemory();
    }

    return parameter;
}

RETCODE Parameter_dbrpcparam(struct Parameter* parameter, DBPROCESS* dbproc, const char* paramname)
{
    return dbrpcparam(dbproc,
                      paramname,
                      (parameter->output) ? DBRPCRETURN : 0,
                      parameter->tdstype,
                      (parameter->output) ? parameter->tdstypesize : -1,
                      (DBINT)parameter->ninput,
                      (BYTE*)((parameter->output) ? parameter->output : parameter->input));
}

RETCODE Parameter_bcp_bind(struct Parameter* parameter, DBPROCESS* dbproc, size_t column)
{
    /*
        FreeTDS' bulk insert does not support passing Unicode types. If the caller
        has requested a Unicode type, map it to the equivalent single-byte representation.
    */
    enum TdsType tdstype;
    if ((parameter->tdstype == TDSNTEXT) || (parameter->tdstype == TDSNVARCHAR))
    {
        /*
            FreeTDS does not support passing *VARCHAR(MAX) types.
            Use the *TEXT types instead.
        */
        tdstype = (parameter->ninput > TDS_CHAR_MAX_SIZE) ? TDSTEXT : TDSVARCHAR;
    }
    else
    {
        tdstype = parameter->tdstype;
    }
    return bcp_bind(dbproc,
                    (BYTE*)parameter->input,
                    0,
                    /* Use the input byte count for non-NULL, variable-length types. */
                    (parameter->tdstypesize > 0) ? (DBINT)parameter->ninput : parameter->tdstypesize,
                    NULL,
                    0,
                    tdstype,
                    (int)column);
}

bool Parameter_output(struct Parameter* rpcparam)
{
    return (NULL != rpcparam->output);
}

char* Parameter_sqltype(struct Parameter* rpcparam)
{
    char* sql = NULL;
    switch (rpcparam->tdstype)
    {
#define CONST_CASE(_type) \
        case TDS ## _type: { sql = strdup(STRINGIFY(_type)); break; }

        case TDSNVARCHAR:
        {
            if (rpcparam->tdstypesize > TDS_NCHAR_MAX_SIZE)
            {
                sql = strdup("NVARCHAR(MAX)");
                break;
            }
            /* Intentional fall-though. */
        }
        case TDSNCHAR:
        {
            /* The typesize will be 0 for NULL values, but the SQL type size must be 1. */
            assert(0 <= rpcparam->tdstypesize && rpcparam->tdstypesize <= TDS_NCHAR_MAX_SIZE);
            sql = (char*)tds_mem_malloc(ARRAYSIZE("NVARCHAR(" STRINGIFY(TDS_NCHAR_MAX_SIZE) ")"));
            if (sql)
            {
                (void)sprintf(sql,
                              "N%sCHAR(%d)",
                              (TDSNCHAR == rpcparam->tdstype) ? "" : "VAR",
                              MAX(rpcparam->tdstypesize, 1));
            }
            break;
        }
        case TDSVARCHAR:
        {
            if (rpcparam->tdstypesize > TDS_CHAR_MAX_SIZE)
            {
                sql = strdup("VARCHAR(MAX)");
                break;
            }
            /* Intentional fall-though. */
        }
        case TDSCHAR:
        {
            /* The typesize will be 0 for NULL values, but the SQL type size must be 1. */
            assert(0 <= rpcparam->tdstypesize && rpcparam->tdstypesize <= TDS_CHAR_MAX_SIZE);
            sql = (char*)tds_mem_malloc(ARRAYSIZE("VARCHAR(" STRINGIFY(TDS_CHAR_MAX_SIZE) ")"));
            if (sql)
            {
                (void)sprintf(sql,
                              "%sCHAR(%d)",
                              (TDSCHAR == rpcparam->tdstype) ? "" : "VAR",
                              MAX(rpcparam->tdstypesize, 1));
            }
            break;
        }
        CONST_CASE(NTEXT)
        CONST_CASE(TEXT)

        case TDSBITN:
        CONST_CASE(BIT)

        case TDSINTN:
            /* INTN can represent up to 32 bit integers. */
        CONST_CASE(INT)
        CONST_CASE(TINYINT)
        CONST_CASE(SMALLINT)
        CONST_CASE(BIGINT)

        case TDSFLOAT:
        case TDSFLOATN:
        {
            /* $TODO: support variable sized floats */
            sql = strdup("FLOAT");
            break;
        }
        CONST_CASE(REAL)

        case TDSDATETIMEN:
        CONST_CASE(DATETIME)
        CONST_CASE(SMALLDATETIME)
        CONST_CASE(DATE)
        CONST_CASE(TIME)

        CONST_CASE(IMAGE)

        CONST_CASE(SMALLMONEY)
        case TDSMONEYN:
        CONST_CASE(MONEY)

        case TDSNUMERIC:
        case TDSDECIMAL:
        {
            sql = (char*)tds_mem_malloc(ARRAYSIZE("DECIMAL(38,38)"));
            if (sql)
            {
                const DBDECIMAL* dbdecimal = (const DBDECIMAL*)rpcparam->input;
                (void)sprintf(sql,
                              "%s(%d,%d)",
                              (TDSNUMERIC == rpcparam->tdstype) ? "NUMERIC" : "DECIMAL",
                              (dbdecimal) ? dbdecimal->precision : 1,
                              (dbdecimal) ? dbdecimal->scale : 0);
            }
            break;
        }

        case TDSVARBINARY:
        {
            if (rpcparam->tdstypesize > 8000)
            {
                sql = strdup("VARBINARY(MAX)");
                break;
            }
            /* Intentional fall-though. */
        }
        case TDSBINARY:
        {
            assert(1 <= rpcparam->tdstypesize && rpcparam->tdstypesize <= 8000);
            sql = (char*)tds_mem_malloc(ARRAYSIZE("VARBINARY(8000)"));
            if (sql)
            {
                (void)sprintf(sql,
                              "%sBINARY(%d)",
                              (TDSBINARY == rpcparam->tdstype) ? "" : "VAR",
                              rpcparam->tdstypesize);
            }
            break;
        }

        CONST_CASE(GUID)
        CONST_CASE(XML)
        CONST_CASE(VOID)
        default:
        {
            break;
        }
    }
    return sql;
}

PyObject* Parameter_value(struct Parameter* rpcparam)
{
    return rpcparam->value;
}

#if !(defined(CTDS_USE_SP_EXECUTESQL) && (CTDS_USE_SP_EXECUTESQL != 0))

char* Parameter_serialize(struct Parameter* rpcparam, size_t* nserialized)
{
    char* serialized = NULL;
    char* value = NULL;
    bool convert = true;
    if (NULL == rpcparam->input)
    {
        value = strdup("NULL");
        if (value)
        {
            *nserialized = ARRAYSIZE("NULL") - 1;
        }
        else
        {
            PyErr_NoMemory();
        }
    }
    else
    {
        switch (rpcparam->tdstype)
        {
            case TDSNCHAR:
            case TDSCHAR:
            case TDSNVARCHAR:
            case TDSVARCHAR:
            case TDSNTEXT:
            case TDSTEXT:
            {
                const char* input = (const char*)rpcparam->input;
                size_t written = 0;

                /*
                    Escape the string for SQL by replacing "'" with "''".

                    The first pass computes the size of the escaped string.
                    The second pass allocates a buffer and then escapes the string
                    to the buffer.
                */
                int write;
                for (write = 0; write < 2; ++write)
                {
                    size_t ixsrc;
                    if (write)
                    {
                        value = (char*)tds_mem_malloc(written);
                        if (!value)
                        {
                            PyErr_NoMemory();
                            break;
                        }
                        written = 0;
                    }

                    if (write) { value[written] = '\''; }
                    ++written;

                    for (ixsrc = 0; ixsrc < MIN((size_t)rpcparam->tdstypesize, rpcparam->ninput); ++ixsrc)
                    {
                        switch (input[ixsrc])
                        {
                            case '\'':
                            {
                                if (write) { value[written] = '\''; }
                                ++written;
                                /* Intentional fall-through. */
                            }
                            default:
                            {
                                if (write) { value[written] = input[ixsrc]; }
                                ++written;
                                break;
                            }
                        }
                    }

                    if (write) { value[written] = '\''; }
                    ++written;
                    if (write) { value[written] = '\0'; }
                    ++written;
                }
                *nserialized = written - 1;

                convert = ((TDSCHAR == rpcparam->tdstype) ||
                           (TDSNCHAR == rpcparam->tdstype) ||
                           ((size_t)rpcparam->tdstypesize != rpcparam->ninput));
                break;
            }
            case TDSBINARY:
            case TDSVARBINARY:
            case TDSIMAGE:
            {
                size_t ix, written = 0;

                /* Large enough for the hexadecimal representation. */
                value = (char*)tds_mem_malloc(ARRAYSIZE("0x") + rpcparam->ninput * 2 + 1 /* '\0' */);
                if (!value)
                {
                    PyErr_NoMemory();
                    break;
                }
                written += (size_t)sprintf(&value[written], "0x");
                for (ix = 0; ix < rpcparam->ninput; ++ix)
                {
                    const unsigned char byte = ((unsigned char*)rpcparam->input)[ix];
                    written += (size_t)sprintf(&value[written], "%02x", byte);
                }
                value[written] = '\0';
                *nserialized = written - 1;
                break;
            }
            case TDSDATE:
            case TDSDATETIME:
            case TDSDATETIME2:
            case TDSTIME:
            case TDSMONEY:
            case TDSDECIMAL:
            case TDSFLOAT:
            {
                size_t written = 0;
                DBINT result;

                size_t nvalue;
                bool quoted = false;

                switch (rpcparam->tdstype)
                {
                    case TDSMONEY:
                    case TDSDECIMAL:
                    {
                        nvalue = 1 /* '-' */ + 38 + 1 /* '.' */ + 1 /* '\0' */;
                        break;
                    }
                    case TDSFLOAT:
                    {
                        nvalue = 1 /* '-' */ + 15 + 1 /* '.' */ + 5 /* e+123 */ + 1 /* '\0' */;
                        break;
                    }
                    default:
                    {
                        quoted = true;
                        nvalue = ARRAYSIZE("'MMM DD YYYY hh:mm:ss:nnnPM'");
                        break;
                    }
                }

                value = (char*)tds_mem_malloc(nvalue);
                if (!value)
                {
                    PyErr_NoMemory();
                    break;
                }

                if (quoted)
                {
                    value[written++] = '\'';
                }
                result = dbconvert(NULL,
                                   rpcparam->tdstype,
                                   (BYTE*)rpcparam->input,
                                   -1,
                                   TDSCHAR,
                                   (BYTE*)&value[written],
                                   (DBINT)(nvalue - written));
                assert(-1 != result);
                written += (size_t)result;
                if (quoted)
                {
                    value[written++] = '\'';
                }
                value[written] = '\0';
                *nserialized = written - 1;
                break;
            }
            case TDSBITN:
            case TDSTINYINT:
            case TDSSMALLINT:
            case TDSINT:
            case TDSBIGINT:
            {
                union
                {
                    uint8_t tiny;
                    int16_t small;
                    int32_t int_;
                    int64_t big;
                } ints;
                memset(&ints, 0, sizeof(ints));

                value = (char*)tds_mem_malloc(ARRAYSIZE("-9223372036854775808"));
                if (!value)
                {
                    PyErr_NoMemory();
                    break;
                }
                memcpy(&ints, rpcparam->input, rpcparam->ninput);
                if (TDSBIGINT == rpcparam->tdstype)
                {
                    *nserialized = (size_t)sprintf(value, "%lli", (long long int)ints.big);
                }
                else if (TDSINT == rpcparam->tdstype)
                {
                    *nserialized = (size_t)sprintf(value, "%i", ints.int_);
                }
                else if (TDSSMALLINT == rpcparam->tdstype)
                {
                    *nserialized = (size_t)sprintf(value, "%i", ints.small);
                }
                else
                {
                    *nserialized = (size_t)sprintf(value, "%u", ints.tiny);
                }
                break;
            }
            default:
            {
                PyErr_Format(PyExc_tds_InternalError, "failed to serialize TDS type %d", rpcparam->tdstype);
                break;
            }
        }
    }

    if (!PyErr_Occurred())
    {
        if (convert)
        {
            char* type = Parameter_sqltype(rpcparam);
            if (type)
            {
                serialized = (char*)tds_mem_malloc(
                    ARRAYSIZE("CONVERT(") + strlen(type) + ARRAYSIZE(",") + strlen(value) + ARRAYSIZE(")")
                );
                if (serialized)
                {
                    *nserialized = (size_t)sprintf(serialized, "CONVERT(%s,%s)", type, value);
                }
                else
                {
                    PyErr_NoMemory();
                }
                tds_mem_free(type);
            }
        }
        else
        {
            serialized = value;
            value = NULL;
        }
    }

    if (PyErr_Occurred())
    {
        tds_mem_free(serialized);
        serialized = NULL;
        *nserialized = 0;
    }

    tds_mem_free(value);

    return serialized;
}
#endif /* if !(defined(CTDS_USE_SP_EXECUTESQL) && (CTDS_USE_SP_EXECUTESQL != 0)) */

PyTypeObject* ParameterType_init(void)
{
    if (0 != PyType_Ready(&ParameterType))
    {
        return NULL;
    }
    return &ParameterType;
}

PyTypeObject* ParameterType_get(void)
{
    Py_INCREF(&ParameterType);
    return &ParameterType;
}

int Parameter_Check(PyObject* o)
{
    return PyObject_TypeCheck(o, &ParameterType);
}
