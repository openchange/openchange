/*
   OpenChange MAPI implementation.

   Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PYMAPI_H_
#define _PYMAPI_H_

#include <libmapi/libmapi.h>
#include <Python.h>

/* mapi.Session */
PyAPI_DATA(PyTypeObject) PyMapiSessionType;
PyObject *PyMapiSession_FromMapiSession(struct mapi_session *session);

/* mapi.Object */
typedef struct {
	PyObject_HEAD	
	mapi_object_t *object;
} PyMapiObjectObject;

PyAPI_DATA(PyTypeObject) PyMapiObjectType;
mapi_object_t *PyMapiObject_GetMapiObject(PyObject *);
#define PyMapiObject_Check(op) PyObject_TypeCheck(op, &PyMapiObjectType)

/* mapi.MessageStore */
PyAPI_DATA(PyTypeObject) PyMapiMsgStoreType;

/* Set a MAPISTATUS error as a Python exception */
#define PyErr_FromMAPISTATUS(err) Py_BuildValue("(i,z)", err, NULL)
#define PyErr_SetMAPISTATUS(err) PyErr_SetObject(PyExc_RuntimeError, PyErr_FromMAPISTATUS(err))
#define PyErr_MAPISTATUS_IS_ERR_RAISE(err) \
	if (err != MAPI_E_SUCCESS) { \
		PyErr_SetMAPISTATUS(err); \
		return NULL; \
	}

#endif
