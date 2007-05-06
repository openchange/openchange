/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2007.
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
#include <libmapi/dlinklist.h>

/**
 * mapi_handles destructor: flush the list
 */

uint32_t mapi_handles_dtor(struct mapi_handles *mapi_handles)
{
	return 0;
}

/**
 * Add a handle within mapi_handles list
 */

uint32_t mapi_handles_add(struct mapi_handles *mapi_handles, uint32_t handle)
{
	TALLOC_CTX		*mem_ctx = (TALLOC_CTX *)mapi_handles;
	struct mapi_handles	*p;

	p = talloc_zero(mem_ctx, struct mapi_handles);
	p->handle = handle;
	DLIST_ADD(mapi_handles, p);
	return 0;
}

/**
 * Remove handle from mapi_handles list
 */

uint32_t mapi_handles_del(struct mapi_handles *mapi_handles, uint32_t handle)
{
	return 0;
}

/**
 * Get the mapi_handles list, fill the handes array and return handles
 * count
 */

uint32_t mapi_handles_get(struct mapi_handles *mapi_handles, uint32_t **handles)
{
	return 0;
}
