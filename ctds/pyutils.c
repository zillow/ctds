#include "include/push_warnings.h"
#include <Python.h>
/* `datetime.h` must only be included here. */
#include <datetime.h>
#include "include/pop_warnings.h"

#include <sys/types.h>

#include "include/pyutils.h"

int PyObject_SetAttrStringLongValue(PyObject* o, const char* name, long v)
{
    int result = -1;
    PyObject* value = PyLong_FromLong(v);
    if (value)
    {
        result = PyObject_SetAttrString(o, name, value);
    }
    Py_XDECREF(value);
    return result;
}

int PyDict_SetItemStringLongValue(PyObject* d, const char* name, long v)
{
    int result = -1;
    PyObject* value = PyLong_FromLong(v);
    if (value)
    {
        result = PyDict_SetItemString(d, name, value);
    }
    Py_XDECREF(value);
    return result;
}

int PyDict_SetItemStringStringValue(PyObject* d, const char* name, const char* v)
{
    int result = -1;
    PyObject* value = PyUnicode_FromString(v);
    if (value)
    {
        result = PyDict_SetItemString(d, name, value);
    }
    Py_XDECREF(value);
    return result;
}


/* decimal.Decimal reference. */
static PyObject* PyDecimalType = NULL;

int PyDecimalType_init(void)
{
    PyObject* moddecimal;
    assert(!PyDecimalType); /* should not be importing multiple times. */
    moddecimal = PyImport_ImportModule("decimal");
    if (moddecimal)
    {
        PyDecimalType = PyObject_GetAttrString(moddecimal, "Decimal");
        Py_DECREF(moddecimal);
    }
    return !!PyDecimalType;
}

void PyDecimalType_free(void)
{
    Py_XDECREF(PyDecimalType);
}

int PyDecimal_Check(PyObject* o)
{
    return PyObject_TypeCheck(o, (PyTypeObject*)PyDecimalType);
}

PyObject* PyDecimal_FromString(const char* str, Py_ssize_t size)
{
    return PyObject_CallFunction(PyDecimalType, "s#", str, size);
}

/* uuid.UUID reference */
static PyObject* PyUuidType = NULL;

int PyUuidType_init(void)
{
    PyObject* moduuid;
    assert(!PyUuidType);
    moduuid = PyImport_ImportModule("uuid"); /* should not be importing multiple times. */
    if (moduuid)
    {
        PyUuidType = PyObject_GetAttrString(moduuid, "UUID");
        Py_DECREF(moduuid);
    }
    return !!PyUuidType;
}

void PyUuidType_free(void)
{
    Py_XDECREF(PyUuidType);
}

int PyUuid_Check(PyObject* o)
{
    return PyObject_TypeCheck(o, (PyTypeObject*)PyUuidType);
}

PyObject* PyUuid_FromBytes(const char* bytes, Py_ssize_t size)
{
    PyObject* args = NULL;
    PyObject* kwargs = NULL;
    PyObject* obytes = NULL;

    PyObject* uuid = NULL;
    do
    {
        args = PyTuple_New(0);
        if (!args) break;

        kwargs = PyDict_New();
        if (!kwargs) break;

        obytes = PyBytes_FromStringAndSize(bytes, size);
        if (!obytes) break;

#ifdef __BIG_ENDIAN__
#  define UUID_BYTES "bytes"
#else
#  define UUID_BYTES "bytes_le"
#endif
        if (0 != PyDict_SetItemString(kwargs, UUID_BYTES, obytes)) break;

        uuid = PyObject_Call(PyUuidType, args, kwargs);
    }
    while (0);

    Py_XDECREF(args);
    Py_XDECREF(kwargs);
    Py_XDECREF(obytes);

    return uuid;
}


int PyDateTimeType_init(void)
{
    assert(!PyDateTimeAPI);
    return !!(PyDateTime_IMPORT);
}

void PyDateTimeType_free(void)
{
    Py_XDECREF(PyDateTimeAPI);
}

int PyDateTime_Check_(PyObject* o) { return PyDateTime_Check(o); }
int PyDate_Check_(PyObject* o) { return PyDate_Check(o); }
int PyTime_Check_(PyObject* o) { return PyTime_Check(o); }

PyObject* PyDateTime_FromDateAndTime_(int year, int month, int day, int hour, int minute, int second, int usecond)
{
    return PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, usecond);
}

PyObject* PyDateTime_FromTimestamp_(PyObject* args)
{
    return PyDateTime_FromTimestamp(args);
}

PyObject* PyDate_FromDate_(int year, int month, int day)
{
    return PyDate_FromDate(year, month, day);
}

PyObject* PyDate_FromTimestamp_(PyObject* args)
{
    return PyDate_FromTimestamp(args);
}

PyObject* PyTime_FromTime_(int hour, int minute, int second, int usecond)
{
    return PyTime_FromTime(hour, minute, second, usecond);
}

int PyDateTime_GET_YEAR_(PyObject* o) { return PyDateTime_GET_YEAR(o); }
int PyDateTime_GET_MONTH_(PyObject* o) { return PyDateTime_GET_MONTH(o); }
int PyDateTime_GET_DAY_(PyObject* o) { return PyDateTime_GET_DAY(o); }
int PyDateTime_DATE_GET_HOUR_(PyObject* o) { return PyDateTime_DATE_GET_HOUR(o); }
int PyDateTime_DATE_GET_MINUTE_(PyObject* o) { return PyDateTime_DATE_GET_MINUTE(o); }
int PyDateTime_DATE_GET_SECOND_(PyObject* o) { return PyDateTime_DATE_GET_SECOND(o); }
int PyDateTime_DATE_GET_MICROSECOND_(PyObject* o) { return PyDateTime_DATE_GET_MICROSECOND(o); }

int PyDateTime_TIME_GET_HOUR_(PyObject* o) { return PyDateTime_TIME_GET_HOUR(o); }
int PyDateTime_TIME_GET_MINUTE_(PyObject* o) { return PyDateTime_TIME_GET_MINUTE(o); }
int PyDateTime_TIME_GET_SECOND_(PyObject* o) { return PyDateTime_TIME_GET_SECOND(o); }
int PyDateTime_TIME_GET_MICROSECOND_(PyObject* o) { return PyDateTime_TIME_GET_MICROSECOND(o); }

