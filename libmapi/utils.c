/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2006.

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
#include <gen_ndr/ndr_exchange.h>

/**
   \file utils.c

   \brief General utility functions
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

_PUBLIC_ char *guid_delete_dash(TALLOC_CTX *mem_ctx, const char *recipient_id)
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

/*
  Constructs a PR_ENTRYID value for recipients.
 */
_PUBLIC_ struct Binary_r *generate_recipient_entryid(TALLOC_CTX *mem_ctx, const char *recipient_id)
{
	struct Binary_r	*entryid;
	uint32_t	off;
	char		*guid = (char *) NULL;

	entryid = talloc(mem_ctx, struct Binary_r);
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

/**
 * convert utf8 windows string into classic utf8
 * NOTE: windows utf8 encoding is equal or larger to classic utf8
 *       we should anyway find a better way to allocate the output buf
 */

int yyparse_utf8(char *, const char *);

_PUBLIC_ char *windows_to_utf8(TALLOC_CTX *mem_ctx, const char *input)
{
	char	*tmp = NULL;
	char	*output;

	if (!input) return NULL;

	tmp = malloc(strlen(input) + 1);
	yyparse_utf8(tmp, input);
	output = talloc_strdup(mem_ctx, tmp);
	free(tmp);
	
	return output;
}


/**
   \details Create a FID from an EntryID

   \param cb count of lpb bytes
   \param lpb pointer on an array of bytes
   \param parent_fid the parent folder identifier
   \param fid pointer to the returned fid

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS GetFIDFromEntryID(uint16_t cb, 
					   uint8_t *lpb, 
					   uint64_t parent_fid, 
					   uint64_t *fid)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!lpb, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(cb < 8, MAPI_E_INVALID_PARAMETER, NULL);

	*fid = 0;
	*fid += ((uint64_t)lpb[cb - 3] << 56);
	*fid += ((uint64_t)lpb[cb - 4] << 48);
	*fid += ((uint64_t)lpb[cb - 5] << 40);
	*fid += ((uint64_t)lpb[cb - 6] << 32);
	*fid += ((uint64_t)lpb[cb - 7] << 24);
	*fid += ((uint64_t)lpb[cb - 8] << 16);
	/* WARNING: for some unknown reason the latest byte of folder
	   ID may change (0x1 or 0x4 values identified so far).
	   However this byte sounds the same than the parent folder
	   one */
	*fid += (parent_fid & 0xFFFF);

	return MAPI_E_SUCCESS;
}
