/*
   OpenChange MAPI implementation.

   Python interface to mapistore

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

#ifndef	__PYMAPISTORE_H_
#define	__PYMAPISTORE_H_

#include <Python.h>
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mgmt/mapistore_mgmt.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include <tevent.h>

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
	struct ldb_context		*samdb_ctx;
	struct ldb_context		*ocdb_ctx;
} PyMAPIStoreObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistore_mgmt_context	*mgmt_ctx;
	PyMAPIStoreObject		*parent;
} PyMAPIStoreMGMTObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct mapistore_context	*mstore_ctx;
	struct ldb_context		*ocdb_ctx;
	uint64_t			fid;
	void				*folder_object;
	uint32_t			context_id;
	PyMAPIStoreObject		*parent;
} PyMAPIStoreContextObject;

typedef struct {
	PyObject_HEAD
	PyMAPIStoreContextObject	*context;
	void				*folder_object;
	uint64_t			fid;
} PyMAPIStoreFolderObject;

typedef struct {
	PyObject_HEAD	
} PyMAPIStoreTableObject;

PyAPI_DATA(PyTypeObject)	PyMAPIStore;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreMGMT;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreContext;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreFolder;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreTable;

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS
void PyErr_SetMAPIStoreError(uint32_t);
__END_DECLS

#define PyErr_MAPIStore_IS_ERR_RAISE(retval)		\
	if (retval != MAPISTORE_SUCCESS) {		\
		PyErr_SetMAPIStoreError(retval);	\
		return NULL;				\
        }

#endif	/* ! __PYMAPISTORE_H_ */
