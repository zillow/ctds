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
# if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8
/* Ignore "'tp_print' has been explicitly marked deprecated here" */
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
#  endif /* if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 8 */

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
              value = tds_mem_malloc(sizeof(_type)); \
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

#elif defined(_WIN32)

#  include "include/push_warnings.h"
#  include <Windows.h>
#  include "include/pop_warnings.h"

#  define TLS_DECLARE(_name, _type, _destructor) \
      static DWORD tls_ ## _name = TLS_OUT_OF_INDEXES; \
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
          tls_ ## _name = TlsAlloc(); \
          return (TLS_OUT_OF_INDEXES == tls_ ## _name) ? -1 : 0; \
      } \
      static _type* _name ## _get(void) \
      { \
          _type* value = ((_type*)TlsGetValue(tls_ ## _name)); \
          if (!value && (ERROR_SUCCESS == GetLastError())) \
          { \
              value = tds_mem_malloc(sizeof(_type)); \
              if (value) \
              { \
                  if (TlsSetValue(tls_ ## _name, (LPVOID)value)) \
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

#else /* elif defined(_WIN32) */

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

#endif /* else elif defined(_WIN32) */

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

struct DatabaseMsg
{
    DBINT msgno;
    int msgstate;
    int severity;
    char* msgtext;
    char* srvname;
    char* proc;
    int line;

    bool warned;

    struct DatabaseMsg* next;
};

static void DatabaseMsg_clear(struct DatabaseMsg* dbmsg)
{
    if (dbmsg)
    {
        tds_mem_free(dbmsg->msgtext);
        tds_mem_free(dbmsg->srvname);
        tds_mem_free(dbmsg->proc);

        DatabaseMsg_clear(dbmsg->next);
        tds_mem_free(dbmsg->next);

        memset(dbmsg, 0, sizeof(struct DatabaseMsg));
    }
}

TLS_DECLARE(LastMsg, struct DatabaseMsg, DatabaseMsg_clear)

#if defined(__GNUC__)
__attribute__((destructor)) void fini(void)
{
    LastError_clear(LastError_get());
    DatabaseMsg_clear(LastMsg_get());
}
#endif


struct Connection {
    PyObject_VAR_HEAD

    LOGINREC* login;
    DBPROCESS* dbproc;

    /* Should execute calls be auto-committed? */
    bool autocommit;

    /* Last seen error, as set by the error handler: Connection_dberrhandler. */
    struct LastError lasterror;

    /*
        List of buffered database messages generated by the current (or previous) command.
        These messages are set by the message handler: Connection_dbmsghandler.
    */
    struct DatabaseMsg* messages;

    /*
        The query timeout for this connection.
        This is only stored here because there is currently no way to
        retrieve it from dblib.
    */
    int query_timeout;

    /*
        The paramstyle to use on .execute*() calls on ctds.Cursor() objects created
        by this connection.
    */
    enum ParamStyle paramstyle;
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

static PyObject* build_message_dict(const struct DatabaseMsg* lastmsg)
{
    PyObject* dict;

    if (!lastmsg) Py_RETURN_NONE;

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
                            const struct DatabaseMsg* lastmsg)
{
    PyObject* args;

    /* Was the last error a database error or lower level? */
    const char* message;
    switch (lasterror->dberr)
    {
        case SYBESMSG:
        case SYBEFCON:
        {
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */
            if (lastmsg && lastmsg->msgtext)
            {
                message = lastmsg->msgtext;
                break;
            }
#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif
            /* Intentional fall-through. */
        }
        default:
        {
            /* Older versions of FreeTDS set the dberr to the message number. */
            if (lastmsg && lasterror->dberr == lastmsg->msgno)
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
        PyObject* error = PyObject_Call(exception, args, (PyObject*)NULL);
        if (error)
        {
            PyObject* db_error = NULL;
            PyObject* os_error = NULL;
            PyObject* last_message = NULL;
            if (
                /* severity : int */
                -1 != PyObject_SetAttrStringLongValue(error, "severity", (long)lasterror->severity) &&

                /* db_error : {'number': int, 'description': string} */
                NULL != (db_error = build_lastdberr_dict(lasterror)) &&
                -1 != PyObject_SetAttrString(error, "db_error", db_error) &&

                /* os_error : None | {'number': int, 'description': string} */
                NULL != (os_error = build_lastoserr_dict(lasterror)) &&
                -1 != PyObject_SetAttrString(error, "os_error", os_error) &&

                /* last_message : None | {'number': int, 'state': int, 'severity': int, 'description': string, 'server': string, 'proc': string, 'line': int} */
                NULL != (last_message = build_message_dict(lastmsg)) &&
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

static void Connection_clear_messages(struct Connection* connection)
{
    DatabaseMsg_clear(connection->messages);
    tds_mem_free(connection->messages);
    connection->messages = NULL;
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
            FreeTDS versions prior to 0.95 leak the database name string in the LOGIN struct.
            Explicitly clear it to avoid this.
        */
        dbsetlname(connection->login, "", DBSETDBNAME);
        dbloginfree(connection->login);
        connection->login = NULL;

        LastError_clear(&connection->lasterror);
        Connection_clear_messages(connection);
    }
}


/* Raise the last seen error associated with a connection. */
void Connection_raise_lasterror(struct Connection* connection)
{
    PyObject* exception;

    const struct DatabaseMsg* lastmsg = connection->messages;

    int msgno = (lastmsg) ? lastmsg->msgno : 0;
    if (msgno < 50000 /* start of custom range */)
    {
        /*
            Categorize by severity for levels > 10, which are considered errors by SQL
            Server.

            See https://docs.microsoft.com/en-us/sql/relational-databases/errors-events/database-engine-error-severities?view=sql-server-ver15
            for descriptions of each severity level.
        */
        switch ((lastmsg) ? lastmsg->severity : 0)
        {
            /*
                Indicates that the given object or entity does not exist.
            */
            case 11:
            {
                exception = PyExc_tds_ProgrammingError;
                break;
            }

            /*
                A special severity for queries that do not use locking because of special
                query hints. In some cases, read operations performed by these statements
                could result in inconsistent data, since locks are not taken to guarantee
                consistency.
            */
            case 12:
            {
                exception = PyExc_tds_IntegrityError;
                break;
            }

            /*
                Indicates transaction deadlock errors.
            */
            case 13:
            {
                exception = PyExc_tds_InternalError;
                break;
            }

            /*
                Indicates security-related errors, such as permission denied.
            */
            case 14:
            {
                switch (msgno)
                {
                    case 2601: /* Cannot insert duplicate key row in object '%.*ls' with unique index '%.*ls'. */
                    case 2627: /* Violation of %ls constraint '%.*ls'. Cannot insert duplicate key in object '%.*ls'. */
                    {
                        exception = PyExc_tds_IntegrityError;
                        break;
                    }

                    default:
                    {
                        exception = PyExc_tds_DatabaseError;
                        break;
                    }
                }
                break;
            }

            /*
                Indicates syntax errors in the Transact-SQL command.
            */
            case 15:
            {
                exception = PyExc_tds_ProgrammingError;
                break;
            }

            /*
                Indicates general errors that can be corrected by the user.
            */
            case 16:
            {
                /*
                    Attempt to map SQL Server error codes to the appropriate DB API
                    exception.

                    https://technet.microsoft.com/en-us/library/cc645603(v=sql.105).aspx
                */
                switch (msgno)
                {
                    case 220: /* Arithmetic overflow error for data type %ls, value = %ld. */
                    case 517: /* Adding a value to a '%ls' column caused an overflow. */
                    case 518: /* Cannot convert data type %ls to %ls. */
                    case 529: /* Explicit conversion from data type %ls to %ls is not allowed. */
                    case 8114: /* Error converting data type %ls to %ls. */
                    case 8115: /* Arithmetic overflow error converting %ls to data type %ls. */
                    case 8134: /* Divide by zero error encountered. */
                    case 8152: /* String or binary data would be truncated. */
                    {
                        exception = PyExc_tds_DataError;
                        break;
                    }

                    case 515: /* Cannot insert the value NULL into column '%.*ls', table '%.*ls'; column does not allow nulls. %ls fails. */
                    case 544: /* Cannot insert explicit value for identity column in table '%.*ls' when IDENTITY_INSERT is set to OFF. */
                    case 545: /* Explicit value must be specified for identity column in table '%.*ls' either when IDENTITY_INSERT is set to ON or when a replication user is inserting into a NOT FOR REPLICATION identity column. */
                    case 547: /* The %ls statement conflicted with the %ls constraint "%.*ls". The conflict occurred in database "%.*ls", table "%.*ls"%ls%.*ls%ls. */
                    case 548: /*  The insert failed. It conflicted with an identity range check constraint in database '%.*ls', replicated table '%.*ls'%ls%.*ls%ls. */
                    {
                        exception = PyExc_tds_IntegrityError;
                        break;
                    }

                    default:
                    {
                        exception = PyExc_tds_ProgrammingError;
                        break;
                    }
                }
                break;
            }

            /*
                [17-19] Indicate software errors that cannot be corrected by the user.
            */
            case 17:
            case 18:
            case 19:

            /*
                [20-24] Indicate system problems and are fatal errors, which means that the
                Database Engine task that is executing a statement or batch is no longer
                running.
            */
            case 20:
            case 21:
            case 22:
            case 23:
            case 24:
            {
                exception = PyExc_tds_OperationalError;
                break;
            }

            default:
            {
                exception = PyExc_tds_DatabaseError;
                break;
            }
        }
    }
    else
    {
        /* Default to ProgrammingError for user-defined error numbers. */
        exception = PyExc_tds_ProgrammingError;
    }
    raise_lasterror(exception, &connection->lasterror, lastmsg);
}

void Connection_raise_closed(struct Connection* connection)
{
    PyErr_Format(PyExc_tds_InterfaceError, "connection closed");
    UNUSED(connection);
}

void Connection_clear_lastwarning(struct Connection* connection)
{
    Connection_clear_messages(connection);
}

int Connection_raise_lastwarning(struct Connection* connection)
{
    int error = 0;

    /*
        Ignore messages == 0, which includes informational things, e.g.
          * session property changes
          * database change messages
          * PRINT statements
    */
    struct DatabaseMsg* lastmsg;
    for (lastmsg = connection->messages; lastmsg; lastmsg = lastmsg->next)
    {
        if ((lastmsg->msgno > 0) && (!lastmsg->warned))
        {
            /*
                If the last message has a high enough severity, consider it an error instead
                of an "informational" warning. Some of these errors aren't reported by
                FreeTDS' DB-lib implementation, but seemingly should be. As a workaround,
                assume any message of sufficient severity should be treated as an error
                and not only when a DB-lib call returned non-success.
            */
            if (lastmsg->severity <= 10)
            {
                lastmsg->warned = true;

                /* $TODO: figure out some way to include the other metadata in the warning */
                error = PyErr_WarnEx(PyExc_tds_Warning, lastmsg->msgtext, 1);
            }
            else
            {
                Connection_raise_lasterror(connection);
                error = 1;
            }
            if (error)
            {
                break;
            }
        }
    }

    return error;
}

static int Connection_use_internal(struct Connection* connection, const char* database)
{
    RETCODE retcode;

    Py_BEGIN_ALLOW_THREADS

        retcode = dbuse(connection->dbproc, database);

    Py_END_ALLOW_THREADS

    if (FAIL == retcode)
    {
        Connection_raise_lasterror(connection);
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
    lasterror->dberrstr = (dberrstr) ? tds_mem_strdup(dberrstr) : NULL;
    lasterror->oserrstr = (oserrstr) ? tds_mem_strdup(oserrstr) : NULL;

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
    struct Connection* connection = (struct Connection*)dbgetuserdata(dbproc);
    if (connection)
    {
        struct DatabaseMsg* msg = tds_mem_malloc(sizeof(struct DatabaseMsg));
        if (msg)
        {
            struct DatabaseMsg* prev;
            struct DatabaseMsg* curr;

            memset(msg, 0, sizeof(struct DatabaseMsg));

            msg->msgno = msgno;
            msg->msgstate = msgstate;
            msg->severity = severity;
            msg->msgtext = (msgtext) ? tds_mem_strdup(msgtext) : NULL;
            msg->srvname = (srvname) ? tds_mem_strdup(srvname) : NULL;
            msg->proc = (proc) ? tds_mem_strdup(proc) : NULL;
            msg->line = line;

            /* Insert the message into the list in order of descending severity. */
            prev = NULL;
            curr = connection->messages;
            while (NULL != curr && curr->severity >= severity)
            {
                prev = curr;
                curr = curr->next;
            }

            msg->next = curr;
            if (!prev)
            {
                connection->messages = msg;
            }
            else
            {
                prev->next = msg;
            }
        }
    }
    else
    {
        /* Fall back to the thread-local message buffer. */
        struct DatabaseMsg* lastmsg = LastMsg_get();
        DatabaseMsg_clear(lastmsg);

        lastmsg->msgno = msgno;
        lastmsg->msgstate = msgstate;
        lastmsg->severity = severity;
        lastmsg->msgtext = (msgtext) ? tds_mem_strdup(msgtext) : NULL;
        lastmsg->srvname = (srvname) ? tds_mem_strdup(srvname) : NULL;
        lastmsg->proc = (proc) ? tds_mem_strdup(proc) : NULL;
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
        Connection_raise_lasterror(connection);
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
    "If :py:data:`False`, operations must be committed explicitly using\n"
    ":py:meth:`.commit`.\n"
    "\n"
    ":rtype: bool\n";

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
    "The current database or :py:data:`None` if the connection is closed.\n"
    "\n"
    ":rtype: str\n";

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

static const char s_Connection_messages_doc[] =
    "A list of any informational messages received from the last\n"
    ":py:meth:`ctds.Cursor.execute`, :py:meth:`ctds.Cursor.executemany`, or\n"
    ":py:meth:`ctds.Cursor.callproc` call.\n"
    "For example, this will include messages produced by the T-SQL `PRINT`\n"
    "and `RAISERROR` statements. Messages are preserved until the next call\n"
    "to any of the above methods. :py:data:`None` is returned if the\n"
    "connection is closed.\n"
    "\n"
    ":pep:`0249#connection-messages`\n"
    "\n"
    ".. versionadded:: 1.4\n"
    "\n"
    ":rtype: list(dict)\n";

static PyObject* Connection_messages_get(PyObject* self, void* closure)
{
    struct Connection* connection = (struct Connection*)self;

    if (0 != PyErr_WarnEx(PyExc_Warning, "DB-API extension connection.messages used", 1))
    {
        return NULL;
    }

    if (!Connection_closed(connection))
    {
        PyObject* messages = NULL;

        int alloc;
        for (alloc = 1; alloc >= 0; alloc--)
        {
            Py_ssize_t nmsg = 0;

            const struct DatabaseMsg* msg;
            for (msg = connection->messages; NULL != msg; msg = msg->next)
            {
                if (!alloc)
                {
                    PyObject* message = build_message_dict(msg);
                    if (!message)
                    {
                        break;
                    }
                    PyList_SET_ITEM(messages, nmsg, message);
                }

                nmsg++;
            }

            if (alloc)
            {
                messages = PyList_New(nmsg);
                if (!messages)
                {
                    break;
                }
            }

            if (PyErr_Occurred())
            {
                break;
            }
        }

        if (PyErr_Occurred())
        {
            Py_XDECREF(messages);
            messages = NULL;
        }
        return messages;
    }
    else
    {
        Py_RETURN_NONE;
    }

    UNUSED(closure);
}

static const char s_Connection_spid_doc[] =
    "The SQL Server Session Process ID (SPID) for the connection or\n"
    ":py:data:`None` if the connection is closed.\n"
    "\n"
    ":rtype: int\n";

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
    "The TDS version in use for the connection or :py:data:`None` if the\n"
    "connection is closed.\n"
    "\n"
    ":rtype: str\n";

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
    "The connection timeout, in seconds, or :py:data:`None` if the connection\n"
    "is closed.\n"
    "\n"
    ".. note:: Setting the timeout requires FreeTDS version 1.00 or later.\n"
    "\n"
    ":raises ctds.NotSupportedError: `cTDS` was compiled against a version of\n"
    "    FreeTDS which does not support setting the timeout on a connection.\n"
    "\n"
    ":rtype: int\n";

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
            (void)PyOS_snprintf(str, sizeof(str), "%d", (int)timeout);
            if (FAIL == dbsetopt(connection->dbproc, DBSETTIME, str, (int)timeout))
            {
                Connection_raise_lasterror(connection);
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
    { (char*)"messages",    Connection_messages_get,    NULL,                      (char*)s_Connection_messages_doc,    NULL },
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

    return Cursor_create(connection, connection->paramstyle);
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
    "    Version 1.9 supports passing rows as :py:class:`dict`. Keys must map\n"
    "    to column names and must exist for all non-NULL columns.\n"
    ":type rows: :ref:`typeiter <python:typeiter>`\n"

    ":param int batch_size: An optional batch size.\n"

    ":param bool tablock: Should the `TABLOCK` hint be passed?\n"

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

        rpcparams = tds_mem_calloc((size_t)size, sizeof(struct Parameter*));
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
                rpcparams[ix] = Parameter_create(value, false /* output */);
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

            if (0 != Parameter_bind(rpcparams[ix], Connection_DBPROCESS(connection)))
            {
                break;
            }

            if (PyUnicode_Check(Parameter_value(rpcparams[ix])))
            {
                if (0 != PyErr_WarnEx(PyExc_Warning,
                                      "Direct bulk insert of a Python str object may result in unexpected character encoding. "
                                      "It is recommended to explicitly encode Python str values for bulk insert.",
                                      1))
                {
                    break;
                }
            }

            /* bcp_bind does not make a network request, so no need to release the GIL. */

            /* bcp_bind expects a 1-based column index. */
            retcode = Parameter_bcp_bind(rpcparams[ix], connection->dbproc, (size_t)(ix + 1));
            if (FAIL == retcode)
            {
                Connection_raise_lasterror(connection);
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
            Connection_raise_lasterror(connection);
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

            DBINT processed = 0;

            DBINT ncolumns = 0;
            struct {
                bool nullable;
                bool identity;
                char* name;
            }* columns = NULL;
            bool initialized = false;

            while (NULL != (row = PyIter_Next(irows)))
            {
#define INVALID_SEQUENCE_FMT "invalid sequence for row %zd"
                PyObject* sequence = NULL;

                char msg[ARRAYSIZE(INVALID_SEQUENCE_FMT) + ARRAYSIZE(STRINGIFY(UINT64_MAX))];

                /* Initialize only if there are rows to send. */
                if (!initialized)
                {
                    size_t column;

                    Py_BEGIN_ALLOW_THREADS

                        do
                        {
                            retcode = bcp_init(connection->dbproc, table, NULL, NULL, DB_IN);
                            if (FAIL == retcode)
                            {
                                break;
                            }

                            initialized = true;

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
                        Connection_raise_lasterror(connection);
                        break;
                    }

                    /*
                        Store an ordered list of table column names. Once
                        insertion starts this information won't be available.
                    */
                    ncolumns = dbnumcols(connection->dbproc);
                    assert(ncolumns > 0);
                    columns = tds_mem_calloc((size_t)ncolumns, sizeof(*columns));
                    if (!columns)
                    {
                        PyErr_NoMemory();
                        break;
                    }

                    for (column = 0; column < (size_t)ncolumns; ++column)
                    {
                        DBCOL dbcol;
                        retcode = dbcolinfo(connection->dbproc, CI_REGULAR, (DBINT)column + 1, 0 /* ignored by dblib */,
                                            &dbcol);
                        if (FAIL == retcode)
                        {
                            Connection_raise_lasterror(connection);
                            break;
                        }

                        columns[column].nullable = dbcol.Null;
                        columns[column].identity = dbcol.Identity;
                        columns[column].name = tds_mem_strdup(dbcol.ActualName);
                        if (!columns[column].name)
                        {
                            PyErr_NoMemory();
                            break;
                        }
                    }

                    if (PyErr_Occurred())
                    {
                        break;
                    }
                }

                if (PyMapping_Check(row) && !PySequence_Check(row))
                {
                    PyObject* tuple = PyTuple_New((Py_ssize_t)ncolumns);
                    if (tuple)
                    {
                        /* Construct the sequence based on the column info. */
                        size_t column;
                        for (column = 0; column < (size_t)ncolumns; ++column)
                        {
                            /* Retrieve the value by name from the row. */
                            PyObject* value = PyMapping_GetItemString(row, columns[column].name);
                            if (!value)
                            {
                                if (columns[column].nullable || columns[column].identity)
                                {
                                    PyErr_Clear();
                                    value = Py_None;
                                    Py_INCREF(value);
                                }
                                else
                                {
                                    assert(PyErr_Occurred()); /* set by PyMapping_GetItemString */
                                    break;
                                }
                            }

                            PyTuple_SET_ITEM(tuple, (Py_ssize_t)column, value);
                            /* value reference stolen by PyTuple_SET_ITEM */
                        }

                        if (!PyErr_Occurred())
                        {
                            sequence = PySequence_Fast(tuple, "internal error");
                        }

                        Py_DECREF(tuple);
                    }
                }
                else
                {
                    (void)sprintf(msg, INVALID_SEQUENCE_FMT, sent);
                    sequence = PySequence_Fast(row, msg);
                }

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
            } /* while (NULL != (row = PyIter_Next(irows))) */

            if (columns)
            {
                size_t column;
                for (column = 0; column < (size_t)ncolumns; ++column)
                {
                    tds_mem_free(columns[column].name);
                }
                tds_mem_free(columns);
            }

            if (initialized)
            {
                /* Always call bcp_done() regardless of previous errors. */
                Py_BEGIN_ALLOW_THREADS

                    processed = bcp_done(connection->dbproc);

                Py_END_ALLOW_THREADS
            }

            if (-1 != processed)
            {
                saved += processed;
            }
            else
            {
                /* Don't overwrite a previous error if bcp_done fails. */
                if (!PyErr_Occurred())
                {
                    Connection_raise_lasterror(connection);
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
    ":returns: :py:data:`None`\n";

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
    "    is raised in the context, otherwise :py:data:`None`.\n"
    ":param Exception exc_val: The exception value, if an exception\n"
    "    is raised in the context, otherwise :py:data:`None`.\n"
    ":param object exc_tb: The exception traceback, if an exception\n"
    "    is raised in the context, otherwise :py:data:`None`.\n"
    "\n"
    ":returns: :py:data:`None`\n";

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


#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif /* if defined(__GNUC__) && (__GNUC__ > 7) */

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

#if defined(__GNUC__) && (__GNUC__ > 7)
#  pragma GCC diagnostic pop
#endif

PyObject* Connection_create(const char* server, uint16_t port, const char* instance,
                            const char* username, const char* password,
                            const char* database, const char* appname, const char* hostname,
                            unsigned int login_timeout, unsigned int timeout,
                            const char* tds_version, bool autocommit,
                            bool ansi_defaults, bool enable_bcp,
                            enum ParamStyle paramstyle,
                            bool read_only, bool ntlmv2)
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

            DBINT dbversion = DBVERSION_UNKNOWN;

            int flag;

            int written;

            /* Determine the maximum size the server connection string may require. */
            size_t nservername = strlen(server) + 1 /* for '\\' or ':' */ +
                ((instance) ? strlen(instance) : STRLEN(STRINGIFY(UINT16_MAX)) /* maximum port number length */) +
                1 /* for '\0' */;

            servername = tds_mem_malloc(nservername);
            if (!servername)
            {
                PyErr_NoMemory();
                break;
            }

            written = PyOS_snprintf(servername, nservername, "%s", server);

            if (instance)
            {
                (void)PyOS_snprintf(&servername[written], nservername - (size_t)written, "\\%s", instance);
            }
            else
            {
                (void)PyOS_snprintf(&servername[written], nservername - (size_t)written, ":%d", port);
            }

            connection->login = dblogin();
            if (!connection->login)
            {
                PyErr_NoMemory();
                break;
            }

#if defined(CTDS_USE_UTF16)
            /*
                Force the connections to use UTF-16 over UCS-2.
            */
            if (FAIL == DBSETLUTF16(connection->login, 1))
            {
                PyErr_SetString(PyExc_RuntimeError, "failed to set connection encoding");
                break;
            }
#endif /* if defined(CTDS_USE_UTF16) */

            /*
                Note: FreeTDS only supports single-byte encodings (e.g. latin-1, UTF-8).
                UTF-8 is really the only choice if Unicode support is desired.
            */
            if (FAIL == DBSETLCHARSET(connection->login, "UTF-8"))
            {
                PyErr_SetString(PyExc_RuntimeError, "failed to set client charset");
                break;
            }

            if (username)
            {
                if (FAIL == DBSETLUSER(connection->login, username))
                {
                    PyErr_SetString(PyExc_tds_InterfaceError, username);
                    break;
                }
            }
            if (password)
            {
                if (FAIL == DBSETLPWD(connection->login, password))
                {
                    PyErr_SetString(PyExc_tds_InterfaceError, password);
                    break;
                }
            }
            if (appname)
            {
                if (FAIL == DBSETLAPP(connection->login, appname))
                {
                    PyErr_SetString(PyExc_tds_InterfaceError, appname);
                    break;
                }
            }
            if (hostname)
            {
                if (FAIL == DBSETLHOST(connection->login, hostname))
                {
                    PyErr_SetString(PyExc_tds_InterfaceError, hostname);
                    break;
                }
            }

            if (ntlmv2)
            {
#if defined(CTDS_HAVE_NTLMV2)
                if (FAIL == DBSETLNTLMV2(connection->login, (int)ntlmv2))
                {
                    PyErr_SetString(PyExc_RuntimeError, "failed to set NTLMv2");
                    break;
                }
#else /* if defined(CTDS_HAVE_NTLMV2) */
                PyErr_Format(PyExc_NotImplementedError, "NTLMv2 is not supported");
                break;
#endif /* else if defined(CTDS_HAVE_NTLMV2) */
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
                if ((DBVERSION_UNKNOWN == dbversion) || (FAIL == DBSETLVERSION(connection->login, (BYTE)dbversion)))
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
                    PyErr_Format(PyExc_RuntimeError, "failed to set TDS version");
                    break;
                }
            }

            if (read_only)
            {
#if defined(CTDS_HAVE_READONLY_INTENT)
                if (FAIL == DBSETLREADONLY(connection->login, (int)read_only))
                {
                    PyErr_SetString(PyExc_RuntimeError, "failed to set read-only intent");
                    break;
                }
#else /* if defined(CTDS_HAVE_READONLY_INTENT) */
                PyErr_Format(PyExc_NotImplementedError, "read-only intent is not supported");
                break;
#endif /* else if defined(CTDS_HAVE_READONLY_INTENT) */
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
            /*
                Prior to 1.00.40 FreeTDS reversed the boolean for enabling BCP.
                See https://github.com/FreeTDS/freetds/issues/119.

                Try both true and false until the desired result is achieved.
            */
            for (flag = 1; flag >= 0; --flag)
            {
                if (FAIL == BCP_SETL(connection->login, flag))
                {
                    PyErr_SetString(PyExc_RuntimeError, "failed to enable bcp");
                    break;
                }
                if (bcp_getl(connection->login) == enable_bcp)
                {
                    break;
                }
            }
            if (PyErr_Occurred())
            {
                break;
            }
            assert(bcp_getl(connection->login) == enable_bcp);

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
                DatabaseMsg_clear(LastMsg_get());

                connection->dbproc = dbopen(connection->login, servername);

            Py_END_ALLOW_THREADS

            if (NULL == connection->dbproc)
            {
                const struct DatabaseMsg* lastmsg = LastMsg_get();
                raise_lasterror(PyExc_tds_OperationalError, LastError_get(), lastmsg->msgno ? lastmsg : NULL);
                break;
            }

            connection->query_timeout = (int)timeout;
            connection->paramstyle = paramstyle;

            dbsetuserdata(connection->dbproc, (BYTE*)connection);

            if (0 != Connection_execute(connection,
                                        4,
                                        (ansi_defaults) ? s_ansi_default_stmt : "",
                                        "SET IMPLICIT_TRANSACTIONS ",
                                        (autocommit) ? "OFF" : "ON",
                                        ";"))
            {
                break;
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
    "ctds.Connection",             /* tp_name */
    sizeof(struct Connection),     /* tp_basicsize */
    0,                             /* tp_itemsize */
    Connection_dealloc,            /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0,                             /* tp_vectorcall_offset */
#else
    NULL,                          /* tp_print */
#endif /* if PY_VERSION_HEX >= 0x03080000 */
    NULL,                          /* tp_getattr */
    NULL,                          /* tp_setattr */
    NULL,                          /* tp_reserved */
    NULL,                          /* tp_repr */
    NULL,                          /* tp_as_number */
    NULL,                          /* tp_as_sequence */
    NULL,                          /* tp_as_mapping */
    NULL,                          /* tp_hash */
    NULL,                          /* tp_call */
    NULL,                          /* tp_str */
    NULL,                          /* tp_getattro */
    NULL,                          /* tp_setattro */
    NULL,                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,            /* tp_flags */
    s_tds_Connection_doc,          /* tp_doc */
    NULL,                          /* tp_traverse */
    NULL,                          /* tp_clear */
    NULL,                          /* tp_richcompare */
    0,                             /* tp_weaklistoffset */
    NULL,                          /* tp_iter */
    NULL,                          /* tp_iternext */
    Connection_methods,            /* tp_methods */
    NULL,                          /* tp_members */
    Connection_getset,             /* tp_getset */
    NULL,                          /* tp_base */
    NULL,                          /* tp_dict */
    NULL,                          /* tp_descr_get */
    NULL,                          /* tp_descr_set */
    0,                             /* tp_dictoffset */
    NULL,                          /* tp_init */
    NULL,                          /* tp_alloc */
    NULL,                          /* tp_new */
    NULL,                          /* tp_free */
    NULL,                          /* tp_is_gc */
    NULL,                          /* tp_bases */
    NULL,                          /* tp_mro */
    NULL,                          /* tp_cache */
    NULL,                          /* tp_subclasses */
    NULL,                          /* tp_weaklist */
    NULL,                          /* tp_del */
    0,                             /* tp_version_tag */
#if PY_VERSION_HEX >= 0x03040000
    NULL,                          /* tp_finalize */
#endif /* if PY_VERSION_HEX >= 0x03040000 */
#if PY_VERSION_HEX >= 0x03080000
    NULL,                          /* tp_vectorcall */
#  if PY_VERSION_HEX < 0x03090000
    NULL,                          /* tp_print */
#  endif /* if PY_VERSION_HEX < 0x03090000 */
#endif /* if PY_VERSION_HEX >= 0x03080000 */
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
