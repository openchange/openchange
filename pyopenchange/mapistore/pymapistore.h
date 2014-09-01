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
#include "mapiproxy/libmapiproxy/backends/openchangedb_ldb.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"
#include <tevent.h>

typedef struct {
	PyObject			*datetime_module;
	PyObject			*datetime_datetime_class;
	struct ldb_context		*samdb_ctx;
	struct openchangedb_context 	*ocdb_ctx;
} PyMAPIStoreGlobals;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	struct mapistore_context	*mstore_ctx;
	char				*username;
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
	uint64_t			fid;
	void				*folder_object;
	uint32_t			context_id;
	PyMAPIStoreObject		*parent;
} PyMAPIStoreContextObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreContextObject	*context;
	void				*folder_object;
	uint64_t			fid;
} PyMAPIStoreFolderObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreFolderObject		*folder;
	size_t				curr_index;
	size_t				count;
	uint64_t			*fids;
} PyMAPIStoreFoldersObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreContextObject	*context;
	void				*message_object;
	uint64_t			mid;
} PyMAPIStoreMessageObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	PyMAPIStoreFolderObject		*folder;
	size_t				curr_index;
	size_t				count;
	uint64_t			*mids;
} PyMAPIStoreMessagesObject;

typedef struct {
	PyObject_HEAD	

	PyObject *timestamp;

	PyObject *publish_start;
	PyObject *publish_end;

	PyObject *free;
	PyObject *tentative;
	PyObject *busy;
	PyObject *away;

	PyObject *merged;
} PyMAPIStoreFreeBusyPropertiesObject;

typedef struct {
	PyObject_HEAD	
} PyMAPIStoreTableObject;

typedef struct {
	PyObject_HEAD
	TALLOC_CTX			*mem_ctx;
	const char			*username;
	struct loadparm_context		*lp_ctx;
	struct indexing_context		*ictx;
} PyMAPIStoreIndexingObject;

PyAPI_DATA(PyTypeObject)	PyMAPIStore;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreMGMT;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreContext;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreFolder;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreFolders;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreMessage;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreMessages;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreTable;
PyAPI_DATA(PyTypeObject)	PyMAPIStoreIndexing;

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
void PyErr_SetMAPISTATUSError(enum MAPISTATUS retval);

/* internal calls */
PyMAPIStoreGlobals *get_PyMAPIStoreGlobals(void);

void initmapistore_context(PyObject *);
void initmapistore_folder(PyObject *);
void initmapistore_message(PyObject *);
void initmapistore_mgmt(PyObject *);
void initmapistore_freebusy_properties(PyObject *);
void initmapistore_table(PyObject *);
void initmapistore_errors(PyObject *);
void initmapistore_indexing(PyObject *);

PyMAPIStoreFreeBusyPropertiesObject* instantiate_freebusy_properties(struct mapistore_freebusy_properties *);

__END_DECLS

#endif	/* ! __PYMAPISTORE_H_ */
