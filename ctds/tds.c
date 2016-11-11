#include "include/push_warnings.h"
#include <Python.h>
#include <sybdb.h>
#include <ctpublic.h>
#include "include/pop_warnings.h"

#include <stdint.h>

#include "include/connection.h"
#include "include/cursor.h"
#include "include/macros.h"
#include "include/parameter.h"
#include "include/pyutils.h"
#include "include/tds.h"
#include "include/type.h"

#ifdef __GNUC__
/*
    Ignore "string length ‘1189’ is greater than the length ‘509’ ISO C90
    compilers are required to support [-Werror=overlength-strings]".
*/
#  pragma GCC diagnostic ignored "-Woverlength-strings"
#endif /* ifdef __GNUC__ */

PyObject* PyExc_tds_Warning = NULL;
PyObject* PyExc_tds_Error = NULL;
PyObject* PyExc_tds_InterfaceError = NULL;
PyObject* PyExc_tds_DatabaseError = NULL;
PyObject* PyExc_tds_DataError = NULL;
PyObject* PyExc_tds_OperationalError = NULL;
PyObject* PyExc_tds_IntegrityError = NULL;
PyObject* PyExc_tds_InternalError = NULL;
PyObject* PyExc_tds_ProgrammingError = NULL;
PyObject* PyExc_tds_NotSupportedError = NULL;

#ifndef CTDS_MAJOR_VERSION
#  define CTDS_MAJOR_VERSION 0
#endif

#ifndef CTDS_MINOR_VERSION
#  define CTDS_MINOR_VERSION 0
#endif

#ifndef CTDS_PATCH_VERSION
#  define CTDS_PATCH_VERSION 0
#endif

/**
   tds module definition.
 */
static const char s_tds_doc[] =
    "DB API 2.0-compliant Python library for TDS-based databases.\n";

/* Default values for connect. */
#ifndef DEFAULT_PORT
#  define DEFAULT_PORT 1433
#endif
#ifndef DEFAULT_USERNAME
#  define DEFAULT_USERNAME ""
#endif
#ifndef DEFAULT_PASSWORD
#  define DEFAULT_PASSWORD ""
#endif
#ifndef DEFAULT_APPNAME
#  define DEFAULT_APPNAME "ctds"
#endif
#ifndef DEFAULT_LOGIN_TIMEOUT
#  define DEFAULT_LOGIN_TIMEOUT 5
#endif
#ifndef DEFAULT_TIMEOUT
#  define DEFAULT_TIMEOUT 5
#endif
#ifndef DEFAULT_AUTOCOMMIT
#  define DEFAULT_AUTOCOMMIT 0
#endif
#ifndef DEFAULT_ANSI_DEFAULTS
#  define DEFAULT_ANSI_DEFAULTS 1
#endif
#ifndef DEFAULT_ENABLE_BCP
#  define DEFAULT_ENABLE_BCP 1
#endif

#if DEFAULT_AUTOCOMMIT
#  define DEFAULT_AUTOCOMMIT_STR "True"
#else
#  define DEFAULT_AUTOCOMMIT_STR "False"
#endif

#if DEFAULT_ANSI_DEFAULTS
#  define DEFAULT_ANSI_DEFAULTS_STR "True"
#else
#  define DEFAULT_ANSI_DEFAULTS_STR "False"
#endif

#if DEFAULT_ENABLE_BCP
#  define DEFAULT_ENABLE_BCP_STR "True"
#else
#  define DEFAULT_ENABLE_BCP_STR "False"
#endif


static const char s_tds_connect_doc[] =
    "connect(server, "
            "port=" STRINGIFY(DEFAULT_PORT) ", "
            "instance=None, "
            "user='" DEFAULT_USERNAME "', "
            "password='" DEFAULT_PASSWORD "', "
            "database=None, "
            "appname='" DEFAULT_APPNAME "', "
            "login_timeout=" STRINGIFY(DEFAULT_LOGIN_TIMEOUT) ", "
            "timeout=" STRINGIFY(DEFAULT_LOGIN_TIMEOUT) ", "
            "tds_version=None, "
            "autocommit=" DEFAULT_AUTOCOMMIT_STR ", "
            "ansi_defaults=" DEFAULT_ANSI_DEFAULTS_STR ", "
            "enable_bcp=" DEFAULT_ENABLE_BCP_STR ")\n"
    "\n"
    "Connect to a database.\n"
    "\n"
    ".. note::\n"
    "\n"
    "    :py:meth:`ctds.Connection.close` should be called when the returned\n"
    "    connection object is no longer required.\n"
    "\n"
    ":pep:`0249#connect`\n"
    "\n"
    ":param str server: The database server host.\n"

    ":param int port: The database server port. This value is ignored if\n"
    "    `instance` is provided.\n"

    ":param str instance: An optional database instance to connect to.\n"

    ":param str user: The database server username.\n"

    ":param str password: The database server password.\n"

    ":param str database: An optional database to initially connect to.\n"

    ":param str appname: An optional application name to associate with\n"
    "    the connection.\n"

    ":param int login_timeout: An optional login timeout, in seconds.\n"

    ":param int timeout: An optional timeout for database requests, in seconds.\n"

    ":param str tds_version: The TDS protocol version to use. If None is specified,\n"
    "    the highest version supported by FreeTDS will be used.\n"

    ":param bool autocommit: Autocommit transactions on the connection.\n"

    ":param bool ansi_defaults: Set `ANSI_DEFAULTS` and related settings to mimic ODBC drivers.\n"

    ":param bool enable_bcp: Enable bulk copy support on the connection. This is required for\n"
    "    :py:meth:`.bulk_insert` to function.\n"

    ":return: A new `Connection` object connected to the database.\n"
    ":rtype: Connection\n";

/**
   https://www.python.org/dev/peps/pep-0249/#connect
*/
static PyObject* tds_connect(PyObject* self, PyObject* args, PyObject* kwargs)
{
    static char* s_kwlist[] =
    {
        "server",
        "port",
        "instance",
        "user",
        "password",
        "database",
        "appname",
        "login_timeout",
        "timeout",
        "tds_version",
        "autocommit",
        "ansi_defaults",
        "enable_bcp",
        NULL
    };

    char* server = NULL;
    uint16_t port = DEFAULT_PORT;
    char* instance = NULL;
    char* username = DEFAULT_USERNAME;
    char* password = DEFAULT_PASSWORD;
    char* database = NULL;
    char* appname = DEFAULT_APPNAME;
    unsigned int login_timeout = DEFAULT_LOGIN_TIMEOUT;
    unsigned int timeout = DEFAULT_TIMEOUT;
    char* tds_version = NULL;
    PyObject* autocommit = (DEFAULT_AUTOCOMMIT) ? Py_True : Py_False;
    PyObject* ansi_defaults = (DEFAULT_ANSI_DEFAULTS) ? Py_True : Py_False;
    PyObject* enable_bcp = (DEFAULT_ENABLE_BCP) ? Py_True : Py_False;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|HzzzzzIIzO!O!O!", s_kwlist, &server,
                                     &port, &instance, &username, &password,
                                     &database, &appname, &login_timeout,
                                     &timeout, &tds_version, &PyBool_Type, &autocommit,
                                     &PyBool_Type, &ansi_defaults,
                                     &PyBool_Type, &enable_bcp))
    {
        return NULL;
    }

    return Connection_create(server, port, instance, username, password,
                             database, appname, login_timeout, timeout,
                             tds_version, (Py_True == autocommit),
                             (Py_True == ansi_defaults),
                             (Py_True == enable_bcp));

    UNUSED(self);
}

#define VERIFY_DATETIME_PART(_part, _min, _max) \
    if ((_part) < (_min) || (_part) > (_max)) \
    { \
        PyErr_Format(PyExc_ValueError, STRINGIFY(_part) " is out of range"); \
        return NULL; \
    }

/* https://www.python.org/dev/peps/pep-0249/#date */
static const char s_tds_Date_doc[] =
    "Date(year, month, day)\n"
    "\n"
    "This function constructs an object holding a date value.\n"
    "\n"
    ":pep:`0249#date`\n"
    "\n"
    ":param int year: The year.\n"

    ":param int month: The month.\n"

    ":param int day: The day.\n"

    ":return: A new `date` object.\n"
    ":rtype: datetime.date\n";

static PyObject* tds_Date(PyObject* self, PyObject* args)
{
    int year, month, day;
    if (!PyArg_ParseTuple(args, "iii", &year, &month, &day))
    {
        return NULL;
    }
    VERIFY_DATETIME_PART(year, 1, 9999);
    VERIFY_DATETIME_PART(month, 1, 12);
    VERIFY_DATETIME_PART(day, 1, 31);

    return PyDate_FromDate_(year, month, day);
    UNUSED(self);
}

/* https://www.python.org/dev/peps/pep-0249/#time */
static const char s_tds_Time_doc[] =
    "Time(hour, minute, second)\n"
    "\n"
    "This function constructs an object holding a time value.\n"
    "\n"
    ":pep:`0249#time`\n"
    "\n"
    ":param int hour: The hour.\n"

    ":param int minute: The minute.\n"

    ":param int second: The second.\n"

    ":return: A new `time` object.\n"
    ":rtype: datetime.time\n";

static PyObject* tds_Time(PyObject* self, PyObject* args)
{
    int hour, minute, second;
    if (!PyArg_ParseTuple(args, "iii", &hour, &minute, &second))
    {
        return NULL;
    }
    VERIFY_DATETIME_PART(hour, 0, 23);
    VERIFY_DATETIME_PART(minute, 0, 59);
    VERIFY_DATETIME_PART(second, 0, 59);

    return PyTime_FromTime_(hour, minute, second, 0);
    UNUSED(self);
}

/* https://www.python.org/dev/peps/pep-0249/#timestamp */
static const char s_tds_Timestamp_doc[] =
    "Timestamp(year, month, day, hour, minute, second)\n"
    "\n"
    "This function constructs an object holding a time stamp value.\n"
    "\n"
    ":pep:`0249#timestamp`\n"
    "\n"
    ":param int year: The year.\n"

    ":param int month: The month.\n"

    ":param int day: The day.\n"

    ":param int hour: The hour.\n"

    ":param int minute: The minute.\n"

    ":param int second: The second.\n"

    ":return: A new `datetime` object.\n"
    ":rtype: datetime.datetime\n";

static PyObject* tds_Timestamp(PyObject* self, PyObject* args)
{
    int year, month, day, hour, minute, second;
    if (!PyArg_ParseTuple(args, "iiiiii", &year, &month, &day, &hour, &minute, &second))
    {
        return NULL;
    }

    VERIFY_DATETIME_PART(year, 1, 9999);
    VERIFY_DATETIME_PART(month, 1, 12);
    VERIFY_DATETIME_PART(day, 1, 31);
    VERIFY_DATETIME_PART(hour, 0, 23);
    VERIFY_DATETIME_PART(minute, 0, 59);
    VERIFY_DATETIME_PART(second, 0, 59);

    return PyDateTime_FromDateAndTime_(year, month, day, hour, minute, second, 0);
    UNUSED(self);
}

/* https://www.python.org/dev/peps/pep-0249/#datefromticks */
static const char s_tds_DateFromTicks_doc[] =
    "DateFromTicks(ticks)\n"
    "\n"
    "This function constructs an object holding a date value from the given\n"
    "ticks value (number of seconds since the epoch; see the documentation of\n"
    "the standard Python time module for details).\n"
    "\n"
    ":pep:`0249#datefromticks`\n"
    "\n"
    ":param int ticks: The number of seconds since the epoch.\n"

    ":return: A new date object.\n"
    ":rtype: datetime.date\n";

static PyObject* tds_DateFromTicks(PyObject* self, PyObject* args)
{
    return PyDate_FromTimestamp_(args);
    UNUSED(self);
}

/* https://www.python.org/dev/peps/pep-0249/#timefromticks */
static const char s_tds_TimeFromTicks_doc[] =
    "TimeFromTicks(ticks)\n"
    "\n"
    "This function constructs an object holding a time value from the given\n"
    "ticks value (number of seconds since the epoch; see the documentation of\n"
    "the standard Python time module for details).\n"
    "\n"
    ":pep:`0249#timefromticks`\n"
    "\n"
    ":param int ticks: The number of seconds since the epoch.\n"

    ":return: A new `time` object.\n"
    ":rtype: datetime.time\n";

static PyObject* tds_TimeFromTicks(PyObject* self, PyObject* args)
{
    PyObject* time = NULL;
    PyObject* datetime = PyDateTime_FromTimestamp_(args);
    if (datetime)
    {
        time = PyTime_FromTime_(PyDateTime_DATE_GET_HOUR_(datetime),
                                PyDateTime_DATE_GET_MINUTE_(datetime),
                                PyDateTime_DATE_GET_SECOND_(datetime),
                                PyDateTime_DATE_GET_MICROSECOND_(datetime));
        Py_DECREF(datetime);
    }

    return time;
    UNUSED(self);
}

/* https://www.python.org/dev/peps/pep-0249/#timestampfromticks */
static const char s_tds_TimestampFromTicks_doc[] =
    "TimestampFromTicks(ticks)\n"
    "\n"
    "This function constructs an object holding a time stamp value from the\n"
    "given ticks value (number of seconds since the epoch; see the\n"
    "documentation of the standard Python time module for details).\n"
    "\n"
    ":pep:`0249#timestampfromticks`\n"
    "\n"
    ":param int ticks: The number of seconds since the epoch.\n"

    ":return: A new `datetime` object.\n"
    ":rtype: datetime.datetime\n";

static PyObject* tds_TimestampFromTicks(PyObject* self, PyObject* args)
{
    return PyDateTime_FromTimestamp_(args);
    UNUSED(self);
}

/* https://www.python.org/dev/peps/pep-0249/#binary */
static const char s_tds_Binary_doc[] =
    "Binary(string)\n"
    "\n"
    "This function constructs an object capable of holding a binary (long)\n"
    "string value.\n"
    "\n"
    ":pep:`0249#binary`\n"
    "\n"
    ":param str string: The string value to convert to binary.\n"

    ":return: A new binary object.\n"
    ":rtype: SqlBinary";

static PyObject* tds_Binary(PyObject* self, PyObject* args)
{
    return SqlBinary_create(self, args, NULL);
}

static PyMethodDef s_tds_methods[] = {
    { "connect",            (PyCFunction)tds_connect, METH_VARARGS | METH_KEYWORDS, s_tds_connect_doc },
    { "Date",               tds_Date,                 METH_VARARGS,                 s_tds_Date_doc },
    { "Time",               tds_Time,                 METH_VARARGS,                 s_tds_Time_doc },
    { "Timestamp",          tds_Timestamp,            METH_VARARGS,                 s_tds_Timestamp_doc },
    { "DateFromTicks",      tds_DateFromTicks,        METH_VARARGS,                 s_tds_DateFromTicks_doc },
    { "TimeFromTicks",      tds_TimeFromTicks,        METH_VARARGS,                 s_tds_TimeFromTicks_doc },
    { "TimestampFromTicks", tds_TimestampFromTicks,   METH_VARARGS,                 s_tds_TimestampFromTicks_doc },
    { "Binary",             tds_Binary,               METH_VARARGS,                 s_tds_Binary_doc },
    { NULL,                 NULL,                     0,                            NULL }
};

static PyObject* version_info = NULL;

#if PY_MAJOR_VERSION >= 3
static void tds_free(void* ctx)
{
    dbexit();
    PyDateTimeType_free();
    PyDecimalType_free();
    PyUuidType_free();

    Py_XDECREF(PyExc_tds_Warning);
    Py_XDECREF(PyExc_tds_Error);
    Py_XDECREF(PyExc_tds_InterfaceError);
    Py_XDECREF(PyExc_tds_DatabaseError);
    Py_XDECREF(PyExc_tds_DataError);
    Py_XDECREF(PyExc_tds_OperationalError);
    Py_XDECREF(PyExc_tds_IntegrityError);
    Py_XDECREF(PyExc_tds_InternalError);
    Py_XDECREF(PyExc_tds_ProgrammingError);
    Py_XDECREF(PyExc_tds_NotSupportedError);
    Py_XDECREF(version_info);
    UNUSED(ctx);
}
#endif /* if PY_MAJOR_VERSION >= 3 */

#if PY_MAJOR_VERSION >= 3
#  define PyErr_NewExceptionWithDoc_ PyErr_NewExceptionWithDoc
#else /* if PY_MAJOR_VERSION >= 3 */
#  if PY_VERSION_HEX < 0x02070000
static PyObject* PyErr_NewExceptionWithDoc_(const char* name, const char* doc, PyObject* base, PyObject* dict)
{
    return PyErr_NewException((char*)name, base, dict);
    UNUSED(doc);
}
#  else
#    define PyErr_NewExceptionWithDoc_(_name, _doc, _base, _dict) \
         PyErr_NewExceptionWithDoc((char*)(_name), (char*)_doc, (_base), (_dict))
#  endif /* if PY_VERSION_HEX < 0x02070000 */
#endif /* else if PY_MAJOR_VERSION >= 3 */


/**
   https://www.python.org/dev/peps/pep-0249/#warning
*/
static const char s_tds_Warning_doc[] =
    "Warning\n"
    "\n"
    ":pep:`0249#warning`\n"
    "\n"
    "Exception raised for important warnings like data truncations while\n"
    "inserting, etc.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#error
*/
static const char s_tds_Error_doc[] =
    "Error\n"
    "\n"
    ":pep:`0249#error`\n"
    "\n"
    "Exception that is the base class of all other error exceptions. You\n"
    "can use this to catch all errors with one single except statement.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#interfaceerror
*/
static const char s_tds_InterfaceError_doc[] =
    "InterfaceError\n"
    "\n"
    ":pep:`0249#interfaceerror`\n"
    "\n"
    "Exception raised for errors that are related to the database interface\n"
    "rather than the database itself.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#databaseerror
*/
static const char s_tds_DatabaseError_doc[] =
    "DatabaseError\n"
    "\n"
    ":pep:`0249#databaseerror`\n"
    "\n"
    "Exception raised for errors that are related to the database.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#dataerror
*/
static const char s_tds_DataError_doc[] =
    "DataError\n"
    "\n"
    ":pep:`0249#dataerror`\n"
    "\n"
    "Exception raised for errors that are due to problems with the\n"
    "processed data like division by zero, numeric value out of range,\n"
    "etc.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#operationalerror
*/
static const char s_tds_OperationalError_doc[] =
    "OperationalError\n"
    "\n"
    ":pep:`0249#operationalerror`\n"
    "\n"
    "Exception raised for errors that are related to the database's\n"
    "operation and not necessarily under the control of the programmer,\n"
    "e.g. an unexpected disconnect occurs, the data source name is not\n"
    "found, a transaction could not be processed, a memory allocation\n"
    "error occurred during processing, etc.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#integrityerror
*/
static const char s_tds_IntegrityError_doc[] =
    "IntegrityError\n"
    "\n"
    ":pep:`0249#integrityerror`\n"
    "\n"
    "Exception raised when the relational integrity of the database is\n"
    "affected, e.g. a foreign key check fails.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#programmingerror
*/
static const char s_tds_ProgrammingError_doc[] =
    "ProgrammingError\n"
    "\n"
    ":pep:`0249#programmingerror`\n"
    "\n"
    "Exception raised for programming errors, e.g. table not found or\n"
    "already exists, syntax error in the SQL statement, wrong number of\n"
    "parameters specified, etc.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#internalerror
*/
static const char s_tds_InternalError_doc[] =
    "InternalError\n"
    "\n"
    ":pep:`0249#internalerror`\n"
    "\n"
    "Exception raised when the database encounters an internal error,\n"
    "e.g. the cursor is not valid anymore, the transaction is out of\n"
    "sync, etc.\n";

/**
   https://www.python.org/dev/peps/pep-0249/#notsupportederror
*/
static const char s_tds_NotSupportedError_doc[] =
    "NotSupportedError\n"
    "\n"
    ":pep:`0249#notsupportederror`\n"
    "\n"
    "Exception raised in case a method or database API was used which is\n"
    "not supported by the database, e.g. calling \n"
    ":py:func:`~ctds.Connection.rollback()` on a connection that does not\n"
    "support transactions or has transactions turned off.\n";


#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC init_tds(void)
{
#  define FAIL_MODULE_INIT do { Py_XDECREF(module); return; } while (0)
#else /* if PY_MAJOR_VERSION < 3 */
PyMODINIT_FUNC PyInit__tds(void)
{
#  define FAIL_MODULE_INIT do { Py_XDECREF(module); return NULL; } while (0)
#endif /* else if PY_MAJOR_VERSION < 3 */

    char freetds_version[100];
    int written;

    /* Create module object. */
#if PY_MAJOR_VERSION < 3
    PyObject* module = Py_InitModule3("_tds", s_tds_methods, s_tds_doc);
    Py_XINCREF(module); /* Py_InitModule3 returns a borrowed reference */
#else /* if PY_MAJOR_VERSION < 3 */
    static struct PyModuleDef moduledef =
    {
        PyModuleDef_HEAD_INIT,
        "_tds",        /* m_name */
        s_tds_doc,     /* m_doc */
        -1,            /* m_size */
        s_tds_methods, /* m_methods */
        NULL,          /* m_slots */
        NULL,          /* m_traverse */
        NULL,          /* m_clear */
        tds_free       /* m_free */
    };
    PyObject* module = PyModule_Create(&moduledef);
#endif /* else if PY_MAJOR_VERSION < 3 */

    if (!module) FAIL_MODULE_INIT;

    /***** Define module-level constants. *****/

    /**
       https://www.python.org/dev/peps/pep-0249/#threadsafety
    */
    if (0 != PyModule_AddIntConstant(module, "threadsafety", 1)) FAIL_MODULE_INIT;

    /**
       https://www.python.org/dev/peps/pep-0249/#apilevel
    */
    if (0 != PyModule_AddStringConstant(module, "apilevel", "2.0")) FAIL_MODULE_INIT;

    /**
       https://www.python.org/dev/peps/pep-0249/#pyformat
    */
    if (0 != PyModule_AddStringConstant(module, "paramstyle", "numeric")) FAIL_MODULE_INIT;

    if (0 != PyModule_AddIntMacro(module, TDSCHAR)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSVARCHAR)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSNCHAR)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSNVARCHAR)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSTEXT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSNTEXT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSBIT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSBITN)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSINTN)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSTINYINT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSSMALLINT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSINT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSBIGINT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSFLOAT)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSFLOATN)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSREAL)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSDATETIME)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSSMALLDATETIME)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSDATETIMEN)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSDATE)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSTIME)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSDATETIME2)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSIMAGE)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSSMALLMONEY)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSMONEY)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSMONEYN)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSNUMERIC)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSDECIMAL)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSBINARY)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSVARBINARY)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSGUID)) FAIL_MODULE_INIT;
    if (0 != PyModule_AddIntMacro(module, TDSVOID)) FAIL_MODULE_INIT;

    /***** Define exception types. *****/

    #define ADD_EXCEPTION_TO_MODULE(name) \
        Py_INCREF(PyExc_tds_##name); /* add ref for module to steal (global variable owns existing ref) */ \
        PyModule_AddObject(module, #name, PyExc_tds_##name);

#if PY_MAJOR_VERSION >= 3
#  define PyExc_StandardError PyExc_Exception
#endif

    if (!(PyExc_tds_Warning = PyErr_NewExceptionWithDoc_("_tds.Warning", s_tds_Warning_doc, PyExc_StandardError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(Warning);
    if (!(PyExc_tds_Error = PyErr_NewExceptionWithDoc_("_tds.Error", s_tds_Error_doc, PyExc_StandardError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(Error);
    if (!(PyExc_tds_InterfaceError = PyErr_NewExceptionWithDoc_("_tds.InterfaceError", s_tds_InterfaceError_doc, PyExc_tds_Error, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(InterfaceError);
    if (!(PyExc_tds_DatabaseError = PyErr_NewExceptionWithDoc_("_tds.DatabaseError", s_tds_DatabaseError_doc, PyExc_tds_Error, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(DatabaseError);
    if (!(PyExc_tds_DataError = PyErr_NewExceptionWithDoc_("_tds.DataError", s_tds_DataError_doc, PyExc_tds_DatabaseError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(DataError);
    if (!(PyExc_tds_OperationalError = PyErr_NewExceptionWithDoc_("_tds.OperationalError", s_tds_OperationalError_doc, PyExc_tds_DatabaseError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(OperationalError);
    if (!(PyExc_tds_IntegrityError = PyErr_NewExceptionWithDoc_("_tds.IntegrityError", s_tds_IntegrityError_doc, PyExc_tds_DatabaseError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(IntegrityError);
    if (!(PyExc_tds_InternalError = PyErr_NewExceptionWithDoc_("_tds.InternalError", s_tds_InternalError_doc, PyExc_tds_DatabaseError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(InternalError);
    if (!(PyExc_tds_ProgrammingError = PyErr_NewExceptionWithDoc_("_tds.ProgrammingError", s_tds_ProgrammingError_doc, PyExc_tds_DatabaseError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(ProgrammingError);
    if (!(PyExc_tds_NotSupportedError = PyErr_NewExceptionWithDoc_("_tds.NotSupportedError", s_tds_NotSupportedError_doc, PyExc_tds_DatabaseError, NULL))) FAIL_MODULE_INIT;
    ADD_EXCEPTION_TO_MODULE(NotSupportedError);

    /***** Define custom types. *****/

    if (0 != PyModule_AddObject(module, "Connection", (PyObject*)ConnectionType_init())) FAIL_MODULE_INIT;
    if (0 != PyModule_AddObject(module, "Cursor", (PyObject*)CursorType_init())) FAIL_MODULE_INIT;
    if (0 != PyModule_AddObject(module, "Parameter", (PyObject*)ParameterType_init())) FAIL_MODULE_INIT;

    if (0 != SqlTypes_init()) FAIL_MODULE_INIT;

    /* Add SQL type wrappers (in alphabetical order). */
#define SQL_TYPE_INIT(_type) \
        if (0 != PyModule_AddObject(module, "Sql" #_type, (PyObject*)Sql ## _type ## Type_init())) FAIL_MODULE_INIT

    /* Note: types added here must be added to __init__.py */
    SQL_TYPE_INIT(BigInt);
    SQL_TYPE_INIT(Binary);
    SQL_TYPE_INIT(Char);
    SQL_TYPE_INIT(Date);
    SQL_TYPE_INIT(Decimal);
    SQL_TYPE_INIT(Int);
    SQL_TYPE_INIT(NVarChar);
    SQL_TYPE_INIT(SmallInt);
    SQL_TYPE_INIT(TinyInt);
    SQL_TYPE_INIT(VarBinary);
    SQL_TYPE_INIT(VarChar);

    /***** Import datetime module *****/
    if (!PyDateTimeType_init()) FAIL_MODULE_INIT;

    /***** Import decimal type. *****/
    if (!PyDecimalType_init()) FAIL_MODULE_INIT;

    /***** Import uuid type. *****/
    if (!PyUuidType_init()) FAIL_MODULE_INIT;

    /***** Library version *****/
    if (!(version_info = PyTuple_New(3))) FAIL_MODULE_INIT;
    PyTuple_SET_ITEM(version_info, 0, PyLong_FromLong(CTDS_MAJOR_VERSION));
    PyTuple_SET_ITEM(version_info, 1, PyLong_FromLong(CTDS_MINOR_VERSION));
    PyTuple_SET_ITEM(version_info, 2, PyLong_FromLong(CTDS_PATCH_VERSION));

    if (0 != PyModule_AddObject(module, "version_info", version_info)) FAIL_MODULE_INIT;
    Py_INCREF(version_info);

    /***** Initialize DB-lib. *****/

    /*
        FreeTDS version.

        Use the CT-library API since older versions of FreeTDS don't return
        a proper version string from dbversion()
    */
    (void)ct_config(NULL, CS_GET, CS_VERSION, freetds_version, sizeof(freetds_version), &written);
    if (0 != PyModule_AddStringConstant(module, "freetds_version", freetds_version)) FAIL_MODULE_INIT;

    if (FAIL == dbinit()) FAIL_MODULE_INIT;
    dberrhandle(Connection_dberrhandler);
    dbmsghandle(Connection_dbmsghandler);

#if PY_MAJOR_VERSION >= 3
    return module;
#endif /* if PY_MAJOR_VERSION >= 3 */
}
