#include "include/push_warnings.h"
#include <Python.h>
#include <sybdb.h>
#include "include/pop_warnings.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "include/cursor.h"
#include "include/connection.h"
#include "include/macros.h"
#include "include/parameter.h"
#include "include/pyutils.h"
#include "include/tds.h"
#include "include/type.h"

#ifdef __GNUC__
/*
    Ignore "string length '%d' is greater than the length '509' ISO C90
    compilers are required to support [-Werror=overlength-strings]".
*/
#  pragma GCC diagnostic ignored "-Woverlength-strings"

/* Ignore "ISO C90 does not support 'long long' [-Werror=long-long]". */
#  pragma GCC diagnostic ignored "-Wlong-long"
#endif /* ifdef __GNUC__ */


struct Column {
    DBCOL dbcol;

    sql_topython topython;
};

/* A description of the columns in a result set. */
struct ResultSetDescription {

    size_t _refs;

    /*
        Number of columns in the current resultset. This will be 0 if there
        is no current result set.
    */
    size_t ncolumns;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    /* A dynamically-sized array of columns, of length `ncolumns`. */
    struct Column columns[];
#pragma GCC diagnostic pop
};

#define ResultSetDescription_size(_ncolumns) \
    (offsetof(struct ResultSetDescription, columns) + (sizeof(struct Column) * (_ncolumns)))

struct ResultSetDescription* ResultSetDescription_create(size_t ncolumns)
{
    struct ResultSetDescription* description =
        (struct ResultSetDescription*)tds_mem_malloc(ResultSetDescription_size(ncolumns));
    if (description)
    {
        description->_refs = 1;
        description->ncolumns = ncolumns;
        memset(description->columns, 0, sizeof(struct Column) * ncolumns);
    }
    return description;
}

#define ResultSetDescription_increment(_description) \
    (_description)->_refs++

static void ResultSetDescription_decrement(struct ResultSetDescription* description)
{
    description->_refs--;
    if (0 == description->_refs)
    {
        tds_mem_free(description);
    }
}


struct Cursor {
    PyObject_VAR_HEAD

    /*
        The underlying database connection used by this cursor.
        The cursor holds a reference to the connection while open.
        Closing the cursor (via close() or __del__()) releases the
        reference and effectively makes the cursor useless.
    */
    struct Connection* connection;

    /*
        The batch size for the fetchmany operation.
        See https://www.python.org/dev/peps/pep-0249/#arraysize.
    */
    unsigned PY_LONG_LONG arraysize;

    /* A description of the current result set columns. */
    struct ResultSetDescription* description;

    /* The number of rows read from the current result set. */
    size_t rowsread;
};

#define warn_extension_used(_method) \
    PyErr_Warn(PyExc_tds_Warning, "DB-API extension " _method " used")

#define Cursor_verify_open(_cursor) \
    if (!(_cursor)->connection) \
    { \
        PyErr_Format(PyExc_tds_InterfaceError, "cursor closed"); \
        return NULL; \
    }

#define Cursor_verify_connection_open(_cursor) \
    if (!Connection_DBPROCESS((_cursor)->connection)) \
    { \
        Connection_raise_closed((_cursor)->connection); \
        return NULL; \
    }

/*
    Close a cursor's connection.

    This method can be called multiple times on the same cursor.
*/
static void Cursor_close_connection(struct Cursor* cursor)
{
    if (cursor->description)
    {
        ResultSetDescription_decrement(cursor->description);
        cursor->description = NULL;
    }
    Py_XDECREF(cursor->connection);
    cursor->connection = NULL;
}

/*
    Load the next resultset column data.

    @note This method does not manipulate the GIL. Callers should release
        the GIL when calling this method.

    @param cursor [in] The cursor object.
    @param retcode [out] The dblib result. If this is not FAIL on a failure,
        consult errno for the system failure.

    @return 0 if the call succeeded, -1 if it failed.
*/
static int Cursor_next_resultset(struct Cursor* cursor, RETCODE* retcode)
{
    DBINT ncolumns;
    DBINT column;
    DBPROCESS* dbproc = Connection_DBPROCESS(cursor->connection);

    if (cursor->description)
    {
        ResultSetDescription_decrement(cursor->description);
        cursor->description = NULL;
    }

    cursor->rowsread = 0;

    /* Read any unprocessed rows from the database. */
    while (dbnextrow(dbproc) != NO_MORE_ROWS) {}

    /*
        dbresults() sometimes returns SUCCEED and dbnumcols() returns 0.
        In this case, keep looking for the next resultset.
    */
    for (ncolumns = 0; 0 == ncolumns; ncolumns = dbnumcols(dbproc))
    {
        *retcode = dbresults(dbproc);
        if (SUCCEED != *retcode /* retcode may be NO_MORE_RESULTS */)
        {
            return (NO_MORE_RESULTS == *retcode) ? 0 : -1;
        }
    }

    cursor->description = ResultSetDescription_create((size_t)ncolumns);
    if (!cursor->description)
    {
        return -1;
    }

    for (column = 1; column <= ncolumns; ++column)
    {
        *retcode = dbcolinfo(dbproc, CI_REGULAR, column, 0 /* ignored by dblib */,
                             &cursor->description->columns[column - 1].dbcol);
        if (FAIL == *retcode)
        {
            break;
        }

        cursor->description->columns[column - 1].topython =
            sql_topython_lookup((enum TdsType)cursor->description->columns[column - 1].dbcol.Type);
    }

    if (FAIL == *retcode)
    {
        ResultSetDescription_decrement(cursor->description);
        cursor->description = NULL;
        return -1;
    }

    return 0;
}


/*
   Python tds.Cursor type definition.
*/
PyTypeObject CursorType;

static const char s_tds_Cursor_doc[] =
    "A database cursor used to manage the context of a fetch operation.\n"
    "\n"
    ":pep:`0249#cursor-objects`\n";

static void Cursor_dealloc(PyObject* self)
{
    Cursor_close_connection((struct Cursor*)self);
    PyObject_Del(self);
}

/*
    tds.Cursor attributes
*/

static const char s_Cursor_arraysize_doc[] =
    "The number of rows to fetch at a time with :py:meth:`.fetchmany`.\n"
    "\n"
    ":pep:`0249#arraysize`\n";

static PyObject* Cursor_arraysize_get(PyObject* self, void* closure)
{
    struct Cursor* cursor = (struct Cursor*)self;

    return PyLong_FromUnsignedLongLong(cursor->arraysize);

    UNUSED(closure);
}

static int Cursor_arraysize_set(PyObject* self, PyObject* value, void* closure)
{
    unsigned PY_LONG_LONG arraysize;
    struct Cursor* cursor = (struct Cursor*)self;
    if (
        (
#if PY_MAJOR_VERSION < 3
        !PyInt_Check(value) &&
#endif /* if PY_MAJOR_VERSION < 3 */
        !PyLong_Check(value)
        ) || PyBool_Check(value))
    {
        PyErr_SetString(PyExc_TypeError, "arraysize");
        return -1;
    }
#if PY_MAJOR_VERSION < 3
    if (PyInt_Check(value))
    {
        /* Note: This does not check for overflow. */
        arraysize = PyInt_AsUnsignedLongLongMask(value);
    }
#endif /* if PY_MAJOR_VERSION < 3 */
    else
    {
        arraysize = PyLong_AsUnsignedLongLong(value);
    }
    if (PyErr_Occurred())
    {
        return -1;
    }
    cursor->arraysize = arraysize;
    return 0;

    UNUSED(closure);
}

static const char s_Cursor_description_doc[] =
    "A description of the current result set columns.\n"
    "The description is a sequence of tuples, one tuple per column in the\n"
    "result set. The tuple describes the column data as follows:\n"
    "\n"
    "+---------------+--------------------------------------+\n"
    "| name          | The name of the column, if provided. |\n"
    "+---------------+--------------------------------------+\n"
    "| type_code     | The specific type of the column.     |\n"
    "+---------------+--------------------------------------+\n"
    "| display_size  | The SQL type size of the column.     |\n"
    "+---------------+--------------------------------------+\n"
    "| internal_size | The client size of the column.       |\n"
    "+---------------+--------------------------------------+\n"
    "| precision     | The precision of NUMERIC and DECIMAL |\n"
    "|               | columns.                             |\n"
    "+---------------+--------------------------------------+\n"
    "| scale         | The scale of NUMERIC and DECIMAL     |\n"
    "|               | columns.                             |\n"
    "+---------------+--------------------------------------+\n"
    "| null_ok       | Whether the column allows NULL.      |\n"
    "+---------------+--------------------------------------+\n"
    "\n"
    ":pep:`0249#description`\n"
    "\n"
    ":return: A sequence of tuples or None if no results are available.\n"
    ":rtype: tuple\n";

#if PY_MAJOR_VERSION >= 3

static PyTypeObject DescriptionType;

static int Description_init(void)
{
    static PyStructSequence_Field s_fields[] =
    {
        /* name, doc */
        { "name", NULL },
        { "type_code", NULL },
        { "display_size", NULL },
        { "internal_size", NULL },
        { "precision", NULL },
        { "scale", NULL },
        { "null_ok", NULL },
        { NULL, NULL }
    };
    static PyStructSequence_Desc s_desc = {
        "description",                   /* name */
        (char*)s_Cursor_description_doc, /* doc */
        s_fields,                        /* fields */
        ARRAYSIZE(s_fields)  - 1         /* n_in_sequence */
    };

#  if PY_VERSION_HEX >= 0x03040000
    return PyStructSequence_InitType2(&DescriptionType, &s_desc);
#  else
    PyStructSequence_InitType(&DescriptionType, &s_desc);
    return 0;
#  endif /* if PY_VERSION_HEX >= 0x03040000 */

#  define Description_New() PyStructSequence_New(&DescriptionType)
#  define Description_SET_ITEM PyStructSequence_SET_ITEM

}
#else /* if PY_MAJOR_VERSION < 3 */

#  define Description_init() 0
#  define Description_New() PyTuple_New(7)
#  define Description_SET_ITEM PyTuple_SET_ITEM

#endif /* else if PY_MAJOR_VERSION >= 3 */

/*
  Construct the 7-item sequence, as descibed by:
  https://www.python.org/dev/peps/pep-0249/#description.

    * name
    * type_code
    * display_size
    * internal_size
    * precision
    * scale
    * null_ok
*/
static PyObject* create_column_description(const struct Column* column)
{
    PyObject* tuple = Description_New();
    if (tuple)
    {
        do
        {
            PyObject* item;
            Py_ssize_t ix = 0;

            /* name */
            item = PyUnicode_DecodeUTF8(column->dbcol.ActualName,
                                        (Py_ssize_t)strlen(column->dbcol.ActualName),
                                        "strict");
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by Description_SET_ITEM */

            /* type_code */
            item = PyLong_FromLong(column->dbcol.Type);
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by Description_SET_ITEM */

            /* $TODO: figure this out: freetds seems to return 0x7fffffff or 0x3fffffff for max size */
#define IS_COLUMN_MAX(_len) ((INT32_MAX == (_len)) || ((INT32_MAX >> 1) == (_len)))

            /* display_size */
            item = PyLong_FromLongLong(IS_COLUMN_MAX(column->dbcol.MaxLength) ? -1 : (PY_LONG_LONG)column->dbcol.MaxLength);
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by Description_SET_ITEM */

            /* internal_size */
            /* For now, just use the display_size value for this. */
            item = PyLong_FromLongLong(IS_COLUMN_MAX(column->dbcol.MaxLength) ? -1 : (PY_LONG_LONG)column->dbcol.MaxLength);
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by Description_SET_ITEM */

            /* precision */
            item = PyLong_FromUnsignedLong((unsigned long)column->dbcol.Precision);
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by PyTuple_SET_ITEM */

            /* scale */
            item = PyLong_FromUnsignedLong((unsigned long)column->dbcol.Scale);
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by Description_SET_ITEM */

            /* null_ok */
            item = PyBool_FromLong((long)column->dbcol.Null);
            if (!item)
            {
                break;
            }
            Description_SET_ITEM(tuple, ix++, item); /* item reference stolen by Description_SET_ITEM */

            return tuple;
        } while (0);

        Py_DECREF(tuple);
    }
    PyErr_NoMemory();
    return NULL;
}

static PyObject* Cursor_description_get(PyObject* self, void* closure)
{
    PyObject* description = NULL;

    struct Cursor* cursor = (struct Cursor*)self;

    /*
        Return None if the resultset is empty (i.e. has no columns).
        This will also cover cases where no operation has occurred yet.
    */
    if (!cursor->description)
    {
        Py_RETURN_NONE;
    }

    description = PyTuple_New((Py_ssize_t)cursor->description->ncolumns);
    if (description)
    {
        bool error = false;

        size_t ix;
        for (ix = 0; ix < cursor->description->ncolumns; ++ix)
        {
            PyObject* column = create_column_description(&cursor->description->columns[ix]);
            if (column)
            {
                PyTuple_SET_ITEM(description, (Py_ssize_t)ix, column); /* column reference stolen by PyTuple_SET_ITEM */
            }
            else
            {
                error = true;
                break;
            }
        }

        if (error)
        {
            Py_DECREF(description);
            description = NULL;
        }
    }
    else
    {
        PyErr_NoMemory();
    }
    return description;

    UNUSED(closure);
}

static const char s_Cursor_rowcount_doc[] =
    "The number of rows that the last :py:meth:`.execute` produced or affected.\n"
    "\n"
    ".. note::\n"
    "\n"
    "    This will currently always return -1.\n"
    "\n"
    ":pep:`0249#rowcount`\n"
    "\n"
    ":rtype: int\n";

static PyObject* Cursor_rowcount_get(PyObject* self, void* closure)
{
    struct Cursor* cursor = (struct Cursor*)self;
    Cursor_verify_open(cursor);

    /*
        Note: the affected row count is always -1 after an RPC call.
        Because .execute*() is implemented using `sp_executesql`, this
        value will always be -1.
    */
    return PyLong_FromLong(dbcount(Connection_DBPROCESS(cursor->connection)));

    UNUSED(closure);
}

static const char s_Cursor_connection_doc[] =
    "A reference to the Connection object on which the cursor was created.\n"
    "\n"
    ":pep:`0249#id28`\n"
    "\n"
    ":return: The :py:class:`ctds.Connection` object which created this cursor.\n"
    ":rtype: ctds.Connection\n";

static PyObject* Cursor_connection_get(PyObject* self, void* closure)
{
    struct Cursor* cursor = (struct Cursor*)self;

    (void)warn_extension_used("cursor.connection");

    Cursor_verify_open(cursor);

    Py_INCREF(cursor->connection);
    return (PyObject*)cursor->connection;

    UNUSED(closure);
}

static const char s_Cursor_rownumber_doc[] =
    "The current 0-based index of the cursor in the result set or None if the\n"
    "index cannot be determined.\n"
    "\n"
    ":pep:`0249#rownumber`\n"
    "\n"
    ":return: None if no results are available.\n"
    ":rtype: int\n";

static PyObject* Cursor_rownumber_get(PyObject* self, void* closure)
{
    struct Cursor* cursor = (struct Cursor*)self;

    (void)warn_extension_used("cursor.rownumber");

    if (cursor->description)
    {
        return PyLong_FromSize_t(cursor->rowsread);
    }
    else
    {
        Py_RETURN_NONE;
    }

    UNUSED(closure);
}

static const char s_Cursor_spid_doc[] =
    "Retrieve the SQL Server Session Process ID (SPID) for the connection.\n"
    "\n"
    ":return: None if the connection is closed.\n"
    ":rtype: int\n";

static PyObject* Cursor_spid_get(PyObject* self, void* closure)
{
    struct Cursor* cursor = (struct Cursor*)self;

    DBPROCESS* dbproc;
    Cursor_verify_open(cursor);

    dbproc = Connection_DBPROCESS(cursor->connection);
    if (dbproc)
    {
        int spid = dbspid(dbproc);
        return PyLong_FromLong((long)spid);
    }
    Py_RETURN_NONE;

    UNUSED(closure);
}

static const char s_Cursor_Parameter_doc[] =
    "Convenience method to :py:class:`ctds.Parameter`.\n"
    "\n"
    ":return: A new Parameter object.\n"
    ":rtype: ctds.Parameter\n";

static PyObject* Cursor_Parameter_get(PyObject* self, void* closure)
{
    return (PyObject*)ParameterType_get();

    UNUSED(self);
    UNUSED(closure);
}


static PyGetSetDef Cursor_getset[] = {
    /* name, get, set, doc, closure */
    { (char*)"arraysize",   Cursor_arraysize_get,   Cursor_arraysize_set, (char*)s_Cursor_arraysize_doc,   NULL },
    { (char*)"description", Cursor_description_get, NULL,                 (char*)s_Cursor_description_doc, NULL },
    { (char*)"rowcount",    Cursor_rowcount_get,    NULL,                 (char*)s_Cursor_rowcount_doc,    NULL },

    { (char*)"connection",  Cursor_connection_get,  NULL,                 (char*)s_Cursor_connection_doc,  NULL },
    { (char*)"rownumber",   Cursor_rownumber_get,   NULL,                 (char*)s_Cursor_rownumber_doc,   NULL },
    { (char*)"spid",        Cursor_spid_get,        NULL,                 (char*)s_Cursor_spid_doc,        NULL },
    { (char*)"Parameter",   Cursor_Parameter_get,   NULL,                 (char*)s_Cursor_Parameter_doc,   NULL },
    { NULL,                 NULL,                   NULL,                 NULL,                            NULL }
};

/*
    tds.Cursor methods
*/

static const char* Cursor_extract_parameter_name(PyObject* value, PyObject** utf8)
{
    const char* name;
    assert(NULL == *utf8);
    if (
#if PY_MAJOR_VERSION < 3
        !PyString_Check(value) &&
#endif /* if PY_MAJOR_VERSION < 3 */
        !PyUnicode_Check(value))
    {
        return NULL;
    }
#if PY_MAJOR_VERSION < 3
    if (PyUnicode_Check(value))
    {
        *utf8 = PyUnicode_AsUTF8String(value);
        if (!*utf8)
        {
            return NULL;
        }
        name = PyString_AS_STRING(*utf8);
    }
    else
#endif /* if PY_MAJOR_VERSION < 3 */
    {
        Py_INCREF(value);
        *utf8 = value;

#if PY_MAJOR_VERSION < 3
        assert(PyString_Check(value));
        name = PyString_AS_STRING(*utf8);
#else /* if PY_MAJOR_VERSION < 3 */
        name = PyUnicode_AsUTF8(*utf8);
#endif /* else if PY_MAJOR_VERSION < 3 */
    }

    /* Only allow string keys starting with '@' */
    if ('@' != name[0])
    {
        return NULL;
    }

    return name;
}
/*
    Bind Python objects to RPC parameters for use in an RPC call.

    This method sets an appropriate Python exception on failure.

    @note This method requires the current thread own the GIL.
    @note A new reference is returned.
    @note dbrpcinit *must* be called before this.

    @param cursor [in] A Cusor object.
    @param parameters [in] A Python dict or tuple object of parameters to bind.
        The parameters can be of any Python type.

    @return A Python dict or tuple object containg the bound parameters.
*/
static PyObject* Cursor_bind(struct Cursor* cursor, PyObject* parameters)
{
    RETCODE retcode = SUCCEED;

    Py_ssize_t nparameters = (PyDict_Check(parameters)) ? PyDict_Size(parameters) : PyTuple_GET_SIZE(parameters);
    DBPROCESS* dbproc = Connection_DBPROCESS(cursor->connection);

    PyObject* rpcparams = (PyDict_Check(parameters)) ? PyDict_New() : PyTuple_New(nparameters);
    if (!rpcparams)
    {
        PyErr_NoMemory();
        return NULL;
    }

    if (PyTuple_Check(parameters))
    {
        Py_ssize_t ix;
        for (ix = 0; ix < nparameters; ++ix)
        {
            struct Parameter* rpcparam;
            PyObject* value = PyTuple_GET_ITEM(parameters, ix);
            if (!Parameter_Check(value))
            {
                rpcparam = Parameter_create(value, 0 /* output */);
                if (!rpcparam)
                {
                    break;
                }
            }
            else
            {
                Py_INCREF(value);
                rpcparam = (struct Parameter*)value;
            }
            PyTuple_SET_ITEM(rpcparams, ix, (PyObject*)rpcparam); /* rpcparam reference stolen by PyTuple_SET_ITEM */

            retcode = Parameter_dbrpcparam(rpcparam,
                                           dbproc,
                                           NULL /* pass arguments by position */);
            if (FAIL == retcode)
            {
                break;
            }
        }
    }
    else /* if (PyDict_Check(parameters)) */
    {
        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(parameters, &pos, &key, &value))
        {
            struct Parameter* rpcparam = NULL;
            PyObject* utf8key = NULL;
            do
            {
                const char* keystr = Cursor_extract_parameter_name(key, &utf8key);
                if (!keystr)
                {
                    PyObject* repr = PyObject_Repr(key);
                    if (repr)
                    {
#if PY_MAJOR_VERSION < 3
                        const char* reprstr = PyString_AS_STRING(repr);
#else /* if PY_MAJOR_VERSION < 3 */
                        const char* reprstr = PyUnicode_AsUTF8(repr);
#endif /* else if PY_MAJOR_VERSION < 3 */
                        PyErr_Format(PyExc_tds_InterfaceError, "invalid parameter name \"%s\"", reprstr);
                        Py_DECREF(repr);
                    }
                    break;
                }

                if (!Parameter_Check(value))
                {
                    rpcparam = Parameter_create(value, 0 /* output */);
                    if (!rpcparam)
                    {
                        break;
                    }
                }
                else
                {
                    Py_INCREF(value);
                    rpcparam = (struct Parameter*)value;
                }
                if (0 != PyDict_SetItem(rpcparams, key, (PyObject*)rpcparam))
                {
                    break;
                }

                retcode = Parameter_dbrpcparam(rpcparam, dbproc, keystr);
                if (FAIL == retcode)
                {
                    break;
                }
            }
            while (0);

            Py_XDECREF(utf8key);
            Py_XDECREF(rpcparam);

            if (PyErr_Occurred() || (FAIL == retcode))
            {
                break;
            }
        }
    }
    if (FAIL == retcode)
    {
        Connection_raise(cursor->connection);
    }

    if (PyErr_Occurred())
    {
        Py_DECREF(rpcparams);
        rpcparams = NULL;
    }

    return rpcparams;
}


struct OutputParameter
{
    const char* name;
    enum TdsType tdstype;
    const void* data;
    size_t ndata;
};


/*
    Unbind Python objects from RPC parameters after completion of an RPC call.

    This method sets an appropriate Python exception on failure.

    @note This method requires the current thread own the GIL.
    @note A new reference is returned.

    @param cursor [in] A Cusor object.
    @param rpcparams [in] A Python dict or tuple object of bound parameters.
        These are assumed to all be instances of type `struct Parameter`.
    @param outputparams [in] The output parameters returned by the RPC call.
    @param noutputparams [in] The number of output parameters in `outputparams`.
    @param noutputs [out] The number of output parameters seen.

    @return A Python dict or tuple object containg the result parameters.
*/
static PyObject* Cursor_unbind(PyObject* rpcparams,
                               const struct OutputParameter* outputparams,
                               size_t noutputparams, size_t* noutputs)
{
    PyObject* results = NULL;
    *noutputs = 0;
    if (PyTuple_Check(rpcparams))
    {
        Py_ssize_t ix;
        Py_ssize_t nrpcparams = PyTuple_GET_SIZE(rpcparams);
        results = PyTuple_New(nrpcparams);
        if (!results)
        {
            PyErr_NoMemory();
            return NULL;
        }

        for (ix = 0; ix < nrpcparams; ++ix)
        {
            PyObject* output = NULL;
            struct Parameter* rpcparam = (struct Parameter*)PyTuple_GET_ITEM(rpcparams, ix);
            if (Parameter_output(rpcparam))
            {
                if (*noutputs < noutputparams)
                {
                    const struct OutputParameter* outputparam = &outputparams[*noutputs];
                    sql_topython topython = sql_topython_lookup(outputparam->tdstype);
                    assert(topython);
                    output = topython(outputparam->tdstype,
                                      outputparam->data,
                                      outputparam->ndata);
                }
                else
                {
                    /*
                        If the parameter was not found, this is likely due to
                        the presence of a resultset. Return None for the
                        output parameter in this case.
                    */
                    output = Py_None;
                    Py_INCREF(output);
                }
                ++(*noutputs);
            }
            else
            {
                output = Parameter_value(rpcparam);
                Py_INCREF(output);
            }
            if (!output)
            {
                assert(PyErr_Occurred());
                break;
            }
            PyTuple_SET_ITEM(results, ix, output); /* output reference stolen by PyTuple_SET_ITEM */
        }
    }
    else /* if (PyDict_Check(parameters)) */
    {
        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;

        results = PyDict_New();
        if (!results)
        {
            PyErr_NoMemory();
            return NULL;
        }

        while (PyDict_Next(rpcparams, &pos, &key, &value))
        {
            PyObject* output = NULL;
            struct Parameter* rpcparam = (struct Parameter*)value;
            if (Parameter_output(rpcparam))
            {
                const struct OutputParameter* outputparam = NULL;
                const char* name;
                size_t ix;
#if PY_MAJOR_VERSION < 3
                PyObject* utf8key;
                if (PyUnicode_Check(key))
                {
                    utf8key = PyUnicode_AsUTF8String(key);
                    if (!utf8key)
                    {
                        break;
                    }
                }
                else
                {
                    assert(PyString_Check(key));
                    Py_INCREF(key);
                    utf8key = key;
                }
                name = PyString_AS_STRING(utf8key);
#else /* if PY_MAJOR_VERSION < 3 */
                name = PyUnicode_AsUTF8(key);
#endif /* else if PY_MAJOR_VERSION < 3 */
                for (ix = 0; ix < noutputparams; ++ix)
                {
                    if (strcmp(name, outputparams[ix].name) == 0)
                    {
                        outputparam = &outputparams[ix];
                        break;
                    }
                }
#if PY_MAJOR_VERSION < 3
                Py_DECREF(utf8key);
#endif /* if PY_MAJOR_VERSION < 3 */
                if (outputparam)
                {
                    sql_topython topython = sql_topython_lookup(outputparam->tdstype);
                    assert(topython);
                    output = topython(outputparam->tdstype,
                                      outputparam->data,
                                      outputparam->ndata);
                }
                else
                {
                    /*
                        If the parameter was not found, this is likely due to
                        the presence of a resultset. Return None for the
                        output parameter in this case.
                    */
                    output = Py_None;
                    Py_INCREF(output);
                }
                ++(*noutputs);
            }
            else
            {
                output = Parameter_value(rpcparam);
                Py_INCREF(output);
            }
            if (!output)
            {
                assert(PyErr_Occurred());
                break;
            }
            (void)PyDict_SetItem(results, key, output);
            Py_DECREF(output);
        }
    }

    if (PyErr_Occurred())
    {
        Py_DECREF(results);
        results = NULL;
    }

    return results;
}

/*
    Bind input arguments to their SQL types, call a stored procedure, and
    process the resulting output values.

    @note This does not check the cursor state (i.e. open, connected).

    @param cursor [in] The Cursor.
    @param procname [in] The stored procedure name, UTF-8 encoded.
    @param parameters [in] A Python dict or tuple containing the parameters
        to the stored procedure. The arguments can be of any Python type.

    @return A copy of the input arguments, including any output parameters in
        place.
*/
static PyObject* Cursor_callproc_internal(struct Cursor* cursor, const char* procname,
                                          PyObject* parameters)
{
    PyObject* results = NULL;

    PyObject* rpcparams = NULL;
    struct OutputParameter* outputparams = NULL;
    DBINT retstatus;

    Connection_clear_lastwarning(cursor->connection);

    do
    {
        DBPROCESS* dbproc = Connection_DBPROCESS(cursor->connection);

        int error = -1;
        RETCODE retcode;

        int noutputparams = 0;
        size_t noutputs;

        if (FAIL == dbrpcinit(dbproc, procname, 0 /* options */))
        {
            Connection_raise(cursor->connection);
            break;
        }

        rpcparams = Cursor_bind(cursor, parameters);
        if (!rpcparams)
        {
            /* Clear the RPC call initialization if it fails to send. */
            (void)dbrpcinit(dbproc, procname, DBRPCRESET);
            break;
        }

        Py_BEGIN_ALLOW_THREADS

            do
            {
                int ix;

                retcode = dbcancel(dbproc);
                if (FAIL == retcode)
                {
                    break;
                }
                retcode = dbrpcsend(dbproc);
                if (FAIL == retcode)
                {
                    break;
                }
                retcode = dbsqlok(dbproc);
                if (FAIL == retcode)
                {
                    break;
                }

                error = Cursor_next_resultset(cursor, &retcode);
                if (!error)
                {
                    noutputparams = dbnumrets(dbproc);

                    if (noutputparams)
                    {
                        outputparams = (struct OutputParameter*)tds_mem_calloc((size_t)noutputparams,
                                                                               sizeof(struct OutputParameter));
                        if (!outputparams)
                        {
                            break;
                        }

                        for (ix = 0; ix < noutputparams; ++ix)
                        {
                            outputparams[ix].name = (const char*)dbretname(dbproc, ix + 1);
                            outputparams[ix].tdstype = (enum TdsType)dbrettype(dbproc, ix + 1);
                            outputparams[ix].data = (const void*)dbretdata(dbproc, ix + 1);
                            outputparams[ix].ndata = (size_t)dbretlen(dbproc, ix + 1);
                        }
                    }
                    retstatus = dbretstatus(dbproc);
                }
            } while (0);

        Py_END_ALLOW_THREADS

        if (FAIL == retcode)
        {
            Connection_raise(cursor->connection);
            break;
        }
        if (error)
        {
            assert(FAIL != retcode);
            PyErr_NoMemory();
            break;
        }

        results = Cursor_unbind(rpcparams,
                                outputparams,
                                (size_t)noutputparams,
                                &noutputs);
        if ((NULL != results) && (noutputs > 0) && (NO_MORE_RESULTS != retcode))
        {
            /*
                TDS returns output parameter data (and status) only after all
                resultsets have been read. This is inherently incompatible with
                the DB API 2.0 interface. If an attempt is made to use the API
                in this way, return the resultsets and raise a warning.
            */
            (void)PyErr_Warn(PyExc_tds_Warning,
                             "output parameters are not supported with result sets");
        }
    } while (0);

    if (!PyErr_Occurred())
    {
        Connection_raise_lastwarning(cursor->connection);
    }

    tds_mem_free(outputparams);

    Py_XDECREF(rpcparams);

    return results;

    /* $TODO: figure out some way to expose the sproc return status. */
    UNUSED(retstatus);
}

/* https://www.python.org/dev/peps/pep-0249/#callproc */
static const char s_Cursor_callproc_doc[] =
    "callproc(sproc, parameters)\n"
    "\n"
    "Call a stored database procedure with the given name. The sequence of\n"
    "parameters must contain one entry for each argument that the procedure\n"
    "expects. The result of the call is returned as modified copy of the input\n"
    "sequence. Input parameters are left untouched. Output and input/output\n"
    "parameters are replaced with output values.\n"
    "\n"
    ".. warning:: Due to `FreeTDS` implementation details, stored procedures\n"
    "    with both output parameters and resultsets are not supported.\n"
    "\n"
    ".. warning:: Currently `FreeTDS` does not support passing empty string\n"
    "    parameters. Empty strings are converted to `NULL` values internally\n"
    "    before being transmitted to the database.\n"
    "\n"
    ":pep:`0249#callproc`\n"
    "\n"
    ":param str sproc: The name of the stored procedure to execute.\n"

    ":param parameters: Parameters to pass to the stored procedure.\n"
    "    Parameters passed in a :py:class:`dict` must map from the parameter\n"
    "    name to value and start with the **@** character. Parameters passed\n"
    "    in a :py:class:`tuple` are passed in the tuple order.\n"
    ":type parameters: dict or tuple\n"

    ":return: The input `parameters` with any output parameters replaced with\n"
    "    the output values.\n"
    ":rtype: dict or tuple\n";

static PyObject* Cursor_callproc(PyObject* self, PyObject* args)
{
    struct Cursor* cursor = (struct Cursor*)self;

    char* procname = NULL;
    PyObject* parameters = NULL;
    if (!PyArg_ParseTuple(args, "sO", &procname, &parameters))
    {
        return NULL;
    }
    /*
        Allow either a tuple or dictionary for the stored procedure arguments.
        Note that this is a deviation from the DB API 2.0 specification.
    */
    if (!PyDict_Check(parameters) && !PyTuple_Check(parameters))
    {
        PyErr_SetString(PyExc_TypeError, "must be dict or tuple");
        return NULL;
    }

    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    return Cursor_callproc_internal(cursor, procname, parameters);
}

/* https://www.python.org/dev/peps/pep-0249/#Cursor.close */
static const char s_Cursor_close_doc[] =
    "close()\n"
    "\n"
    "Close the cursor.\n"
    "\n"
    ":pep:`0249#Cursor.close`\n";

static PyObject* Cursor_close(PyObject* self, PyObject* args)
{
    struct Cursor* cursor = (struct Cursor*)self;

    Cursor_verify_open(cursor);

    Cursor_close_connection(cursor);

    Py_RETURN_NONE;
    UNUSED(args);
}


/*
    Append one string to an existing one by reallocating the existing string.
    This will free the existing string if the reallocation fails.
*/
static char* strappend(char* existing, size_t nexisting, const char* suffix, size_t nsuffix)
{
    char* tmp = (char*)tds_mem_realloc(existing, nexisting + nsuffix + 1 /* '\0' */);
    if (tmp)
    {
        memcpy(tmp + nexisting, suffix, nsuffix);
        tmp[nexisting + nsuffix] = '\0';
    }
    else
    {
        tds_mem_free(existing);
    }
    return tmp;
}


/*
    Generate a SQL statement suitable for use as the `@stmt` parameter to
    sp_executesql or format the SQL statement for direct execution.

    The format string uses the "numeric" format syntax, as defined here:
    https://www.python.org/dev/peps/pep-0249/#paramstyle.

    @param format [in] The SQL command format string.
    @param nparameters [in] The number of parameters available.

    @return The formatted SQL statement. That caller is responsible freeing the returned value.
*/
static char* build_executesql_stmt(const char* format,
                                   PyObject* parameters,
                                   Py_ssize_t nparameters,
                                   size_t* nsql)
{
    char* sql = NULL;
    *nsql = 0;
    do
    {
        bool literal = false; /* currently in a string literal? */

        /* Construct the SQL query string. */
        const char* chunk = format; /* the start of the next chunk to copy */
        size_t nchunk = 0;
        while ('\0' != *format)
        {
            /* \' characters can only be escaped in string literals. */
            if ('\'' == *format)
            {
                literal = !literal;
                format++;
            }
            else if (!literal && (':' == *format))
            {
                long int paramnum;
                char* paramnum_end;
                const char* paramnum_start = format + 1; /* skip over the ':' */
                paramnum = strtol(paramnum_start, &paramnum_end, 10);
                if (paramnum_start != paramnum_end)
                {
                    if ((0 <= paramnum) && (paramnum < nparameters))
                    {
                        nchunk = (size_t)(format - chunk);

                        /* Append the prior chunk. */
                        sql = strappend(sql, *nsql, chunk, nchunk);
                        if (sql)
                        {
#if CTDS_USE_SP_EXECUTESQL != 0
                            char param[ARRAYSIZE("@param" STRINGIFY(UINT64_MAX))];
                            size_t nparam = (size_t)sprintf(param, "@param%lu", (size_t)paramnum);
                            UNUSED(parameters);
#else /* if CTDS_USE_SP_EXECUTESQL != 0 */
                            /* Serialize the parameter to a string. */
                            PyObject* oparam = NULL;
                            char* param = NULL;
                            size_t nparam = 0;
                            do
                            {
                                oparam = PySequence_Fast_GET_ITEM(parameters, paramnum);
                                if (!Parameter_Check(oparam))
                                {
                                    /* `oparam` is a borrowed reference, so safe to overwrite. */
                                    oparam = (PyObject*)Parameter_create(oparam, 0 /* output */);
                                    if (!oparam)
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    /* `oparam` is a borrowed reference, so increment. */
                                    Py_INCREF(oparam);
                                }

                                param = Parameter_serialize((struct Parameter*)oparam, &nparam);
                                if (!param)
                                {
                                    break;
                                }
                            }
                            while (0);

                            Py_XDECREF(oparam);

                            if (PyErr_Occurred())
                            {
                                assert(!param);
                                break;
                            }
#endif /* else if CTDS_USE_SP_EXECUTESQL != 0 */

                            *nsql += nchunk;
                            nchunk = 0;

                            format = chunk = paramnum_end;

                            sql = strappend(sql, *nsql, param, nparam);

#if !(CTDS_USE_SP_EXECUTESQL != 0)
                            tds_mem_free(param);
#endif /* if !(CTDS_USE_SP_EXECUTESQL != 0) */

                            if (!sql)
                            {
                                PyErr_NoMemory();
                                break;
                            }
                            *nsql += nparam;
                        }
                        else
                        {
                            PyErr_NoMemory();
                            break;
                        }
                    }
                    else
                    {
                        PyErr_Format(PyExc_IndexError, "%ld", paramnum);
                        break;
                    }
                }
                else
                {
                    /* Missing the index following the parameter marker. */
                    PyErr_Format(PyExc_tds_InterfaceError, "invalid parameter marker");
                    break;
                }
            }
            else
            {
                format++;
            }
        }

        if (PyErr_Occurred())
        {
            break;
        }

        /* Append the last chunk. */
        nchunk = (size_t)(format - chunk);
        sql = strappend(sql, *nsql, chunk, nchunk);
        *nsql += nchunk;
        if (!sql)
        {
            PyErr_NoMemory();
            break;
        }
    } while (0);

    if (PyErr_Occurred())
    {
        tds_mem_free(sql);
        sql = NULL;
    }

    return sql;
}


#if CTDS_USE_SP_EXECUTESQL != 0

/*
    Generate a params string suitable for use as the `@params` parameter to
    sp_executesql.

    @param parameters [in] A Python sequence of the parameters.

    @return A new reference to a Python string object.
*/
static PyObject* build_executesql_params(PyObject* parameters)
{
    PyObject* object = NULL;

    Py_ssize_t nparameters = PySequence_Fast_GET_SIZE(parameters);
    char* params = NULL;
    Py_ssize_t nparams = 0;

    Py_ssize_t ix;
    for (ix = 0; ix < nparameters; ++ix)
    {
        char* sqltype = NULL;
        char* paramN = NULL;
        size_t nparamN = 0;
        struct Parameter* rpcparam = Parameter_create(PySequence_Fast_GET_ITEM(parameters, ix), 0);
        if (!rpcparam)
        {
            break;
        }

        do
        {
            int written;
            size_t required;
            sqltype = Parameter_sqltype(rpcparam);
            if (!sqltype)
            {
                PyErr_NoMemory();
                break;
            }

            /*
                Generate the param description for this parameter using the
                following format:

                    @paramN data_type [ OUT | OUTPUT ]
            */
            required = ((ix) ? STRLEN(", ") : STRLEN("")) +
                       STRLEN("@param" STRINGIFY(UINT64_MAX)) +
                       STRLEN(" ") +
                       strlen(sqltype) +
                       STRLEN(" OUTPUT") +
                       1 /* '\0' */;
            if (required > nparamN)
            {
                char* tmp = (char*)tds_mem_realloc(paramN, required);
                if (!tmp)
                {
                    PyErr_NoMemory();
                    break;
                }
                free(paramN);
                paramN = tmp;
                nparamN = required;
            }
            written = snprintf(paramN,
                               nparamN,
                               "%s@param%lu %s%s",
                               (ix) ? ", " : "",
                               ix,
                               sqltype,
                               (Parameter_output(rpcparam)) ? " OUTPUT" : "");
            assert(written < nparamN);
            params = strappend(params, (size_t)nparams, paramN, (size_t)written);
            if (!params)
            {
                PyErr_NoMemory();
                break;
            }
            nparams += written;
        } while (0);
        tds_mem_free(sqltype);
        tds_mem_free(paramN);

        Py_DECREF((PyObject*)rpcparam);
    }
    if (!PyErr_Occurred())
    {
        object = PyUnicode_DecodeUTF8(params, nparams, "strict");
    }
    tds_mem_free(params);

    return object;
}

/*
    Construct and execute a SQL statement. This will execute the SQL using the
    `sp_executesql` stored procedure, to support optimal batching support.

    @note This does not check the cursor state (i.e. open, connected).

    @param cursor [in] The Cursor.
    @param sqlfmt [in] A SQL format string, using the numeric DB API 2 for
       parameter markers.
    @param sequence [in] An optional sequence of parameters to the SQL
       statement.

    @return 0 on success, -1 on error.
*/
static int Cursor_execute_internal(struct Cursor* cursor, const char* sqlfmt, PyObject* sequence)
{
    PyObject* isequence = PyObject_GetIter(sequence);
    if (isequence)
    {
        size_t ix = 0;

        PyObject* parameters = NULL; /* current parameters, if any */
        PyObject* callprocargs = NULL;
        Py_ssize_t nparameters = 0;

        PyObject* nextparams = PyIter_Next(isequence);
        do
        {
            if (nextparams)
            {
                static const char s_fmt[] = "invalid parameter sequence item %ld";

                char msg[ARRAYSIZE(s_fmt) + ARRAYSIZE(STRINGIFY(UINT64_MAX))];
                (void)sprintf(msg, s_fmt, ix);

                Py_XDECREF(parameters);
                parameters = PySequence_Fast(nextparams, msg);
                if (!parameters)
                {
                    break;
                }
                ix++;
            }
            if (callprocargs)
            {
                if (nparameters != PySequence_Fast_GET_SIZE(parameters))
                {
                    PyErr_Format(PyExc_tds_InterfaceError,
                                 "unexpected parameter count in sequence item %ld",
                                 ix);
                    break;
                }
            }
            else
            {
                char* sql;
                size_t nsql;
                PyObject* stmt;

                /* Create the callproc arguments on the first (and possibly only) iteration. */
                nparameters = (parameters) ? PySequence_Fast_GET_SIZE(parameters) : 0;
                callprocargs = PyTuple_New(1 + ((nparameters) ? 1 : 0) + nparameters);
                if (!callprocargs)
                {
                    break;
                }

                sql = build_executesql_stmt(sqlfmt, parameters, nparameters, &nsql);
                if (!sql)
                {
                    break;
                }

                stmt = PyUnicode_DecodeUTF8(sql, (Py_ssize_t)nsql, "strict");
                tds_mem_free(sql);
                if (!stmt)
                {
                    break;
                }

                PyTuple_SET_ITEM(callprocargs, 0, stmt); /* stmt reference stolen by PyTuple_SET_ITEM */

                /*
                    Construct the @params parameter based on the first parameters tuple
                    in the sequence.
                 */
                if (nparameters)
                {
                    PyObject* params = build_executesql_params(parameters);
                    if (params)
                    {
                        PyTuple_SET_ITEM(callprocargs, 1, params); /* params reference stolen by PyTuple_SET_ITEM */
                    }
                    else
                    {
                        break;
                    }
                }
            }

            if (parameters)
            {
                Py_ssize_t ixparam;
                for (ixparam = 0; ixparam < nparameters; ++ixparam)
                {
                    PyObject* param = PySequence_Fast_GET_ITEM(parameters, ixparam);
                    Py_INCREF(param); /* param reference stolen by PyTuple_SetItem */

                    /* Use PyTuple_SetItem to properly overwrite existing values. */
                    if (0 != PyTuple_SetItem(callprocargs, ixparam + 2 /* @stmt, @params */, param))
                    {
                        Py_DECREF(param);
                        break;
                    }
                }
            }
            if (!PyErr_Occurred())
            {
                PyObject* output = Cursor_callproc_internal(cursor,
                                                            "sp_executesql",
                                                            callprocargs);
                Py_XDECREF(output);
            }
            if (PyErr_Occurred())
            {
                break;
            }
            Py_XDECREF(nextparams);
        }
        while (NULL != (nextparams = PyIter_Next(isequence)));

        Py_XDECREF(nextparams);
        Py_XDECREF(parameters);
        Py_XDECREF(callprocargs);

        Py_DECREF(isequence);
    }

    return (PyErr_Occurred()) ? -1 : 0;
}

#else /* if CTDS_USE_SP_EXECUTESQL != 0 */

static int Cursor_execute_internal(struct Cursor* cursor, const char* sqlfmt, PyObject* sequence)
{
    PyObject* isequence = PyObject_GetIter(sequence);
    if (isequence)
    {
        size_t ix = 0;

        PyObject* parameters = NULL; /* current parameters, if any */
        Py_ssize_t nparameters = 0;

        PyObject* nextparams = PyIter_Next(isequence);
        do
        {
            RETCODE retcode;
            int error;

            DBPROCESS* dbproc = Connection_DBPROCESS(cursor->connection);

            char* sql;
            size_t nsql;

            Connection_clear_lastwarning(cursor->connection);

            if (nextparams)
            {
                static const char s_fmt[] = "invalid parameter sequence item %ld";

                char msg[ARRAYSIZE(s_fmt) + ARRAYSIZE(STRINGIFY(UINT64_MAX))];
                (void)sprintf(msg, s_fmt, ix);

                assert(NULL == parameters);
                parameters = PySequence_Fast(nextparams, msg);
                if (!parameters)
                {
                    break;
                }
                /* Set the expected parameter count based on the first sequence. */
                if (0 == ix)
                {
                    nparameters = PySequence_Fast_GET_SIZE(parameters);
                }
                ix++;
            }

            if (nparameters != ((parameters) ? PySequence_Fast_GET_SIZE(parameters) : 0))
            {
                PyErr_Format(PyExc_tds_InterfaceError,
                             "unexpected parameter count in sequence item %ld",
                             ix);
                break;
            }

            sql = build_executesql_stmt(sqlfmt, parameters, nparameters, &nsql);
            Py_XDECREF(parameters);
            parameters = NULL;
            if (!sql)
            {
                break;
            }

            /* Clear any existing command buffer. */
            dbfreebuf(dbproc);

            retcode = dbcmd(dbproc, sql);
            tds_mem_free(sql);
            if (FAIL == retcode)
            {
                Connection_raise(cursor->connection);
                break;
            }

            Py_BEGIN_ALLOW_THREADS

                do
                {
                    retcode = dbcancel(dbproc);
                    if (FAIL == retcode)
                    {
                        break;
                    }
                    retcode = dbsqlsend(dbproc);
                    if (FAIL == retcode)
                    {
                        break;
                    }
                    retcode = dbsqlok(dbproc);
                    if (FAIL == retcode)
                    {
                        break;
                    }

                    error = Cursor_next_resultset(cursor, &retcode);
                } while (0);

            Py_END_ALLOW_THREADS

            if (FAIL == retcode)
            {
                Connection_raise(cursor->connection);
                break;
            }
            if (error)
            {
                assert(FAIL != retcode);
                PyErr_NoMemory();
                break;
            }

            /* Raise any warnings that may have occurred. */
            Connection_raise_lastwarning(cursor->connection);
        }
        while (NULL != (nextparams = PyIter_Next(isequence)));

        Py_XDECREF(nextparams);
        Py_XDECREF(parameters);

        Py_DECREF(isequence);
    }

    return (PyErr_Occurred()) ? -1 : 0;
}

#endif /* else if CTDS_USE_SP_EXECUTESQL != 0 */


/* https://www.python.org/dev/peps/pep-0249/#execute */
static const char s_Cursor_execute_doc[] =
    "execute(sql, parameters=None)\n"
    "\n"
    "Prepare and execute a database operation.\n"
    "Parameters may be provided as sequence and will be bound to variables\n"
    "specified in the SQL statement. Parameter notation is specified by\n"
    ":py:const:`ctds.paramstyle`.\n"
    "\n"
    ":pep:`0249#execute`\n"
    "\n"
    ":param str sql: The SQL statement to execute.\n"

    ":param tuple parameters: Optional variables to bind.\n";

static PyObject* Cursor_execute(PyObject* self, PyObject* args)
{
    char* sqlfmt;
    PyObject* parameters = NULL;
    PyObject* sequence;
    int error;

    struct Cursor* cursor = (struct Cursor*)self;
    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    if (!PyArg_ParseTuple(args, "s|O", &sqlfmt, &parameters))
    {
        return NULL;
    }
    if (parameters && !PySequence_Check(parameters))
    {
        PyErr_SetObject(PyExc_TypeError, parameters);
        return NULL;
    }

    sequence = PyTuple_New((parameters) ? 1 : 0);
    if (!sequence)
    {
        return NULL;
    }

    if (parameters)
    {
        Py_INCREF(parameters);
        PyTuple_SET_ITEM(sequence, 0, parameters); /* parameters reference stolen by PyTuple_SET_ITEM */
    }

    error = Cursor_execute_internal(cursor, sqlfmt, sequence);
    Py_DECREF(sequence);
    if (error)
    {
        return NULL;
    }
    Py_RETURN_NONE;
}

/* https://www.python.org/dev/peps/pep-0249/#executemany */
static const char s_Cursor_executemany_doc[] =
    "executemany(sql, seq_of_parameters)\n"
    "\n"
    "Prepare a database operation (query or command) and then execute it\n"
    "against all parameter sequences or mappings found in the sequence\n"
    "`seq_of_parameters`.\n"
    "\n"
    ":pep:`0249#executemany`\n"
    "\n"
    ":param str sql: The SQL statement to execute.\n"

    ":param seq_of_parameters: An iterable of parameter sequences to bind.\n"
    ":type seq_of_parameters: :ref:`typeiter <python:typeiter>`\n";

PyObject* Cursor_executemany(PyObject* self, PyObject* args)
{
    char* sqlfmt;
    PyObject* iterable;

    struct Cursor* cursor = (struct Cursor*)self;
    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    if (!PyArg_ParseTuple(args, "sO", &sqlfmt, &iterable))
    {
        return NULL;
    }

    if (Cursor_execute_internal(cursor, sqlfmt, iterable))
    {
        return NULL;
    }
    Py_RETURN_NONE;
}

struct ColumnBuffer
{
    /* The size of this column, in bytes. */
    size_t size;

    /*
        The TDS type of this column. This is necessary for COMPUTE columns
        which may have a different type than the regular column.
    */
    enum TdsType tdstype;

    /* The column data. This member must always be last. */
    union {
        /* Allocated separately on heap. */
        void* variable;

        /* Allocated as part of this structure. */
        uint8_t* fixed;
    } data;
};

/*
    Check whether a database column is variable length or not.

    @param _dbcol [in] A DBCOL* describing the column.
*/
#define Column_IsVariableLength(_dbcol) \
    !!(_dbcol)->VarLength

/*
    Determine the column buffer size required for a database column.

    @param _dbcol [in] A DBCOL* describing the column.
*/
#define ColumnBuffer_size(_dbcol) \
    ((Column_IsVariableLength(_dbcol)) ? \
        sizeof(struct ColumnBuffer) : ((size_t)(_dbcol)->MaxLength + offsetof(struct ColumnBuffer, data)))


struct RowBuffer
{
    /* The next row buffer in the resultset. */
    struct RowBuffer* next;

    /*
        A dynamically-sized block of memory large enough for all the row's
        columns. Because columns themselves are dynamically sized, indexing
        into this array is non-trivial.

        Iterating column buffers requires use of ColumnBuffer_size() to
        determine the actual size of the ColumnBuffer.
    */
#pragma GCC diagnostic push
/* Ignore "ISO C90 does not support flexible array members". */
#pragma GCC diagnostic ignored "-Wpedantic"
    struct ColumnBuffer columns[];
#pragma GCC diagnostic pop
};

/*
    Determine the row buffer size for the current resultset.

    @param description [in] A description of the result set.

    @return The size, in bytes, for a RowBuffer describing
        the current resultset.
*/
static size_t ResultSetDescription_RowBuffer_size(const struct ResultSetDescription* description)
{
    if (description)
    {
        size_t size = 0;
        size_t ix;
        for (ix = 0; ix < description->ncolumns; ++ix)
        {
            size += ColumnBuffer_size(&description->columns[ix].dbcol);
        }

        return offsetof(struct RowBuffer, columns) + size;
    }
    else
    {
        return 0;
    }
}

/*
    Release all memory referenced by a RowBuffer.

    @param description [in] A description of the result set for the `rowbuffer`.
    @param rowbuffer [in] The first row in the resultset.
*/
static void ResultSetDescription_RowBuffer_free(const struct ResultSetDescription* description,
                                                struct RowBuffer* rowbuffer)
{
    while (rowbuffer)
    {
        struct RowBuffer* next = rowbuffer->next;

        struct ColumnBuffer* column = rowbuffer->columns;
        size_t ix;
        for (ix = 0; ix < description->ncolumns; ++ix)
        {
            const DBCOL* dbcol = &description->columns[ix].dbcol;
            if (Column_IsVariableLength(dbcol))
            {
                tds_mem_free(column->data.variable);
                column->data.variable = 0;
            }
            column = (struct ColumnBuffer*)((char*)column + ColumnBuffer_size(dbcol));
        }
        tds_mem_free(rowbuffer);
        rowbuffer = next;
    }
}

struct Row {
    PyObject_VAR_HEAD

    struct ResultSetDescription* description;

    PyObject* values[1]; /* space for row data values is added by tp_alloc() */
};

static void Row_dealloc(PyObject* self)
{
    struct Row* row = (struct Row*)self;
    size_t ix;
    for (ix = 0; ix < row->description->ncolumns; ++ix)
    {
        Py_XDECREF(row->values[ix]);
    }
    ResultSetDescription_decrement(row->description);
    PyObject_Del(self);
}

PyTypeObject RowType; /* forward decl. */
static struct Row* Row_create(struct ResultSetDescription* description,
                              const struct RowBuffer* rowbuffer)
{
    struct Row* row = PyObject_NewVar(struct Row, &RowType, (Py_ssize_t)description->ncolumns);
    if (row)
    {
        /* The offset to the next column buffer in the row. */
        size_t offset = 0;
        size_t ixcol;

        memset(row->values, 0, description->ncolumns * sizeof(*row->values));
        row->description = description;
        ResultSetDescription_increment(description);

        for (ixcol = 0; ixcol < description->ncolumns; ++ixcol)
        {
            const struct Column* column = &description->columns[ixcol];

            if (column->topython)
            {
                const struct ColumnBuffer* colbuffer = (const struct ColumnBuffer*)(((const char*)rowbuffer->columns) + offset);

                const void* data = (Column_IsVariableLength(&column->dbcol)) ?
                    colbuffer->data.variable : &colbuffer->data.fixed;

                PyObject* object = NULL;

                /*
                    Used the cached column converter if the type is expected.
                    The type may differ for COMPUTE columns, in which case the
                    converter won't be cached.
                */
                if ((enum TdsType)column->dbcol.Type == colbuffer->tdstype)
                {
                    object = column->topython(colbuffer->tdstype,
                                              data,
                                              colbuffer->size);
                }
                else
                {
                    sql_topython topython = sql_topython_lookup(colbuffer->tdstype);
                    assert(topython);
                    object = topython(colbuffer->tdstype,
                                      data,
                                      colbuffer->size);
                }
                if (!object)
                {
                    break;
                }

                row->values[ixcol] = object; /* object reference stolen */
            }
            else
            {
                PyErr_Format(PyExc_tds_NotSupportedError,
                             "unsupported type %d for column \"%s\"",
                             column->dbcol.Type,
                             column->dbcol.ActualName);
                break;
            }
            offset += ColumnBuffer_size(&column->dbcol);
        }
        if (PyErr_Occurred())
        {
            Py_DECREF(row);
            row = NULL;
        }
    }
    else
    {
        PyErr_NoMemory();
    }
    return row;
}

static PyObject* Row_lookup_column(PyObject* self, PyObject* item, PyObject* error)
{
    struct Row* row = (struct Row*)self;
    PyObject* value = NULL;
    const char* name = NULL;
    size_t ix;

#if PY_MAJOR_VERSION < 3
    PyObject* utf8item = NULL;
    if (PyUnicode_Check(item))
    {
        utf8item = PyUnicode_AsUTF8String(item);
        if (!utf8item)
        {
            return NULL;
        }
        name = PyString_AS_STRING(utf8item);
    }
    else
    {
        if (PyString_Check(item))
        {
            name = PyString_AS_STRING(item);
        }
    }
#else /* if PY_MAJOR_VERSION < 3 */
    if (PyUnicode_Check(item))
    {
        name = PyUnicode_AsUTF8(item);
    }
#endif /* else if PY_MAJOR_VERSION < 3 */

    if (name)
    {
        for (ix = 0; ix < row->description->ncolumns; ++ix)
        {
            if (0 == strcmp(name, row->description->columns[ix].dbcol.ActualName))
            {
                Py_INCREF(row->values[ix]);
                value = row->values[ix];
                break;
            }
        }
    }

#if PY_MAJOR_VERSION < 3
    Py_XDECREF(utf8item);
#endif /* if PY_MAJOR_VERSION < 3 */

    if (!value && error != NULL)
    {
        PyErr_SetObject(error, item);
    }
    return value;
}

static Py_ssize_t Row_len(PyObject* self)
{
    struct Row* row = (struct Row*)self;
    return (Py_ssize_t)row->description->ncolumns;
}

static PyObject* Row_item(PyObject* self, Py_ssize_t ix)
{
    struct Row* row = (struct Row*)self;
    if (ix < 0 || ix >= (Py_ssize_t)row->description->ncolumns) {
        PyErr_SetString(PyExc_IndexError, "index is out of range");
        return NULL;
    }
    Py_INCREF(row->values[ix]);
    return row->values[ix];
}

static int Row_contains(PyObject* self, PyObject* value)
{
    PyObject* item = Row_lookup_column(self, value, NULL);
    int contains = (NULL != item) ? 1 : 0;
    Py_XDECREF(item);
    return contains;
}

static PySequenceMethods s_Row_as_sequence = {
    Row_len,      /* sq_length */
    NULL,         /* sq_concat */
    NULL,         /* sq_repeat */
    Row_item,     /* sq_item */
    NULL,         /* sq_ass_item */
    NULL,         /* was_sq_slice */
    NULL,         /* was_sq_ass_slice */
    Row_contains, /* sq_contains */
    NULL,         /* sq_inplace_concat */
    NULL,         /* sq_inplace_repeat */
};

static PyObject* Row_subscript(PyObject* self, PyObject* item)
{
    if (PyUnicode_Check(item)
#if PY_MAJOR_VERSION < 3
        || PyString_Check(item)
#endif /* if PY_MAJOR_VERSION < 3 */
    )
    {
        return Row_lookup_column(self, item, PyExc_KeyError);
    }
    else if (PyLong_Check(item))
    {
        Py_ssize_t ix = PyLong_AsSsize_t(item);
        if (PyErr_Occurred())
        {
            return NULL;
        }
        return Row_item(self, ix);
    }
#if PY_MAJOR_VERSION < 3
    else if (PyInt_Check(item))
    {
        return Row_item(self, PyInt_AsSsize_t(item));
    }
#endif /* if PY_MAJOR_VERSION < 3 */

    PyErr_SetObject(PyExc_KeyError, item);
    return NULL;
}

static PyMappingMethods s_Row_as_mapping = {
    Row_len,       /* mp_length */
    Row_subscript, /* mp_subscript */
    NULL,          /* mp_ass_subscript */
};

static PyObject* Row_getattro(PyObject* self, PyObject* attr)
{
    return Row_lookup_column(self, attr, PyExc_AttributeError);
}


PyTypeObject RowType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    STRINGIFY(tds) ".Row",                    /* tp_name */
    sizeof(struct Row),                       /* tp_basicsize */
    sizeof(PyObject*),                        /* tp_itemsize */
    Row_dealloc,                              /* tp_dealloc */
    NULL,                                     /* tp_print */
    NULL,                                     /* tp_getattr */
    NULL,                                     /* tp_setattr */
    NULL,                                     /* tp_reserved */
    NULL,                                     /* tp_repr */
    NULL,                                     /* tp_as_number */
    &s_Row_as_sequence,                       /* tp_as_sequence */
    &s_Row_as_mapping,                        /* tp_as_mapping */
    NULL,                                     /* tp_hash */
    NULL,                                     /* tp_call */
    NULL,                                     /* tp_str */
    Row_getattro,                             /* tp_getattro */
    NULL,                                     /* tp_setattro */
    NULL,                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */
    NULL,                                     /* tp_doc */
    NULL,                                     /* tp_traverse */
    NULL,                                     /* tp_clear */
    NULL,                                     /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    NULL,                                     /* tp_iter */
    NULL,                                     /* tp_iternext */
    NULL,                                     /* tp_methods */
    NULL,                                     /* tp_members */
    NULL,                                     /* tp_getset */
    NULL,                                     /* tp_base */
    NULL,                                     /* tp_dict */
    NULL,                                     /* tp_descr_get */
    NULL,                                     /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    NULL,                                     /* tp_init */
    NULL,                                     /* tp_alloc */
    NULL,                                     /* tp_new */
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
};

/* Stores the `struct RowBuffer` for a row until requested by the client. */
struct LazilyCreatedRow {
    bool converted;
    union {
      PyObject* python;
      struct RowBuffer* rowbuffer;
    } row;
};

struct RowList {
    PyObject_VAR_HEAD

    struct ResultSetDescription* description;

    struct LazilyCreatedRow rows[1]; /* space for rows is added by tp_alloc() */
};

static void RowList_dealloc(PyObject* self)
{
    struct RowList* rowlist = (struct RowList*)self;
    Py_ssize_t ix;
    for (ix = 0; ix < Py_SIZE(self); ++ix)
    {
        if (rowlist->rows[ix].converted)
        {
            Py_DECREF(rowlist->rows[ix].row.python);
        }
        else
        {
            ResultSetDescription_RowBuffer_free(rowlist->description,
                                                rowlist->rows[ix].row.rowbuffer);
        }
    }

    ResultSetDescription_decrement(rowlist->description);

    PyObject_Del(self);
}

PyTypeObject RowListType; /* forward decl. */
static struct RowList* RowList_create(struct ResultSetDescription* description,
                                      size_t nrows,
                                      struct RowBuffer* rowbuffers)
{
    struct RowList* rowlist = PyObject_NewVar(struct RowList, &RowListType, (Py_ssize_t)nrows);
    if (rowlist)
    {
        size_t ix = 0;

        rowlist->description = description;
        ResultSetDescription_increment(description);

        while (rowbuffers)
        {
            rowlist->rows[ix].converted = false;
            rowlist->rows[ix].row.rowbuffer = rowbuffers;
            rowbuffers = rowbuffers->next;
            rowlist->rows[ix].row.rowbuffer->next = NULL;
            ++ix;
        }
        assert(ix == nrows);
    }
    else
    {
        PyErr_NoMemory();
    }
    return rowlist;
}

static Py_ssize_t RowList_len(PyObject* self)
{
    return Py_SIZE(self);
}

static PyObject* RowList_item(PyObject* self, Py_ssize_t ix)
{
    struct RowList* rowlist = (struct RowList*)self;
    if (ix < 0 || ix >= Py_SIZE(self)) {
        PyErr_SetString(PyExc_IndexError, "index is out of range");
        return NULL;
    }

    /*
        Convert the `struct RowBuffer` raw data to a Python object on
        first access.
    */
    if (!rowlist->rows[ix].converted)
    {
        struct RowBuffer* rowbuffer = rowlist->rows[ix].row.rowbuffer;
        struct Row* row = Row_create(rowlist->description, rowbuffer);
        if (!row)
        {
            assert(PyErr_Occurred());
            return NULL;
        }

        ResultSetDescription_RowBuffer_free(rowlist->description,
                                            rowbuffer);

        rowlist->rows[ix].row.python = (PyObject*)row; /* claim reference */
        rowlist->rows[ix].converted = true;
    }
    Py_INCREF(rowlist->rows[ix].row.python);
    return rowlist->rows[ix].row.python;
}

static PySequenceMethods s_RowList_as_sequence = {
    RowList_len,  /* sq_length */
    NULL,         /* sq_concat */
    NULL,         /* sq_repeat */
    RowList_item, /* sq_item */
    NULL,         /* sq_ass_item */
    NULL,         /* was_sq_slice */
    NULL,         /* sq_contains */
    NULL,         /* was_sq_ass_slice */
    NULL,         /* sq_inplace_concat */
    NULL,         /* sq_inplace_repeat */
};

PyTypeObject RowListType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    STRINGIFY(tds) ".RowList",                /* tp_name */
    sizeof(struct RowList),                   /* tp_basicsize */
    sizeof(struct LazilyCreatedRow),          /* tp_itemsize */
    RowList_dealloc,                          /* tp_dealloc */
    NULL,                                     /* tp_print */
    NULL,                                     /* tp_getattr */
    NULL,                                     /* tp_setattr */
    NULL,                                     /* tp_reserved */
    NULL,                                     /* tp_repr */
    NULL,                                     /* tp_as_number */
    &s_RowList_as_sequence,                   /* tp_as_sequence */
    NULL,                                     /* tp_as_mapping */
    NULL,                                     /* tp_hash */
    NULL,                                     /* tp_call */
    NULL,                                     /* tp_str */
    NULL,                                     /* tp_getattro */
    NULL,                                     /* tp_setattro */
    NULL,                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */
    NULL,                                     /* tp_doc */
    NULL,                                     /* tp_traverse */
    NULL,                                     /* tp_clear */
    NULL,                                     /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    NULL,                                     /* tp_iter */
    NULL,                                     /* tp_iternext */
    NULL,                                     /* tp_methods */
    NULL,                                     /* tp_members */
    NULL,                                     /* tp_getset */
    NULL,                                     /* tp_base */
    NULL,                                     /* tp_dict */
    NULL,                                     /* tp_descr_get */
    NULL,                                     /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    NULL,                                     /* tp_init */
    NULL,                                     /* tp_alloc */
    NULL,                                     /* tp_new */
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
};

#define FETCH_ALL ((size_t)-1)

/*
    Fetch rows for the current result set.

    This method wil fetch and buffer all requested rows without holding
    the GIL. While this is more memory intensive, it does provide much better
    throughput. In applications with many Python threads constantly acquiring
    and releasing the GIL can be expensive. Multiple threads, each requesting
    large resultsets can lead to massive GIL churn and poor performance.

    @note This method sets an appropriate Python exception on error.
    @note This method returns a new reference.

    @param cursor [in] The cursor.
    @param n [in] The number of rows to fetch from the server.

    @return A `struct RowList` object.
    @return NULL on failure.
*/
static struct RowList* Cursor_fetchrows(struct Cursor* cursor, size_t n)
{
    RETCODE retcode = NO_MORE_ROWS;

    /* Buffer all rows. */
    struct RowBuffer* rowbuffers = NULL;
    struct ResultSetDescription* description = cursor->description;
    size_t rowsize = ResultSetDescription_RowBuffer_size(description);

    size_t rows; /* count of rows processed */

    DBPROCESS* dbproc = Connection_DBPROCESS(cursor->connection);

    /* Verify there are results */
    if (!cursor->description)
    {
        PyErr_Format(PyExc_tds_InterfaceError, "no results");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    {
        struct RowBuffer* last_rowbuffer = NULL;
        size_t colnum;
        for (rows = 0; rows < n || FETCH_ALL == n; ++rows)
        {
            /* The offset to the next column buffer in the row. */
            size_t offset = 0;

            struct RowBuffer* new_rowbuffer;

            retcode = dbnextrow(dbproc);
            if ((NO_MORE_ROWS == retcode) || (FAIL == retcode))
            {
                break;
            }
            assert(BUF_FULL != retcode);

            new_rowbuffer = (struct RowBuffer*)tds_mem_malloc(rowsize);
            if (!new_rowbuffer)
            {
                ResultSetDescription_RowBuffer_free(description, rowbuffers);
                rowbuffers = NULL;
                break;
            }
            memset(new_rowbuffer, 0, rowsize);

            if (!rowbuffers)
            {
                rowbuffers = last_rowbuffer = new_rowbuffer;
            }
            else
            {
                last_rowbuffer->next = new_rowbuffer;
                last_rowbuffer = new_rowbuffer;
            }

            for (colnum = 1; colnum <= description->ncolumns; ++colnum)
            {
                const struct Column* column = &description->columns[colnum - 1];
                struct ColumnBuffer* colbuffer = (struct ColumnBuffer*)(((char*)last_rowbuffer->columns) + offset);

                const BYTE* data;
                DBINT ndata;

                void* dest;

                if (REG_ROW == retcode)
                {
                    colbuffer->tdstype = (enum TdsType)column->dbcol.Type;
                    data = dbdata(dbproc, (DBINT)colnum);
                    ndata = dbdatlen(dbproc, (DBINT)colnum);
                }
                else
                {
                    /* retcode is a compute ID. */
                    int type = dbalttype(dbproc, retcode, (DBINT)colnum);
                    if (-1 != type)
                    {
                        colbuffer->tdstype = (enum TdsType)type;
                        data = dbadata(dbproc, retcode, (DBINT)colnum);
                        ndata = dbadlen(dbproc, retcode, (DBINT)colnum);
                    }
                    else
                    {
                        /* For missing compute columns, return None. */
                        colbuffer->tdstype = (enum TdsType)column->dbcol.Type;
                        data = NULL;
                        ndata = 0;
                    }
                }

                if (Column_IsVariableLength(&column->dbcol))
                {
                    /*
                        Allocate a buffer for the variable length data.
                        `data` will be NULL if the value is NULL.
                    */
                    if (data)
                    {
                        colbuffer->data.variable = tds_mem_malloc((size_t)ndata);
                        if (!colbuffer->data.variable)
                        {
                            ResultSetDescription_RowBuffer_free(description, rowbuffers);
                            rowbuffers = NULL;
                            break;
                        }
                    }
                    dest = colbuffer->data.variable;
                }
                else
                {
                    /* Fixed length data buffer was allocated as part of the row buffer. */
                    dest = &colbuffer->data.fixed;
                }
                colbuffer->size = (size_t)ndata;
                memcpy(dest, data, colbuffer->size);

                offset += ColumnBuffer_size(&column->dbcol);
            }
        }
    }
    Py_END_ALLOW_THREADS

    /* Update the rows read count before returning any errors. */
    cursor->rowsread += rows;

    if (NO_MORE_ROWS != retcode)
    {
        if (!rowbuffers)
        {
            PyErr_NoMemory();
            return NULL;
        }
    }

    if (FAIL == retcode)
    {
        ResultSetDescription_RowBuffer_free(description, rowbuffers);
        Connection_raise(cursor->connection);
        return NULL;
    }

    return RowList_create(description, rows, rowbuffers);
}


/* https://www.python.org/dev/peps/pep-0249/#fetchone */
static const char s_Cursor_fetchone_doc[] =
    "fetchone()\n"
    "\n"
    "Fetch the next row of a query result set, returning a single sequence, or\n"
    "None when no more data is available.\n"
    "\n"
    ":pep:`0249#fetchone`\n"
    "\n"
    ":return: A result row or None.\n";

static PyObject* Cursor_fetchone(PyObject* self, PyObject* args)
{
    struct Cursor* cursor = (struct Cursor*)self;
    struct RowList* rowlist;
    PyObject* row = NULL;

    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    rowlist = Cursor_fetchrows(cursor, 1);
    if (rowlist)
    {
        if (Py_SIZE(rowlist) > 0)
        {
            row = RowList_item((PyObject*)rowlist, 0);
        }
        else
        {
            row = Py_None;
            Py_INCREF(row);
        }
        Py_DECREF(rowlist);
    }

    return row;
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#fetchmany */
static const char s_Cursor_fetchmany_doc[] =
    "fetchmany(size=self.arraysize)\n"
    "\n"
    "Fetch the next set of rows of a query result, returning a sequence of\n"
    "sequences. An empty sequence is returned when no more rows are available.\n"
    "\n"
    ":pep:`0249#fetchmany`\n"
    "\n"
    ":return: A sequence of result rows.\n";

static PyObject* Cursor_fetchmany(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct Cursor* cursor = (struct Cursor*)self;

    static char* s_kwlist[] =
    {
        "size",
        NULL
    };
    unsigned PY_LONG_LONG size = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|K", s_kwlist, &size))
    {
        return NULL;
    }
    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    return (PyObject*)Cursor_fetchrows(cursor, (size_t)((size) ? size : cursor->arraysize));
}

/* https://www.python.org/dev/peps/pep-0249/#fetchall */
static const char s_Cursor_fetchall_doc[] =
    "fetchall()\n"
    "\n"
    "Fetch all (remaining) rows of a query result, returning them as a\n"
    "sequence of sequences.\n"
    "\n"
    ":pep:`0249#fetchall`\n"
    "\n"
    ":return: A sequence of result rows.\n";

static PyObject* Cursor_fetchall(PyObject* self, PyObject* args)
{
    struct Cursor* cursor = (struct Cursor*)self;
    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    return (PyObject*)Cursor_fetchrows(cursor, FETCH_ALL);
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#nextset */
static const char s_Cursor_nextset_doc[] =
    "nextset()\n"
    "\n"
    "Skip to the next available set, discarding any remaining rows from the\n"
    "current set.\n"
    "\n"
    ":pep:`0249#nextset`\n"
    "\n"
    ":return: True if there was another result set or None if not.\n";

static PyObject* Cursor_nextset(PyObject* self, PyObject* args)
{
    RETCODE retcode;
    int error;

    struct Cursor* cursor = (struct Cursor*)self;
    Cursor_verify_open(cursor);
    Cursor_verify_connection_open(cursor);

    Py_BEGIN_ALLOW_THREADS

        error = Cursor_next_resultset(cursor, &retcode);

    Py_END_ALLOW_THREADS

    if (error)
    {
        if (FAIL == retcode)
        {
            Connection_raise(cursor->connection);
        }
        else
        {
            /* Assume out of memory. */
            PyErr_NoMemory();
        }
        return NULL;
    }

    if (cursor->description)
    {
        Py_RETURN_TRUE;
    }
    else
    {
        Py_RETURN_NONE;
    }
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#setinputsizes */
static const char s_Cursor_setinputsizes_doc[] =
    "setinputsizes()\n"
    "\n"
    "This method has no effect.\n"
    "\n"
    ":pep:`0249#setinputsizes`\n";

static PyObject* Cursor_setinputsizes(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
    UNUSED(self);
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#setoutputsize */
static const char s_Cursor_setoutputsize_doc[] =
    "setoutputsize()\n"
    "\n"
    "This method has no effect.\n"
    "\n"
    ":pep:`0249#setoutputsize`\n";

static PyObject* Cursor_setoutputsize(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
    UNUSED(self);
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#next */
static const char s_Cursor_next_doc[] =
    "next()\n"
    "\n"
    "Return the next row from the currently executing SQL statement using the\n"
    "same semantics as :py:meth:`.fetchone`. A :py:exc:`StopIteration` exception\n"
    "is raised when the result set is exhausted.\n"
    "\n"
    ":pep:`0249#next`\n";

static PyObject* Cursor_next_internal(PyObject* self, PyObject* args)
{
    PyObject* row = Cursor_fetchone(self, args);
    if (Py_None == row)
    {
        PyErr_SetNone(PyExc_StopIteration);
        Py_DECREF(row);
        row = NULL;
    }
    return row;
}

static PyObject* Cursor_next(PyObject* self, PyObject* args)
{
    (void)warn_extension_used("cursor.next()");
    return Cursor_next_internal(self, args);
}

static const char s_Cursor___enter___doc[] =
    "__enter__()\n"
    "\n"
    "Enter the cursor's runtime context. On exit, the cursor is\n"
    "closed automatically.\n"
    "\n"
    ":return: The cursor object.\n"
    ":rtype: ctds.Cursor\n";

static PyObject* Cursor___enter__(PyObject* self, PyObject* args)
{
    Py_INCREF(self);

    return self;
    UNUSED(args);
}

static const char s_Cursor___exit___doc[] =
    "__exit__(exc_type, exc_val, exc_tb)\n"
    "\n"
    "Exit the cursor's runtime context, closing the cursor.\n"
    "\n"
    ":param type exc_type: The exception type, if an exception\n"
    "    is raised in the context, otherwise `None`.\n"
    ":param Exception exc_val: The exception value, if an exception\n"
    "    is raised in the context, otherwise `None`.\n"
    ":param object exc_tb: The exception traceback, if an exception\n"
    "    is raised in the context, otherwise `None`.\n"
    "\n"
    ":rtype: None\n";

static PyObject* Cursor___exit__(PyObject* self, PyObject* args)
{
    return Cursor_close(self, NULL);
    UNUSED(args);
}

static PyMethodDef Cursor_methods[] = {
    /* ml_name, ml_meth, ml_flags, ml_doc */
    /* DB API-2.0 required methods. */
    { "callproc",      Cursor_callproc,              METH_VARARGS,                  s_Cursor_callproc_doc },
    { "close",         Cursor_close,                 METH_NOARGS,                   s_Cursor_close_doc },
    { "execute",       Cursor_execute,               METH_VARARGS,                  s_Cursor_execute_doc },
    { "executemany",   Cursor_executemany,           METH_VARARGS,                  s_Cursor_executemany_doc },
    { "fetchone",      Cursor_fetchone,              METH_NOARGS,                   s_Cursor_fetchone_doc },
    /*
        fetchmany does not have *args, but the flag is required for kwargs to function properly.
        See https://bugs.python.org/issue15657.
    */
    { "fetchmany",     (PyCFunction)Cursor_fetchmany, METH_VARARGS | METH_KEYWORDS, s_Cursor_fetchmany_doc },
    { "fetchall",      Cursor_fetchall,               METH_NOARGS,                  s_Cursor_fetchall_doc },
    { "nextset",       Cursor_nextset,                METH_NOARGS,                  s_Cursor_nextset_doc },
    { "setinputsizes", Cursor_setinputsizes,          METH_VARARGS,                 s_Cursor_setinputsizes_doc },
    { "setoutputsize", Cursor_setoutputsize,          METH_VARARGS,                 s_Cursor_setoutputsize_doc },

    /*
        DB API-2.0 extensions. See
        https://www.python.org/dev/peps/pep-0249/#optional-db-api-extensions
    */
    { "next",          Cursor_next,                  METH_NOARGS,                   s_Cursor_next_doc },

    /* Non-DB API 2.0 methods. */
    { "__enter__",     Cursor___enter__,             METH_NOARGS,                   s_Cursor___enter___doc },
    { "__exit__",      Cursor___exit__,              METH_VARARGS,                  s_Cursor___exit___doc },
    { NULL,            NULL,                         0,                             NULL }
};

static PyObject* Cursor_iter(PyObject* self)
{
    warn_extension_used("cursor.__iter__()");
    Py_INCREF(self);
    return self;
}

static PyObject* Cursor_iternext(PyObject* self)
{
    return Cursor_next_internal(self, NULL);
}

PyObject* Cursor_create(struct Connection* connection)
{
    struct Cursor* cursor = PyObject_New(struct Cursor, &CursorType);
    if (NULL != cursor)
    {
        memset((char*)cursor + offsetof(struct Cursor, connection), 0,
               (sizeof(struct Cursor) - offsetof(struct Cursor, connection)));

        /*
            Defaults the arraysize to 1, per
            https://www.python.org/dev/peps/pep-0249/#arraysize
        */
        cursor->arraysize = 1;

        Py_INCREF((PyObject*)connection);
        cursor->connection = connection;
    }
    else
    {
        PyErr_NoMemory();
    }


    return (PyObject*)cursor;
}

PyTypeObject CursorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    STRINGIFY(tds) ".Cursor",     /* tp_name */
    sizeof(struct Cursor),        /* tp_basicsize */
    0,                            /* tp_itemsize */
    Cursor_dealloc,               /* tp_dealloc */
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
    s_tds_Cursor_doc,             /* tp_doc */
    NULL,                         /* tp_traverse */
    NULL,                         /* tp_clear */
    NULL,                         /* tp_richcompare */
    0,                            /* tp_weaklistoffset */
    Cursor_iter,                  /* tp_iter */
    Cursor_iternext,              /* tp_iternext */
    Cursor_methods,               /* tp_methods */
    NULL,                         /* tp_members */
    Cursor_getset,                /* tp_getset */
    NULL,                         /* tp_base */
    NULL,                         /* tp_dict */
    NULL,                         /* tp_descr_get */
    NULL,                         /* tp_descr_set */
    0,                            /* tp_dictoffset */
    NULL,                         /* tp_init */
    NULL,                         /* tp_alloc */
    NULL,                         /* tp_new */
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

PyTypeObject* CursorType_init(void)
{
    do
    {
        if (0 != Description_init()) break;
        if (0 != PyType_Ready(&RowType)) break;
        if (0 != PyType_Ready(&RowListType)) break;
        if (0 != PyType_Ready(&CursorType)) break;
        return &CursorType;
    } while (0);

    return NULL;
}
