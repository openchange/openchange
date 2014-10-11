/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Python interface to ocpf

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
#include "pyopenchange/pyocpf.h"
#include "pyopenchange/pymapi.h"
#include "libocpf/ocpf.h"

static PyTypeObject *SPropValue_Type;

void initocpf(void);

PyAPI_DATA(PyTypeObject) PyOcpf;

static PyObject *py_new_context(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX	*mem_ctx;
	PyOcpfObject	*ocpf_object;
	char		*kwnames[] = { "filename", "flags", NULL };
	int		ret;
	const char	*filename;
	uint32_t	context_id;
	uint8_t		flags;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sH", kwnames,
					 &filename, &flags)) {
		return NULL;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	printf("filename = %s, flags = 0x%x\n", filename, flags);
	ret = ocpf_new_context(filename, &context_id, flags);
	if (ret == OCPF_ERROR) {
		return NULL;
	}

	printf("context_id = %d\n", context_id);
	ocpf_object = PyObject_New(PyOcpfObject, &PyOcpf);
	ocpf_object->context_id = context_id;
	ocpf_object->mem_ctx = mem_ctx;

	return (PyObject *) ocpf_object;
}

static PyObject *py_ocpf_parse(PyOcpfObject *self)
{
	int	ret;

	ret = ocpf_parse(self->context_id);
	return PyInt_FromLong(ret);
}

static PyObject *py_ocpf_set_SPropValue_array(PyOcpfObject *self)
{
	int	ret;

	ret = ocpf_server_set_SPropValue(self->mem_ctx, self->context_id);
	return PyInt_FromLong(ret);
}

static PyObject *py_ocpf_get_SPropValue(PyOcpfObject *self)
{
	uint32_t		cValues = 0;
	PyObject		*mod_mapi;
	PySPropValueObject	*pySPropValue;
	struct SPropValue	*SPropValue;

	mod_mapi = PyImport_ImportModule("openchange.mapi");
	if (mod_mapi == NULL) {
		printf("Can't load module\n");
		return NULL;
	}
	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue");
	if (SPropValue_Type == NULL)
		return NULL;

	SPropValue = ocpf_get_SPropValue(self->context_id, &cValues);

	pySPropValue = (PySPropValueObject *)SPropValue_Type->tp_new(NULL, NULL, NULL);
	talloc_free(pySPropValue->SPropValue);
	pySPropValue->SPropValue = SPropValue;
	talloc_steal(pySPropValue->mem_ctx, pySPropValue->SPropValue);
	pySPropValue->cValues = cValues;

	return (PyObject *) pySPropValue;
}


static PyObject *py_ocpf_add_SPropValue(PyOcpfObject *self, PyObject *args)
{
	PyObject		*mod_mapi;
	PyObject		*pySPropValue;
	PySPropValueObject	*SPropValue;
	int			ret;
	int			i;

	mod_mapi = PyImport_ImportModule("openchange.mapi");
	if (mod_mapi == NULL) {
		printf("Can't load module\n");
		return NULL;
	}

	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue");
	if (SPropValue_Type == NULL) {
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "O", &pySPropValue)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(pySPropValue, SPropValue_Type)) {
		PyErr_SetString(PyExc_TypeError, "Function requires SPropValue obnject");
		return NULL;
	}

	SPropValue = (PySPropValueObject *) pySPropValue;

	for (i = 0; i < SPropValue->cValues; i++) {
		ret = ocpf_server_add_SPropValue(self->context_id, &SPropValue->SPropValue[i]);
		if (ret) {
			return PyInt_FromLong(ret);
		}
	}

	return PyInt_FromLong(ret);
}


static PyObject *py_ocpf_dump(PyOcpfObject *self)
{
	ocpf_dump(self->context_id);
	Py_RETURN_NONE;
}

static PyObject *py_ocpf_write_init(PyOcpfObject *self, PyObject *args)
{
	int		ret;
	int64_t		folder_id;

	if (!PyArg_ParseTuple(args, "L", &folder_id)) {
		return NULL;
	}

	ret = ocpf_write_init(self->context_id, folder_id);
	return PyInt_FromLong(ret);
}

static PyObject *py_ocpf_write(PyOcpfObject *self, PyObject *args)
{
	int				ret;
	PyObject			*pySPropValue;
	PySPropValueObject		*SPropValue;
	struct mapi_SPropValue_array	mapi_lpProps;
	PyObject			*mod_mapi;
	int				i;

	mod_mapi = PyImport_ImportModule("openchange.mapi");
	if (mod_mapi == NULL) {
		printf("Can't load module\n");
		return NULL;
	}
	SPropValue_Type = (PyTypeObject *)PyObject_GetAttrString(mod_mapi, "SPropValue");
	if (SPropValue_Type == NULL)
		return NULL;


	if (!PyArg_ParseTuple(args, "O", &pySPropValue)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(pySPropValue, SPropValue_Type)) {
		PyErr_SetString(PyExc_TypeError, "Function require SPropValue object");
		return NULL;
	}
	SPropValue = (PySPropValueObject *) pySPropValue;

	mapi_lpProps.cValues = SPropValue->cValues;
	mapi_lpProps.lpProps = talloc_array(SPropValue->mem_ctx, struct mapi_SPropValue, SPropValue->cValues);
	for (i = 0; i < SPropValue->cValues; i++) {
	  cast_mapi_SPropValue((TALLOC_CTX *)mapi_lpProps.lpProps,
			       &(mapi_lpProps.lpProps[i]), &(SPropValue->SPropValue[i]));
	}

	ret = ocpf_write_auto(self->context_id, NULL, &mapi_lpProps);
	talloc_free(mapi_lpProps.lpProps);
	return PyInt_FromLong(ret);
}

static PyObject *py_ocpf_write_commit(PyOcpfObject *self)
{
	int		ret;

	ret = ocpf_write_commit(self->context_id);
	return PyInt_FromLong(ret);
}

static PyObject *py_ocpf_set_type(PyOcpfObject *self, PyObject *args)
{
	const char	*type;

	if (!PyArg_ParseTuple(args, "s", &type)) {
		return NULL;
	}

	return PyInt_FromLong(ocpf_server_set_type(self->context_id, type));
}

static PyMethodDef py_ocpf_global_methods[] = {
	{ NULL }
};

static PyMethodDef ocpf_methods[] = {
	{ "parse", (PyCFunction) py_ocpf_parse, METH_NOARGS },
	{ "dump", (PyCFunction) py_ocpf_dump, METH_NOARGS },
	{ "write_init", (PyCFunction) py_ocpf_write_init, METH_VARARGS },
	{ "write", (PyCFunction) py_ocpf_write, METH_VARARGS },
	{ "write_commit", (PyCFunction) py_ocpf_write_commit, METH_NOARGS },
	{ "set_SPropValue_array", (PyCFunction) py_ocpf_set_SPropValue_array, METH_NOARGS },
	{ "get_SPropValue", (PyCFunction) py_ocpf_get_SPropValue, METH_NOARGS },
	{ "add_SPropValue", (PyCFunction) py_ocpf_add_SPropValue, METH_VARARGS },
	{ "set_type", (PyCFunction) py_ocpf_set_type, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef ocpf_getsetters[] = {
	{ NULL }
};

static void py_ocpf_dealloc(PyObject *_self)
{
	PyOcpfObject *self = (PyOcpfObject *)_self;

	talloc_free(self->mem_ctx);

	printf("ocpf_del_context\n");
	ocpf_del_context(self->context_id);
	PyObject_Del(_self);
}

PyTypeObject PyOcpf = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "OCPF",
	.tp_basicsize = sizeof(PyOcpfObject),
	.tp_methods = ocpf_methods,
	.tp_getset = ocpf_getsetters,
	.tp_doc = "OCPF context",
	.tp_new = py_new_context,
	.tp_dealloc = (destructor)py_ocpf_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

void initocpf(void)
{
	PyObject	*m;

	if (PyType_Ready(&PyOcpf) < 0) {
		return;
	}

	m = Py_InitModule3("ocpf", py_ocpf_global_methods, 
			   "An interface to OCPF, an OpenChange file format used to represent MAPI messages");
	if (m == NULL) {
		return;
	}

	PyModule_AddObject(m, "OCPF_FLAGS_RDWR", PyInt_FromLong(OCPF_FLAGS_RDWR));
	PyModule_AddObject(m, "OCPF_FLAGS_READ", PyInt_FromLong(OCPF_FLAGS_READ));
	PyModule_AddObject(m, "OCPF_FLAGS_WRITE", PyInt_FromLong(OCPF_FLAGS_WRITE));
	PyModule_AddObject(m, "OCPF_FLAGS_CREATE", PyInt_FromLong(OCPF_FLAGS_CREATE));

	Py_INCREF(&PyOcpf);

	PyModule_AddObject(m, "Ocpf", (PyObject *)&PyOcpf);

	/* Initialize OCPF subsystem */
	ocpf_init();
}
