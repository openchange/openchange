/*
   OpenChange Server implementation

   MAPI handles API implementation

   Copyright (C) Julien Kerihuel 2009

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

/**
   \file mapi_handles.c

   \brief API for MAPI handles management
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "libmapiproxy.h"

/**
   \details Initialize MAPI handles context

   \param mem_ctx pointer to the memory context

   \return Allocated MAPI handles context on success, otherwise NULL
 */
_PUBLIC_ struct mapi_handles_context *mapi_handles_init(TALLOC_CTX *mem_ctx)
{
	struct mapi_handles_context	*handles_ctx;

	/* Step 1. Initialize the context */
	handles_ctx = talloc_zero(mem_ctx, struct mapi_handles_context);
	if (!handles_ctx) return NULL;

	/* Step 2. Initialize the TDB context */
	handles_ctx->tdb_ctx = tdb_open(NULL, 0, TDB_INTERNAL, O_RDWR|O_CREAT, 0600);

	/* Step 3. Initialize the handles list */
	handles_ctx->handles = NULL;

	/* Step 4. Set last_handle to the first valid value */
	handles_ctx->last_handle = 1;

	return handles_ctx;
}


/**
   \details Release MAPI handles context

   \param handles_ctx pointer to the MAPI handles context

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_handles_release(struct mapi_handles_context *handles_ctx)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handles_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	tdb_close(handles_ctx->tdb_ctx);
	talloc_free(handles_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Search for a record in the TDB database

   \param handles_ctx pointer to the MAPI handles context
   \param handle MAPI handle to lookup
   \param rec pointer to the MAPI handle structure the function
   returns
   
   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_handles_search(struct mapi_handles_context *handles_ctx,
					     uint32_t handle, struct mapi_handles **rec)
{
	TALLOC_CTX		*mem_ctx;
	TDB_DATA		key;
	TDB_DATA		dbuf;
	struct mapi_handles	*el;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handles_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!handles_ctx->tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(handle == MAPI_HANDLES_RESERVED, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!rec, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapi_handles_search");

	/* Step 1. Search for the handle within TDB database */
	key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", handle);
	key.dsize = strlen((const char *)key.dptr);

	dbuf = tdb_fetch(handles_ctx->tdb_ctx, key);
	talloc_free(key.dptr);
	OPENCHANGE_RETVAL_IF(!dbuf.dptr, MAPI_E_NOT_FOUND, mem_ctx);
	OPENCHANGE_RETVAL_IF(!dbuf.dsize, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);
	/* Ensure this is not a free'd record */
	if (dbuf.dsize == (sizeof(MAPI_HANDLES_NULL) - 1) && !strncmp((char *)dbuf.dptr, MAPI_HANDLES_NULL, dbuf.dsize)) {
		free(dbuf.dptr);
		return MAPI_E_NOT_FOUND;
	}
	free(dbuf.dptr);

	/* Step 2. Return the record within the double chained list */
	for (el = handles_ctx->handles; el; el = el->next) {
		if (el->handle == handle) {
			*rec = el;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_CORRUPT_STORE;
}


/**
   \details Set a TDB record data as null meaning it can be reused in
   the future.

   \param handles_ctx pointer to the MAPI handles context
   \param handle handle key value to free

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS mapi_handles_tdb_free(struct mapi_handles_context *handles_ctx,
					     uint32_t handle)
{
	TALLOC_CTX		*mem_ctx;
	TDB_DATA		key;
	TDB_DATA		dbuf;
	int			ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handles_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!handles_ctx->tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(handle == MAPI_HANDLES_RESERVED, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapi_handles_tdb_free");
	
	key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", handle);
	key.dsize = strlen((const char *)key.dptr);

	/* Step 1. Makes sure the record exists */
	ret = tdb_exists(handles_ctx->tdb_ctx, key);
	OPENCHANGE_RETVAL_IF(!ret, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Update existing record */
	dbuf.dptr = (unsigned char *)MAPI_HANDLES_NULL;
	dbuf.dsize = sizeof(MAPI_HANDLES_NULL) - 1;

	ret = tdb_store(handles_ctx->tdb_ctx, key, dbuf, TDB_MODIFY);
	talloc_free(mem_ctx);
	if (ret == -1) {
		OC_DEBUG(3, "Unable to create 0x%x record: %s\n",
			  handle, tdb_errorstr(handles_ctx->tdb_ctx));
		return MAPI_E_CORRUPT_STORE;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Update a TDB record

   
 */
static enum MAPISTATUS mapi_handles_tdb_update(struct mapi_handles_context *handles_ctx,
					       uint32_t handle, uint32_t container_handle)
{
	TALLOC_CTX	*mem_ctx;
	TDB_DATA	key;
	TDB_DATA	dbuf;
	int		ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handles_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!handles_ctx->tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!handle, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapi_handles_tdb_update");

	key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", handle);
	key.dsize = strlen((const char *)key.dptr);

	/* Step 1. Makes sure the record exists */
	ret = tdb_exists(handles_ctx->tdb_ctx, key);
	OPENCHANGE_RETVAL_IF(!ret, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Update record */
	dbuf.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", container_handle);
	dbuf.dsize = strlen((const char *)dbuf.dptr);

	ret = tdb_store(handles_ctx->tdb_ctx, key, dbuf, TDB_MODIFY);
	talloc_free(mem_ctx);
	if (ret == -1) {
		OC_DEBUG(3, "Unable to update 0x%x record: %s\n",
			  handle, tdb_errorstr(handles_ctx->tdb_ctx));

		return MAPI_E_CORRUPT_STORE;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Traverse TDB database and search for the first record
   which dbuf value is "null" string.
 
   \param tdb_ctx pointer to the TDB context
   \param key the current TDB key
   \param dbuf the current TDB value
   \param state pointer on private data

   \return 1 when a free record is found, otherwise 0
*/
static int mapi_handles_traverse_null(TDB_CONTEXT *tdb_ctx,
				      TDB_DATA key, TDB_DATA dbuf,
				      void *state)
{
	TALLOC_CTX	*mem_ctx;
	uint32_t	*handle = (uint32_t *) state;
	char		*handle_str = NULL;

	if (dbuf.dptr && (dbuf.dsize == sizeof(MAPI_HANDLES_NULL) - 1) && strncmp((const char *)dbuf.dptr, MAPI_HANDLES_NULL, dbuf.dsize) == 0) {
		mem_ctx = talloc_named(NULL, 0, "mapi_handles_traverse_null");
		handle_str = talloc_strndup(mem_ctx, (char *)key.dptr, key.dsize);
		*handle = strtol((const char *) handle_str, NULL, 16);
		talloc_free(mem_ctx);

		return 1;
	}

	return 0;
}


/**
   \details Add a handles to the database and return a pointer on
   created record

   \param handles_ctx pointer to the MAPI handles context
   \param container_handle the container handle if available
   \param rec pointer on pointer to the MAPI handle structure the function
   returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_handles_add(struct mapi_handles_context *handles_ctx,
					  uint32_t container_handle, struct mapi_handles **rec)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	TDB_DATA		key;
	TDB_DATA		dbuf;
	uint32_t		handle = 0;
	struct mapi_handles	*el;
	int			ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handles_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!handles_ctx->tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(handle == MAPI_HANDLES_RESERVED, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!rec, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapi_handles_add");

	/* Step 1. Seek the TDB database for the first free record */
	ret = tdb_traverse(handles_ctx->tdb_ctx, mapi_handles_traverse_null, (void *)&handle);
	if (ret > -1 && handle > 0) {
		OC_DEBUG(5, "We have found free record 0x%x", handle);
		retval = mapi_handles_tdb_update(handles_ctx, handle, container_handle);
		OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);
		
		el = talloc_zero((TALLOC_CTX *)handles_ctx, struct mapi_handles);
		if (!el) {
			mapi_handles_tdb_free(handles_ctx, handle);
			talloc_free(mem_ctx);
			return MAPI_E_NOT_ENOUGH_RESOURCES;
		}

		el->handle = handle;
		el->parent_handle = container_handle;
		el->private_data = NULL;
		*rec = el;
		DLIST_ADD_END(handles_ctx->handles, el, struct mapi_handles *);

		talloc_free(mem_ctx);

		return MAPI_E_SUCCESS;
	}

	/* Step 2. If no record is available, create a new one */
	key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", handles_ctx->last_handle);
	key.dsize = strlen((const char *)key.dptr);

	if (container_handle) {
		dbuf.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", container_handle);
		dbuf.dsize = strlen((const char *)dbuf.dptr);
	} else {
		dbuf.dptr = (unsigned char *) MAPI_HANDLES_ROOT;
		dbuf.dsize = strlen(MAPI_HANDLES_ROOT);
	}

	ret = tdb_store(handles_ctx->tdb_ctx, key, dbuf, TDB_INSERT);
	if (ret == -1) {
		OC_DEBUG(3, "Unable to create 0x%x record: %s",
			  handles_ctx->last_handle, tdb_errorstr(handles_ctx->tdb_ctx));
		talloc_free(mem_ctx);

		return MAPI_E_CORRUPT_STORE;
	}

	el = talloc_zero((TALLOC_CTX *)handles_ctx, struct mapi_handles);
	if (!el) {
		mapi_handles_tdb_free(handles_ctx, handles_ctx->last_handle);
		talloc_free(mem_ctx);
		return MAPI_E_NOT_ENOUGH_RESOURCES;
	}

	el->handle = handles_ctx->last_handle;
	el->parent_handle = container_handle;
	el->private_data = NULL;
	*rec = el;
	DLIST_ADD_END(handles_ctx->handles, el, struct mapi_handles *);

	OC_DEBUG(5, "handle 0x%.2x is a father of 0x%.2x", container_handle, el->handle);
	handles_ctx->last_handle += 1;
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Get the private data associated to a MAPI handle

   \param handle pointer to the MAPI handle structure
   \param private_data pointer on pointer to the private data the
   function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS mapi_handles_get_private_data(struct mapi_handles *handle, void **private_data)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handle, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!private_data, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handle->private_data, MAPI_E_NOT_FOUND, NULL);

	*private_data = handle->private_data;
	
	return MAPI_E_SUCCESS;
}


/**
   \details Set the private data associated to a MAPI handle

   \param handle pointer to the MAPI handle structure
   \param private_data pointer to the private data to associate to the
   MAPI handle

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_handles_set_private_data(struct mapi_handles *handle, void *private_data)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handle, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(handle->private_data, MAPI_E_UNABLE_TO_COMPLETE, NULL);

	handle->private_data = private_data;

	return MAPI_E_SUCCESS;
}


struct mapi_handles_private {
	struct mapi_handles_context	*handles_ctx;
	uint32_t			container_handle;
};

/**
   \details Traverse TDB database and search for records
   which dbuf value is set to state.
 
   \param tdb_ctx pointer to the TDB context
   \param key the current TDB key (potential child handle)
   \param dbuf the current TDB value (parent handle)
   \param state pointer on private data

   \return 1 when a free record is found, otherwise 0
*/
static int mapi_handles_traverse_delete(TDB_CONTEXT *tdb_ctx,
					TDB_DATA key, TDB_DATA dbuf,
					void *state)
{
	TALLOC_CTX			*mem_ctx;
	struct mapi_handles_private	*handles_private = (struct mapi_handles_private *) state;
	uint32_t			handle;
	char				*container_handle_str = NULL;
	char				*handle_str = NULL;

	mem_ctx = talloc_named(NULL, 0, "mapi_handles_traverse_delete");
	container_handle_str = talloc_asprintf(mem_ctx, "0x%x", handles_private->container_handle);

	if (dbuf.dptr && strlen(container_handle_str) == dbuf.dsize && strncmp((const char *)dbuf.dptr, container_handle_str, dbuf.dsize) == 0) {
		handle_str = talloc_strndup(mem_ctx, (char *)key.dptr, key.dsize);
		OC_DEBUG(5, "handles being released must NOT have child handles attached to them (%s is a child of %s)", handle_str, container_handle_str);
		handle = strtol((const char *) handle_str, NULL, 16);
		/* abort(); */
		/* DEBUG(5, ("deleting child handle: %d, %s\n", handle, handle_str)); */
		mapi_handles_delete(handles_private->handles_ctx, handle);
	}

	talloc_free(mem_ctx);

	return 0;
}



/**
   \details Remove the MAPI handle referenced by the handle parameter
   from the double chained list and mark its associated TDB record as
   null

   \param handles_ctx pointer to the MAPI handles context
   \param handle the handle to delete

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_handles_delete(struct mapi_handles_context *handles_ctx, 
					     uint32_t handle)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	TDB_DATA			key;
	struct mapi_handles		*el;
	struct mapi_handles_private	handles_private;
	int				ret;
	bool				found = false;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!handles_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!handles_ctx->tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(handle == MAPI_HANDLES_RESERVED, MAPI_E_INVALID_PARAMETER, NULL);

	OC_DEBUG(4, "Deleting MAPI handle 0x%x (handles_ctx: %p, tdb_ctx: %p)",
		  handle, handles_ctx, handles_ctx->tdb_ctx);

	mem_ctx = talloc_named(NULL, 0, "mapi_handles_delete");

	key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", handle);
	key.dsize = strlen((const char *)key.dptr);

	/* Step 1. Make sure the record exists */
	ret = tdb_exists(handles_ctx->tdb_ctx, key);
	OPENCHANGE_RETVAL_IF(!ret, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Delete this record from the double chained list */
	for (el = handles_ctx->handles; el; el = el->next) {
		if (el->handle == handle) {
			DLIST_REMOVE(handles_ctx->handles, el);
			talloc_free(el);
			found = true;
			break;
		}
	}
	/* This case should never occur */
	OPENCHANGE_RETVAL_IF(found == false, MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 3. Free this record within the TDB database */
	retval = mapi_handles_tdb_free(handles_ctx, handle);
	/* if (retval) { */
	/* 	abort(); */
	/* } */
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 4. Delete hierarchy of children */
	handles_private.handles_ctx = handles_ctx;
	handles_private.container_handle = handle;
	ret = tdb_traverse(handles_ctx->tdb_ctx, mapi_handles_traverse_delete, (void *)&handles_private);

	talloc_free(mem_ctx);

	OC_DEBUG(4, "Deleting MAPI handle 0x%x COMPLETE", handle);

	return MAPI_E_SUCCESS;
}
