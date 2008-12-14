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

#include "pymapi/pymapi.h"

typedef struct {
	PyObject_HEAD	
	struct mapi_session *session;
} PyMapiSessionObject;

static PyObject *py_session_create(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	/* FIXME */
	return (PyObject *)PyObject_New(PyMapiSessionObject, type);
}

static PyObject *py_session_login(PyObject *args, PyObject *kwargs)
{
	/* FIXME */
	return NULL;
}

static PyMethodDef session_methods[] = {
	{ "login", (PyCFunction) py_session_login, METH_VARARGS|METH_KEYWORDS },
	{ NULL },
};

static PyGetSetDef session_getsetters[] = {
	{ "default_profile_path", NULL, NULL },
	{ "profile_name", NULL, NULL },
	{ "message_store", NULL, NULL },
	{ NULL }
};

PyTypeObject PyMapiSessionType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "Session",
	.tp_basicsize = sizeof(PyMapiSessionObject),
	.tp_methods = session_methods,
	.tp_getset = session_getsetters,
	.tp_doc = "MAPI Session",
	.tp_new = py_session_create,
};

