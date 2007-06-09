/*
 * OpenChange Perl bindings wrappers
 *
 *  Copyright (C) Julien Kerihuel  2007.
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

/**
 * Note: Most of these wrapper functions are useless and are just here
 * cause we don't use swig properly.
 */

void lw_dumpdata(void)
{
	global_mapi_ctx->dumpdata = true;
}

uint32_t lw_SetCommonColumns(mapi_object_t *obj)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct SPropTagArray	*SPropTagArray;

	mem_ctx = (TALLOC_CTX *) global_mapi_ctx->session;
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(obj, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	
	return retval;
}

uint32_t lw_SRowSetEnd(struct SRowSet *SRowSet)
{
	if (!SRowSet->cRows) return 0;

	return SRowSet->cRows;
}

uint64_t *lw_getID(struct SRowSet *SRowSet, uint32_t ulPropTag, uint32_t idx)
{
	struct SPropValue *lpProp;
	uint64_t *id;

	lpProp = get_SPropValue_SRow((&SRowSet->aRow[idx]), ulPropTag);
	return get_SPropValue(lpProp, ulPropTag);
}
