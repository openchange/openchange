/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Inverse inc. 2010

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
   \file libmapiserver_oxcfxics.c

   \brief OXCFXICS ROP Response size calculations
 */

#include "libmapiserver.h"

/**
   \details Calculate FastTransferSourceCopyTo (0x4d) Rop size

   \param response pointer to the FastTransferSourceCopyTo EcDoRpc_MAPI_REPL
   structure

   \return Size of FastTransferSourceCopyTo response
 */
_PUBLIC_ uint16_t libmapiserver_RopFastTransferSourceCopyTo_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate FastTransferSourceGetBuffer (0x4d) Rop size

   \param response pointer to the FastTransferSourceGetBuffer EcDoRpc_MAPI_REPL
   structure

   \return Size of FastTransferSourceGetBuffer response
 */
_PUBLIC_ uint16_t libmapiserver_RopFastTransferSourceGetBuffer_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		if (response->error_code == ecServerBusy) {
			size += sizeof (uint32_t); /* size of BackoffTime */
		}
		return size;
	}

	size += SIZE_DFLT_ROPFASTTRANSFERSOURCEGETBUFFER + response->u.mapi_FastTransferSourceGetBuffer.TransferBuffer.length;

	return size;
}
