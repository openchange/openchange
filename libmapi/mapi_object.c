/*
   OpenChange MAPI implementation.

   Copyright (C) Fabien Le Mentec 2007.

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>


/**
   \file mapi_object.c

   \brief mapi_object_t support functions
*/


/* fixme: mapi_object functions should return error codes */


/* keep intern to this file
 */
#define INVALID_HANDLE_VALUE 0xffffffff


/* mapi object interface implementation
 */

static void mapi_object_reset(mapi_object_t *obj)
{
	obj->handle = INVALID_HANDLE_VALUE;
	obj->id = 0;
	obj->handles = 0;
	obj->private_data = 0;
}


/**
   \details Initialize MAPI object

   This function is required to be called before any 
   manipulation of this MAPI object.

   \param obj the object to initialize
   
   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized.

   \sa mapi_object_release
*/
_PUBLIC_ enum MAPISTATUS mapi_object_init(mapi_object_t *obj)
{
	mapi_object_reset(obj);

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details Release MAPI object

   This function is required to be called when this MAPI object
   is no longer required.

   \param obj the object to release

   \sa mapi_object_initialize, Release
*/
_PUBLIC_ void mapi_object_release(mapi_object_t *obj)
{
	enum MAPISTATUS retval;

	if (!obj) return;

	retval = Release(obj);
	if (retval == MAPI_E_SUCCESS) {
		if (obj->private_data) {
			talloc_free(obj->private_data);
		}
		mapi_object_reset(obj);
	}
}


int mapi_object_is_invalid(mapi_object_t *obj)
{
	if (mapi_object_get_handle(obj) == INVALID_HANDLE_VALUE) {
		return 1;
	}
	
	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve an object ID for a given MAPI object
 
   \param obj the object to get the ID for

   \return the object ID, or -1 if the object does not exist
*/
_PUBLIC_ mapi_id_t mapi_object_get_id(mapi_object_t *obj)
{
	return (!obj) ? -1 : obj->id;
}


void mapi_object_set_id(mapi_object_t *obj, mapi_id_t id)
{
	obj->id = id;
}


mapi_handle_t mapi_object_get_handle(mapi_object_t *obj)
{
	return (!obj) ? -1 : obj->handle;
}


void mapi_object_set_handle(mapi_object_t *obj, mapi_handle_t handle)
{
	obj->handle = handle;
}


/**
   \details Dump a MAPI object (for debugging)

   \param obj the MAPI object to dump out
*/
_PUBLIC_ void mapi_object_debug(mapi_object_t *obj)
{
	DEBUG(0, ("mapi_object {\n"));
	DEBUG(0, (" .handle == 0x%x\n", obj->handle));
	DEBUG(0, (" .id     == 0x%llx\n", obj->id));
	DEBUG(0, ("};\n"));
}


void mapi_object_table_init(mapi_object_t *obj_table)
{
	mapi_ctx_t		*mapi_ctx;
	mapi_object_table_t	*table = NULL;

	mapi_ctx = global_mapi_ctx;

	if (obj_table->private_data == NULL) {
		obj_table->private_data = talloc_zero((TALLOC_CTX *)mapi_ctx->session, mapi_object_table_t);
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


enum MAPISTATUS mapi_object_bookmark_find(mapi_object_t *obj_table, uint32_t bkPosition,
					  struct SBinary_short *bin)
{
	mapi_object_table_t	*table;
	mapi_object_bookmark_t	*bookmark;

	table = (mapi_object_table_t *)obj_table->private_data;
	bookmark = table->bookmark;

	/* Sanity check */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!table, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!bookmark, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(bkPosition > table->bk_last, MAPI_E_INVALID_BOOKMARK, NULL);
	
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


_PUBLIC_ enum MAPISTATUS mapi_object_bookmark_get_count(mapi_object_t *obj_table, 
							uint32_t *count)
{
	mapi_object_table_t	*table;

	table = (mapi_object_table_t *)obj_table->private_data;

	/* Sanity check */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!table, MAPI_E_NOT_INITIALIZED, NULL);

	*count = table->bk_last - 3;

	return MAPI_E_SUCCESS;
}


_PUBLIC_ enum MAPISTATUS mapi_object_bookmark_debug(mapi_object_t *obj_table)
{
	mapi_object_table_t	*table;
	mapi_object_bookmark_t	*bookmark;

	table = (mapi_object_table_t *)obj_table->private_data;
	bookmark = table->bookmark;

	/* Sanity check */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!table, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!bookmark, MAPI_E_NOT_INITIALIZED, NULL);
	
	while (bookmark) {
		DEBUG(0, ("mapi_object_bookmark {\n"));
		DEBUG(0, (".index == %u\n", bookmark->index));
		dump_data(0, bookmark->bin.lpb, bookmark->bin.cb);
		DEBUG(0, ("};\n"));

		bookmark = bookmark->next;
	}	
	return MAPI_E_SUCCESS;
}
