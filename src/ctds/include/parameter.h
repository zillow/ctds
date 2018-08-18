#ifndef __PARAMETER_H__
#define __PARAMETER_H__

#include "push_warnings.h"
#include <Python.h>
#include <sybdb.h>
#include "pop_warnings.h"

#include "c99bool.h"

#include "tds.h"

PyTypeObject* ParameterType_init(void);
int Parameter_Check(PyObject* o);
PyTypeObject* ParameterType_get(void);

struct Parameter; /* forward decl. */

/**
    Create/bind an RPC parameter to a Python object.

    @note This method sets an appropriate Python error on failure.
    @note This method returns a new reference.

    @param value [in] The Python object to bind to the parameter.
    @param output [in] Whether or not the parameter is an output parameter.

    @return The bound RPC parameter.
    @retval NULL on failure.
*/
struct Parameter* Parameter_create(PyObject* value, bool output);

RETCODE Parameter_dbrpcparam(struct Parameter* parameter, DBPROCESS* dbproc, const char* paramname);

RETCODE Parameter_bcp_bind(struct Parameter* parameter, DBPROCESS* dbproc, size_t column);


bool Parameter_output(struct Parameter* rpcparam);

/**
    Get the SQL type of this parameter.

    @note The caller is required to release the returned value using free().

    @param minimum_width [in] Use the MAX width for variable width types instead of
      inferring it from the size of the parameter.
*/
char* Parameter_sqltype(struct Parameter* rpcparam, bool maximum_width);

PyObject* Parameter_value(struct Parameter* rpcparam);

#if !defined(CTDS_USE_SP_EXECUTESQL)

char* Parameter_serialize(struct Parameter* rpcparam, bool maximum_width, size_t* nserialized);

#endif /* if !defined(CTDS_USE_SP_EXECUTESQL) */

#endif /* ifndef __PARAMETER_H__ */
