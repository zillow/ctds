#ifndef __PYUTILS_H__
#define __PYUTILS_H__

#include "push_warnings.h"
#include <Python.h>
#include "pop_warnings.h"

/* Wrapper around PyObject_SetAttrString; returns -1 on failure. */
int PyObject_SetAttrStringLongValue(PyObject* o, const char* name, long v);

/* Wrapper around PyDict_SetItemString; returns -1 on failure. */
int PyDict_SetItemStringLongValue(PyObject* d, const char* name, long v);

/* Wrapper around PyDict_SetItemString; returns -1 on failure. */
int PyDict_SetItemStringStringValue(PyObject* d, const char* name, const char* v);

/* Initialize the decimal.Decimal type; returns 0 on failure. */
int PyDecimalType_init(void);
void PyDecimalType_free(void);

int PyDecimal_Check(PyObject* o);

/*
    Create a decimal.Decimal object from a string; returns NULL on failure.
    PyDecimalType_init() must be called prior.
*/
PyObject* PyDecimal_FromString(const char* str, Py_ssize_t size);

/* Initialize the uuid.UUID type; returns 0 on failure. */
int PyUuidType_init(void);
void PyUuidType_free(void);

int PyUuid_Check(PyObject* o);

/*
    Create a uuid.UUID object from a byte string; returns NULL on failure.
    PyUuidType_init() must be called prior.
*/
PyObject* PyUuid_FromBytes(const char* bytes, Py_ssize_t size);


/* Initialize the datetime module; returns 0 on failure. */
int PyDateTimeType_init(void);
void PyDateTimeType_free(void);

/*
    Because the `datetime.h` header for Python datetime objects also includes
    the global object variable for the types, the `datetime.h` can only be
    included in one location otherwise multiple global variables will be
    compiled in the binary. In order to still have access to the various
    Datetime-related macros and functions, wrappers must be exposed here
    with implementations with access to the actual datetime type global
    variable (i.e. in the file where `datetime.h` is included).
*/
int PyDateTime_Check_(PyObject* o);
int PyDate_Check_(PyObject* o);
int PyTime_Check_(PyObject* o);

PyObject* PyDateTime_FromDateAndTime_(int year, int month, int day, int hour, int minute, int second, int usecond);
PyObject* PyDateTime_FromTimestamp_(PyObject* args);
PyObject* PyDate_FromDate_(int year, int month, int day);
PyObject* PyDate_FromTimestamp_(PyObject* args);
PyObject* PyTime_FromTime_(int hour, int minute, int second, int usecond);

int PyDateTime_GET_YEAR_(PyObject* o);
int PyDateTime_GET_MONTH_(PyObject* o);
int PyDateTime_GET_DAY_(PyObject* o);
int PyDateTime_DATE_GET_HOUR_(PyObject* o);
int PyDateTime_DATE_GET_MINUTE_(PyObject* o);
int PyDateTime_DATE_GET_SECOND_(PyObject* o);
int PyDateTime_DATE_GET_MICROSECOND_(PyObject* o);

int PyDateTime_TIME_GET_HOUR_(PyObject* o);
int PyDateTime_TIME_GET_MINUTE_(PyObject* o);
int PyDateTime_TIME_GET_SECOND_(PyObject* o);
int PyDateTime_TIME_GET_MICROSECOND_(PyObject* o);

#endif /* ifndef __PYUTILS_H__ */
