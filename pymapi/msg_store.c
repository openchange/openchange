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

static PyMethodDef msg_store_methods[] = {
	{ NULL },
};

static PyGetSetDef msg_store_getsetters[] = {
	{ NULL }
};

static PyObject *msg_store_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	PyErr_SetString(PyExc_NotImplementedError, "Message Stores can only be obtained from a MAPI Session");
	return NULL;
}

PyTypeObject PyMapiMsgStoreType = {
	PyObject_HEAD_INIT(NULL) 0,
	.tp_name = "MessageStore",
	.tp_basicsize = sizeof(PyMapiObjectObject),
	.tp_methods = msg_store_methods,
	.tp_getset = msg_store_getsetters,
	.tp_doc = "MAPI Message Store",
	.tp_new = msg_store_new,
	.tp_base = &PyMapiObjectType,
	.tp_flags = Py_TPFLAGS_DEFAULT,
};

