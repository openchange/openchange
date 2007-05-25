/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Fabien Le Mentec 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>

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
	obj->session = 0;
	obj->handles = 0;
	obj->private_data = 0;
}

_PUBLIC_ enum MAPISTATUS mapi_object_init(mapi_object_t *obj)
{
	mapi_object_reset(obj);

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	obj->session = global_mapi_ctx->session;
	return MAPI_E_SUCCESS;
}

_PUBLIC_ void mapi_object_release(mapi_object_t *obj)
{
	enum MAPISTATUS retval;

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


_PUBLIC_ mapi_id_t mapi_object_get_id(mapi_object_t *obj)
{
	return obj->id;
}


void mapi_object_set_id(mapi_object_t *obj, mapi_id_t id)
{
	obj->id = id;
}


mapi_handle_t mapi_object_get_handle(mapi_object_t *obj)
{
	return obj->handle;
}


void mapi_object_set_handle(mapi_object_t *obj, mapi_handle_t handle)
{
	obj->handle = handle;
}


_PUBLIC_ void mapi_object_debug(mapi_object_t* obj)
{
	DEBUG(0, ("mapi_object {\n"));
	DEBUG(0, (" .handle == 0x%x\n", obj->handle));
	DEBUG(0, (" .id     == 0x%llx\n", obj->id));
	DEBUG(0, ("};\n"));
}
