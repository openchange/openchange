/*
   OpenChange MAPI implementation.

   Python interface to mapistore

   Copyright (C) Julien Kerihuel 2010-2011.

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

#ifndef	__PYMAPISTORE_H_
#define	__PYMAPISTORE_H_

#include <Python.h>
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
} PyMAPIStoreObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistoredb_context	*mdb_ctx;
} PyMAPIStoreDBObject;

PyAPI_DATA(PyTypeObject)	PyMAPIStore;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreDB;

#endif	/* ! __PYMAPISTORE_H_ */
