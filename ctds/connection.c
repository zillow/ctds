#include "include/push_warnings.h"
#include <Python.h>
#include <sybdb.h>
#include "include/pop_warnings.h"

#include <stddef.h>

#include "include/macros.h"
#include "include/tds.h"
#include "include/connection.h"
#include "include/cursor.h"
#include "include/pyutils.h"
#include "include/parameter.h"

#ifdef __GNUC__
/*
    Ignore "string length '%d' is greater than the length '509' ISO C90
    compilers are required to support [-Werror=overlength-strings]".
*/
#  pragma GCC diagnostic ignored "-Woverlength-strings"
#endif /* ifdef __GNUC__ */

/* Platform-specific thread-local storage support. */
#ifdef __clang__
#  include <pthread.h>

#  define TLS_DECLARE(_name, _type, _destructor) \
      static pthread_key_t tls_ ## _name = 0; \
      static void _name ## _destructor(void* tls) \
      { \
          if (tls) \
          { \
              _destructor((_type*)tls); \
          } \
          tds_mem_free(tls); \
      } \
      static int _name ## _init(void) \
      { \
          return pthread_key_create(&(tls_ ## _name), _name ## _destructor); \
      } \
      static _type* _name ## _get(void) \
      { \
          _type* value = ((_type*)pthread_getspecific(tls_ ## _name)); \
          if (!value) \
          { \
              value = (_type*)tds_mem_malloc(sizeof(_type)); \
              if (value) \
              { \
                  if (0 == pthread_setspecific(tls_ ## _name, value)) \
                  { \
                      memset(value, 0, sizeof(_type)); \
                  } \
                  else \
                  { \
                      tds_mem_free(value); \
                      value = NULL; \
                  } \
              } \
          } \
          return value; \
      }

#else /* ifdef __clang__ */

#  define TLS_DECLARE(_name, _type, _destructor) \
      __thread _type tls_ ## _name; \
      static int _name ## _init(void) \
      { \
          memset(&(tls_ ## _name), 0, sizeof(_type)); \
          return 0; \
      } \
      static _type* _name ## _get(void) \
      { \
          return &(tls_ ## _name); \
      } \

#endif /* else ifdef __clang__ */

struct LastError
{
    int severity;
    int dberr;
    int oserr;
    char* dberrstr;
    char* oserrstr;
};

static void LastError_clear(struct LastError* lasterror)
{
    lasterror->severity = 0;
    lasterror->dberr = 0;
    lasterror->oserr = 0;

    tds_mem_free(lasterror->dberrstr);
    lasterror->dberrstr = NULL;

    tds_mem_free(lasterror->oserrstr);
    lasterror->oserrstr = NULL;
}

TLS_DECLARE(LastError, struct LastError, LastError_clear)

struct LastMsg
{
    DBINT msgno;
    int msgstate;
    int severity;
    char* msgtext;
    char* srvname;
    char* proc;
    int line;
};

static void LastMsg_clear(struct LastMsg* lastmsg)
{
    lastmsg->msgno = 0;
    lastmsg->msgstate = 0;
    lastmsg->severity = 0;

    tds_mem_free(lastmsg->msgtext);
    lastmsg->msgtext = NULL;

    tds_mem_free(lastmsg->srvname);
    lastmsg->srvname = NULL;

    tds_mem_free(lastmsg->proc);
    lastmsg->proc = NULL;

    lastmsg->line = 0;
}

TLS_DECLARE(LastMsg, struct LastMsg, LastMsg_clear)

#if defined(__GNUC__)
__attribute__((destructor)) void fini(void)
{
    LastError_clear(LastError_get());
    LastMsg_clear(LastMsg_get());
}
#endif


struct Connection {
    PyObject_VAR_HEAD

    LOGINREC* login;
    DBPROCESS* dbproc;

    /* Should execute calls be auto-committed? */
    bool autocommit;

    /* Last seen error, as set by the error handler. */
    struct LastError lasterror;

    /* Last seen database message, as set by the message handler. */
    struct LastMsg lastmsg;

    /*
        The query timeout for this connection.
        This is only stored here because there is currently no way to
        retrieve it from dblib.
    */
    int query_timeout;
};

static PyObject* build_lastdberr_dict(const struct LastError* lasterror)
{
    PyObject* dict = PyDict_New();
    if (dict)
    {
        if (-1 == PyDict_SetItemStringLongValue(dict, "number", (long)lasterror->dberr) ||
            -1 == PyDict_SetItemStringStringValue(dict, "description", lasterror->dberrstr ? lasterror->dberrstr : ""))
        {
            Py_DECREF(dict);
            dict = NULL;
        }
    }
    return dict;
}

static PyObject* build_lastoserr_dict(const struct LastError* lasterror)
{
    PyObject* dict;

    /* dblib sets oserr to DBNOERR for some errors... */
    if ((lasterror->oserr == DBNOERR) || (lasterror->oserr == 0)) Py_RETURN_NONE;

    dict = PyDict_New();
    if (dict)
    {
        if (-1 == PyDict_SetItemStringLongValue(dict, "number", (long)lasterror->oserr) ||
            -1 == PyDict_SetItemStringStringValue(dict, "description", lasterror->oserrstr ? lasterror->oserrstr : ""))
        {
            Py_DECREF(dict);
            dict = NULL;
        }
    }
    return dict;
}

static PyObject* build_lastmessage_dict(const struct LastMsg* lastmsg)
{
    PyObject* dict;

    if (0 == lastmsg->msgno) Py_RETURN_NONE;

    dict = PyDict_New();
    if (dict)
    {
        if (-1 == PyDict_SetItemStringLongValue(dict, "number", (long)lastmsg->msgno) ||
            -1 == PyDict_SetItemStringLongValue(dict, "state", (long)lastmsg->msgstate) ||
            -1 == PyDict_SetItemStringLongValue(dict, "severity", (long)lastmsg->severity) ||
            -1 == PyDict_SetItemStringStringValue(dict, "description", lastmsg->msgtext ? lastmsg->msgtext : "") ||
            -1 == PyDict_SetItemStringStringValue(dict, "server", lastmsg->srvname ? lastmsg->srvname : "") ||
            -1 == PyDict_SetItemStringStringValue(dict, "proc", lastmsg->proc ? lastmsg->proc : "") ||
            -1 == PyDict_SetItemStringLongValue(dict, "line", (long)lastmsg->line))
        {
            Py_DECREF(dict);
            dict = NULL;
        }
    }
    return dict;
}

/* Raise the given error. */
static void raise_lasterror(PyObject* exception, const struct LastError* lasterror,
                            const struct LastMsg* lastmsg)
{
    PyObject* args;

    /* Was the last error a database error or lower level? */
    const char* message;
    switch (lasterror->dberr)
    {
        case SYBESMSG:
        case SYBEFCON:
        {
            if (lastmsg->msgtext)
            {
                message = lastmsg->msgtext;
                break;
            }
            /* Intentional fall-through. */
        }
        default:
        {
            /* Older versions of FreeTDS set the dberr to the message number. */
            if (lasterror->dberr == lastmsg->msgno)
            {
                message = lastmsg->msgtext;
            }
            else
            {
                message = lasterror->dberrstr;
            }
            break;
        }
    }

    args = Py_BuildValue("(s)", message);
    if (args)
    {
        PyObject* error = PyEval_CallObject(exception, args);
        if (error)
        {
            PyObject* db_error = NULL;
            PyObject* os_error = NULL;
            PyObject* last_message = NULL;
            if (
                /* severity : int */
                -1 != PyObject_SetAttrStringLongValue(error, "severity", (long)lasterror->severity) &&

                /* db_error : {'code': int, 'description': string} */
                NULL != (db_error = build_lastdberr_dict(lasterror)) &&
                -1 != PyObject_SetAttrString(error, "db_error", db_error) &&

                /* os_error : None | {'code': int, 'description': string} */
                NULL != (os_error = build_lastoserr_dict(lasterror)) &&
                -1 != PyObject_SetAttrString(error, "os_error", os_error) &&

                /* last_message : None | {'number': int, 'state': int, 'severity': int, 'description': string, 'server': string, 'proc': string, 'line': int} */
                NULL != (last_message = build_lastmessage_dict(lastmsg)) &&
                -1 != PyObject_SetAttrString(error, "last_message", last_message)
                )
            {
                PyErr_SetObject(exception, error);
            }
            Py_XDECREF(db_error);
            Py_XDECREF(os_error);
            Py_XDECREF(last_message);
            Py_DECREF(error);
        }
        Py_DECREF(args);
    }
}

DBPROCESS* Connection_DBPROCESS(struct Connection* connection)
{
    return connection->dbproc;
}

int Connection_closed(struct Connection* connection)
{
    return (!connection->dbproc);
}

/* Close a connection, cancelling any currently executing command. */
static void Connection_close_internal(struct Connection* connection)
{
    Py_BEGIN_ALLOW_THREADS

        if (!Connection_closed(connection))
        {
            dbclose(connection->dbproc);
            connection->dbproc = NULL;
        }

    Py_END_ALLOW_THREADS
}

/*
    Frees all data associated with a Connection object, but does _not_ free
    the memory itself.
*/
static void Connection_free(struct Connection* connection)
{
    if (connection)
    {
        Connection_close_internal(connection);

        /*
            FreeTDS version prior to 0.95 leak the database name string in the LOGIN struct.
            Explicitly clear it to avoid this.
        */
        dbsetlname(connection->login, "", DBSETDBNAME);
        dbloginfree(connection->login);
        connection->login = NULL;

        LastError_clear(&connection->lasterror);
        LastMsg_clear(&connection->lastmsg);
    }
}


/* Raise the last seen error associated with a connection. */
void Connection_raise_lasterror(PyObject* exception, struct Connection* connection)
{
    if (!exception)
    {
        /*
            Attempt to map SQL Server error codes to the appropriate DB API
            exception.

            https://technet.microsoft.com/en-us/library/cc645603(v=sql.105).aspx
        */
        if (
            (101 <= connection->lastmsg.msgno && connection->lastmsg.msgno < 8000) ||
            /* >= 50000 are user-defined errors. */
            (50000 <= connection->lastmsg.msgno)
            )
        {
            exception = PyExc_tds_ProgrammingError;
        }
        else if (8000 <= connection->lastmsg.msgno && connection->lastmsg.msgno < 9000)
        {
            exception = PyExc_tds_DataError;
        }
        else
        {
            exception = PyExc_tds_DatabaseError;
        }
    }
    raise_lasterror(exception, &connection->lasterror, &connection->lastmsg);
}

void Connection_raise_closed(struct Connection* connection)
{
    PyErr_Format(PyExc_tds_InterfaceError, "connection closed");
    UNUSED(connection);
}

void Connection_clear_lastwarning(struct Connection* connection)
{
    LastMsg_clear(&connection->lastmsg);
}

void Connection_raise_lastwarning(struct Connection* connection)
{
    /*
        Ignore messages == 0, which includes informational things, e.g.
          * session property changes
          * database change messages
          * PRINT statements
    */
    if (connection->lastmsg.msgno > 0)
    {
        /* $TODO: figure out some way to include the other metadata in the warning */
        (void)PyErr_Warn(PyExc_tds_Warning,
                         connection->lastmsg.msgtext);
    }
}

static int Connection_use_internal(struct Connection* connection, const char* database)
{
    RETCODE retcode;

    Py_BEGIN_ALLOW_THREADS

        retcode = dbuse(connection->dbproc, database);

    Py_END_ALLOW_THREADS

    if (FAIL == retcode)
    {
        Connection_raise(connection);
        return -1;
    }
    return 0;
}

/*
    WARNING: No Python methods can be called from inside this function.
    They are executed from within a scope which does not hold the GIL.
*/
int Connection_dberrhandler(DBPROCESS* dbproc, int severity, int dberr,
                            int oserr, char *dberrstr, char *oserrstr)
{
    struct LastError* lasterror = NULL;
    if (dbproc)
    {
        struct Connection* connection = (struct Connection*)dbgetuserdata(dbproc);
        if (connection)
        {
            lasterror = &connection->lasterror;
        }
    }
    if (!lasterror)
    {
        lasterror = LastError_get();
    }

    LastError_clear(lasterror);

    lasterror->severity = severity;
    lasterror->dberr = dberr;
    lasterror->oserr = oserr;
    lasterror->dberrstr = (dberrstr) ? strdup(dberrstr) : NULL;
    lasterror->oserrstr = (oserrstr) ? strdup(oserrstr) : NULL;

    /*
        Possible return values included:

        * INT_CANCEL: The db-lib function that encountered the error will
            return FAIL.
        * INT_TIMEOUT: The db-lib function will cancel the operation and
            return FAIL. dbproc remains useable.
        * INT_CONTINUE: The db-lib function will retry the operation.

        Always raise INT_CANCEL here. INT_TIMEOUT is desirable for timeouts
        raised when waiting for SQL operations to complete. However, the error
        handler is also called for timeouts trying to cancel an operation
        after an error occurrs. It is simpler to always abort the connection
        on error than try to determine if the timeout is recoverable or not.
    */
    return INT_CANCEL;
}

/*
    WARNING: No Python methods can be called from inside this function.
    They are executed from within a scope which does not hold the GIL.
*/
int Connection_dbmsghandler(DBPROCESS* dbproc, DBINT msgno, int msgstate,
                            int severity, char* msgtext, char* srvname,
                            char* proc, int line)
{
    struct LastMsg* lastmsg = NULL;
    if (dbproc)
    {
        struct Connection* connection = (struct Connection*)dbgetuserdata(dbproc);
        if (connection)
        {
            lastmsg = &connection->lastmsg;
        }
    }
    if (!lastmsg)
    {
        lastmsg = LastMsg_get();
    }

    /*
        Only overwrite an existing message with one of greater severity.
        For example, message 3621 ("The statement has been terminated.")
        is returned after some other errors and shouldn't overwrite the prior,
        more informative, error.
    */
    if (lastmsg->msgno == 0 /* no message */ || severity > lastmsg->severity)
    {
        LastMsg_clear(lastmsg);

        lastmsg->msgno = msgno;
        lastmsg->msgstate = msgstate;
        lastmsg->severity = severity;
        lastmsg->msgtext = (msgtext) ? strdup(msgtext) : NULL;
        lastmsg->srvname = (srvname) ? strdup(srvname) : NULL;
        lastmsg->proc = (proc) ? strdup(proc) : NULL;
        lastmsg->line = line;
    }

    return 0;
}

/*
    Execute a static SQL statement and discard any results.
*/
static int Connection_execute(struct Connection* connection, size_t ncmds, ...)
{
    va_list vargs;
    RETCODE retcode;
    size_t ix;

    va_start(vargs, ncmds);

    Py_BEGIN_ALLOW_THREADS

        do
        {
            retcode = dbcancel(connection->dbproc);
            if (FAIL == retcode)
            {
                break;
            }
            for (ix = 0; ix < ncmds; ++ix)
            {
                const char* cmd = va_arg(vargs, char*);
                retcode = dbcmd(connection->dbproc, cmd);
                if (FAIL == retcode)
                {
                    break;
                }
            }
            if (FAIL == retcode)
            {
                break;
            }

            retcode = dbsqlexec(connection->dbproc);
            if (FAIL == retcode)
            {
                break;
            }
            while (NO_MORE_RESULTS != (retcode = dbresults(connection->dbproc)))
            {
                if (FAIL == retcode)
                {
                    break;
                }
                while (NO_MORE_ROWS != (retcode = dbnextrow(connection->dbproc)))
                {
                    if (FAIL == retcode)
                    {
                        break;
                    }
                }
                if (FAIL == retcode)
                {
                    break;
                }
            }
        } while (0);

    Py_END_ALLOW_THREADS

    va_end(vargs);

    if (FAIL == retcode)
    {
        Connection_raise(connection);
        return -1;
    }

    return 0;
}

int Connection_transaction_commit(struct Connection* connection)
{
    return Connection_execute(connection, 1, "IF @@TRANCOUNT > 0 COMMIT TRANSACTION");
}

int Connection_transaction_rollback(struct Connection* connection)
{
    return Connection_execute(connection, 1, "IF @@TRANCOUNT > 0 ROLLBACK TRANSACTION");
}

/*
   Python tds.Connection type definition.
*/
PyTypeObject ConnectionType;

static const char s_tds_Connection_doc[] =
    "A connection to the database server.\n"
    "\n"
    ":pep:`0249#connection-objects`\n";

static void Connection_dealloc(PyObject* self)
{
    Connection_free((struct Connection*)self);
    PyObject_Del(self);
}

/*
    tds.Connection attributes
*/

static const char s_Connection_autocommit_doc[] =
    "Auto-commit transactions after :py:meth:`ctds.Cursor.execute`,\n"
    ":py:meth:`ctds.Cursor.executemany`, and :py:meth:`ctds.Cursor.callproc`.\n"
    "If this is False, operations must be committed explicitly using\n"
    ":py:meth:`.commit`.\n";

static PyObject* Connection_autocommit_get(PyObject* self, void* closure)
{
    struct Connection* connection = (struct Connection*)self;
    return PyBool_FromLong(connection->autocommit);
    UNUSED(closure);
}

static int Connection_autocommit_set(PyObject* self, PyObject* value, void* closure)
{
    int error = 0;
    bool autocommit;

    struct Connection* connection = (struct Connection*)self;
    if (Connection_closed(connection))
    {
        Connection_raise_closed(connection);
        return -1;
    }

    if (!PyBool_Check(value))
    {
        PyErr_SetObject(PyExc_TypeError, value);
        return -1;
    }

    autocommit = (Py_True == value);
    if (autocommit != connection->autocommit)
    {
        /*
            If enabling auto-commit, commit the current transaction.
            If disabling auto-commit, do nothing. The transaction will
            be created on-demand as needed.
        */
        error = Connection_execute(connection,
                                   4,
                                   (!connection->autocommit) ? "IF @@TRANCOUNT > 0 COMMIT TRANSACTION;" : "",
                                   "SET IMPLICIT_TRANSACTIONS ",
                                   (autocommit) ? "OFF" : "ON",
                                   ";");
        if (!error)
        {
            connection->autocommit = autocommit;
        }
    }
    return error;

    UNUSED(closure);
}

static const char s_Connection_database_doc[] =
    "The current database.\n"
    "\n"
    ":return: None if the connection is closed.\n";

static PyObject* Connection_database_get(PyObject* self, void* closure)
{
    struct Connection* connection = (struct Connection*)self;
    if (!Connection_closed(connection))
    {
        /* This API shouldn't block, so don't bother releasing the GIL. */
        const char* database = dbname(connection->dbproc);

        return PyUnicode_DecodeUTF8(database, (Py_ssize_t)strlen(database), NULL);
    }
    Py_RETURN_NONE;

    UNUSED(closure);
}

static int Connection_database_set(PyObject* self, PyObject* value, void* closure)
{
    struct Connection* connection = (struct Connection*)self;
    const char* databasestr;
    if (
#if PY_MAJOR_VERSION < 3
        !PyString_Check(value) &&
#endif /* if PY_MAJOR_VERSION < 3 */
        !PyUnicode_Check(value)
        )
    {
        PyErr_SetObject(PyExc_TypeError, value);
        return -1;
    }

    if (Connection_closed(connection))
    {
        Connection_raise_closed(connection);
        return -1;
    }

#if PY_MAJOR_VERSION < 3
    if (PyUnicode_Check(value))
    {
        int error = -1;
        PyObject* utf8 = PyUnicode_AsUTF8String(value);
        if (utf8)
        {
            error = Connection_use_internal(connection, PyString_AS_STRING(utf8));
            Py_DECREF(utf8);
        }
        return error;
    }
    databasestr = PyString_AS_STRING(value);
#else /* if PY_MAJOR_VERSION < 3 */
    databasestr = PyUnicode_AsUTF8(value);
#endif /* else if PY_MAJOR_VERSION < 3 */

    return Connection_use_internal(connection, databasestr);

    UNUSED(closure);
}

static const char s_Connection_spid_doc[] =
    "Retrieve the SQL Server Session Process ID (SPID) for the connection.\n"
    "\n"
    ":return: None if the connection is closed.\n";

static PyObject* Connection_spid_get(PyObject* self, void* closure)
{
    struct Connection* connection = (struct Connection*)self;

    if (connection->dbproc)
    {
        /* This API shouldn't block, so don't bother releasing the GIL. */
        int spid = dbspid(connection->dbproc);

        return PyLong_FromLong((long)spid);
    }
    Py_RETURN_NONE;

    UNUSED(closure);
}

static const char s_Connection_tds_version_doc[] =
    "The TDS version in use for the connection.\n"
    "\n"
    ":return: None if the connection is closed.\n";

static PyObject* Connection_tds_version_get(PyObject* self, void* closure)
{
    struct Connection* connection = (struct Connection*)self;

    if (connection->dbproc)
    {
        const char* tds_version;

        /* This API shouldn't block, so don't bother releasing the GIL. */
        switch (DBTDS(connection->dbproc))
        {
#ifdef DBTDS_2_0
            case DBTDS_2_0: { tds_version = "2.0"; break; }
#endif
#ifdef DBTDS_3_4
            case DBTDS_3_4: { tds_version = "3.4"; break; }
#endif
#ifdef DBTDS_4_0
            case DBTDS_4_0: { tds_version = "4.0"; break; }
#endif
#ifdef DBTDS_4_2
            case DBTDS_4_2: { tds_version = "4.2"; break; }
#endif
#ifdef DBTDS_4_6
            case DBTDS_4_6: { tds_version = "4.6"; break; }
#endif
#ifdef DBTDS_4_9_5
            case DBTDS_4_9_5: { tds_version = "4.9.5"; break; }
#endif
#ifdef DBTDS_5_0
            case DBTDS_5_0: { tds_version = "5.0"; break; }
#endif
#ifdef DBTDS_7_0
            case DBTDS_7_0: { tds_version = "7.0"; break; }
#endif
#ifdef DBTDS_7_1
            case DBTDS_7_1: { tds_version = "7.1"; break; }
#endif
#ifdef DBTDS_7_2
            case DBTDS_7_2: { tds_version = "7.2"; break; }
#endif
#ifdef DBTDS_7_3
            case DBTDS_7_3: { tds_version = "7.3"; break; }
#endif
#ifdef DBTDS_7_4
            case DBTDS_7_4: { tds_version = "7.4"; break; }
#endif
            default: { tds_version = NULL; break; }
        }

        if (tds_version)
        {
            return PyUnicode_DecodeASCII(tds_version,
                                         (Py_ssize_t)strlen(tds_version),
                                         NULL);
        }
    }
    Py_RETURN_NONE;

    UNUSED(closure);
}

static const char s_Connection_timeout_doc[] =
    "The connection timeout, in seconds.\n"
    "\n"
    ".. note:: Setting the timeout requires FreeTDS version 1.00 or later.\n"
    "\n"
    ":raises ctds.NotSupportedError: `cTDS` was compiled against a version of\n"
    "    FreeTDS which does not support setting the timeout on a connection.\n"
    "\n";

static PyObject* Connection_timeout_get(PyObject* self, void* closure)
{
    struct Connection* connection = (struct Connection*)self;

    PyObject* timeout = NULL;
    do
    {
        if (!Connection_closed(connection))
        {
            /* This API shouldn't block, so don't bother releasing the GIL. */
            timeout = PyLong_FromLong(connection->query_timeout);
        }
        else
        {
            timeout = Py_None;
            Py_INCREF(timeout);
        }
    }
    while (0);

    return timeout;

    UNUSED(closure);
}

static int Connection_timeout_set(PyObject* self, PyObject* value, void* closure)
{
    struct Connection* connection = (struct Connection*)self;

    if (
#if PY_MAJOR_VERSION < 3
        !PyInt_Check(value) &&
#endif /* if PY_MAJOR_VERSION < 3 */
        !PyLong_Check(value)
    )
    {
        PyErr_SetObject(PyExc_TypeError, value);
        return -1;
    }

    do
    {
        long timeout = PyLong_AsLong(value);
        if (timeout > INT_MAX || timeout < 0)
        {
            PyErr_SetObject(PyExc_ValueError, value);
            break;
        }

        if (!Connection_closed(connection))
        {
#ifdef DBVERSION_74 /* indicates FreeTDS 1.00+ */
            /* The timeout must be passed as a string. */
            char str[ARRAYSIZE("2147483648")];
            (void)snprintf(str, sizeof(str), "%d", (int)timeout);
            if (FAIL == dbsetopt(connection->dbproc, DBSETTIME, str, (int)timeout))
            {
                Connection_raise(connection);
                break;
            }

            connection->query_timeout = (int)timeout;
#else /* ifdef DBVERSION_74 */
            PyErr_SetString(PyExc_tds_NotSupportedError, "FreeTDS does not support the DBSETTIME option");
            break;
#endif /* else ifdef DBVERSION_74 */
        }
        else
        {
            Connection_raise_closed(connection);
            break;
        }
    }
    while (0);

    return (PyErr_Occurred()) ? -1 : 0;

    UNUSED(closure);
}

static PyGetSetDef Connection_getset[] = {
    /* name, get, set, doc, closure */
    { (char*)"autocommit",  Connection_autocommit_get,  Connection_autocommit_set, (char*)s_Connection_autocommit_doc,  NULL },
    { (char*)"database",    Connection_database_get,    Connection_database_set,   (char*)s_Connection_database_doc,    NULL },
    { (char*)"spid",        Connection_spid_get,        NULL,                      (char*)s_Connection_spid_doc,        NULL },
    { (char*)"tds_version", Connection_tds_version_get, NULL,                      (char*)s_Connection_tds_version_doc, NULL },
    { (char*)"timeout",     Connection_timeout_get,     Connection_timeout_set,    (char*)s_Connection_timeout_doc,     NULL },
    { NULL,                 NULL,                       NULL,                      NULL,                                NULL }
};

/*
    tds.Connection methods
*/


/* https://www.python.org/dev/peps/pep-0249/#Connection.close */
static const char s_Connection_close_doc[] =
    "close()\n"
    "\n"
    "Close the connection now. Pending transactions will be rolled back.\n"
    "Subsequent calls to this object or any :py:class:`ctds.Cursor` objects it\n"
    "created will raise :py:exc:`ctds.InterfaceError`.\n"
    "\n"
    ":pep:`0249#Connection.close`\n";

PyObject* Connection_close(PyObject* self, PyObject* args)
{
    struct Connection* connection = (struct Connection*)self;
    if (Connection_closed(connection))
    {
        Connection_raise_closed(connection);
        return NULL;
    }

    Connection_close_internal(connection);

    Py_RETURN_NONE;
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#commit */
static const char s_Connection_commit_doc[] =
    "commit()\n"
    "\n"
    "Commit any pending transaction to the database.\n"
    "\n"
    ":pep:`0249#commit`\n";

static PyObject* Connection_commit(PyObject* self, PyObject* args)
{
    struct Connection* connection = (struct Connection*)self;
    if (Connection_closed(connection))
    {
        Connection_raise_closed(connection);
        return NULL;
    }

    /*
        Only commit transactions if autocommit is disabled or the connection
        is dead. The later should always occur to ensure the client is notified
        of a dead connection.
    */
    if (!connection->autocommit || DBDEAD(connection->dbproc))
    {
        if (0 != Connection_transaction_commit(connection))
        {
            return NULL;
        }
    }

    Py_RETURN_NONE;
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#Connection.rollback */
static const char s_Connection_rollback_doc[] =
    "rollback()\n"
    "\n"
    "Rollback any pending transaction to the database.\n"
    "\n"
    ":pep:`0249#rollback`\n";

static PyObject* Connection_rollback(PyObject* self, PyObject* args)
{
    struct Connection* connection = (struct Connection*)self;
    if (Connection_closed(connection))
    {
        Connection_raise_closed(connection);
        return NULL;
    }

    /*
        Only commit transactions if autocommit is disabled or the connection
        is dead. The later should always occur to ensure the client is notified
        of a dead connection.
    */
    if (!connection->autocommit || DBDEAD(connection->dbproc))
    {
        if (0 != Connection_transaction_rollback(connection))
        {
            return NULL;
        }
    }

    Py_RETURN_NONE;
    UNUSED(args);
}

/* https://www.python.org/dev/peps/pep-0249/#Connection.cursor */
static const char s_Connection_cursor_doc[] =
    "cursor()\n"
    "\n"
    "Return a new :py:class:`ctds.Cursor` object using the connection.\n"
    "\n"
    ".. note::\n"
    "\n"
    "    :py:meth:`ctds.Cursor.close` should be called when the returned\n"
    "    cursor is no longer required.\n"
    "\n"
    ".. warning::\n"
    "\n"
    "    Only one :py:class:`ctds.Cursor` object should be used per\n"
    "    connection. The last command executed on any cursor associated\n"
    "    with a connection will overwrite any previous results from all\n"
    "    other cursors.\n"
    "\n"
    ":pep:`0249#cursor`\n"
    "\n"
    ":return: A new Cursor object.\n"
    ":rtype: ctds.Cursor\n";

static PyObject* Connection_cursor(PyObject* self, PyObject* args)
{
    struct Connection* connection = (struct Connection*)self;
    if (Connection_closed(connection))
    {
        Connection_raise_closed(connection);
        return NULL;
    }

    return Cursor_create(connection);
    UNUSED(args);
}

static const char s_Connection_bulk_insert_doc[] =
    "bulk_insert(table, rows, batch_size=None, tablock=False)\n"
    "\n"
    "Bulk insert rows into a given table.\n"
    "This method utilizes the `BULK INSERT` functionality of SQL Server\n"
    "to efficiently insert large amounts of data into a table. By default,\n"
    "rows are not validated until all rows have been processed.\n"
    "\n"
    "An optional batch size may be specified to validate the inserted rows\n"
    "after `batch_size` rows have been copied to server.\n"
    "\n"
    ":param str table: The table in which to insert the rows.\n"

    ":param rows: An iterable of data rows. Data rows are Python `sequence`\n"
    "    objects. Each item in the data row is inserted into the table in\n"
    "    sequential order.\n"
    ":type rows: :ref:`typeiter <python:typeiter>`\n"

    ":param int batch_size: An optional batch size.\n"

    ":param bool tablock: Should the `TABLOCK` hint be passed.\n"

    ":return: The number of rows saved to the table.\n"
    ":rtype: int\n";

static DBINT Connection_bulk_insert_sendrow(struct Connection* connection,
                                            PyObject* sequence,
                                            bool send_batch)
{
    DBINT saved = 0;

    Py_ssize_t ix;
    Py_ssize_t size = PySequence_Fast_GET_SIZE(sequence);
    struct Parameter** rpcparams;
    do
    {
        RETCODE retcode;

        rpcparams = (struct Parameter**)tds_mem_calloc((size_t)size, sizeof(struct Parameter*));
        if (!rpcparams)
        {
            PyErr_NoMemory();
            break;
        }

        for (ix = 0; ix < size; ++ix)
        {
            PyObject* value = PySequence_Fast_GET_ITEM(sequence, ix);
            if (!Parameter_Check(value))
            {
                rpcparams[ix] = Parameter_create(value, 0 /* output */);
                if (!rpcparams[ix])
                {
                    break;
                }
            }
            else
            {
                Py_INCREF(value);
                rpcparams[ix] = (struct Parameter*)value;
            }

            if (PyUnicode_Check(Parameter_value(rpcparams[ix])))
            {
                (void)PyErr_Warn(PyExc_tds_Warning,
                                 "Direct bulk insert of a Python str object may result in unexpected character encoding. "
                                 "It is recommended to explicitly encode Python str values for bulk insert.");
            }

            /* bcp_bind does not make a network request, so no need to release the GIL. */

            /* bcp_bind expects a 1-based column index. */
            retcode = Parameter_bcp_bind(rpcparams[ix], connection->dbproc, (size_t)(ix + 1));
            if (FAIL == retcode)
            {
                Connection_raise(connection);
                break;
            }
        }

        if (PyErr_Occurred())
        {
            break;
        }

        Py_BEGIN_ALLOW_THREADS

            do
            {
                retcode = bcp_sendrow(connection->dbproc);
                if (FAIL == retcode)
                {
                    break;
                }

                if (send_batch)
                {
                    saved = bcp_batch(connection->dbproc);
                    if (-1 == saved)
                    {
                        retcode = FAIL;
                        break;
                    }
                }
            }
            while (0);

        Py_END_ALLOW_THREADS

        if (FAIL == retcode)
        {
            Connection_raise(connection);
            break;
        }
    }
    while (0);

    if (rpcparams)
    {
        for (ix = 0; ix < size; ++ix)
        {
            Py_XDECREF(rpcparams[ix]);
        }
        tds_mem_free(rpcparams);
    }

    return (PyErr_Occurred()) ? -1 : saved;
}

static PyObject* Connection_bulk_insert(PyObject* self, PyObject* args, PyObject* kwargs)
{
    struct Connection* connection = (struct Connection*)self;

    DBINT saved = 0;

    PyObject* irows;
    size_t batches = 0;

    static char* s_kwlist[] =
    {
        "table",
        "rows",
        "batch_size",
        "tablock",
        NULL
    };
    char* table;
    PyObject* rows;
    PyObject* batch_size = Py_None;
    PyObject* tablock = Py_False;
    if (!PyArg_ParseTupleAndKeywords(args,
                                     kwargs,
                                     "sO|OO!",
                                     s_kwlist,
                                     &table,
                                     &rows,
                                     &batch_size,
                                     &PyBool_Type,
                                     &tablock))
    {
        return NULL;
    }

    if (Py_None != batch_size)
    {
        do
        {
            if (!(
#if PY_MAJOR_VERSION < 3
                     PyInt_Check(batch_size) ||
#endif /* if PY_MAJOR_VERSION < 3 */
                     PyLong_Check(batch_size)
               ))
            {
                PyErr_SetObject(PyExc_TypeError, batch_size);
                break;
            }
#if PY_MAJOR_VERSION < 3
            if (PyInt_Check(batch_size))
            {
                batches = (size_t)PyInt_AsSsize_t(batch_size);
            }
            else
            {
                batches = (size_t)PyLong_AsSsize_t(batch_size);
            }
#else /* if PY_MAJOR_VERSION < 3 */
            batches = PyLong_AsSize_t(batch_size);
#endif /* else if PY_MAJOR_VERSION < 3 */
        }
        while (0);

        if (PyErr_Occurred())
        {
            return NULL;
        }
    }

    irows = PyObject_GetIter(rows);
    if (!irows)
    {
        PyErr_SetObject(PyExc_TypeError, rows);
        return NULL;
    }

    do
    {
        if (!bcp_getl(connection->login))
        {
            PyErr_Format(PyExc_tds_NotSupportedError, "bulk copy is not enabled");
            break;
        }

        if (Connection_closed(connection))
        {
            Connection_raise_closed(connection);
            break;
        }

        do
        {
            PyObject* row;

            RETCODE retcode;
            size_t sent = 0;

            DBINT processed;

            Py_BEGIN_ALLOW_THREADS

                do
                {
                    retcode = bcp_init(connection->dbproc, table, NULL, NULL, DB_IN);
                    if (FAIL == retcode)
                    {
                        break;
                    }

                    if (Py_True == tablock)
                    {
                        static const char s_TABLOCK[] = "TABLOCK";
                        retcode = bcp_options(connection->dbproc,
                                              BCPHINTS,
                                              (BYTE*)s_TABLOCK,
                                              ARRAYSIZE(s_TABLOCK));
                        if (FAIL == retcode)
                        {
                            break;
                        }
                    }
                }
                while (0);

            Py_END_ALLOW_THREADS

            if (FAIL == retcode)
            {
                Connection_raise(connection);
                break;
            }

            while (NULL != (row = PyIter_Next(irows)))
            {
                static const char s_fmt[] = "invalid sequence for row %ld";
                PyObject* sequence;

                char msg[ARRAYSIZE(s_fmt) + ARRAYSIZE(STRINGIFY(UINT64_MAX))];
                (void)sprintf(msg, s_fmt, sent);

                sequence = PySequence_Fast(row, msg);
                if (sequence)
                {
                    bool send_batch = ((0 != batches) && ((sent + 1) % batches == 0));
                    processed = Connection_bulk_insert_sendrow(connection, sequence, send_batch);
                    if (-1 != processed)
                    {
                        saved += processed;
                    }
                    Py_DECREF(sequence);
                }
                Py_DECREF(row);

                if (PyErr_Occurred())
                {
                    break;
                }

                sent++;
            }

            /* Always call bcp_done() regardless of previous errors. */
            Py_BEGIN_ALLOW_THREADS

                processed = bcp_done(connection->dbproc);

            Py_END_ALLOW_THREADS

            if (-1 != processed)
            {
                saved += processed;
            }
            else
            {
                /* Don't overwrite a previous error if bcp_done fails. */
                if (!PyErr_Occurred())
                {
                    Connection_raise(connection);
                    break;
                }
            }
        }
        while (0);
    }
    while (0);

    Py_DECREF(irows);

    if (PyErr_Occurred())
    {
        return NULL;
    }

    return PyLong_FromLong(saved);
}

static const char s_Connection_use_doc[] =
    "use(database)\n"
    "\n"
    "Set the current database.\n"
    "\n"
    ":param str database: The database.\n"
    ":rtype: None\n";

static PyObject* Connection_use(PyObject* self, PyObject* args)
{
    char* database;
    if (!PyArg_ParseTuple(args, "s", &database))
    {
        return NULL;
    }
    if (0 == Connection_database_set(self, PyTuple_GET_ITEM(args, 0), NULL))
    {
        Py_RETURN_NONE;
    }
    else
    {
        return NULL;
    }
}


static const char s_Connection___enter___doc[] =
    "__enter__()\n"
    "\n"
    "Enter the connection's runtime context. On exit, the connection is\n"
    "closed automatically.\n"
    "\n"
    ":return: The connection object.\n"
    ":rtype: ctds.Connection\n";

static PyObject* Connection___enter__(PyObject* self, PyObject* args)
{
    Py_INCREF(self);

    return self;
    UNUSED(args);
}

static const char s_Connection___exit___doc[] =
    "__exit__(exc_type, exc_val, exc_tb)\n"
    "\n"
    "Exit the connection's runtime context, closing the connection.\n"
    "If no error occurred, any pending transaction will be committed\n"
    "prior to closing the connection. If an error occurred, the transaction\n"
    "will be implicitly rolled back when the connection is closed.\n"
    "\n"
    ":param type exc_type: The exception type, if an exception\n"
    "    is raised in the context, otherwise `None`.\n"
    ":param Exception exc_val: The exception value, if an exception\n"
    "    is raised in the context, otherwise `None`.\n"
    ":param object exc_tb: The exception traceback, if an exception\n"
    "    is raised in the context, otherwise `None`.\n"
    "\n"
    ":rtype: None\n";

static PyObject* Connection___exit__(PyObject* self, PyObject* args)
{
    struct Connection* connection = (struct Connection*)self;

    PyObject* exc_type;
    PyObject* exc_val;
    PyObject* exc_tb;
    if (!PyArg_ParseTuple(args, "OOO", &exc_type, &exc_val, &exc_tb))
    {
        return NULL;
    }
    if (Py_None == exc_type && !PyErr_Occurred())
    {
        if (!connection->autocommit)
        {
            if (0 != Connection_transaction_commit(connection))
            {
                return NULL;
            }
        }
    }
    return Connection_close(self, NULL);
}


static PyMethodDef Connection_methods[] = {
    /* ml_name, ml_meth, ml_flags, ml_doc */
    /* DB API-2.0 required methods. */
    { "close",       Connection_close,                    METH_NOARGS,                  s_Connection_close_doc },
    { "commit",      Connection_commit,                   METH_NOARGS,                  s_Connection_commit_doc },
    { "rollback",    Connection_rollback,                 METH_NOARGS,                  s_Connection_rollback_doc },
    { "cursor",      Connection_cursor,                   METH_NOARGS,                  s_Connection_cursor_doc },

    /* Non-DB API 2.0 methods. */
    { "bulk_insert", (PyCFunction)Connection_bulk_insert, METH_VARARGS | METH_KEYWORDS, s_Connection_bulk_insert_doc },
    { "use",         Connection_use,                      METH_VARARGS,                 s_Connection_use_doc },
    { "__enter__",   Connection___enter__,                METH_NOARGS,                  s_Connection___enter___doc },
    { "__exit__",    Connection___exit__,                 METH_VARARGS,                 s_Connection___exit___doc },
    { NULL,          NULL,                                0,                            NULL }
};

PyObject* Connection_create(const char* server, uint16_t port, const char* instance,
                            const char* username, const char* password,
                            const char* database, const char* appname,
                            unsigned int login_timeout, unsigned int timeout,
                            const char* tds_version, bool autocommit,
                            bool ansi_defaults, bool enable_bcp)
{
    struct Connection* connection = PyObject_New(struct Connection, &ConnectionType);
    if (NULL != connection)
    {
        char* servername = NULL;

        memset((((char*)connection) + offsetof(struct Connection, login)),
               0,
               (sizeof(struct Connection) - offsetof(struct Connection, login)));
        do
        {
            /* Only support SQL Server 7.0 and up. */
            static const struct {
                const char* tds_version;
                DBINT dbversion;
            } s_supported_tds_versions[] = {
                /* Note: The versions MUST be kept in descending order. */
#ifdef DBVERSION_74
                { "7.4", DBVERSION_74 },
#endif
#ifdef DBVERSION_73
                { "7.3", DBVERSION_73 },
#endif
#ifdef DBVERSION_72
                { "7.2", DBVERSION_72 },
#endif
#ifdef DBVERSION_71
                { "7.1", DBVERSION_71 },
#endif
#ifdef DBVERSION_70
                { "7.0", DBVERSION_70 }
#endif
            };
            DBINT dbversion = DBVERSION_UNKNOWN;

            int written;

            /* Determine the maximum size the server connection string may require. */
            size_t nservername = strlen(server) +
                STRLEN(STRINGIFY(UINT16_MAX)) /* maximum port number length */ +
                ((instance) ? (strlen(instance) + 1 /* for '\\' */) : 0) +
                1 /* for '\0' */;

            servername = (char*)tds_mem_malloc(nservername);
            if (!servername)
            {
                PyErr_NoMemory();
                break;
            }

            written = snprintf(servername, nservername, "%s", server);

            if (instance)
            {
                (void)snprintf(&servername[written], nservername - (size_t)written, "\\%s", instance);
            }
            else
            {
                (void)snprintf(&servername[written], nservername - (size_t)written, ":%d", port);
            }

            connection->login = dblogin();
            if (!connection->login)
            {
                PyErr_NoMemory();
                break;
            }

#if CTDS_USE_UTF16 != 0
            /*
                Force the connections to use UTF-16 over UCS-2.
            */
            if (FAIL == DBSETLUTF16(connection->login, 1))
            {
                PyErr_SetString(PyExc_tds_InternalError, "failed to set connection encoding");
                break;
            }
#endif /* if CTDS_USE_UTF16 != 0 */

            /*
                Note: FreeTDS only supports single-byte encodings (e.g. latin-1, UTF-8).
                UTF-8 is really the only choice if Unicode support is desired.
            */
            if (FAIL == DBSETLCHARSET(connection->login, "UTF-8"))
            {
                PyErr_SetString(PyExc_tds_InternalError, "failed to set client charset");
                break;
            }

            if (username)
            {
                if (FAIL == DBSETLUSER(connection->login, username))
                {
                    PyErr_SetString(PyExc_ValueError, username);
                    break;
                }
            }
            if (password)
            {
                if (FAIL == DBSETLPWD(connection->login, password))
                {
                    PyErr_SetString(PyExc_ValueError, password);
                    break;
                }
            }
            if (appname)
            {
                if (FAIL == DBSETLAPP(connection->login, appname))
                {
                    PyErr_SetString(PyExc_ValueError, appname);
                    break;
                }
            }

            if (tds_version)
            {
                size_t ix;
                for (ix = 0; ix < ARRAYSIZE(s_supported_tds_versions); ++ix)
                {
                    if (strcmp(tds_version, s_supported_tds_versions[ix].tds_version) == 0)
                    {
                        dbversion = s_supported_tds_versions[ix].dbversion;
                        break;
                    }
                }
                if (FAIL == DBSETLVERSION(connection->login, (BYTE)dbversion))
                {
                    PyErr_Format(PyExc_tds_InterfaceError, "unsupported TDS version \"%s\"", tds_version);
                    break;
                }
            }
            else
            {
                /*
                    Default to the most recent.
                    Note: Version 0.92.x of FreeTDS defines DBVERSION_73, but doesn't
                    actually support it. Try the defined TDS versions in descending order.
                */
                RETCODE retcode = FAIL;
                size_t ix;
                for (ix = 0; ix < ARRAYSIZE(s_supported_tds_versions); ++ix)
                {
                    dbversion = s_supported_tds_versions[ix].dbversion;
                    retcode = DBSETLVERSION(connection->login, (BYTE)dbversion);
                    if (FAIL != retcode) break;
                }
                if (FAIL == retcode)
                {
                    PyErr_Format(PyExc_tds_InternalError, "failed to set TDS version");
                    break;
                }
            }

            if (database)
            {
                /*
                    If setting the database name in the login fails, this is likely due
                    to it exceeding the allowed DB name limit of 30 in older versions
                    of FreeTDS. In this case, attempt to set it after connection.
                */
                if (FAIL != dbsetlname(connection->login, database, DBSETDBNAME))
                {
                    database = NULL;
                }
            }
            if (enable_bcp)
            {
                /* FreeTDS seems to reverse the boolean logic for enabling bcp. ?!?! */
                if (FAIL == BCP_SETL(connection->login, 0))
                {
                    PyErr_SetString(PyExc_tds_InternalError, "failed to enable bcp");
                    break;
                }
            }

            connection->autocommit = autocommit;

            /*
                $TODO: this is global. Setting a per-connection login timeout will
                require additions to db-lib.
            */
            (void)dbsetlogintime((int)login_timeout);
            (void)dbsettime((int)timeout);

            Py_BEGIN_ALLOW_THREADS

                /* Clear last error prior to the connection attempt. */
                LastError_clear(LastError_get());
                LastMsg_clear(LastMsg_get());

                connection->dbproc = dbopen(connection->login, servername);

            Py_END_ALLOW_THREADS

            if (NULL == connection->dbproc)
            {
                raise_lasterror(PyExc_tds_OperationalError, LastError_get(), LastMsg_get());
                break;
            }

            connection->query_timeout = (int)timeout;

            dbsetuserdata(connection->dbproc, (BYTE*)connection);

            if (ansi_defaults)
            {
                /*
                    These settings could easily be passed via the login packet, though that
                    requires an update to freetds' dbopen method to actually pass the flag.
                */
                /* Mimic the settings used by ODBC connections. */
                static const char s_ansi_default_stmt[] =
                    /* https://msdn.microsoft.com/en-us/library/ms190306.aspx */
                    "SET ARITHABORT ON;"

                    /* https://msdn.microsoft.com/en-us/library/ms188340.aspx */
                    "SET ANSI_DEFAULTS ON;"

                    /* https://msdn.microsoft.com/en-us/library/ms176056.aspx */
                    "SET CONCAT_NULL_YIELDS_NULL ON;"

                    /* https://msdn.microsoft.com/en-us/library/ms186238.aspx */
                    "SET TEXTSIZE 2147483647;";

                if (0 != Connection_execute(connection,
                                            4,
                                            s_ansi_default_stmt,
                                            "SET IMPLICIT_TRANSACTIONS ",
                                            (autocommit) ? "OFF" : "ON",
                                            ";"))
                {
                    break;
                }
            }

            if (database)
            {
                if (0 != Connection_use_internal(connection, database))
                {
                    break;
                }
            }
        } while (0);

        tds_mem_free(servername);

        if (PyErr_Occurred())
        {
            Py_DECREF((PyObject*)connection);
            connection = NULL;
        }
    }
    else
    {
        PyErr_NoMemory();
    }

    return (PyObject*)connection;
}


PyTypeObject ConnectionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    STRINGIFY(tds) ".Connection", /* tp_name */
    sizeof(struct Connection),    /* tp_basicsize */
    0,                            /* tp_itemsize */
    Connection_dealloc,           /* tp_dealloc */
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
    s_tds_Connection_doc,         /* tp_doc */
    NULL,                         /* tp_traverse */
    NULL,                         /* tp_clear */
    NULL,                         /* tp_richcompare */
    0,                            /* tp_weaklistoffset */
    NULL,                         /* tp_iter */
    NULL,                         /* tp_iternext */
    Connection_methods,           /* tp_methods */
    NULL,                         /* tp_members */
    Connection_getset,            /* tp_getset */
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

PyTypeObject* ConnectionType_init(void)
{
    if ((0 != LastError_init()) ||
        (0 != LastMsg_init()))
    {
        return NULL;
    }
    return (PyType_Ready(&ConnectionType) == 0) ? &ConnectionType : NULL;
}
