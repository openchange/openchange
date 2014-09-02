/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2012.

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "gen_ndr/ndr_exchange.h"

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

	guid = (char *)talloc_zero_size(mem_ctx, count+1);
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

	entryid->lpb = (uint8_t *)talloc_zero_size(mem_ctx, entryid->cb);
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
					   int64_t parent_fid,
					   int64_t *fid)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!lpb, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(cb < 8, MAPI_E_INVALID_PARAMETER, NULL);

	*fid = 0;
	*fid += ((int64_t)lpb[cb - 3] << 56);
	*fid += ((int64_t)lpb[cb - 4] << 48);
	*fid += ((int64_t)lpb[cb - 5] << 40);
	*fid += ((int64_t)lpb[cb - 6] << 32);
	*fid += ((int64_t)lpb[cb - 7] << 24);
	*fid += ((int64_t)lpb[cb - 8] << 16);
	/* WARNING: for some unknown reason the latest byte of folder
	   ID may change (0x1 or 0x4 values identified so far).
	   However this byte sounds the same than the parent folder
	   one */
	*fid += (parent_fid & 0xFFFF);

	return MAPI_E_SUCCESS;
}

/**
   \details Build an EntryID for message from folder and message source ID

 */
_PUBLIC_ enum MAPISTATUS EntryIDFromSourceIDForMessage(TALLOC_CTX *mem_ctx,
						       mapi_object_t *obj_store,
						       mapi_object_t *obj_folder,
						       mapi_object_t *obj_message,
						       struct SBinary_short *entryID)
{
	enum MAPISTATUS			retval;
	struct ndr_push			*ndr;
	mapi_object_store_t		*store;
	const struct SBinary_short	*FolderSourceKey;
	const struct SBinary_short	*MessageSourceKey;
	struct SPropTagArray		*SPropTagArray;
	struct SPropValue		*lpPropsf;
	struct SPropValue		*lpPropsm;
	uint32_t			count;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!obj_store, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!entryID, MAPI_E_INVALID_PARAMETER, NULL);

	store = (mapi_object_store_t *)obj_store->private_data;

	/* Step 1. Retrieve folder Source Key */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PidTagSourceKey);
	retval = GetProps(obj_folder, 0, SPropTagArray, &lpPropsf, &count);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return MAPI_E_NOT_FOUND;
	FolderSourceKey = (const struct SBinary_short *)get_SPropValue(lpPropsf, PidTagSourceKey);
	if (FolderSourceKey == NULL) return MAPI_E_NOT_FOUND;

	/* Step 2. Retrieve message Source Key */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PidTagSourceKey);
	retval = GetProps(obj_message, 0, SPropTagArray, &lpPropsm, &count);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return MAPI_E_NOT_FOUND;
	MessageSourceKey = (const struct SBinary_short *)get_SPropValue(lpPropsm, PidTagSourceKey);
	if (MessageSourceKey == NULL) return MAPI_E_NOT_FOUND;

	/* Step 3. Fill PidTagEntryID */
	/* PidTagEntryId size for message is 70 bytes */
	/* Flags (4 bytes) + store guid (16 bytes) + object type (2
	 * bytes) + Folder SourceKey (22 bytes) + Pad (2 bytes) +
	 * Message Source (22 bytes) + Pad (2 bytes) == 70 bytes */
	ndr = ndr_push_init_ctx(NULL);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;

	ndr_push_uint32(ndr, NDR_SCALARS, 0x0);
	if (store->store_type == PublicFolder) {
		const uint8_t	ProviderUID[16] = { 0x1A, 0x44, 0x73, 0x90, 0xAA, 0x66, 0x11, 0xCD, 0x9B, 0xC8, 0x00, 0xAA, 0x00, 0x2F, 0xC4, 0x5A };

		ndr_push_array_uint8(ndr, NDR_SCALARS, ProviderUID, 16);
		ndr_push_uint16(ndr, NDR_SCALARS, 0x0009);
	} else {
		ndr_push_GUID(ndr, NDR_SCALARS, &store->guid);
		ndr_push_uint16(ndr, NDR_SCALARS, 0x0007);
	}
	ndr_push_array_uint8(ndr, NDR_SCALARS, FolderSourceKey->lpb, FolderSourceKey->cb);
	ndr_push_uint16(ndr, NDR_SCALARS, 0x0);
	ndr_push_array_uint8(ndr, NDR_SCALARS, MessageSourceKey->lpb, MessageSourceKey->cb);
	ndr_push_uint16(ndr, NDR_SCALARS, 0x0);

	entryID->cb = ndr->offset;
	entryID->lpb = talloc_steal(mem_ctx, ndr->data);

	talloc_free(ndr);

	return MAPI_E_SUCCESS;
}
