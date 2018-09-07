#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "push_warnings.h"
#include <Python.h>
#include <sybdb.h>
#include "pop_warnings.h"

#include "c99bool.h"
#include "c99int.h"

#include "tds.h"


PyTypeObject* ConnectionType_init(void);
extern PyTypeObject ConnectionType;

/**
    Create a new connection to the database.

    If the connection creation fails, a Python Exception will be set
    appropriately.

    @note This method returns a new reference.

    @param server [in] The database hostname to connect to.

    @param port [in] The port the database service is running on.

    @param instance [in] The name of the SQL Server instance to connect to.
            This may be NULL.

    @param username [in] The username to use for connection authentication.
            This may be NULL.

    @param password [in] The password to use for connection authentication.
            This may be NULL.

    @param database [in] An optional database to connect to initially.
            This may be NULL.

    @param appname [in] An optional application name to associate with the
            connection. This may be NULL.

    @param hostname [in] An optional client host name to associate with the
            connection. This may be NULL.

    @param login_timeout [in] An optional timeout, in seconds, to use during
            initial connection.

    @param timeout [in] An optional timeout, in seconds, to use during
            data retrieval operations.

    @param tds_version [in] The version of the TDS protocoal to use for the
            connection.

    @param autocommit [in] Should cursor calls be wrapped in a transaction
            requiring an explicit commit or automatically committed?

    @param ansi_defaults [in] Should the ANSI_DEFAULTS settings (and related)
            be turned on. This behavior is typically desired to mimic both ODBC
            drivers and SQL Server Management Studio.

    @param enable_bcp [in] Should bulk copy (bcp) be enabled on this connection.
            Typically there is no harm in enabling it, aside from older databases
            not supporting it and failing the connection.

    @param paramstyle [in] The paramstyle to associate with the connection.

    @param read_only [in] Include read-only application intent in login request.

    @param ntlmv2 [in] Include NTLMv2 authentication support in login request.

    @return NULL if the connection creation failed.
    @return The created connection object.

 */
PyObject* Connection_create(const char* server, uint16_t port, const char* instance,
                            const char* username, const char* password,
                            const char* database, const char* appname, const char* hostname,
                            unsigned int login_timeout, unsigned int timeout,
                            const char* tds_version, bool autocommit,
                            bool ansi_defaults, bool enable_bcp,
                            enum ParamStyle paramstyle,
                            bool read_only, bool ntlmv2);


struct Connection; /* forward declaration */

/**
    Check if a connection is closed.

    @param connection [in] The connection.

    @return A boolean indicating if the connection is closed.
 */
int Connection_closed(struct Connection* connection);

/**
    Raise the last error seen on this connection as a Python Exception.

    @param connection [in] The connection.
*/
void Connection_raise_lasterror(struct Connection* connection);

/**
    Raise the connection closed Python Exception.

    @param connection [in] The connection.
*/
void Connection_raise_closed(struct Connection* connection);

/**
    Clear the last warning associated with a connection.

    This should be called prior to any database operation in order
    to properly report any non-error messages issued by the database.

    @param connection [in] The connection.
*/
void Connection_clear_lastwarning(struct Connection* connection);

/**
    Raise the last warning associated with a connection, if any.

    This should be called after a successful database operation to
    notify the client of any non-error messages issued by the database.

    @param connection [in] The connection.

    @retval 0 The warning was raised.
    @retval -1 The caller requested the warning be turned into an error.
*/
int Connection_raise_lastwarning(struct Connection* connection);

/**
    Commit a SQL transaction on a connection.

    @note This method sets an appropriate Python error on failure.

    @param connection [in] The connection.

    @retval 0 The command succeeded.
    @retval -1 The command failed.
*/
int Connection_transaction_commit(struct Connection* connection);

/**
    Rollback a SQL transaction on a connection.

    @note This method sets an appropriate Python error on failure.

    @param connection [in] The connection.

    @retval 0 The command succeeded.
    @retval -1 The command failed.
*/
int Connection_transaction_rollback(struct Connection* connection);

/* dblib handlers for error/message processing. */
int Connection_dberrhandler(DBPROCESS* dbproc, int severity, int dberr,
                            int oserr, char* dberrstr, char* oserrstr);

int Connection_dbmsghandler(DBPROCESS* dbproc, DBINT msgno, int msgstate,
                            int severity, char* msgtext, char* srvname,
                            char* proc, int line);


DBPROCESS* Connection_DBPROCESS(struct Connection* connection);

#endif /* ifndef __CONNECTION_H__ */
