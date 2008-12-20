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

void initmapi(void);

static PyObject *py_get_proptag_value(PyObject *mod, PyObject *args)
{
	char *propname;
	if (!PyArg_ParseTuple(args, "s", &propname))
		return NULL;

	return PyInt_FromLong(get_proptag_value(propname));
}

static PyObject *py_get_proptag_name(PyObject *mod, PyObject *args)
{
	uint32_t proptag;
	const char *name;
	if (!PyArg_ParseTuple(args, "i", &proptag))
		return NULL;

	name = get_proptag_name(proptag);
	if (name == NULL)
		return Py_None;
	return PyString_FromString(name);
}

static PyObject *py_get_importance(PyObject *mod, PyObject *args)
{
	uint32_t importance;
	const char *name;
	if (!PyArg_ParseTuple(args, "i", &importance))
		return NULL;

	name = get_importance(importance);
	if (name == NULL)
		return Py_None;
	return PyString_FromString(name);
}

static PyObject *py_get_task_status(PyObject *mod, PyObject *args)
{
	uint32_t task_status;
	const char *name;
	if (!PyArg_ParseTuple(args, "i", &task_status))
		return NULL;

	name = get_task_status(task_status);
	if (name == NULL)
		return Py_None;
	return PyString_FromString(name);
}

static PyMethodDef mapi_methods[] = {
	{ "get_proptag_value", (PyCFunction)py_get_proptag_value, METH_VARARGS, NULL },
	{ "get_proptag_name", (PyCFunction)py_get_proptag_name, METH_VARARGS, NULL },
	{ "get_importance", (PyCFunction)py_get_importance, METH_VARARGS, NULL },
	{ "get_task_status", (PyCFunction)py_get_task_status, METH_VARARGS, NULL },
	{ NULL }
};

void initmapi(void)
{
	PyObject *m;

	if (PyType_Ready(&PyMapiSessionType) < 0)
		return;

	if (PyType_Ready(&PyMapiObjectType) < 0)
		return;

	if (PyType_Ready(&PyMapiMsgStoreType) < 0)
		return;

	m = Py_InitModule3("mapi", mapi_methods, "MAPI/RPC Python bindings");
	if (m == NULL)
		return;

	Py_INCREF((PyObject *)&PyMapiSessionType);
	PyModule_AddObject(m, "Session", (PyObject *)&PyMapiSessionType);

	Py_INCREF((PyObject *)&PyMapiObjectType);
	PyModule_AddObject(m, "Object", (PyObject *)&PyMapiObjectType);

	Py_INCREF((PyObject *)&PyMapiMsgStoreType);
	PyModule_AddObject(m, "MessageStore", (PyObject *)&PyMapiMsgStoreType);
}
