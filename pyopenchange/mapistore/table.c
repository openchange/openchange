/*
   OpenChange MAPI implementation.

   Python interface to mapistore table

   Copyright (C) Julien Kerihuel 2011.

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
#include "pyopenchange/mapistore/pymapistore.h"
#include "gen_ndr/exchange.h"

static void py_MAPIStoreTable_dealloc(PyObject *_self)
{
	PyMAPIStoreTableObject *self = (PyMAPIStoreTableObject *)_self;

	Py_XDECREF(self->context);
	talloc_free(self->columns);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreTable_set_columns(PyMAPIStoreTableObject *self, PyObject *args, PyObject *kwargs)
{
	char				*kwnames[] = { "tag_list", NULL };
	PyObject			*tag_list = NULL, *py_tag;
	struct SPropTagArray		*properties;
	uint32_t			count, tag, i;
	enum mapistore_error		retval;
	enum MAPISTATUS			ret;

	/* Read list of property names/tags */
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwnames, &tag_list)) {
		return NULL;
	}

	/* Check 'tag_list' type */
	if (PyList_Check(tag_list) == false) {
		PyErr_SetString(PyExc_TypeError, "Input argument must be a list");
		return NULL;
	}

	/* Build the SPropTagArray structure */
	properties = talloc_zero(self->mem_ctx, struct SPropTagArray);
	if (properties == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	properties->aulPropTag = talloc_zero(properties, void);
	if (properties == NULL) {
		PyErr_NoMemory();
		goto end;
	}

	count = PyList_Size(tag_list);

	for (i = 0; i < count; i++) {
		py_tag = PyList_GetItem(tag_list, i);
		if (PyString_Check(py_tag)) {
			tag = openchangedb_property_get_tag(PyString_AsString(py_tag));
			if (tag == 0xFFFFFFFF) {
				DEBUG(0, ("[WARN][%s]: Unsupported property tag '%s' \n",
						__location__, PyString_AsString(py_tag)));
				PyErr_SetMAPIStoreError(MAPISTORE_ERR_INVALID_DATA);
				goto end;
			}
		} else if (PyInt_Check(py_tag)) {
			tag = PyInt_AsUnsignedLongMask(py_tag);
		} else {
			PyErr_SetString(PyExc_TypeError,
					"Invalid type in list: only strings and integers accepted");
			goto end;
		}

		ret = SPropTagArray_add(self->mem_ctx, properties, tag);
		if (ret != MAPI_E_SUCCESS) {
			PyErr_SetMAPISTATUSError(ret);
			goto end;
		}
	}

	/* Set the columns */
	retval = mapistore_table_set_columns(self->context->mstore_ctx, self->context->context_id,
			self->table_object, properties->cValues, properties->aulPropTag);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Update self-> columns */
	talloc_free(self->columns);
	self->columns = properties;

	Py_RETURN_NONE;
end:
	talloc_free(properties);
	return NULL;
}

static PyObject *py_MAPIStoreTable_get_row(PyMAPIStoreTableObject *self, PyObject *args, PyObject *kwargs)
{
	TALLOC_CTX			*mem_ctx;
	char				*kwnames[] = { "line", NULL };
	PyObject			*py_ret;
	struct mapistore_property_data	*row_data;
	enum mapistore_error		retval;
	uint32_t			line, row_count;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I", kwnames, &line)) {
		return NULL;
	}

	/* Check the 'line' range */
	retval = mapistore_table_get_row_count(self->context->mstore_ctx, self->context->context_id,
			self->table_object, MAPISTORE_PREFILTERED_QUERY, &row_count);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	if ((line < 0) || (line >= row_count)) {
		PyErr_SetString(PyExc_ValueError, "'line' argument out of range");
		return NULL;
	}

	/* Retrieve the row */
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	retval = mapistore_table_get_row(self->context->mstore_ctx, self->context->context_id,
			self->table_object, mem_ctx, MAPISTORE_PREFILTERED_QUERY, line, &row_data);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		goto end;
	}

	/* Build the dictionary */
	py_ret = pymapistore_python_dict_from_properties(self->columns->aulPropTag, row_data,
			self->columns->cValues);
	if (py_ret == NULL) {
		PyErr_SetString(PyExc_SystemError, "Error building the dictionary");
		goto end;
	}

	talloc_free(mem_ctx);
	return py_ret;

end:
	talloc_free(mem_ctx);
	return NULL;
}

static PyMethodDef mapistore_table_methods[] = {
	{ "set_columns", (PyCFunction)py_MAPIStoreTable_set_columns, METH_VARARGS | METH_KEYWORDS },
	{ "get_row", (PyCFunction)py_MAPIStoreTable_get_row, METH_VARARGS | METH_KEYWORDS },
	{ NULL },
};

static PyObject *py_MAPIStoreTable_get_count(PyMAPIStoreTableObject *self, void *closure)
{
	uint32_t			row_count = 0;
	enum mapistore_error		retval;

	retval = mapistore_table_get_row_count(self->context->mstore_ctx, self->context->context_id,
			self->table_object, MAPISTORE_PREFILTERED_QUERY, &row_count);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}

	return PyLong_FromLong(row_count);
}

static PyObject *py_MAPIStoreTable_get_rows(PyMAPIStoreTableObject *self, void* closure)
{
	PyMAPIStoreRowsObject		*row;
	uint32_t			row_count;
	enum mapistore_error		retval;

	/* Get the row count */
	retval = mapistore_table_get_row_count(self->context->mstore_ctx, self->context->context_id,
			self->table_object, MAPISTORE_PREFILTERED_QUERY, &row_count);
	if (retval != MAPISTORE_SUCCESS) {
		PyErr_SetMAPIStoreError(retval);
		return NULL;
	}
	if (row_count < 1) {
		PyErr_SetMAPIStoreError(MAPISTORE_ERR_NOT_FOUND);
		return NULL;
	}

	/* Return the PyMAPIStoreFolders object*/
	row = PyObject_New(PyMAPIStoreRowsObject, &PyMAPIStoreRows);
	if (row == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	row->mem_ctx = self->mem_ctx;
	row->table = self;
	Py_INCREF(row->table);

	row->count = row_count;
	row->curr_index = 0;
	return (PyObject *) row;
}

static PyObject *py_MAPIStoreTable_get_columns(PyMAPIStoreTableObject *self, void* closure)
{
	PyObject		*py_columns, *py_key;
	Py_ssize_t		count, i;
	const char		*skey;
	unsigned		key;
	int			ret;

	count = (Py_ssize_t)self->columns->cValues;

	py_columns = PyList_New(count);
	if (py_columns == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	/* Build the columnn list */
	for (i = 0; i < count; i++) {
		key = self->columns->aulPropTag[i];

		skey = get_proptag_name(key);
		if (skey == NULL) {
			py_key = PyString_FromFormat("0x%x", key);
		} else {
			py_key = PyString_FromString(skey);
		}

		ret = PyList_SetItem(py_columns, i , py_key);
		if (ret != 0) {
			PyErr_SetString(PyExc_SystemError, "Unable to set element in column list");
			Py_DecRef(py_columns);
			return NULL;
		}
	}

	return py_columns;
}

static PyGetSetDef mapistore_table_getsetters[] = {
	{ (char *)"columns", (getter)py_MAPIStoreTable_get_columns, NULL, NULL },
	{ (char *)"rows", (getter)py_MAPIStoreTable_get_rows, NULL, NULL },
	{ (char *)"count", (getter)py_MAPIStoreTable_get_count, NULL, NULL },
	{ NULL }
};

PyTypeObject PyMAPIStoreTable = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "MAPIStoreTable",
	.tp_basicsize = sizeof (PyMAPIStoreTableObject),
	.tp_methods = mapistore_table_methods,
	.tp_getset = mapistore_table_getsetters,
	.tp_doc = "mapistore table object",
	.tp_dealloc = (destructor)py_MAPIStoreTable_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

static void py_MAPIStoreRows_dealloc(PyObject *_self)
{
	PyMAPIStoreRowsObject *self = (PyMAPIStoreRowsObject *)_self;

	Py_XDECREF(self->table);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreRows_iter(PyObject *self)
{
	Py_INCREF(self);
	return self;
}

static PyObject *py_MAPIStoreRows_next(PyObject *_self)
{
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreRowsObject		*self;
	PyObject			*py_ret;
	struct mapistore_property_data	*row_data;
	enum mapistore_error		retval;

	self = (PyMAPIStoreRowsObject *)_self;
	if (!self) {
		PyErr_SetString(PyExc_TypeError, "Expected object of type 'MAPIStoreRows'");
		return NULL;
	}

	/* Check if there are remaining rows */
	if(self->curr_index < self->count) {
		/* Retrieve the row */
		mem_ctx = talloc_new(NULL);
		if (mem_ctx == NULL) {
			PyErr_NoMemory();
			return NULL;
		}

		retval = mapistore_table_get_row(self->table->context->mstore_ctx,
				self->table->context->context_id, self->table->table_object,
				mem_ctx, MAPISTORE_PREFILTERED_QUERY, self->curr_index, &row_data);
		if (retval != MAPISTORE_SUCCESS) {
			PyErr_SetMAPIStoreError(retval);
			goto end;
		}

		/* Build the dictionary */
		py_ret = pymapistore_python_dict_from_properties(self->table->columns->aulPropTag, row_data,
				self->table->columns->cValues);
		if (py_ret == NULL) {
			PyErr_SetString(PyExc_SystemError, "Error building the dictionary");
			goto end;
		}

		/* Increment the index */
		self->curr_index += 1;

		talloc_free(mem_ctx);
		return (PyObject *)py_ret;
	} else {
	    PyErr_SetNone(PyExc_StopIteration);
	    return NULL;
	}
end:
	talloc_free(mem_ctx);
	return NULL;
}

PyTypeObject PyMAPIStoreRows = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapirow",
	.tp_basicsize = sizeof (PyMAPIStoreRowsObject),
	.tp_iter = py_MAPIStoreRows_iter,
	.tp_iternext = py_MAPIStoreRows_next,
	.tp_doc = "iterator over folder MAPIStore Table rows",
	.tp_dealloc = (destructor)py_MAPIStoreRows_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,
};

void initmapistore_table(PyObject *m)
{
	if (PyType_Ready(&PyMAPIStoreTable) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreTable);

	if (PyType_Ready(&PyMAPIStoreRows) < 0) {
		return;
	}
	Py_INCREF(&PyMAPIStoreRows);
}
