#ifndef __TDS_H__
#define __TDS_H__

#include <Python.h>
#include <stdlib.h>

PyObject* PyExc_tds_Warning;
PyObject* PyExc_tds_Error;
PyObject* PyExc_tds_InterfaceError;
PyObject* PyExc_tds_DatabaseError;
PyObject* PyExc_tds_DataError;
PyObject* PyExc_tds_OperationalError;
PyObject* PyExc_tds_IntegrityError;
PyObject* PyExc_tds_InternalError;
PyObject* PyExc_tds_ProgrammingError;
PyObject* PyExc_tds_NotSupportedError;

#define tds_mem_malloc malloc
#define tds_mem_realloc realloc
#define tds_mem_calloc calloc
#define tds_mem_free free

#endif /* ifndef __TDS_H__ */
