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

#ifdef __clang__
# if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
/* Ignore "'tp_print' has been explicitly marked deprecated here" */
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
#  endif /* if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8 */
#endif /* ifdef __clang__ */


#define TDS_TYPE_SIZE_FIXED (-1)

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
#if defined(CTDS_HAVE_TDS73_SUPPORT)
        DBDATETIMEALL dbdatetime;
#else /* if defined(CTDS_HAVE_TDS73_SUPPORT) */
        DBDATETIME dbdatetime;
#endif /* else if defined(CTDS_HAVE_TDS73_SUPPORT) */
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

    bool isoutput;
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

static PyObject* Parameter_repr(PyObject* self)
{
    const struct Parameter* parameter = (const struct Parameter*)self;
    PyObject* repr = NULL;

#if PY_MAJOR_VERSION < 3
    /*
        Python2.6's implementation of `PyUnicode_FromFormat` is buggy and
        will crash when the '%R' format specifier is used. Additionally
        a conversion to the `str` type is required. Avoid all this in Python2.
    */
    PyObject* value = PyObject_Repr(parameter->value);
    if (value)
    {
        repr = PyString_FromFormat(
            (parameter->isoutput) ? "%s(%s, output=True)" : "%s(%s)",
            Py_TYPE(self)->tp_name,
            PyString_AS_STRING(value)
        );
        Py_DECREF(value);
    }
#else /* if PY_MAJOR_VERSION < 3 */
    repr = PyUnicode_FromFormat(
        (parameter->isoutput) ? "%s(%R, output=True)" : "%s(%R)",
        Py_TYPE(self)->tp_name,
        parameter->value
    );
#endif /* else if PY_MAJOR_VERSION < 3 */
    return repr;
}

PyTypeObject ParameterType; /* forward declaration */

static PyObject* Parameter_richcompare(PyObject* self, PyObject* other, int op)
{
    if (PyObject_TypeCheck(other, &ParameterType))
    {
        other = ((const struct Parameter*)other)->value;
    }

    return PyObject_RichCompare(((const struct Parameter*)self)->value, other, op);
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
    "ctds.Parameter",             /* tp_name */
    sizeof(struct Parameter),     /* tp_basicsize */
    0,                            /* tp_itemsize */
    Parameter_dealloc,            /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0,                            /* tp_vectorcall_offset */
#else
    NULL,                         /* tp_print */
#endif /* if PY_VERSION_HEX >= 0x03080000 */
    NULL,                         /* tp_getattr */
    NULL,                         /* tp_setattr */
    NULL,                         /* tp_reserved */
    Parameter_repr,               /* tp_repr */
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
    Parameter_richcompare,        /* tp_richcompare */
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
#if PY_VERSION_HEX >= 0x03080000
    NULL,                         /* tp_vectorcall */
#  if PY_VERSION_HEX < 0x03090000
    NULL,                         /* tp_print */
#  endif /* if PY_VERSION_HEX < 0x03090000 */
#endif /* if PY_VERSION_HEX >= 0x03080000 */
};

/*
    Convert a python object to a parameter for use in a SQL statement.

    An appropriate Python error is set on failure.

    @param parameter [in] The parameter to bind the Python object to.
    @param value [in] The Python object to bind.

    @return -1 on error, 0 on success.
*/
static int Parameter_convert_to_tds(struct Parameter* parameter, DBPROCESS* dbproc)
{
    assert(parameter->value);

    if (SqlType_Check(parameter->value))
    {
        /* Explicitly specified a SQL type. */
        struct SqlType* sqltype = (struct SqlType*)parameter->value;

        parameter->input = sqltype->data;
        parameter->ninput = sqltype->ndata;
        parameter->tdstypesize = (DBINT)sqltype->size;
        parameter->tdstype = sqltype->tdstype;

        if (TDSDATE == sqltype->tdstype)
        {
            /* FreeTDS doesn't support passing TDSDATE properly. Fallback to DATETIME. */
            parameter->tdstype = TDSDATETIME;
        }
    }
    else
    {
        /* Infer the SQL type from the Python type. */

        /* Default to a fixed size TDS type. */
        parameter->tdstypesize = -1;

        do
        {
            if (PyUnicode_Check(parameter->value))
            {
                size_t nchars; /* number of unicode characters in the string */

                const char* utf8bytes;

                Py_XDECREF(parameter->source);
                parameter->source = encode_for_dblib(parameter->value,
                                                     &utf8bytes,
                                                     &parameter->ninput,
                                                     &nchars);
                if (!parameter->source)
                {
                    break;
                }
                parameter->input = (const void*)utf8bytes;

                /*
                    FreeTDS does not support passing *VARCHAR(MAX) types.
                    Use the *TEXT types instead.
                */

                /*
                    FreeTDS doesn't have good support for NCHAR types prior
                    to 0.95. Fallback to VARCHAR with somewhat crippled
                    functionality.
                */

#if defined(CTDS_USE_NCHARS)
                parameter->tdstype = (nchars > TDS_NCHAR_MAX_SIZE) ? TDSNTEXT : TDSNVARCHAR;
#else /* if defined(CTDS_USE_NCHARS) */
                parameter->tdstype = (nchars > TDS_CHAR_MAX_SIZE) ? TDSTEXT : TDSVARCHAR;
#endif /* else if defined(CTDS_USE_NCHARS) */
                parameter->tdstypesize = (DBINT)nchars;
            }
            /* Check for bools prior to integers, which are treated as a boolean type by Python. */
            else if (PyBool_Check(parameter->value))
            {
                parameter->buffer.byte = (BYTE)(Py_True == parameter->value);

                parameter->input = (const void*)&parameter->buffer;
                parameter->ninput = sizeof(parameter->buffer.byte);

                parameter->tdstype = TDSBITN;
                parameter->tdstypesize = (DBINT)parameter->ninput;
            }
            else if (
#if PY_MAJOR_VERSION < 3
                     PyInt_Check(parameter->value) ||
#endif /* if PY_MAJOR_VERSION < 3 */
                     PyLong_Check(parameter->value)
                     )
            {
                PY_LONG_LONG ll = 0;
#if PY_MAJOR_VERSION < 3
                if (PyInt_Check(parameter->value))
                {
                    ll = PyInt_AsLong(parameter->value);
                }
                else
#endif /* if PY_MAJOR_VERSION < 3 */
                {
                    /* This will raise an expected OverflowError on overflow. */
                    ll = PyLong_AsLongLong(parameter->value);
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

                parameter->input = (const void*)&parameter->buffer;
            }
            else if (PyBytes_Check(parameter->value) || PyByteArray_Check(parameter->value))
            {
                if (PyBytes_Check(parameter->value))
                {
                    char* data;
                    Py_ssize_t size;
                    (void)PyBytes_AsStringAndSize(parameter->value, &data, &size);
                    parameter->input = (const void*)data;
                    parameter->ninput = (size_t)size;
                }
                else
                {
                    parameter->input = (const void*)PyByteArray_AS_STRING(parameter->value);
                    parameter->ninput = (size_t)PyByteArray_GET_SIZE(parameter->value);
                }

                /*
                    FreeTDS 0.95.74 does not support passing VARBINARY types larger
                    than 8000 characters. Use the IMAGE type instead.
                */
                parameter->tdstype = (parameter->ninput > 8000) ? TDSIMAGE : TDSVARBINARY;

                /* Non-NULL values must have a non-zero length.  */
                parameter->tdstypesize = (DBINT)parameter->ninput;
            }
            else if (PyFloat_Check(parameter->value))
            {
                parameter->buffer.dbflt8 = (DBFLT8)PyFloat_AS_DOUBLE(parameter->value);

                parameter->input = (const void*)&parameter->buffer;
                parameter->ninput = sizeof(parameter->buffer.dbflt8);

                parameter->tdstype = TDSFLOAT;
            }
            else if (PyDecimal_Check(parameter->value))
            {
                PyObject* ostr = NULL;

                do
                {
                    Py_ssize_t nutf8;
                    const char* str;

                    ostr = PyDecimal_ToString(parameter->value);
                    if (!ostr)
                    {
                        break;
                    }

#if PY_MAJOR_VERSION < 3
                    str = PyString_AS_STRING(ostr);
                    nutf8 = PyString_GET_SIZE(ostr);
#else /* if PY_MAJOR_VERSION < 3 */
                    str = PyUnicode_AsUTF8AndSize(ostr, &nutf8);
#endif /* else if PY_MAJOR_VERSION < 3 */

                    do
                    {
                        DBTYPEINFO dbtypeinfo;
                        DBINT size;

                        /*
                            Determine the precision and scale based on the integer
                            and fractional part lengths. Ideally these would be
                            retrieved directly from the current decimal.Context,
                            but this is not feasible. Assume the desired scale
                            and precision are reflected in the string
                            representation.
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
                            (void)PyOS_snprintf(buffer, ARRAYSIZE(buffer), s_fmt, str);
                            buffer[ARRAYSIZE(buffer) - 1] = '\0';
                            if (0 != PyErr_WarnEx(PyExc_Warning, buffer, 1))
                            {
                                break;
                            }

                            dbtypeinfo.precision = DECIMAL_MAX_PRECISION;
                        }
                        else
                        {
                            dbtypeinfo.precision = (DBINT)(integer + fractional);
                        }

                        dbtypeinfo.scale = (DBINT)MIN(fractional, DECIMAL_MAX_PRECISION - integer);

                        size = dbconvert_ps(dbproc,
                                            TDSCHAR,
                                            (BYTE*)str,
                                            (DBINT)nutf8,
                                            TDSDECIMAL,
                                            (BYTE*)&parameter->buffer.dbdecimal,
                                            sizeof(parameter->buffer.dbdecimal),
                                            &dbtypeinfo);
                        if (-1 == size)
                        {
                            PyErr_Format(PyExc_tds_InterfaceError, "failed to convert Decimal('%s')", str);
                            break;
                        }
                        else
                        {
                            parameter->ninput = (size_t)size;
                            parameter->tdstype = TDSDECIMAL;
                            parameter->input = (const void*)&parameter->buffer;
                        }
                    } while (0);
                } while (0);

                Py_XDECREF(ostr);

                if (PyErr_Occurred()) break;
            }
            else if (PyDate_Check_(parameter->value) || PyTime_Check_(parameter->value))
            {
                int ninput = datetime_to_sql(dbproc,
                                             parameter->value,
                                             &parameter->tdstype,
                                             &parameter->buffer.dbdatetime,
                                             sizeof(parameter->buffer.dbdatetime));
                if (-1 == ninput)
                {
                    PyErr_Format(PyExc_tds_InterfaceError, "failed to convert datetime");
                    break;
                }

                parameter->ninput = (size_t)ninput;
                parameter->input = (const void*)&parameter->buffer;
            }
            else if (PyUuid_Check(parameter->value))
            {
                /*
                    FreeTDS doesn't support passing the raw bytes of the GUID, so pass
                    it as CHAR.
                */
                PyObject* uuidstr = PyObject_Str(parameter->value);
                if (uuidstr)
                {
#if PY_MAJOR_VERSION >= 3
                    Py_ssize_t size;
#endif /* if PY_MAJOR_VERSION >= 3 */
                    Py_XDECREF(parameter->source);
                    parameter->source = uuidstr; /* claim reference */
#if PY_MAJOR_VERSION < 3
                    parameter->input = (const void*)PyString_AS_STRING(uuidstr);
                    parameter->ninput = (size_t)PyString_GET_SIZE(uuidstr);
#else /* if PY_MAJOR_VERSION < 3 */
                    parameter->input = (const void*)PyUnicode_AsUTF8AndSize(uuidstr, &size);
                    parameter->ninput = (size_t)size;
#endif /* else if PY_MAJOR_VERSION < 3 */

                    assert(36 == parameter->ninput);

                    parameter->tdstype = TDSCHAR;
                    parameter->tdstypesize = (DBINT)parameter->ninput;
                }
            }
            else if (Py_None == parameter->value)
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
                PyObject* typestr = PyObject_Str((PyObject*)parameter->value->ob_type);
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

int Parameter_bind(struct Parameter* parameter, DBPROCESS* dbproc)
{
    do
    {
        if (0 != Parameter_convert_to_tds(parameter, dbproc))
        {
            break;
        }

        if (parameter->isoutput)
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
#if defined(CTDS_HAVE_TDS73_SUPPORT)
                        parameter->noutput = sizeof(DBDATETIMEALL);
#else /* if defined(CTDS_HAVE_TDS73_SUPPORT) */
                        parameter->noutput = sizeof(DBDATETIME);
#endif /* else if defined(CTDS_HAVE_TDS73_SUPPORT) */
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

            tds_mem_free(parameter->output);
            parameter->output = tds_mem_malloc(parameter->noutput);
            if (!parameter->output)
            {
                PyErr_NoMemory();
                break;
            }

            memset(parameter->output, 0, parameter->noutput);

            memcpy(parameter->output,
                   parameter->input,
                   MIN(parameter->ninput, parameter->noutput));
        }
    }
    while (0);

    return PyErr_Occurred() ? -1 : 0;
}

struct Parameter* Parameter_create(PyObject* value, bool output)
{
    struct Parameter* parameter = PyObject_New(struct Parameter, &ParameterType);
    if (NULL != parameter)
    {
        memset((((char*)parameter) + offsetof(struct Parameter, value)),
               0,
               (sizeof(struct Parameter) - offsetof(struct Parameter, value)));
        parameter->isoutput = output;

        parameter->value = value;
        Py_INCREF(parameter->value);
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
    BYTE* input = (BYTE*)parameter->input;

    /* Use the input byte count for non-NULL, variable-length types. */
    DBINT cbinput = (parameter->tdstypesize > 0) ? (DBINT)parameter->ninput : parameter->tdstypesize;

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

    /*
        0-length, non-NULL inputs are intended to be empty strings, but to
        properly pass an empty string, a NULL-terminated string must be
        provided to `bcp_bind`.
    */
    if ((0 == parameter->ninput) && (NULL != input))
    {
#if CTDS_SUPPORT_BCP_EMPTY_STRING
        input = (BYTE*)"";
        cbinput = -1;
#else /* if CTDS_SUPPORT_BCP_EMPTY_STRING */
        if (PyErr_WarnEx(PyExc_tds_Warning,
                         "\"\" converted to NULL for compatibility with FreeTDS."
                         " Please update to a recent version of FreeTDS.",
                         1))
        {
            return FAIL;
        }
#endif /* else if CTDS_SUPPORT_BCP_EMPTY_STRING */
    }

    return bcp_bind(dbproc,
                    input,
                    0,
                    cbinput,
                    NULL,
                    0,
                    tdstype,
                    (int)column);
}

bool Parameter_output(struct Parameter* rpcparam)
{
    return (NULL != rpcparam->output);
}

char* Parameter_sqltype(struct Parameter* rpcparam, bool maximum_width)
{
    char* sql = NULL;
    switch (rpcparam->tdstype)
    {
#define CONST_CASE(_type) \
        case TDS ## _type: { sql = tds_mem_strdup(STRINGIFY(_type)); break; }

        case TDSNVARCHAR:
        {
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */
            if ((rpcparam->tdstypesize > TDS_NCHAR_MAX_SIZE) || maximum_width)
            {
                sql = tds_mem_strdup("NVARCHAR(MAX)");
                break;
            }
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif
            /* Intentional fall-though. */
        }
        case TDSNCHAR:
        {
            /* The typesize will be 0 for NULL values, but the SQL type size must be 1. */
            assert(0 <= rpcparam->tdstypesize && rpcparam->tdstypesize <= TDS_NCHAR_MAX_SIZE);
            sql = tds_mem_malloc(ARRAYSIZE("NVARCHAR(2147483647)"));
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
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */
            if ((rpcparam->tdstypesize > TDS_CHAR_MAX_SIZE) || maximum_width)
            {
                sql = tds_mem_strdup("VARCHAR(MAX)");
                break;
            }
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif
            /* Intentional fall-though. */
        }
        case TDSCHAR:
        {
            /* The typesize will be 0 for NULL values, but the SQL type size must be 1. */
            assert(0 <= rpcparam->tdstypesize && rpcparam->tdstypesize <= TDS_CHAR_MAX_SIZE);
            sql = tds_mem_malloc(ARRAYSIZE("VARCHAR(2147483647)"));
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

        case TDSINTN: /* INTN can represent up to 32 bit integers. */
        case TDSINT:
        case TDSTINYINT:
        case TDSSMALLINT:
        case TDSBIGINT:
        {
            const char* prefix = "";
            if (maximum_width || TDSBIGINT == rpcparam->tdstype)
            {
                prefix = "BIG";
            }
            else if (TDSTINYINT == rpcparam->tdstype)
            {
                prefix = "TINY";
            }
            else if (TDSSMALLINT == rpcparam->tdstype)
            {
                prefix = "SMALL";
            }
            sql = tds_mem_malloc(ARRAYSIZE("SMALLINT"));
            if (sql)
            {
                (void)sprintf(sql, "%sINT", prefix);
            }
            break;
        }

        case TDSFLOAT:
        case TDSFLOATN:
        {
            /* $TODO: support variable sized floats */
            sql = tds_mem_strdup("FLOAT");
            break;
        }
        CONST_CASE(REAL)

        case TDSDATETIMEN:
        CONST_CASE(DATETIME)
        CONST_CASE(DATETIME2)
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
            sql = tds_mem_malloc(ARRAYSIZE("DECIMAL(255,255)"));
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
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */
            if ((rpcparam->tdstypesize > TDS_BINARY_MAX_SIZE) || maximum_width)
            {
                sql = tds_mem_strdup("VARBINARY(MAX)");
                break;
            }
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif
            /* Intentional fall-though. */
        }
        case TDSBINARY:
        {
            assert(1 <= rpcparam->tdstypesize && rpcparam->tdstypesize <= TDS_BINARY_MAX_SIZE);
            sql = tds_mem_malloc(ARRAYSIZE("VARBINARY(" STRINGIFY(TDS_BINARY_MAX_SIZE) ")"));
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

#if !defined(CTDS_USE_SP_EXECUTESQL)

char* Parameter_serialize(struct Parameter* rpcparam, bool maximum_width, size_t* nserialized)
{
    char* serialized = NULL;
    char* value = NULL;
    bool convert = true;
    if (NULL == rpcparam->input)
    {
        value = tds_mem_strdup("NULL");
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
                        value = tds_mem_malloc(written);
                        if (!value)
                        {
                            PyErr_NoMemory();
                            break;
                        }
                        written = 0;
                    }

                    if (write) { value[written] = '\''; }
                    ++written;

                    for (ixsrc = 0; ixsrc < rpcparam->ninput; ++ixsrc)
                    {
                        switch (input[ixsrc])
                        {
                            case '\'':
                            {
                                if (write) { value[written] = '\''; }
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */
                                ++written;
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif
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
                value = tds_mem_malloc(ARRAYSIZE("0x") + rpcparam->ninput * 2 + 1 /* '\0' */);
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

                value = tds_mem_malloc(nvalue);
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

                value = tds_mem_malloc(ARRAYSIZE("-9223372036854775808"));
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
                PyErr_Format(PyExc_tds_InterfaceError, "failed to serialize TDS type %d", rpcparam->tdstype);
                break;
            }
        }
    }

    if (!PyErr_Occurred())
    {
        if (convert)
        {
            char* type = Parameter_sqltype(rpcparam, maximum_width);
            if (type)
            {
                serialized = tds_mem_malloc(
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
#endif /* if !defined(CTDS_USE_SP_EXECUTESQL) */

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
