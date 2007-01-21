/*
   MAPI Implementation: Utils functions

   OpenChange Project

   Copyright (C) Julien Kerihuel 2005-2006

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
#include "ndr_exchange.h"
#include <tdr.h>
#include "libmapi/mapi.h"

/*
  Constructs a PR_ENTRYID value for recipients.
 */

/* 
   FIXME: 
   nor     0x00 0x00 0x00 0x00 at the beginning 
   neither 0x2f at the end should be listed 
*/
static const uint8_t MAPI_LOCAL_UID[] = {
	0xdc, 0xa7, 0x40, 0xc8, 0xc0, 0x42, 0x10, 0x1a,
	0xb4, 0xb9, 0x08, 0x00, 0x2b, 0x2f, 0xe1, 0x82
};

static const uint8_t MAPI_LOCAL_UID_END[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 
	0x00, 0x2f
};

char *guid_delete_dash(TALLOC_CTX *mem_ctx, const char *recipient_id)
{
	char		*guid;
	uint32_t	count,i;

	if (!recipient_id) {
		return NULL;
	}

	for (count=0,i=0;i!=strlen(recipient_id);i++) {
		if (recipient_id[i] != '-') count++;
	}

	guid = talloc_zero_size(mem_ctx, count+1);
	for (count=0,i = 0;i!=strlen(recipient_id);i++) {
		if (recipient_id[i] != '-') {
			guid[count] = recipient_id[i];
			count++;
		}
	}

	return guid;
}

struct SBinary *generate_recipient_entryid(TALLOC_CTX *mem_ctx, const char *recipient_id)
{
	struct SBinary *entryid;
	uint32_t	off;
	char		*guid = (char *) NULL;

	entryid = talloc(mem_ctx, struct SBinary);
	entryid->cb = sizeof (uint32_t) + sizeof (MAPI_LOCAL_UID) + sizeof (MAPI_LOCAL_UID_END) + 1;

	if (recipient_id) {
		guid = guid_delete_dash(mem_ctx, recipient_id);
		entryid->cb += strlen(guid);
	}

	entryid->lpb = talloc_zero_size(mem_ctx, entryid->cb);
	off = 4;
	memcpy(entryid->lpb + off, MAPI_LOCAL_UID, sizeof (MAPI_LOCAL_UID));
	off += sizeof (MAPI_LOCAL_UID);
	
	memcpy(entryid->lpb + off, MAPI_LOCAL_UID_END, sizeof (MAPI_LOCAL_UID_END));
	off += sizeof (MAPI_LOCAL_UID_END);

	if (recipient_id) {
		strcpy((char *)entryid->lpb + off, guid);
		off += strlen(recipient_id);
	}
	
	return entryid;
}

