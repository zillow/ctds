#ifndef __CURSOR_H__
#define __CURSOR_H__

#include "push_warnings.h"
#include <Python.h>
#include "pop_warnings.h"

#include "tds.h"

/**
    Initialize the Cursor Python type object.

    @note This method returns a new reference.

    @return NULL indicating the initialization failed.
    @return The initialized Python type object.
*/
PyTypeObject* CursorType_init(void);

/**
    Initialize the RowList Python type object.

    @note This method returns a new reference.

    @return NULL indicating the initialization failed.
    @return The initialized Python type object.
*/
PyTypeObject* RowListType_init(void);

/**
    Initialize the Row Python type object.

    @note This method returns a new reference.

    @return NULL indicating the initialization failed.
    @return The initialized Python type object.
*/
PyTypeObject* RowType_init(void);

struct Connection; /* forward declaration */

/**
    Create a new cusor object.

    @note This method returns a new reference.
    @note This method steals a reference to the connection object.

    @param connection [in] The connection object which owns this cursor.

    @return NULL indicating the creation failed.
    @return The created Cursor object, with a reference count of 1.
*/
PyObject* Cursor_create(struct Connection* connection, enum ParamStyle paramstyle);

#endif /* ifndef __CURSOR_H__ */
