#include "include/push_warnings.h"
#include <Python.h>
#include "structmember.h"
#include <sybdb.h>
#include "include/pop_warnings.h"

#include <stdarg.h>
#include <stddef.h>

#include "include/c99int.h"

#include "include/connection.h"
#include "include/macros.h"
#include "include/pyutils.h"
#include "include/tds.h"
#include "include/type.h"

#ifdef __GNUC__
/* Ignore "ISO C90 does not support 'long long' [-Werror=long-long]". */
#  pragma GCC diagnostic ignored "-Wlong-long"
#endif /* ifdef __GNUC__ */


#define _SqlType_init_base(_type, _value, _tdstype) \
    Py_INCREF((_value)); \
    ((struct SqlType*)(_type))->value = (_value); \
    ((struct SqlType*)(_type))->tdstype = (_tdstype)

#define SqlType_init_fixed(_type, _value, _tdstype, _member) \
    _SqlType_init_base(_type, _value, _tdstype); \
    ((struct SqlType*)(_type))->data = (Py_None == (_value)) ? NULL : (void*)&(_member); \
    ((struct SqlType*)(_type))->ndata = (Py_None == (_value)) ? 0 : sizeof(_member); \
    ((struct SqlType*)(_type))->data_free = NULL; \
    ((struct SqlType*)(_type))->size = -1

#define SqlType_init_variable(_type, _value, _tdstype, _size, _data, _ndata, _data_free) \
    _SqlType_init_base(_type, _value, _tdstype); \
    ((struct SqlType*)(_type))->data = _data; \
    ((struct SqlType*)(_type))->ndata = _ndata; \
    ((struct SqlType*)(_type))->data_free = _data_free; \
    ((struct SqlType*)(_type))->size = _size

static void SqlType_dealloc(PyObject* self)
{
    struct SqlType* type = (struct SqlType*)self;
    Py_XDECREF(type->value);

    if (type->data_free)
    {
        type->data_free(type->data);
        type->data = NULL;
    }

    self->ob_type->tp_free(self);
}

static PyObject* SqlType_repr(PyObject* self)
{
    const struct SqlType* type = (const struct SqlType*)self;
    PyObject* repr = NULL;

    bool include_size = (-1 != type->size);

#if PY_MAJOR_VERSION < 3
    /*
        Python2.6's implementation of `PyUnicode_FromFormat` is buggy and
        will crash when the '%R' format specifier is used. Additionally
        a conversion to the `str` type is required. Avoid all this in Python2.
    */
    PyObject* value = PyObject_Repr(type->value);
    if (value)
    {
        if (include_size)
        {
            repr = PyString_FromFormat(
                "%s(%s, size=%d)",
                Py_TYPE(self)->tp_name,
                PyString_AS_STRING(value),
                type->size
            );
        }
        else
        {
            repr = PyString_FromFormat(
                "%s(%s)",
                Py_TYPE(self)->tp_name,
                PyString_AS_STRING(value)
            );
        }
        Py_DECREF(value);
    }
#else /* if PY_MAJOR_VERSION < 3 */
    if (include_size)
    {
        repr = PyUnicode_FromFormat(
            "%s(%R, size=%d)",
            Py_TYPE(self)->tp_name,
            type->value,
            type->size
        );
    }
    else
    {
        repr = PyUnicode_FromFormat(
            "%s(%R)",
            Py_TYPE(self)->tp_name,
            type->value
        );
    }
#endif /* else if PY_MAJOR_VERSION < 3 */
    return repr;
}

static const char s_SqlType_members_doc_size[] =
    "The size of the type. This will be -1 for fixed size values.";

static const char s_SqlType_members_doc_tdstype[] =
    "The TDS type code.";

static const char s_SqlType_members_doc_value[] =
    "The wrapped Python value.";

static PyMemberDef s_SqlType_members[] = {
    /* name, type, offset, flags, doc */
    { (char*)"size",    T_INT,    offsetof(struct SqlType, size),    READONLY, (char*)s_SqlType_members_doc_size },
    { (char*)"tdstype", T_USHORT, offsetof(struct SqlType, tdstype), READONLY, (char*)s_SqlType_members_doc_tdstype },
    { (char*)"value",   T_OBJECT, offsetof(struct SqlType, value),   READONLY, (char*)s_SqlType_members_doc_value },
    { NULL,             0,        0,                                 0,        NULL }
};

static const char s_tds_SqlType_doc[] =
    "An object wrapper to explicitly specify which SQL type a Python object"
    "should be serialized as.";

PyTypeObject SqlTypeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "ctds.SqlType",                           /* tp_name */
    sizeof(struct SqlType),                   /* tp_basicsize */
    0,                                        /* tp_itemsize */
    SqlType_dealloc,                          /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0,                                        /* tp_vectorcall_offset */
#else
    NULL,                                     /* tp_print */
#endif /* if PY_VERSION_HEX >= 0x03080000 */
    NULL,                                     /* tp_getattr */
    NULL,                                     /* tp_setattr */
    NULL,                                     /* tp_reserved */
    SqlType_repr,                             /* tp_repr */
    NULL,                                     /* tp_as_number */
    NULL,                                     /* tp_as_sequence */
    NULL,                                     /* tp_as_mapping */
    NULL,                                     /* tp_hash */
    NULL,                                     /* tp_call */
    NULL,                                     /* tp_str */
    NULL,                                     /* tp_getattro */
    NULL,                                     /* tp_setattro */
    NULL,                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    s_tds_SqlType_doc,                        /* tp_doc */
    NULL,                                     /* tp_traverse */
    NULL,                                     /* tp_clear */
    NULL,                                     /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    NULL,                                     /* tp_iter */
    NULL,                                     /* tp_iternext */
    NULL,                                     /* tp_methods */
    s_SqlType_members,                        /* tp_members */
    NULL,                                     /* tp_getset */
    NULL,                                     /* tp_base */
    NULL,                                     /* tp_dict */
    NULL,                                     /* tp_descr_get */
    NULL,                                     /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    NULL,                                     /* tp_init */
    NULL,                                     /* tp_alloc */
    PyType_GenericNew,                        /* tp_new */
    NULL,                                     /* tp_free */
    NULL,                                     /* tp_is_gc */
    NULL,                                     /* tp_bases */
    NULL,                                     /* tp_mro */
    NULL,                                     /* tp_cache */
    NULL,                                     /* tp_subclasses */
    NULL,                                     /* tp_weaklist */
    NULL,                                     /* tp_del */
    0,                                        /* tp_version_tag */
#if PY_VERSION_HEX >= 0x03040000
    NULL,                                     /* tp_finalize */
#endif /* if PY_VERSION_HEX >= 0x03040000 */
#if PY_VERSION_HEX >= 0x03080000
    NULL,                                     /* tp_vectorcall */
    NULL,                                     /* tp_print */
#endif /* if PY_VERSION_HEX >= 0x03080000 */
};

int SqlType_Check(PyObject* o)
{
    return PyObject_TypeCheck(o, &SqlTypeType);
}

#define SqlType_HEAD \
    struct SqlType type

#if PY_VERSION_HEX >= 0x03040000
#  define _TP_FINALIZE NULL,
#else
#  define _TP_FINALIZE /* NULL */
#endif

#if PY_VERSION_HEX >= 0x03080000
#  define _TP_VECTORCALL_OFFSET 0 /* tp_vectorcall_offset */
#  define _TP_VECTORCALL NULL,
#  define _TP_PRINT NULL,
#else
#  define _TP_VECTORCALL_OFFSET NULL
#  define _TP_VECTORCALL /* NULL */
#  define _TP_PRINT /* NULL */
#endif /* if PY_VERSION_HEX >= 0x03080000 */

#define SQL_TYPE_DEF(_type, _doc) \
    PyTypeObject Sql ## _type ## Type; /* forward decl. */ \
    PyObject* Sql ## _type ## _create(PyObject* self, PyObject* args, PyObject* kwargs) \
    { \
        struct Sql ## _type * sqltype = PyObject_New(struct Sql ## _type, &Sql ## _type ## Type); \
        if (NULL != sqltype) \
        { \
            if (0 != (Sql ## _type ## Type).tp_init((PyObject*)sqltype, args, kwargs)) \
            { \
                Py_DECREF(sqltype); \
                sqltype = NULL; \
            } \
        } \
        else \
        { \
            PyErr_NoMemory(); \
        } \
        return (PyObject*)sqltype; \
        UNUSED(self); \
    } \
    PyTypeObject* Sql ## _type ## Type_init(void) \
    { \
        return (PyType_Ready(&(Sql ## _type ## Type)) == 0) ? &(Sql ## _type ## Type) : NULL; \
    } \
    PyTypeObject Sql ## _type ## Type = { \
        PyVarObject_HEAD_INIT(NULL, 0) \
        "ctds.Sql" STRINGIFY(_type),                /* tp_name */ \
        sizeof(struct Sql ## _type),                /* tp_basicsize */ \
        0,                                          /* tp_itemsize */ \
        NULL,                                       /* tp_dealloc */ \
        _TP_VECTORCALL_OFFSET,                      /* tp_vectorcall_offset */ \
        NULL,                                       /* tp_getattr */ \
        NULL,                                       /* tp_setattr */ \
        NULL,                                       /* tp_reserved */ \
        NULL,                                       /* tp_repr */ \
        NULL,                                       /* tp_as_number */ \
        NULL,                                       /* tp_as_sequence */ \
        NULL,                                       /* tp_as_mapping */ \
        NULL,                                       /* tp_hash */ \
        NULL,                                       /* tp_call */ \
        NULL,                                       /* tp_str */ \
        NULL,                                       /* tp_getattro */ \
        NULL,                                       /* tp_setattro */ \
        NULL,                                       /* tp_as_buffer */ \
        Py_TPFLAGS_DEFAULT,                         /* tp_flags */ \
        (_doc),                                     /* tp_doc */ \
        NULL,                                       /* tp_traverse */ \
        NULL,                                       /* tp_clear */ \
        NULL,                                       /* tp_richcompare */ \
        0,                                          /* tp_weaklistoffset */ \
        NULL,                                       /* tp_iter */ \
        NULL,                                       /* tp_iternext */ \
        NULL,                                       /* tp_methods */ \
        NULL,                                       /* tp_members */ \
        NULL,                                       /* tp_getset */ \
        &SqlTypeType,                               /* tp_base */ \
        NULL,                                       /* tp_dict */ \
        NULL,                                       /* tp_descr_get */ \
        NULL,                                       /* tp_descr_set */ \
        0,                                          /* tp_dictoffset */ \
        Sql ## _type ## _init,                      /* tp_init */ \
        NULL,                                       /* tp_alloc */ \
        NULL,                                       /* tp_new */ \
        NULL,                                       /* tp_free */ \
        NULL,                                       /* tp_is_gc */ \
        NULL,                                       /* tp_bases */ \
        NULL,                                       /* tp_mro */ \
        NULL,                                       /* tp_cache */ \
        NULL,                                       /* tp_subclasses */ \
        NULL,                                       /* tp_weaklist */ \
        NULL,                                       /* tp_del */ \
        0,                                          /* tp_version_tag */ \
        _TP_FINALIZE                                /* tp_finalize */ \
        _TP_VECTORCALL                              /* tp_vectorcall */ \
        _TP_PRINT                                   /* tp_print */ \
    }


static int SqlIntN_parse(PyObject* args, const char* format, ...)
{
    va_list vargs;
    int error;

    if ((PyTuple_GET_SIZE(args) == 1) &&
        (PyTuple_GET_ITEM(args, 0) == Py_None))
    {
        return 1;
    }

    va_start(vargs, format);
    error = PyArg_VaParse(args, format, vargs);
    va_end(vargs);

    return error;
}

struct SqlTinyInt
{
    SqlType_HEAD;
    uint8_t uint8;
};

static const char s_SqlTinyInt_doc[] =
    "SqlTinyInt(value)\n"
    "\n"
    "SQL TINYINT type wrapper.\n"
    "\n"
    ":param int value: The integer value to wrap or :py:data:`None`.\n";

static int SqlTinyInt_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct SqlTinyInt* tinyint = (struct SqlTinyInt*)self;
    if (!SqlIntN_parse(args, "b", &tinyint->uint8))
    {
        return -1;
    }

    SqlType_init_fixed(tinyint, PyTuple_GET_ITEM(args, 0), TDSTINYINT, tinyint->uint8);
    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(TinyInt, s_SqlTinyInt_doc);

struct SqlSmallInt
{
    SqlType_HEAD;
    int16_t int16;
};


static const char s_SqlSmallInt_doc[] =
    "SqlSmallInt(value)\n"
    "\n"
    "SQL SMALLINT type wrapper.\n"
    "\n"
    ":param int value: The integer value to wrap or :py:data:`None`.\n";

static int SqlSmallInt_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct SqlSmallInt* smallint = (struct SqlSmallInt*)self;
    if (!SqlIntN_parse(args, "h", &smallint->int16))
    {
        return -1;
    }

    SqlType_init_fixed(smallint, PyTuple_GET_ITEM(args, 0), TDSSMALLINT, smallint->int16);
    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(SmallInt, s_SqlSmallInt_doc);

struct SqlInt
{
    SqlType_HEAD;
    int32_t int32;
};

static const char s_SqlInt_doc[] =
    "SqlInt(value)\n"
    "\n"
    "SQL INT type wrapper.\n"
    "\n"
    ":param int value: The integer value to wrap or :py:data:`None`.\n";

static int SqlInt_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct SqlInt* int_ = (struct SqlInt*)self;
    if (!SqlIntN_parse(args, "i", &int_->int32))
    {
        return -1;
    }

    SqlType_init_fixed(int_, PyTuple_GET_ITEM(args, 0), TDSINT, int_->int32);
    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(Int, s_SqlInt_doc);

struct SqlBigInt
{
    SqlType_HEAD;
    int64_t int64;
};

static const char s_SqlBigInt_doc[] =
    "SqlBigInt(value)\n"
    "\n"
    "SQL BIGINT type wrapper.\n"
    "\n"
    ":param int value: The integer value to wrap or :py:data:`None`.\n";

static int SqlBigInt_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct SqlBigInt* bigint = (struct SqlBigInt*)self;
    if (!SqlIntN_parse(args, "L", &bigint->int64))
    {
        return -1;
    }

    SqlType_init_fixed(bigint, PyTuple_GET_ITEM(args, 0), TDSBIGINT, bigint->int64);
    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(BigInt, s_SqlBigInt_doc);

struct SqlBinary
{
    SqlType_HEAD;
};

static const char s_SqlBinary_doc[] =
    "SqlBinary(value)\n"
    "\n"
    "SQL BINARY type wrapper.\n"
    "\n"
    ":param object value: The value to wrap or :py:data:`None`.\n";

static int SqlBinary_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* bytes = NULL;
    Py_ssize_t nbytes;
    if (!PyArg_ParseTuple(args, "z#", &bytes, &nbytes))
    {
        return -1;
    }

    SqlType_init_variable(self,
                          PyTuple_GET_ITEM(args, 0),
                          TDSBINARY,
                          (int)MAX(1, nbytes), /* BINARY type size must be >= 1 */
                          (void*)bytes,
                          (size_t)nbytes,
                          NULL);
    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(Binary, s_SqlBinary_doc);


struct SqlVarBinary
{
    SqlType_HEAD;
};

static const char s_SqlVarBinary_doc[] =
    "SqlVarBinary(value, size=None)\n"
    "\n"
    "SQL VARBINARY type wrapper.\n"
    "\n"
    ":param object value: The value to wrap or :py:data:`None`.\n"
    ":param int size: An optional size override. This value will be used for\n"
    "    the output parameter buffer size. It can also be used to truncate\n"
    "    the input parameter.\n";

static int SqlVarBinary_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* bytes = NULL;
    Py_ssize_t nbytes;
    Py_ssize_t size = (Py_ssize_t)-1;
    static char* s_kwlist[] =
    {
        "value",
        "size",
        NULL
    };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "z#|n", s_kwlist, &bytes, &nbytes, &size))
    {
        return -1;
    }

    SqlType_init_variable(self,
                          PyTuple_GET_ITEM(args, 0),
                          TDSVARBINARY,
                          /*
                              If the size is not explicitly specified, infer it from the value.
                              The VARBINARY type size must be >= 1.
                          */
                          (int)MAX(1, (((Py_ssize_t)-1 == size) ? nbytes : size)),
                          (void*)bytes,
                          (size_t)nbytes,
                          NULL);
    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(VarBinary, s_SqlVarBinary_doc);

struct SqlChar
{
    SqlType_HEAD;
};

static const char s_SqlChar_doc[] =
    "SqlChar(value)\n"
    "\n"
    "SQL CHAR type wrapper. The value's UTF-8-encoded length must be <= " STRINGIFY(TDS_CHAR_MAX_SIZE) ".\n"
    "\n"
    ":param object value: The value to wrap or :py:data:`None`.\n";

static int SqlChar_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    char* utf8bytes = NULL;
    Py_ssize_t nutf8bytes = 0;
    if (!PyArg_ParseTuple(args, "et#", "utf-8", &utf8bytes, &nutf8bytes))
    {
        PyObject* none;

        PyErr_Clear();

        if (!PyArg_ParseTuple(args, "O", &none))
        {
            return -1;
        }
        if (Py_None != none)
        {
            PyErr_SetObject(PyExc_TypeError, none);
            return -1;
        }
    }

    if (nutf8bytes > TDS_CHAR_MAX_SIZE)
    {
        PyMem_Free((void*)utf8bytes);
        PyErr_SetObject(PyExc_ValueError, PyTuple_GET_ITEM(args, 0));
        return -1;
    }

    SqlType_init_variable(self,
                          PyTuple_GET_ITEM(args, 0),
                          TDSCHAR,
                          (int)(int)MAX(1, nutf8bytes), /* CHAR type size must be >= 1 */
                          (void*)utf8bytes,
                          (size_t)nutf8bytes,
                          PyMem_Free);

    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(Char, s_SqlChar_doc);


struct SqlVarChar
{
    SqlType_HEAD;
};

static const char s_SqlVarChar_doc[] =
    "SqlVarChar(value, size=None)\n"
    "\n"
    "SQL VARCHAR type wrapper.\n"
    "\n"
    ".. note:: Byte strings are passed through unchanged to the database.\n"
    "\n"
    ":param object value: The value to wrap or :py:data:`None`.\n"
    ":param int size: An optional size override. This value will be used for\n"
    "    the output parameter buffer size. It can also be used to truncate\n"
    "    the input parameter.\n";

static int SqlVarChar_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    char* utf8bytes = NULL;
    Py_ssize_t nutf8bytes = 0;
    Py_ssize_t size = (Py_ssize_t)-1;
    static char* s_kwlist[] =
    {
        "value",
        "size",
        NULL
    };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "et#|n", s_kwlist, "utf-8", &utf8bytes, &nutf8bytes, &size))
    {
        PyObject* none;

        PyErr_Clear();

        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|n", s_kwlist, &none, &size))
        {
            return -1;
        }
        if (Py_None != none)
        {
            PyErr_SetObject(PyExc_TypeError, none);
            return -1;
        }
    }

    SqlType_init_variable(self,
                          PyTuple_GET_ITEM(args, 0),
                          (nutf8bytes > TDS_CHAR_MAX_SIZE) ? TDSTEXT : TDSVARCHAR,
                          /*
                              If the size is not explicitly specified, infer it from the value.
                              The VARCHAR type size must be >= 1.
                          */
                          (int)MAX(1, (((Py_ssize_t)-1 == size) ? nutf8bytes : size)),
                          (void*)utf8bytes,
                          (size_t)nutf8bytes,
                          PyMem_Free);
    return 0;
}

SQL_TYPE_DEF(VarChar, s_SqlVarChar_doc);


struct SqlNVarChar
{
    SqlType_HEAD;
};

static const char s_SqlNVarChar_doc[] =
    "SqlNVarChar(value, size=None)\n"
    "\n"
    "SQL NVARCHAR type wrapper.\n"
    "\n"
    ".. versionadded:: 1.1\n"
    "\n"
    ":param object value: The value to wrap or :py:data:`None`.\n"
    ":param int size: An optional size override. This value will be used for\n"
    "    the output parameter buffer size. It can also be used to truncate\n"
    "    the input parameter.\n";

static int SqlNVarChar_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    const char* utf8bytes = NULL;
    size_t nutf8bytes = 0;
    PyObject* encoded;
    enum TdsType tdstype;

    PyObject* unicode = NULL;
    size_t nchars = 0;
    Py_ssize_t size = (Py_ssize_t)-1;
    static char* s_kwlist[] =
    {
        "value",
        "size",
        NULL
    };
    /* Z# would be ideal here, but is not supported prior to Python 3. */
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|n", s_kwlist, &unicode, &size))
    {
        return -1;
    }

    if (Py_None == unicode)
    {
        /* `None` passed as argument. */
        encoded = PyTuple_GET_ITEM(args, 0);
        Py_INCREF(encoded);
        utf8bytes = NULL;
    }
    else if (PyUnicode_Check(unicode))
    {
        encoded = encode_for_dblib(unicode, &utf8bytes, &nutf8bytes, &nchars);
        if (!encoded)
        {
            return -1;
        }
    }
    else
    {
        PyErr_SetObject(PyExc_TypeError, unicode);
        return -1;
    }
    /*
        FreeTDS doesn't have good support for NCHAR types prior
        to 0.95. Fallback to VARCHAR with somewhat crippled
        functionality.
    */
#if defined(CTDS_USE_NCHARS)
    tdstype = (nchars > TDS_NCHAR_MAX_SIZE) ? TDSNTEXT : TDSNVARCHAR;
#else /* if defined(CTDS_USE_NCHARS) */
    tdstype = (nchars > TDS_CHAR_MAX_SIZE) ? TDSTEXT : TDSVARCHAR;
#endif /* else if defined(CTDS_USE_NCHARS) */

    SqlType_init_variable(self,
                          encoded,
                          tdstype,
                          /*
                              If the size is not explicitly specified, infer it from the value.
                              The NVARCHAR type size must be >= 1.
                          */
                          (int)MAX(1, (((Py_ssize_t)-1 == size) ? nchars : (size_t)size)),
                          (void*)utf8bytes,
                          nutf8bytes,
                          NULL);

    Py_DECREF(encoded);

    return 0;
}

SQL_TYPE_DEF(NVarChar, s_SqlNVarChar_doc);


struct SqlDate
{
    SqlType_HEAD;
#if defined(CTDS_HAVE_TDSTIME)
    DBDATETIMEALL dbdatetime;
#else /* if defined(CTDS_HAVE_TDSTIME) */
    DBDATETIME dbdatetime;
#endif /* else if defined(CTDS_HAVE_TDSTIME) */
};

static const char s_SqlDate_doc[] =
    "SqlDate(value)\n"
    "\n"
    "SQL DATE type wrapper.\n"
    "\n"
    ":param datetime.date value: The date value to wrap or :py:data:`None`.\n";

static int SqlDate_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct SqlDate* date = (struct SqlDate*)self;

    PyObject* value;
    if (!PyArg_ParseTuple(args, "O", &value))
    {
        return -1;
    }

    if (!PyDate_Check_(value) && (Py_None != value))
    {
        PyErr_SetObject(PyExc_TypeError, value);
        return -1;
    }

    if (PyDate_Check_(value))
    {
        enum TdsType unused = (enum TdsType)-1;
        DBINT result = datetime_to_sql(value,
                                       &unused,
                                       &date->dbdatetime,
                                       sizeof(date->dbdatetime));
        if (-1 == result)
        {
            PyErr_SetObject(PyExc_ValueError, value);
            return -1;
        }
    }

    SqlType_init_fixed(self,
                       value,
                       TDSDATE,
                       date->dbdatetime);

    return 0;
    UNUSED(kwargs);
}

SQL_TYPE_DEF(Date, s_SqlDate_doc);

struct SqlDecimal
{
    SqlType_HEAD;
    DBDECIMAL dbdecimal;
};


#define DECIMAL_DEFAULT_PRECISION 18
#define DECIMAL_DEFAULT_SCALE 0

static const char s_SqlDecimal_doc[] =
    "SqlDecimal(value, precision=" STRINGIFY(DECIMAL_DEFAULT_PRECISION) ", scale=" STRINGIFY(DECIMAL_DEFAULT_SCALE) ")\n"
    "\n"
    "SQL DECIMAL type wrapper.\n"
    "\n"
    ":param object value: The value to wrap or :py:data:`None`.\n"
    ":param int precision: The maximum number of total digits stored.\n"
    "    This must be between 1 and 38.\n"
    ":param int scale: The maximum number of digits stored to the right\n"
    "    of the decimal point. 0 <= `scale` <= `precision`.\n";

static int SqlDecimal_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct SqlDecimal* decimal = (struct SqlDecimal*)self;

    DBINT size;
    PyObject* value;
    Py_ssize_t precision = DECIMAL_DEFAULT_PRECISION;
    Py_ssize_t scale = DECIMAL_DEFAULT_SCALE;
    static char* s_kwlist[] =
    {
        "value",
        "precision",
        "scale",
        NULL
    };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|nn", s_kwlist, &value, &precision, &scale))
    {
        return -1;
    }
    if (!((1 <= precision) && (precision <= DECIMAL_MAX_PRECISION)))
    {
        PyErr_Format(PyExc_ValueError, "invalid precision: %ld", precision);
        return -1;
    }
    if (!((0 <= scale) && (scale <= precision)))
    {
        PyErr_Format(PyExc_ValueError, "invalid scale: %ld", scale);
        return -1;
    }

    if (Py_None != value)
    {
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

            dbtypeinfo.precision = (DBINT)precision;
            dbtypeinfo.scale = (DBINT)scale;

            size = dbconvert_ps(NULL,
                                TDSCHAR,
                                (BYTE*)str,
                                (DBINT)nutf8,
                                TDSDECIMAL,
                                (BYTE*)&decimal->dbdecimal,
                                sizeof(decimal->dbdecimal),
                                &dbtypeinfo);
            if (-1 == size)
            {
                PyErr_Format(PyExc_RuntimeError, "failed to convert '%s'", str);
            }
            Py_DECREF(ostr);
        }

        if (PyErr_Occurred())
        {
            return -1;
        }
    }
    else
    {
        decimal->dbdecimal.precision = (BYTE)precision;
        decimal->dbdecimal.scale = (BYTE)scale;
    }

    SqlType_init_fixed(self,
                       value,
                       TDSDECIMAL,
                       decimal->dbdecimal);

    return 0;
}

SQL_TYPE_DEF(Decimal, s_SqlDecimal_doc);


/*
    In order to properly use `dbdatecrack` without a DBPROCESS* handle,
    it must be determined if freetds was comiled with MSDBLIB defined.
    This will control the value for month returned by `dbdatecrack`.
*/
static bool s_freetds_msdblib = false;

int SqlTypes_init(void)
{
    do
    {
        if (0 != PyType_Ready(&SqlTypeType)) break;
        {
            DBDATEREC dbdaterec;
            DBDATETIME dbdatetime = { 0, 0 };
            (void)dbdatecrack(NULL, &dbdaterec, &dbdatetime);
            /* If compiled with MSDBLIB defined, the quarter value will be non-zero. */
            s_freetds_msdblib = (0 != dbdaterec.quarter);
        }

        return 0;
    } while (0);
    return -1;
}

static PyObject* SQLCHAR_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    if (!data) Py_RETURN_NONE;

    return PyUnicode_DecodeUTF8((const char*)data, (Py_ssize_t)ndata, "strict");

    UNUSED(tdstype);
}

static PyObject* SQLINT_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    PY_LONG_LONG value;
    if (!ndata) Py_RETURN_NONE;

    switch (tdstype)
    {
        case TDSTINYINT:
        {
            assert(1 == ndata);

            /* TDSTINYINT type is unsigned. */
            return PyLong_FromUnsignedLong(*((uint8_t*)data));
        }
        case TDSSMALLINT:
        {
            assert(2 == ndata);
            value = *((int16_t*)data);
            break;
        }
        case TDSINT:
        {
            assert(4 == ndata);
            value = *((int32_t*)data);
            break;
        }
        case TDSBIGINT:
        {
            assert(8 == ndata);
            value = *((int64_t*)data);
            break;
        }
        default:
        {
            PyErr_Format(PyExc_RuntimeError, "unsupported integer width %ld", ndata);
            return NULL;
        }
    }

    return PyLong_FromLongLong(value);
    UNUSED(tdstype);
}

static PyObject* SQLBIT_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    long l = 0;
    if (!ndata) Py_RETURN_NONE;

    memcpy(&l, data, ndata);
    return PyBool_FromLong(l);
    UNUSED(tdstype);
}

static PyObject* SQLBINARY_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    if (!ndata) Py_RETURN_NONE;

    return PyBytes_FromStringAndSize((const char*)data, (Py_ssize_t)ndata);
    UNUSED(tdstype);
}

static PyObject* FLOAT_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    if (!ndata) Py_RETURN_NONE;

    return PyFloat_FromDouble((8 == ndata) ? *((double*)data) : *((float*)data));
    UNUSED(tdstype);
}

static PyObject* NUMERIC_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    char buffer[100];
    DBINT size;

    if (!ndata) Py_RETURN_NONE;


    size = dbconvert(NULL,
                     tdstype,
                     data,
                     (DBINT)ndata,
                     SYBCHAR,
                     (BYTE*)buffer,
                     (DBINT)sizeof(buffer));
    if (-1 == size)
    {
        PyErr_Format(PyExc_RuntimeError, "failed to convert NUMERIC to string");
        return NULL;
    }
    return PyDecimal_FromString(buffer, (Py_ssize_t)size);
}

static PyObject* MONEY_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    /*
        Note: dbconvert incorrectly rounds to the nearest hundredth of the monetary unit.
        MSDN indicates that MONEY types are to the nearest ten-thousandth of the monetary
        unit. See https://msdn.microsoft.com/en-us/library/ms179882.aspx.

        To work around this, convert to NUMERIC first with the appropriate scale.
    */

    DBNUMERIC dbnumeric;
    DBTYPEINFO dbtypeinfo;
    DBINT size;
    if (!ndata) Py_RETURN_NONE;

    dbtypeinfo.precision = 38 /* maximum */;
    dbtypeinfo.scale = 4 /* nearest hundredth */;
    size = dbconvert_ps(NULL,
                        tdstype,
                        (BYTE*)data,
                        (DBINT)ndata,
                        SYBNUMERIC,
                        (BYTE*)&dbnumeric,
                        (DBINT)sizeof(dbnumeric),
                        &dbtypeinfo);
    if (-1 == size)
    {
        PyErr_Format(PyExc_RuntimeError, "failed to convert NUMERIC to MONEY");
        return NULL;
    }
    return NUMERIC_topython(TDSNUMERIC,
                            (const uint8_t*)&dbnumeric,
                            (size_t)size);
}

static PyObject* DATETIME_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    DBDATETIME dbdatetime;
    if (!ndata) Py_RETURN_NONE;

    switch (tdstype)
    {
        default:
        {
            DBINT size = dbconvert(NULL,
                                   tdstype,
                                   data,
                                   (DBINT)ndata,
                                   SYBDATETIME,
                                   (BYTE*)&dbdatetime, -1);
            if (-1 == size)
            {
                PyErr_Format(PyExc_RuntimeError, "failed to convert DATETIME");
                return NULL;
            }

            ndata = (size_t)size;
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */
            data = (const uint8_t*)&dbdatetime;
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif
            /* Intentional fall-through. */
        }
#if defined(CTDS_HAVE_TDSTIME)
        case TDSDATE:
        case TDSTIME:
        case TDSDATETIME2:
        case TDSSMALLDATETIME:
#endif /* if defined(CTDS_HAVE_TDSTIME) */
        case TDSDATETIME:
        case TDSDATETIMEN:
        {
            int usecond;
#if defined(CTDS_HAVE_TDSTIME)
            DBDATEREC2 dbdaterec;
            (void)dbanydatecrack(NULL, &dbdaterec, tdstype, data);

            usecond = dbdaterec.nanosecond / 1000;
#else /* if defined(CTDS_HAVE_TDSTIME) */
            DBDATEREC dbdaterec;
            (void)dbdatecrack(NULL, &dbdaterec, (DBDATETIME*)data);
            usecond = dbdaterec.millisecond * 1000;
#endif /* else if defined(CTDS_HAVE_TDSTIME) */

            /*
                If freetds was not compiled with MSDBLIB defined, the month,
                quarter, day of week are 0-based values.
            */
            if (!s_freetds_msdblib)
            {
                dbdaterec.quarter++;
                dbdaterec.month++;
                dbdaterec.weekday++;
            }
            switch (tdstype)
            {
                case TDSDATE:
                {
                    return PyDate_FromDate_(dbdaterec.year,
                                            dbdaterec.month,
                                            dbdaterec.day);
                }
                case TDSTIME:
                {
                    return PyTime_FromTime_(dbdaterec.hour,
                                            dbdaterec.minute,
                                            dbdaterec.second,
                                            usecond);
                }
                default:
                {
                    return PyDateTime_FromDateAndTime_(dbdaterec.year,
                                                       dbdaterec.month,
                                                       dbdaterec.day,
                                                       dbdaterec.hour,
                                                       dbdaterec.minute,
                                                       dbdaterec.second,
                                                       usecond);
                }
            }
            break;
        }
    }
}

static PyObject* GUID_topython(enum TdsType tdstype, const void* data, size_t ndata)
{
    if (!ndata) Py_RETURN_NONE;

    return PyUuid_FromBytes((const char*)data, (Py_ssize_t)ndata);
    UNUSED(tdstype);
}


static const struct {
    enum TdsType tdstype;
    sql_topython topython;
} s_tdstypes[] = {
    { TDSCHAR,          SQLCHAR_topython },
    { TDSVARCHAR,       SQLCHAR_topython },
    { TDSTEXT,          SQLCHAR_topython },

    /* Map the XML type to Python string. */
    { TDSXML,           SQLCHAR_topython },

    { TDSBINARY,        SQLBINARY_topython },
    { TDSVARBINARY,     SQLBINARY_topython },
    { TDSIMAGE,         SQLBINARY_topython },

    { TDSBIT,           SQLBIT_topython },
    { TDSBITN,          SQLBIT_topython },
    { TDSINTN,          SQLINT_topython },
    { TDSTINYINT,       SQLINT_topython },
    { TDSSMALLINT,      SQLINT_topython },
    { TDSINT,           SQLINT_topython },
    { TDSBIGINT,        SQLINT_topython },

    { TDSFLOAT,         FLOAT_topython },
    { TDSFLOATN,        FLOAT_topython },
    { TDSREAL,          FLOAT_topython },

    { TDSSMALLMONEY,    MONEY_topython },
    { TDSMONEY,         MONEY_topython },
    { TDSMONEYN,        MONEY_topython },

    { TDSDECIMAL,       NUMERIC_topython },
    { TDSNUMERIC,       NUMERIC_topython },

    { TDSDATE,          DATETIME_topython },
    { TDSDATETIME,      DATETIME_topython },
    { TDSDATETIME2,     DATETIME_topython },
    { TDSDATETIMEN,     DATETIME_topython },
    { TDSSMALLDATETIME, DATETIME_topython },
    { TDSTIME,          DATETIME_topython },

    { TDSGUID,          GUID_topython },
    { TDSVOID,          NULL },
};


sql_topython sql_topython_lookup(enum TdsType tdstype)
{
    size_t ix;
    for (ix = 0; ix < ARRAYSIZE(s_tdstypes); ++ix)
    {
        if (tdstype == s_tdstypes[ix].tdstype)
        {
            return s_tdstypes[ix].topython;
        }
    }
    return NULL;
}

#if !defined(CTDS_USE_UTF16)

#ifdef _MSC_VER
#  include <wchar.h>
#  define __WCHAR_MAX__ WCHAR_MAX
#endif

#if __WCHAR_MAX__ > 0xFFFF
#  define WCHAR_T_UCS4 1
#endif

static PyObject* translate_to_ucs2(PyObject* o)
{
    PyObject* translated = NULL;
    Py_ssize_t len;
    wchar_t* unicode;

    assert(PyUnicode_Check(o));
#if PY_MAJOR_VERSION < 3
    len = PyUnicode_GetSize(o);
    do
    {
        unicode = tds_mem_malloc((size_t)len * sizeof(wchar_t));
        if (!unicode)
        {
            PyErr_NoMemory();
            break;
        }
        if (-1 == PyUnicode_AsWideChar((PyUnicodeObject*)o, unicode, len))
        {
            break;
        }
    }
    while (0);
#else /* if PY_MAJOR_VERSION < 3 */
    unicode = PyUnicode_AsWideCharString(o, &len);
#endif /* else if PY_MAJOR_VERSION < 3 */

    if (!PyErr_Occurred())
    {
        Py_ssize_t ixsrc, ixdst = 0;
        for (ixsrc = 0; ixsrc < len; ++ixsrc, ++ixdst)
        {
#if defined(WCHAR_T_UCS4)
            if (0xFFFF < unicode[ixsrc])
#else /* if defined(WCHAR_T_UCS4) */
            if (Py_UNICODE_IS_SURROGATE(unicode[ixsrc]))
#endif /* else if defined(WCHAR_T_UCS4) */
            {
                static const char s_fmt[] =
                    "Unicode codepoint U+%08X is not representable in UCS-2; replaced with U+FFFD";
                char buffer[ARRAYSIZE(s_fmt) + 8 /* for codepoint chars */];
#if defined(WCHAR_T_UCS4)
                uint32_t codepoint = (uint32_t)unicode[ixsrc];
#else /* if defined(WCHAR_T_UCS4) */
                uint32_t codepoint;
                assert(((ixsrc + 1) < len) && Py_UNICODE_IS_SURROGATE(unicode[ixsrc + 1]));
                codepoint = Py_UNICODE_JOIN_SURROGATES(unicode[ixsrc], unicode[ixsrc + 1]);
                ++ixsrc;
#endif /* else if defined(WCHAR_T_UCS4) */
                (void)sprintf(buffer, s_fmt, codepoint);
                if (0 != PyErr_WarnEx(PyExc_UnicodeWarning, buffer, 1))
                {
                    break;
                }

                unicode[ixdst] = 0xFFFD; /* unicode replacement character */
            }
#if !defined(WCHAR_T_UCS4)
            else
            {
                unicode[ixdst] = unicode[ixsrc];
            }
#endif /* if !defined(WCHAR_T_UCS4) */
        }

        if (!PyErr_Occurred())
        {
            translated = PyUnicode_FromWideChar(unicode, ixdst);
        }
    }
#if PY_MAJOR_VERSION < 3
    tds_mem_free(unicode);
#else /* if PY_MAJOR_VERSION < 3 */
    PyMem_Free(unicode);
#endif /* else if PY_MAJOR_VERSION < 3 */

    return translated;
}

#endif /* if !defined(CTDS_USE_UTF16) */

PyObject* encode_for_dblib(PyObject* unicode, const char** utf8bytes, size_t* nutf8bytes, size_t* width)
{
    PyObject* encoded = NULL;

    do
    {
#if PY_MAJOR_VERSION >= 3
        Py_ssize_t size;
#endif /* if PY_MAJOR_VERSION >= 3 */

        PyObject* encodable;

#if defined(CTDS_USE_UTF16)
        size_t ix;

        /* FreeTDS supports encoding to UTF-16, so the whole string is encodable. */
        encodable = unicode;
        Py_INCREF(encodable);
#else /* if defined(CTDS_USE_UTF16) */
        /*
            FreeTDS will only convert strings to UCS-2, so translate all
            strings to UCS-2 prior to binding.
        */
        encodable = translate_to_ucs2(unicode);
        if (!encodable)
        {
            break;
        }
        /*
            If the string was translated to UCS-2, the SQL NCHAR width is simply
            the string length.
        */
#  if PY_VERSION_HEX >= 0x03030000
        *width = (size_t)PyUnicode_GET_LENGTH(encodable);
#  else
        *width = (size_t)PyUnicode_GET_SIZE(encodable);
#  endif
#endif /* else if defined(CTDS_USE_UTF16) */

#if PY_MAJOR_VERSION < 3
        encoded = PyUnicode_AsUTF8String(encodable);
        Py_DECREF(encodable);
        if (!encoded)
        {
            break;
        }

        *utf8bytes = PyString_AS_STRING(encoded);
        *nutf8bytes = (size_t)PyString_GET_SIZE(encoded);
#else /* if PY_MAJOR_VERSION < 3 */
        encoded = encodable; /* steal reference */

        *utf8bytes = PyUnicode_AsUTF8AndSize(encoded, &size);
        *nutf8bytes = (size_t)size;
#endif /* else if PY_MAJOR_VERSION < 3 */

#if defined(CTDS_USE_UTF16)
        /*
            Compute the SQL type width, which is really the number of UTF-16
            sequences.
        */
#define IS_UTF8_SINGLE_BYTE(b)       (((b) & 0x80) == 0)
#define IS_UTF8_CONTINUATION_BYTE(b) (((b) & 0xC0) == 0x80)
#define IS_UTF8_FIRST_BYTE_OF_2(b)   (((b) & 0xE0) == 0xC0)
#define IS_UTF8_FIRST_BYTE_OF_3(b)   (((b) & 0xF0) == 0xE0)
#define IS_UTF8_FIRST_BYTE_OF_4(b)   (((b) & 0xF8) == 0xF0)

        *width = 0;
        for (ix = 0; ix < *nutf8bytes;)
        {
            if (IS_UTF8_SINGLE_BYTE((*utf8bytes)[ix]))
            {
                ++(*width);
                ix += 1;
            }
            else if (IS_UTF8_FIRST_BYTE_OF_2((*utf8bytes)[ix]))
            {
                ++(*width);
                ix += 2;
            }
            else if (IS_UTF8_FIRST_BYTE_OF_3((*utf8bytes)[ix]))
            {
                ++(*width);
                ix += 3;
            }
            else
            {
                /* Two-byte UTF-16 sequences require double the width. */
                assert(IS_UTF8_FIRST_BYTE_OF_4((*utf8bytes)[ix]));
                (*width) += 2;
                ix += 4;
            }
        }
#endif /* if defined(CTDS_USE_UTF16) */

    }
    while (0);

    return encoded;
}

int datetime_to_sql(PyObject* o, enum TdsType* tdstype, void* converted, size_t cbconverted)
{
    int written = 0;
    /* Python only supports microsecond precision. */
    char buffer[ARRAYSIZE("YYYY-MM-DD HH:MM:SS.nnnnnn")];

    /*
        The best _supported_ TDS type. Default to DATETIME which is widely
        supported across TDS and FreeTDS versions.
    */
    *tdstype = TDSDATETIME;

    if (PyDate_Check_(o))
    {
        written += sprintf(&buffer[written],
                           "%04d-%02d-%02d",
                           PyDateTime_GET_YEAR_(o),
                           PyDateTime_GET_MONTH_(o),
                           PyDateTime_GET_DAY_(o));
    }
    if (PyDateTime_Check_(o))
    {
        written += sprintf(&buffer[written], " ");
    }
    if (PyTime_Check_(o) || PyDateTime_Check_(o))
    {
        int hours = (PyDateTime_Check_(o)) ?
            PyDateTime_DATE_GET_HOUR_(o) : PyDateTime_TIME_GET_HOUR_(o);
        int minutes = (PyDateTime_Check_(o)) ?
            PyDateTime_DATE_GET_MINUTE_(o) : PyDateTime_TIME_GET_MINUTE_(o);
        int seconds = (PyDateTime_Check_(o)) ?
            PyDateTime_DATE_GET_SECOND_(o) : PyDateTime_TIME_GET_SECOND_(o);
        int useconds = (PyDateTime_Check_(o)) ?
            PyDateTime_DATE_GET_MICROSECOND_(o) : PyDateTime_TIME_GET_MICROSECOND_(o);

        written += sprintf(&buffer[written],
                           "%02d:%02d:%02d",
                           hours,
                           minutes,
                           seconds);

        if (useconds)
        {
#if defined(CTDS_HAVE_TDSTIME)
            written += sprintf(&buffer[written], ".%06d", useconds);
            /* Always use DATETIME2 to preserve fractional second precision. */
            *tdstype = (PyDateTime_Check_(o)) ? TDSDATETIME2 : TDSTIME;
#else /* if defined(CTDS_HAVE_TDSTIME) */
            /*
                For compatibility with the MS SQL DATETIME type, only include
                microsecond granularity.
            */
            written += sprintf(&buffer[written], ".%03d", useconds / 1000);
            *tdstype = TDSDATETIME;
#endif /* else if defined(CTDS_HAVE_TDSTIME) */
        }
#if defined(CTDS_HAVE_TDSTIME)
        else
        {
            *tdstype = (PyDateTime_Check_(o)) ? TDSDATETIME : TDSTIME;
        }
#endif /* if defined(CTDS_HAVE_TDSTIME) */
    }
    return (int)dbconvert(NULL,
                          TDSCHAR,
                          (const BYTE*)buffer,
                          (DBINT)written,
                          *tdstype,
                          (BYTE*)converted,
                          (DBINT)cbconverted);
}
