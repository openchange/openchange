/*
   OpenChange MAPI implementation.

   Python interface to openchange

   Copyright (C) Julien Kerihuel 2010.

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

#include <Python.h>
#include "libmapi/libmapi.h"
#include "pyopenchange/pymapi.h"

void initmapi(void);

static PyObject *py_SPropValue_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX		*mem_ctx;
	PySPropValueObject	*SPropValue;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	SPropValue = PyObject_New(PySPropValueObject, &PySPropValue);
	SPropValue->mem_ctx = mem_ctx;
	SPropValue->SPropValue = talloc_array(mem_ctx, struct SPropValue, 2);
	SPropValue->cValues = 0;

	return (PyObject *) SPropValue;
}

static void py_SPropValue_dealloc(PyObject *_self)
{
	PySPropValueObject	*self = (PySPropValueObject *)_self;

	talloc_free(self->mem_ctx);
	PyObject_Del(_self);
}

static PyObject *py_SPropValue_add(PySPropValueObject *self, PyObject *args)
{
	uint32_t	proptag;
	PyObject	*data;
	int		i;
	NTTIME		nt;

	if (!PyArg_ParseTuple(args, "lO", &proptag, &data)) {
		return NULL;
	}

	/* Ensure this tag has not already been added to the list */
	for (i = 0; i < self->cValues; i++) {
		if (self->SPropValue[i].ulPropTag == proptag) {
			return NULL;
		}
	}

	self->SPropValue = talloc_realloc(self->mem_ctx, self->SPropValue, struct SPropValue, self->cValues + 2);

	switch (proptag & 0xFFFF) {
	case PT_I2:
		if (!PyInt_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires long");
			return NULL;
		}
		self->SPropValue[self->cValues].value.i = (uint16_t) PyInt_AsLong(data);
		break;
	case PT_LONG:
		if (!PyInt_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires long");
			return NULL;
		}
		self->SPropValue[self->cValues].value.l = PyInt_AsLong(data);
		break;
	case PT_DOUBLE:
		if (!PyFloat_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires double");
			return NULL;
		}
		self->SPropValue[self->cValues].value.dbl = PyFloat_AsDouble(data);
		break;
	case PT_BOOLEAN:
		if (!PyBool_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires boolean");
			return NULL;
		}
		self->SPropValue[self->cValues].value.b = PyInt_AsLong(data);
		break;
	case PT_I8:
		if (!PyLong_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires long long int");
			return NULL;
		}
		self->SPropValue[self->cValues].value.d = PyLong_AsLongLong(data);
		break;
	case PT_STRING8:
		if (!PyString_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires string");
			return NULL;
		}
		self->SPropValue[self->cValues].value.lpszA = (uint8_t *)talloc_strdup(self->mem_ctx, PyString_AsString(data));
		break;
	case PT_UNICODE:
		if (!PyString_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires string");
			return NULL;
		}
		self->SPropValue[self->cValues].value.lpszW = talloc_strdup(self->mem_ctx, PyString_AsString(data));
		break;
	case PT_SYSTIME:
		if (!PyFloat_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires float");
			return NULL;
		}
		unix_to_nt_time(&nt, PyFloat_AsDouble(data));
		self->SPropValue[self->cValues].value.ft.dwLowDateTime = (nt << 32) >> 32;
		self->SPropValue[self->cValues].value.ft.dwHighDateTime = nt >> 32;
   		break;
	case PT_ERROR:
		if (!PyInt_Check(data)) {
			PyErr_SetString(PyExc_TypeError, "Property Tag requires long");
			return NULL;
		}
		self->SPropValue[self->cValues].value.err = PyInt_AsLong(data);
		break;
	default:
		printf("Missing support for 0x%.4x type\n", (proptag & 0xFFFF));
		Py_RETURN_NONE;
	}

	self->SPropValue[self->cValues].ulPropTag = proptag;
	self->cValues += 1;
	Py_RETURN_NONE;
}

static PyObject *py_SPropValue_dump(PySPropValueObject *self, PyObject *args)
{
	int	i;
	char	*sep;

	if (!PyArg_ParseTuple(args, "s", &sep)) {
		return NULL;
	}

	for (i = 0; i < self->cValues; i++) {
		mapidump_SPropValue(self->SPropValue[i], sep);
	}
	
	Py_RETURN_NONE;
}

static PyMethodDef mapi_SPropValue_methods[] = {
	{ "add", (PyCFunction)py_SPropValue_add, METH_VARARGS },
	{ "dump", (PyCFunction)py_SPropValue_dump, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapi_SPropValue_getsetters[] = {
	{ NULL }
};

PyTypeObject PySPropValue = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "SPropValue",
	.tp_basicsize = sizeof (PySPropValueObject),
	.tp_methods = mapi_SPropValue_methods,
	.tp_getset = mapi_SPropValue_getsetters,
	.tp_doc = "SPropValue MAPI structure",
	.tp_new = py_SPropValue_new,
	.tp_dealloc = (destructor)py_SPropValue_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static PyMethodDef py_mapi_global_methods[] = {
	{ NULL }
};

void initmapi(void)
{
	PyObject	*m;

	if (PyType_Ready(&PySPropValue) < 0) {
		return;
	}

	m = Py_InitModule3("mapi", py_mapi_global_methods,
			   "An interface to OpenChange MAPI");
	if (m == NULL) {
		return;
	}

	/* Add all properties - generated by mparse.pl */
	pymapi_add_properties(m);

	Py_INCREF(&PySPropValue);
	
	PyModule_AddObject(m, "SPropValue", (PyObject *)&PySPropValue);
} 
