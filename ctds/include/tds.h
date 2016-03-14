#ifndef __TDS_H__
#define __TDS_H__

#include <Python.h>
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

#endif /* ifndef __TDS_H__ */
