#ifndef __TDS_H__
#define __TDS_H__

#include "push_warnings.h"
#include <Python.h>
#include <sybdb.h>
#include "pop_warnings.h"

#include <stdlib.h>

extern PyObject* PyExc_tds_Warning;
extern PyObject* PyExc_tds_Error;
extern PyObject* PyExc_tds_InterfaceError;
extern PyObject* PyExc_tds_DatabaseError;
extern PyObject* PyExc_tds_DataError;
extern PyObject* PyExc_tds_OperationalError;
extern PyObject* PyExc_tds_IntegrityError;
extern PyObject* PyExc_tds_InternalError;
extern PyObject* PyExc_tds_ProgrammingError;
extern PyObject* PyExc_tds_NotSupportedError;

#define tds_mem_malloc malloc
#define tds_mem_realloc realloc
#define tds_mem_calloc calloc
#define tds_mem_free free

#define TDS_CHAR_MIN_SIZE 1
#define TDS_CHAR_MAX_SIZE 8000
#define TDS_NCHAR_MIN_SIZE 1
#define TDS_NCHAR_MAX_SIZE 4000

/*
    If the UTF-16 option can be set on the connection, use UTF-16.
    This option was added in FreeTDS 1.00
*/
#if defined(DBSETUTF16)
#  define CTDS_USE_UTF16 1
#endif

/*
    Use `sp_executesql` when possible for the execute*() methods. This method
    won't work on older versions of FreeTDS which don't properly support passing
    NVARCHAR data.

    Versions 0.95 and later of FreeTDS will support using the `sp_executesql`
    implementation.

    Detection is done via the DATETIME2BIND macro.
*/
#if defined(DATETIME2BIND)
#  define CTDS_USE_SP_EXECUTESQL 1
#  define CTDS_USE_NCHARS 1
#endif

#endif /* ifndef __TDS_H__ */
