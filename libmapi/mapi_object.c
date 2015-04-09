/*
   OpenChange MAPI implementation.

   Copyright (C) Fabien Le Mentec 2007.
   Copyright (C) Julien Kerihuel 2007-2011.

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

/**
   \file mapi_object.c

   \brief mapi_object_t support functions
*/


/* FIXME: mapi_object functions should return error codes */


/**
   keep intern to this file
*/
#define INVALID_HANDLE_VALUE 0xffffffff


/**
   \details Reset a MAPI object structure

   \param obj pointer on the MAPI object to reset
 */
static void mapi_object_reset(mapi_object_t *obj)
{
	obj->handle = INVALID_HANDLE_VALUE;
	obj->logon_id = 0;
	obj->store = false;
	obj->id = 0;
	obj->session = NULL;
	obj->private_data = NULL;
}


/**
   \details Initialize MAPI object

   This function is required to be called before any 
   manipulation of this MAPI object.

   \param obj the object to initialize
   
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
   
   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.

   \sa mapi_object_release
*/
_PUBLIC_ enum MAPISTATUS mapi_object_init(mapi_object_t *obj)
{
	mapi_object_reset(obj);

	return MAPI_E_SUCCESS;
}


/**
   \details Release MAPI object

   This function is required to be called when this MAPI object
   is no longer required.

   \param obj pointer on the MAPI object to release

   \sa mapi_object_initialize, Release
*/
_PUBLIC_ void mapi_object_release(mapi_object_t *obj)
{
	enum MAPISTATUS retval;

	if (!obj) return;
	if (obj->handle == INVALID_HANDLE_VALUE) return;

	retval = Release(obj);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(1, "Release has failed");
	}

	if (obj->private_data) {
		talloc_free(obj->private_data);
	}

	if (obj->store == true && obj->session) {
		obj->session->logon_ids[obj->logon_id] = 0;
	}

	mapi_object_reset(obj);
}


/**
   \details Check if the supplied object has a valid handle

   \param obj pointer on the MAPI object to test

   \return 0 on success, otherwise 1
 */
int mapi_object_is_invalid(mapi_object_t *obj)
{
	if (mapi_object_get_handle(obj) == INVALID_HANDLE_VALUE) {
		return 1;
	}
	
	return MAPI_E_SUCCESS;
}


/**
   \details Copy MAPI object

   This function copies mapi_object data from source to destination.

   \param dst pointer on the destination MAPI object
   \param src pointer on the source MAPI object

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_INITIALIZED

 */
_PUBLIC_ enum MAPISTATUS mapi_object_copy(mapi_object_t *dst, mapi_object_t *src)
{
	mapi_object_reset(dst);

	OPENCHANGE_RETVAL_IF(!dst || !src, MAPI_E_NOT_INITIALIZED, NULL);

	dst->id = src->id;
	dst->handle = src->handle;
	dst->private_data = src->private_data;
	dst->logon_id = src->logon_id;
	dst->session = src->session;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the session associated to the MAPI object

   \param obj the object to get the session for

   \return pointer on a MAPI session on success, otherwise NULL
 */
_PUBLIC_ struct mapi_session *mapi_object_get_session(mapi_object_t *obj)
{
	if (!obj) return NULL;
	if (!obj->session) return NULL;

	return obj->session;
}


/**
   \details Set the session for a given MAPI object

   \param obj pointer on the object to set the session for
   \param session pointer on the MAPI session to associate to the MAPI
   object
 */
_PUBLIC_ void mapi_object_set_session(mapi_object_t *obj, 
				      struct mapi_session *session)
{
	if (obj) {
		obj->session = session;
	}
}


/**
   \details Retrieve an object ID for a given MAPI object
 
   \param obj pointer on the MAPI object to get the ID for

   \return the object ID, or 0xFFFFFFFFFFFFFFFF if the object does not exist
*/
_PUBLIC_ mapi_id_t mapi_object_get_id(mapi_object_t *obj)
{
	return (!obj) ? -1 : obj->id;
}


/**
   \details Set the id for a given MAPI object

   \param obj pointer on the MAPI object to set the session for
   \param id Identifier to set to the object obj
 */
void mapi_object_set_id(mapi_object_t *obj, mapi_id_t id)
{
	obj->id = id;
}


/**
   \details Set the logon id for a given MAPI object

   \param obj pointer to the object to set the logon id for
   \param logon_id the logon identifier to associate to the MAPI
   object
 */
_PUBLIC_ void mapi_object_set_logon_id(mapi_object_t *obj, 
				       uint8_t logon_id)
{
	if (obj) {
		obj->logon_id = logon_id;
	}
}


/**
   \details Retrieve the logon id for a given MAPI object

   \param obj pointer to the object to retrieve the logon id from
   \param logon_id pointer to a variable to store the logon id

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS mapi_object_get_logon_id(mapi_object_t *obj, uint8_t *logon_id)
{
	if (!obj || !logon_id)
		return MAPI_E_INVALID_PARAMETER;

	*logon_id = obj->logon_id; 

	return MAPI_E_SUCCESS;
}


/**
   \details Mark a MAPI object as a store object
   
   \param obj pointer to the object to set the store boolean for
 */
_PUBLIC_ void mapi_object_set_logon_store(mapi_object_t *obj)
{
	if (obj) {
		obj->store = true;
	}
}


/**
   \details Retrieve the handle associated to a MAPI object

   \param obj pointer on the MAPI object to retrieve the handle from

   \return a valid MAPI object handle on success, otherwise 0xFFFFFFFF.
 */
mapi_handle_t mapi_object_get_handle(mapi_object_t *obj)
{
	return (!obj) ? 0xFFFFFFFF : obj->handle;
}


/**
   \details Associate a handle to a MAPI object

   \param obj pointer on the MAPI object on which handle has to be set
   \param handle the MAPI handle value
 */
void mapi_object_set_handle(mapi_object_t *obj, mapi_handle_t handle)
{
	obj->handle = handle;
}


/**
   \details Dump a MAPI object (for debugging)

   \param obj pointer on the MAPI object to dump out
*/
_PUBLIC_ void mapi_object_debug(mapi_object_t *obj)
{
	OC_DEBUG(0, "mapi_object {");
	OC_DEBUG(0, " .handle == 0x%x", obj->handle);
	OC_DEBUG(0, " .id     == 0x%"PRIx64, obj->id);
	OC_DEBUG(0, "};");
}


/**
   \details Initialize MAPI object private data to store a MAPI object
   table

   \param mem_ctx pointer on the memory context
   \param obj_table pointer on the MAPI object
 */
void mapi_object_table_init(TALLOC_CTX *mem_ctx, mapi_object_t *obj_table)
{
	mapi_object_table_t	*table = NULL;

	if (obj_table->private_data == NULL) {
		obj_table->private_data = talloc_zero((TALLOC_CTX *)mem_ctx, mapi_object_table_t);
	}

	table = (mapi_object_table_t *) obj_table->private_data;

	if (table->bookmark == NULL) {
		table->bookmark = talloc_zero((TALLOC_CTX *)table, mapi_object_bookmark_t);
	}


	table->proptags.aulPropTag = 0;
	table->proptags.cValues = 0;
	/* start bookmark index after BOOKMARK_END */ 
	table->bk_last = 3;
}


/**
   \details Fetch a bookmark within a MAPI object table

   \param obj_table pointer on the MAPI object table
   \param bkPosition the bookmark position to find
   \param bin pointer on the Sbinary_short the function fills

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
enum MAPISTATUS mapi_object_bookmark_find(mapi_object_t *obj_table, uint32_t bkPosition,
					  struct SBinary_short *bin)
{
	mapi_object_table_t	*table;
	mapi_object_bookmark_t	*bookmark;

	table = (mapi_object_table_t *)obj_table->private_data;
	bookmark = table->bookmark;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!bookmark, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(bkPosition > table->bk_last, MAPI_E_INVALID_BOOKMARK, NULL);
	
	while (bookmark) {
		if (bookmark->index == bkPosition) {
			bin->cb = bookmark->bin.cb;
			bin->lpb = bookmark->bin.lpb;
			return MAPI_E_SUCCESS;
		}
		bookmark = bookmark->next;
	}
	return MAPI_E_INVALID_BOOKMARK;
}


/**
   \details Retrieve the number of bookmarks stored in a MAPI object table

   \param obj_table pointer to the MAPI object table
   \param count pointer to the number of bookmarks to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS mapi_object_bookmark_get_count(mapi_object_t *obj_table, 
							uint32_t *count)
{
	mapi_object_table_t	*table;

	table = (mapi_object_table_t *)obj_table->private_data;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_NOT_INITIALIZED, NULL);

	*count = table->bk_last - 3;

	return MAPI_E_SUCCESS;
}


/**
   \details Dump bookmarks associated to a MAPI object table

   \param obj_table pointer on the MAPI object table

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
 */
_PUBLIC_ enum MAPISTATUS mapi_object_bookmark_debug(mapi_object_t *obj_table)
{
	mapi_object_table_t	*table;
	mapi_object_bookmark_t	*bookmark;

	table = (mapi_object_table_t *)obj_table->private_data;
	bookmark = table->bookmark;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!table, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!bookmark, MAPI_E_NOT_INITIALIZED, NULL);
	
	while (bookmark) {
		OC_DEBUG(0, "mapi_object_bookmark {");
		OC_DEBUG(0, ".index == %u", bookmark->index);
		dump_data(0, bookmark->bin.lpb, bookmark->bin.cb);
		OC_DEBUG(0, "};");

		bookmark = bookmark->next;
	}	
	return MAPI_E_SUCCESS;
}
