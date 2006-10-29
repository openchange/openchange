/* 
   OpenChange implementation.

   libndr mapi support

   Copyright (C) Julien Kerihuel 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "openchange.h"
#include <ndr.h>
#include <core/nterr.h>
#include "ndr_exchange.h"
#include "ndr_mapi.h"

#define mapi_data_talloc(ctx, ptr, size) mapi_data_talloc_named(ctx, ptr, size, "MAPI_DATA:"__location__)

const char *get_mapi_opname(uint16_t opnum);

static void obfuscate_data(uint8_t *data, uint32_t size, uint8_t salt)
{
	uint32_t i;

	for (i=0; i<size; i++) {
		data[i] ^= salt;
	}
}

/*
  print the MAPI_DATA structure
 */

void ndr_print_MAPI_DATA(struct ndr_print *ndr, const char *name, const struct MAPI_DATA *r)
{
	uint32_t	rlength;

	rlength = r->mapi_len - r->length;

	ndr->print(ndr, "%-25s: MAPI_DATA (content) length=%u", name, r->length);
	ndr->print(ndr, "%-25s: MAPI DATA opnum = %s (0x%.04X)", name, get_mapi_opname(r->opnum), r->opnum);
	if (r->length) {
		dump_data(10, r->content, r->length);
	}
	ndr->print(ndr, "%-25s: MAPI_DATA (remaining) length=%u", name, rlength);
	if (rlength) {
		dump_data(10, r->remaining, rlength);
	}
}

/*
  push a MAPI_DATA onto the wire.  

  MAPI length field includes length bytes. 
  But these bytes do not belong to the mapi content in the user
  context. We have to add them when pushing mapi content length
  (uint16_t) and next substract when pushing the content blob
*/

NTSTATUS ndr_push_MAPI_DATA(struct ndr_push *ndr, int ndr_flags, const struct MAPI_DATA *_blob)
{
	uint32_t	remaining_length;
	struct MAPI_DATA blob = *_blob;

	if (!(ndr->flags & LIBNDR_FLAG_REMAINING)) {
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, blob.mapi_len));
	}
	blob.length += 2;
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, blob.length));

	blob.mapi_len -= 2;
	blob.length -= 2;
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, blob.opnum));
	/* opnum is extracted from content and is 2 bytes long */
	NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, blob.content, blob.length - 2));

	remaining_length = blob.mapi_len - blob.length;
	NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, blob.remaining, remaining_length));

	obfuscate_data(ndr->data, ndr->alloc_size, 0xA5);

	return NT_STATUS_OK;
}

/*
  pull a MAPI_DATA from the wire
*/

NTSTATUS ndr_pull_MAPI_DATA(struct ndr_pull *ndr, int ndr_flags, struct MAPI_DATA *blob)
{
	uint32_t length;
	uint32_t remaining_length;

	obfuscate_data(ndr->data, ndr->data_size, 0xA5);

	if (ndr->flags & LIBNDR_FLAG_REMAINING) {
		length = ndr->data_size - ndr->offset;
	} else {
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &length));
	}
	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &blob->length));
	blob->mapi_len = length - 2;

	/* length includes length field, we have to substract it */
	blob->length -= 2;
	
	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &blob->opnum));
	/* opnum is extracted from content and is 2 bytes long */
	blob->content = talloc_size(NULL, blob->length - 2);
	NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, blob->content, (uint32_t)blob->length - 2));

	remaining_length = blob->mapi_len - blob->length;
	blob->remaining = talloc_size(NULL, remaining_length);
	NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, blob->remaining, remaining_length));

	return NT_STATUS_OK;
}
