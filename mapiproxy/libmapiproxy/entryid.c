/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

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
   \file entryid.c

   \brief EntryID convenient routines
 */

#include <mapiproxy/dcesrv_mapiproxy.h>
#include "libmapiproxy.h"
#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>

/**
   \details Build an Address Book EntryID from a legacyExchangeDN

   \param mem_ctx pointer to the memory context
   \param legacyExchangeDN
 */
_PUBLIC_ enum MAPISTATUS entryid_set_AB_EntryID(TALLOC_CTX *mem_ctx, 
						const char *legacyExchangeDN,
						struct SBinary_short *bin)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!legacyExchangeDN, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!bin, MAPI_E_INVALID_PARAMETER, NULL);

	bin->cb = 28 + strlen(legacyExchangeDN) + 1;
	bin->lpb = talloc_array(mem_ctx, uint8_t, bin->cb);
	
	/* Fill EntryID: FIXME we should use PermanentEntryID here */
	memset(bin->lpb, 0, bin->cb);
	memcpy(&bin->lpb[4], GUID_NSPI, 16);
	bin->lpb[20] = 0x1;
	memcpy(bin->lpb + 28, legacyExchangeDN, strlen(legacyExchangeDN));

	return MAPI_E_SUCCESS;
}
