/*
   OpenChange MAPI implementation.

   Python interface to mapistore management

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

void initmapistore_mgmt(void);

static void py_MAPIStoreMGMT_dealloc(PyObject *_self)
{
	PyMAPIStoreMGMTObject *self = (PyMAPIStoreMGMTObject *)_self;

	printf("deallocate MGMT object\n");
	mapistore_mgmt_release(self->mgmt_ctx);

	Py_DECREF(self->parent);
	PyObject_Del(_self);
}

static PyObject *py_MAPIStoreMGMT_registered_backend(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	int		ret;
	const char	*bname;

	if (!PyArg_ParseTuple(args, "s", &bname)) {
		return NULL;
	}

	ret = mapistore_mgmt_registered_backend(self->mgmt_ctx, bname);
	return PyBool_FromLong(ret == MAPISTORE_SUCCESS ? true : false);

}

static PyObject *py_MAPIStoreMGMT_registered_users(PyMAPIStoreMGMTObject *self, PyObject *args)
{
	PyObject				*dict;
	PyObject				*userlist;
	const char				*backend;
	const char				*vuser;
	struct mapistore_mgmt_users_list	*ulist;
	int					i;

	if (!PyArg_ParseTuple(args, "ss", &backend, &vuser)) {
		return NULL;
	}

	dict = PyDict_New();
	PyDict_SetItemString(dict, "backend", PyString_FromString(backend));
	PyDict_SetItemString(dict, "user", PyString_FromString(vuser));
	
	ulist = mapistore_mgmt_registered_users(self->mgmt_ctx, backend, vuser);
	userlist = PyList_New(0);

	if (ulist && ulist->count != 0) {
		PyDict_SetItem(dict, PyString_FromString("count"), PyLong_FromLong(ulist->count));
		for (i = 0; i < ulist->count; i++) {
			PyList_Append(userlist, PyString_FromString(ulist->user[i]));
		}
	} else {
		PyDict_SetItem(dict, PyString_FromString("count"), PyLong_FromLong(0));
	}
	PyDict_SetItem(dict, PyString_FromString("usernames"), userlist);

	if (ulist) {
		talloc_free(ulist);
	}

	return (PyObject *)dict;
}

static PyMethodDef mapistore_mgmt_methods[] = {
	{ "registered_backend", (PyCFunction)py_MAPIStoreMGMT_registered_backend, METH_VARARGS },
	{ "registered_users", (PyCFunction)py_MAPIStoreMGMT_registered_users, METH_VARARGS },
	{ NULL },
};

static PyGetSetDef mapistore_mgmt_getsetters[] = {
	{ NULL }
};

PyTypeObject PyMAPIStoreMGMT = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "mapistore_mgmt",
	.tp_basicsize = sizeof (PyMAPIStoreMGMTObject),
	.tp_methods = mapistore_mgmt_methods,
	.tp_getset = mapistore_mgmt_getsetters,
	.tp_doc = "mapistore management object",
	.tp_dealloc = (destructor)py_MAPIStoreMGMT_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};
