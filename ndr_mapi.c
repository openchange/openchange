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
#include "libmapi/include/proto.h"
#include "libmapi/tdr_mapi.h"

#define mapi_data_talloc(ctx, ptr, size) mapi_data_talloc_named(ctx, ptr, size, "MAPI_DATA:"__location__)

const char *get_mapi_opname(uint8_t opnum);

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
	if (r->length) {
		struct tdr_pull		*tdr;
		struct tdr_print	*tdr_print;
		TALLOC_CTX		*mem_ctx;
		struct MAPI_REQ		mapi_req;
		struct MAPI_REPL	mapi_repl;
		uint32_t		tmp_offset;
		
		mem_ctx = talloc_init("ndrdump");
		tdr = talloc_zero(mem_ctx, struct tdr_pull);
		tdr_print = talloc_zero(mem_ctx, struct tdr_print);
		tdr_print->print = tdr_print_debug_helper;
		tdr_print->level = ndr->depth;
		
		tdr->data.data = r->content;
		tdr->data.length = r->length;
		
		if (ndr->inout & NDR_IN) {
			while (tdr->offset < r->length) {
				/* 2 = opnum + mapi_flags */
				tmp_offset = tdr->offset + 2;
				tdr_pull_MAPI_REQ(tdr, mem_ctx, &mapi_req);

				if (tmp_offset == tdr->offset) {
					dump_data(0, r->content + tdr->offset + 1, r->length - tdr->offset);
					break;
				} else {
					tdr_print_MAPI_REQ(tdr_print, "blob", &mapi_req);
				}
			}
		}
		
		if (ndr->inout & NDR_OUT) {
			while (tdr->offset < r->length) {
				/* 2 = opnum + mapi_flags */
				tmp_offset = tdr->offset + 2;
				tdr_pull_MAPI_REPL(tdr, mem_ctx, &mapi_repl);
				
				if (tmp_offset == tdr->offset) {
					dump_data(0, r->content + tdr->offset + 1, r->length - tdr->offset);
					break;
				} else {
					tdr_print_MAPI_REPL(tdr_print, "blob", &mapi_repl);
				}
			}
		}
	}
	
	ndr->print(ndr, "%-25s: MAPI_DATA (handles) number=%u", name, rlength / 4);
	
	if (rlength) {
	  uint32_t	number, i;
	  uint32_t	*handles;

	  number = rlength / 4;
	  handles = (uint32_t *)r->remaining;
	  ndr->depth++;

	  for (i = 0; i < number; i++) {
		  ndr_print_uint32(ndr, "handle ID", handles[i]);
	  }
	  ndr->depth--;
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
/* 	NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, blob.opnum)); */
/* 	NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, blob.mapi_flags)); */
	/* opnum + action (request/response) are extracted from content
	   and are 2 bytes long 
	*/
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
	
/* 	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &blob->opnum)); */
/* 	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &blob->mapi_flags)); */
	/* opnum + action are extracted from content and are 2 bytes
	   long 
	*/
	blob->content = talloc_size(NULL, blob->length - 2);
	NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, blob->content, (uint32_t)blob->length - 2));

	remaining_length = blob->mapi_len - blob->length;
	blob->remaining = talloc_size(NULL, remaining_length);
	NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, blob->remaining, remaining_length));

	return NT_STATUS_OK;
}
