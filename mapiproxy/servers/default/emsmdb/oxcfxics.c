/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Wolfgang Sourdeau 2010-2012

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
   \file oxcfxics.c

   \brief FastTransfer and ICS object routines and Rops
 */

#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"
#include "gen_ndr/ndr_exchange.h"

/* a constant time offset by which the first change number ever can be produced by OpenChange */
#define oc_version_time 0x4dbb2dbe

/* the maximum buffer that will be populated during msg synchronization operations (note: this is a soft limit) */
static const size_t max_message_sync_size = 262144;
static const uint32_t message_preload_interval = 150;

/** notes:
 * conventions:
 - binary data must be returned as Binary_r
 - PidTagChangeNumber, PidTagChangeKey and PidTagPredecessorChangeList *must* be handled by the backend code
 - PidTagSourceKey, PidTagParentSourceKey are deduced automatically from PidTagMid/PidTagFolderId and PidTagParentFolderId
 * PidTag*Key should be computed in the same manner in oxcprpt and oxctabl
 - all string properties are fetched via their _UNICODE version
 - "PidTagLastModificationTime" is left to the backend, maybe setprops operations could provide an optional one, for reference...
 ? idea: getprops on tables and objects without property array = get all props
 * no deletions yet for non-mapistore objects
 * no conflict resolution
 * ImportHierarchyChange require the same changes as RopOpenFolder with regards to opening folder and mapistore v2 functionality

 * there is a hack with get props and get table props for root mapistore folders, that can be solved with mapistore v2
 * another missing feature (3.3.5.5.4.1.1): "A move of a folder from one
 parent to another is modeled as a modification of a folder, where the value
 of PidTagParentSourceKey of the folder changes to reflect the new parent."

 * HACK: CnsetSeen = CnsetSeenFAI = CnsetRead */

struct oxcfxics_prop_index {
	uint32_t	parent_fid;
	uint32_t	eid;
	uint32_t	change_number; /* PidTagChangeNumber */
	uint32_t	change_key; /* PidTagChangeKey */
	uint32_t	predecessor_change_list;
	uint32_t	last_modification_time;
	uint32_t	display_name;
	uint32_t	associated;
	uint32_t	message_size;
};

struct oxcfxics_sync_data {
	struct GUID			replica_guid;
	uint8_t				table_type;
	struct oxcfxics_prop_index	prop_index;

	struct ndr_push			*ndr;
	struct ndr_push			*cutmarks_ndr;

	struct rawidset			*eid_set;
	struct rawidset			*cnset_seen;
	struct rawidset			*cnset_read;

	struct rawidset			*deleted_eid_set;

	struct oxcfxics_message_sync_data	*message_sync_data;
};

struct oxcfxics_message_sync_data {
	uint64_t	*mids;
	uint64_t	count;
	uint64_t	max;
};

/** ndr helpers */
#if 1
#define oxcfxics_ndr_check(x,y)
#else
static void oxcfxics_ndr_check(struct ndr_push *ndr, const char *label)
{
	if (ndr->data == NULL) {
		OC_DEBUG(5, ("ndr->data is null!!!\n"));
		abort();
	}
	if (ndr->offset >= ndr->alloc_size) {
		OC_DEBUG(5, ("inconcistency: ndr->alloc_size must be > ndr->offset\n"));
		abort();
	}
	OC_DEBUG(5, ("'%s' state: ptr: %p alloc: %d offset: %d\n", label, ndr->data, ndr->alloc_size, ndr->offset));
}
#endif

#if 1
#define oxcfxics_check_cutmark_buffer(x,y)
#else
static void oxcfxics_check_cutmark_buffer(void *cutmark_buffer, DATA_BLOB *data_buffer)
{
	uint32_t *digits_ptr;
	uint32_t min_size, prev_cutmark, cutmark;
	bool done;

	digits_ptr = cutmark_buffer;
	prev_cutmark = 0;
	done = false;
	while (!done) {
		cutmark = digits_ptr[1];
		if (cutmark == 0xffffffff) {
			done = true;
		}
		else {
			min_size = digits_ptr[0];
			if (min_size > 0) {
				if (min_size != 4 && min_size != 8) {
					OC_DEBUG(0, ("invalid min_size: %d\n", min_size));
					abort();
				}
			}
			if (cutmark < prev_cutmark && prev_cutmark > 0) {
				OC_DEBUG(0, ("cutmark goes backward\n"));
				abort();
			}
			if (cutmark > data_buffer->length) {
				OC_DEBUG(0, ("cutmark goes beyond the buffer size\n"));
				abort();
			}
			digits_ptr += 2;
		}
	}
}
#endif

static const int message_properties_shift = 7;
static const int folder_properties_shift = 7;

static void oxcfxics_ndr_push_simple_data(struct ndr_push *ndr, uint16_t prop_type, const void *value)
{
	uint32_t		string_len;
	uint32_t		saved_flags;
	const struct Binary_r	*bin_value;

	switch (prop_type) {
	case PT_I2:
		ndr_push_uint16(ndr, NDR_SCALARS, *(uint16_t *) value);
		break;
	case PT_LONG:
	case PT_ERROR:
	case PT_OBJECT:
		ndr_push_uint32(ndr, NDR_SCALARS, *(uint32_t *) value);
		break;
	case PT_DOUBLE:
		ndr_push_double(ndr, NDR_SCALARS, *(double *) value);
		break;
	case PT_I8:
		ndr_push_dlong(ndr, NDR_SCALARS, *(uint64_t *) value);
		break;
	case PT_BOOLEAN:
		ndr_push_uint16(ndr, NDR_SCALARS, (*(uint8_t *) value) ? 1 : 0);
		break;
	case PT_STRING8:
		saved_flags = ndr->flags;
		string_len = strlen(value) + 1;
		ndr_push_uint32(ndr, NDR_SCALARS, string_len);
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM|LIBNDR_FLAG_STR_ASCII);
		ndr_push_string(ndr, NDR_SCALARS, (char *) value);
		ndr->flags = saved_flags;
		break;
	case PT_UNICODE:
		saved_flags = ndr->flags;
		string_len = strlen_m_ext((char *) value, CH_UTF8, CH_UTF16LE) * 2 + 2;
		ndr_push_uint32(ndr, NDR_SCALARS, string_len);
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
		ndr_push_string(ndr, NDR_SCALARS, (char *) value);
		ndr->flags = saved_flags;
		break;
	case PT_SVREID:
	case PT_BINARY:
		bin_value = value;
		if (bin_value->cb > 0) {
			ndr_push_Binary_r(ndr, NDR_BUFFERS, bin_value);
		}
		else {
			ndr_push_uint32(ndr, NDR_SCALARS, 0);
		}
		break;
	case PT_CLSID:
		ndr_push_GUID(ndr, NDR_SCALARS, (struct GUID *) value);
		break;
	case PT_SYSTIME:
		ndr_push_FILETIME(ndr, NDR_SCALARS, (struct FILETIME *) value);
		break;
	case PT_NULL:
		break;
	default:
		OC_DEBUG(5, "unsupported property type: %.4x\n", prop_type);
		abort();
	}
}

static uint32_t oxcfxics_compute_cutmark_min_value_buffer(uint16_t prop_type)
{
	uint32_t min_value_buffer;

	if ((prop_type & MV_FLAG)) {
		/* TODO: minimal cutmarks are difficult to deduce for multi values */
		min_value_buffer = 8;
	}
	else {
		switch (prop_type) {
		case PT_I2:
		case PT_LONG:
		case PT_ERROR:
		case PT_OBJECT:
		case PT_FLOAT:
		case PT_DOUBLE:
		case PT_I8:
		case PT_BOOLEAN:
		case PT_SYSTIME:
		case PT_CLSID:
			min_value_buffer = 0;
			break;
		case PT_STRING8:
		case PT_UNICODE:
		case PT_SVREID:
		case PT_BINARY:
			min_value_buffer = 8;
			break;
		default:
			OC_DEBUG(5, "unsupported property type: %.4x\n", prop_type);
			abort();
		}
	}

	return min_value_buffer;
}

static void oxcfxics_ndr_push_properties(struct ndr_push *ndr, struct ndr_push *cutmarks_ndr, void *nprops_ctx, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t		i, j, min_value_buffer;
	enum MAPITAGS		property;
        struct MAPINAMEID       *nameid;
	struct BinaryArray_r	*bin_array;
	struct StringArrayW_r	*unicode_array;
	struct ShortArray_r	*short_array;
	struct LongArray_r	*long_array;
	struct UI8Array_r	*ui8_array;
	uint16_t		prop_type, propID;
        int                     retval;
	uint32_t		saved_flags;

        for (i = 0; i < properties->cValues; i++) {
                if (retvals[i] == MAPI_E_SUCCESS) {
                        property = properties->aulPropTag[i];
			prop_type = property & 0xffff;

			ndr_push_uint32(ndr, NDR_SCALARS, property);
			if (property > 0x80000000) {
				propID = property >> 16;
				retval = mapistore_namedprops_get_nameid(nprops_ctx, propID, NULL, &nameid);
				if (retval != MAPISTORE_SUCCESS) {
					OC_DEBUG(0, "no definition found for named property with id '%.8x'\n", property);
					continue;
				}
				ndr_push_GUID(ndr, NDR_SCALARS, &nameid->lpguid);
				switch (nameid->ulKind) {
				case MNID_ID:
					ndr_push_uint8(ndr, NDR_SCALARS, 0);
					ndr_push_uint32(ndr, NDR_SCALARS, nameid->kind.lid);
					break;
				case MNID_STRING:
					saved_flags = ndr->flags;
					ndr_push_uint8(ndr, NDR_SCALARS, 1);
					ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
					ndr_push_string(ndr, NDR_SCALARS, nameid->kind.lpwstr.Name);
					ndr->flags = saved_flags;
					break;
				}
				talloc_free(nameid);
			}
			ndr_push_uint32(cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(cutmarks_ndr, NDR_SCALARS, ndr->offset);
			if ((prop_type & MV_FLAG)) {
				prop_type &= 0x0fff;

				switch (prop_type) {
				case PT_SHORT:
					short_array = data_pointers[i];
					ndr_push_uint32(ndr, NDR_SCALARS, short_array->cValues);
					for (j = 0; j < short_array->cValues; j++) {
						oxcfxics_ndr_push_simple_data(ndr, prop_type, short_array->lpi + j);
					}
					break;
				case PT_LONG:
					long_array = data_pointers[i];
					ndr_push_uint32(ndr, NDR_SCALARS, long_array->cValues);
					for (j = 0; j < long_array->cValues; j++) {
						oxcfxics_ndr_push_simple_data(ndr, prop_type, long_array->lpl + j);
					}
					break;
				case PT_I8:
					ui8_array = data_pointers[i];
					ndr_push_uint32(ndr, NDR_SCALARS, ui8_array->cValues);
					for (j = 0; j < ui8_array->cValues; j++) {
						oxcfxics_ndr_push_simple_data(ndr, prop_type, ui8_array->lpui8 + j);
					}
					break;
				case PT_BINARY:
					bin_array = data_pointers[i];
					ndr_push_uint32(ndr, NDR_SCALARS, bin_array->cValues);
					for (j = 0; j < bin_array->cValues; j++) {
						oxcfxics_ndr_push_simple_data(ndr, prop_type, bin_array->lpbin + j);
					}
					break;
				case PT_UNICODE:
					unicode_array = data_pointers[i];
					ndr_push_uint32(ndr, NDR_SCALARS, unicode_array->cValues);
					for (j = 0; j < unicode_array->cValues; j++) {
						oxcfxics_ndr_push_simple_data(ndr, prop_type, unicode_array->lppszW[j]);
					}
					break;
				default:
					OC_DEBUG(5, "no handling for multi values of type %.4x\n", prop_type);
					abort();
				}
			}
			else {
				oxcfxics_ndr_push_simple_data(ndr, prop_type, data_pointers[i]);
			}
			min_value_buffer = oxcfxics_compute_cutmark_min_value_buffer(prop_type);
			ndr_push_uint32(cutmarks_ndr, NDR_SCALARS, min_value_buffer);
			ndr_push_uint32(cutmarks_ndr, NDR_SCALARS, ndr->offset);
		}
        }

}

static int oxcfxics_fmid_from_source_key(struct emsmdbp_context *emsmdbp_ctx, const char *owner, struct SBinary_short *source_key, uint64_t *fmidp)
{
	uint64_t	fmid, base;
	uint16_t	replid;
	const uint8_t	*bytes;
	int		i;

	if (emsmdbp_guid_to_replid(emsmdbp_ctx, owner, (const struct GUID *) source_key->lpb, &replid)) {
		return MAPISTORE_ERROR;
	}

	bytes = source_key->lpb + 16;
	fmid = 0;
	base = 1;
	for (i = 0; i < 6; i++) {
		fmid |= (uint64_t) bytes[i] * base;
		base <<= 8;
	}
	fmid <<= 16;
	fmid |= replid;
	*fmidp = fmid;

	return MAPISTORE_SUCCESS;
}

static struct Binary_r *oxcfxics_make_xid(TALLOC_CTX *mem_ctx, struct GUID *replica_guid, uint64_t *id, uint8_t idlength)
{
	struct ndr_push	*ndr;
	struct Binary_r *data;
	uint8_t	i;
	uint64_t current_id;

	if (!mem_ctx) return NULL;
	if (!replica_guid || !id) return NULL;
	if (idlength > 8) return NULL;

	/* GUID */
	ndr = ndr_push_init_ctx(NULL);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;
	ndr_push_GUID(ndr, NDR_SCALARS, replica_guid);

	/* id */
	current_id = *id;
	for (i = 0; i < idlength; i++) {
		ndr_push_uint8(ndr, NDR_SCALARS, current_id & 0xff);
		current_id >>= 8;
	}

	data = talloc_zero(mem_ctx, struct Binary_r);
	data->cb = ndr->offset;
	data->lpb = ndr->data;
	(void) talloc_reference(data, ndr->data);
	talloc_free(ndr);

	return data;
}

static inline struct Binary_r *oxcfxics_make_gid(TALLOC_CTX *mem_ctx, struct GUID *replica_guid, uint64_t id)
{
	return oxcfxics_make_xid(mem_ctx, replica_guid, &id, 6);
}

/**
   \details EcDoRpc EcDoRpc_RopFastTransferSourceCopyTo (0x4d) Rop. This operation initializes a FastTransfer operation to download content from a given messaging object and its descendant subobjects.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the EcDoRpc_RopFastTransferSourceCopyTo EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the EcDoRpc_RopFastTransferSourceCopyTo EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFastTransferSourceCopyTo(TALLOC_CTX *mem_ctx,
							     struct emsmdbp_context *emsmdbp_ctx,
							     struct EcDoRpc_MAPI_REQ *mapi_req,
							     struct EcDoRpc_MAPI_REPL *mapi_repl,
							     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	struct mapi_handles			*parent_object_handle = NULL, *object_handle;
	struct emsmdbp_object			*parent_object = NULL, *object;
	struct FastTransferSourceCopyTo_req	 *request;
	uint32_t				parent_handle_id, i;
	void					*data;
	struct SPropTagArray			*needed_properties;
	void					**data_pointers;
	enum MAPISTATUS				*retvals;
	struct ndr_push				*ndr, *cutmarks_ndr;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] FastTransferSourceCopyTo (0x4d)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_FastTransferSourceCopyTo;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = request->handle_idx;

	/* Step 1. Retrieve object handle */
	parent_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, parent_handle_id, &parent_object_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", parent_handle_id, mapi_req->handle_idx);
		goto end;
	}

	/* Step 2. Check whether the parent object supports fetching properties */
	mapi_handles_get_private_data(parent_object_handle, &data);
	parent_object = (struct emsmdbp_object *) data;

	if (request->Level > 0) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  no support for levels > 0\n");
		goto end;
	}

	if (emsmdbp_object_get_available_properties(mem_ctx, emsmdbp_ctx, parent_object, &needed_properties) == MAPISTORE_SUCCESS) {
		if (needed_properties->cValues > 0) {
			for (i = 0; i < request->PropertyTags.cValues; i++) {
				SPropTagArray_delete(mem_ctx, needed_properties, request->PropertyTags.aulPropTag[i]);
			}

			data_pointers = emsmdbp_object_get_properties(mem_ctx, emsmdbp_ctx, parent_object, needed_properties, &retvals);
			if (data_pointers == NULL) {
				mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
				OC_DEBUG(5, "  unexpected error\n");
				goto end;
			}

			ndr = ndr_push_init_ctx(NULL);
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
			ndr->offset = 0;

			cutmarks_ndr = ndr_push_init_ctx(NULL);
			ndr_set_flags(&cutmarks_ndr->flags, LIBNDR_FLAG_NOALIGN);
			cutmarks_ndr->offset = 0;

			oxcfxics_ndr_push_properties(ndr, cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, needed_properties, data_pointers, retvals);

			retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, parent_handle_id, &object_handle);
			object = emsmdbp_object_ftcontext_init(object_handle, emsmdbp_ctx, parent_object);
			if (object == NULL) {
				mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
				OC_DEBUG(5, "  context object not created\n");
				goto end;
			}

			ndr_push_uint32(cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(cutmarks_ndr, NDR_SCALARS, 0xffffffff);

			(void) talloc_reference(object, ndr->data);
			(void) talloc_reference(object, cutmarks_ndr->data);

			object->object.ftcontext->cutmarks = (uint32_t *) cutmarks_ndr->data;
			object->object.ftcontext->stream.buffer.data = ndr->data;
			object->object.ftcontext->stream.buffer.length = ndr->offset;

			talloc_free(ndr);
			talloc_free(cutmarks_ndr);

			mapi_handles_set_private_data(object_handle, object);
			handles[mapi_repl->handle_idx] = object_handle->handle;

			talloc_free(data_pointers);
			talloc_free(retvals);
		}
	}

end:
	*size += libmapiserver_RopFastTransferSourceCopyTo_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

static void oxcfxics_push_messageChange_recipients(struct emsmdbp_context *emsmdbp_ctx, struct oxcfxics_sync_data *sync_data, struct emsmdbp_object *message_object, struct mapistore_message *msg)
{
	TALLOC_CTX				*local_mem_ctx;
	enum MAPISTATUS				*retvals;
	struct mapistore_message_recipient	*recipient;
	uint32_t				i, j, min_string_value_buffer;
	uint32_t				cn_idx = (uint32_t) -1, email_idx = (uint32_t) -1;

	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagFXDelProp);
	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagMessageRecipients);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

	min_string_value_buffer = oxcfxics_compute_cutmark_min_value_buffer(PT_UNICODE);

	if (msg) {
		local_mem_ctx = talloc_new(NULL);

		if (SPropTagArray_find(*msg->columns, PidTagDisplayName, &cn_idx) == MAPI_E_NOT_FOUND
		    && SPropTagArray_find(*msg->columns, PidTagAddressBookDisplayNamePrintable, &cn_idx) == MAPI_E_NOT_FOUND
		    && SPropTagArray_find(*msg->columns, PidTagRecipientDisplayName, &cn_idx) == MAPI_E_NOT_FOUND) {
			cn_idx = (uint32_t) -1;
		}
		if (SPropTagArray_find(*msg->columns, PidTagEmailAddress, &email_idx) == MAPI_E_NOT_FOUND
		    && SPropTagArray_find(*msg->columns, PidTagSmtpAddress, &email_idx) == MAPI_E_NOT_FOUND) {
			email_idx = (uint32_t) -1;
		}

		retvals = talloc_array(local_mem_ctx, enum MAPISTATUS, msg->columns->cValues);
		for (i = 0; i < msg->recipients_count; i++) {
			recipient = msg->recipients + i;

			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, StartRecip);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagRowid);
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, i);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

			if (email_idx != (uint32_t) -1 && recipient->data[email_idx]) {
				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagAddressType);
				oxcfxics_ndr_push_simple_data(sync_data->ndr, 0x1f, "SMTP");
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, min_string_value_buffer);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagEmailAddress);
				oxcfxics_ndr_push_simple_data(sync_data->ndr, 0x1f, recipient->data[email_idx]);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, min_string_value_buffer);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			}
			if (cn_idx != (uint32_t) -1 && recipient->data[cn_idx]) {
				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagDisplayName);
				oxcfxics_ndr_push_simple_data(sync_data->ndr, 0x1f, recipient->data[cn_idx]);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, min_string_value_buffer);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			}

			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagRecipientType);
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, recipient->type);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

			for (j = 0; j < msg->columns->cValues; j++) {
				if (recipient->data[j] == NULL) {
					retvals[j] = MAPI_E_NOT_FOUND;
				}
				else {
					retvals[j] = MAPI_E_SUCCESS;
				}
			}

			oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, msg->columns, recipient->data, retvals);
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, EndToRecip);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
		}

		talloc_free(local_mem_ctx);
	}
}

/* FIXME: attachment_object should be an emsmdbp_object but we lack time to create the struct */
static void oxcfxics_push_messageChange_attachment_embedded_message(struct emsmdbp_context *emsmdbp_ctx, uint32_t contextID, struct emsmdbp_object_synccontext *synccontext, struct oxcfxics_sync_data *sync_data, void *attachment)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error		ret;
        struct mapistore_message	*msg;
	void				*embedded_message;
	uint64_t			messageID;
	struct SPropTagArray		*properties;
	struct mapistore_property_data  *prop_data;
	void				**data_pointers;
	enum MAPISTATUS			*retvals;
	uint32_t			i;

	mem_ctx = talloc_new(NULL);

	ret = mapistore_message_attachment_open_embedded_message(emsmdbp_ctx->mstore_ctx, contextID, attachment, mem_ctx, &embedded_message, &messageID, &msg);
	if (ret == MAPISTORE_SUCCESS) {
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, StartEmbed);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);


		properties = &synccontext->properties;


		/* ret = mapistore_properties_get_available_properties(emsmdbp_ctx->mstore_ctx, contextID, embedded_message, mem_ctx, &available_properties); */
		prop_data = talloc_array(mem_ctx, struct mapistore_property_data, properties->cValues);
		memset(prop_data, 0, sizeof(struct mapistore_property_data) * properties->cValues);

		ret = mapistore_properties_get_properties(emsmdbp_ctx->mstore_ctx, contextID, embedded_message, mem_ctx, properties->cValues, properties->aulPropTag, prop_data);

		data_pointers = talloc_array(mem_ctx, void *, properties->cValues);
		retvals = talloc_array(mem_ctx, enum MAPISTATUS, properties->cValues);
		for (i = 0; i < properties->cValues; i++) {
			switch (prop_data[i].error) {
			case MAPISTORE_SUCCESS:
				if (prop_data[i].data) {
					data_pointers[i] = prop_data[i].data;
					retvals[i] = MAPI_E_SUCCESS;
				}
				else {
					retvals[i] = MAPI_E_NOT_FOUND;
				}
				break;
			default:
				retvals[i] = MAPI_E_NOT_FOUND;
			}
		}
		oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, properties, data_pointers, retvals);
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagFXDelProp);
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagMessageRecipients);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagFXDelProp);
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagMessageAttachments);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, EndEmbed);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

		RAWIDSET_push_guid_glob(sync_data->eid_set, &sync_data->replica_guid, (messageID >> 16) & 0x0000ffffffffffff);
	}

	talloc_free(mem_ctx);
}

static void oxcfxics_push_messageChange_attachments(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object_synccontext *synccontext, struct oxcfxics_sync_data *sync_data, struct emsmdbp_object *message_object)
{
	TALLOC_CTX		*mem_ctx;
	struct emsmdbp_object	*table_object;
	static enum MAPITAGS	prop_tags[] = { PidTagAttachMethod, PidTagAttachTag, PidTagAttachSize, PidTagAttachEncoding, PidTagAttachFlags, PidTagAttachmentFlags, PidTagAttachmentHidden, PidTagAttachmentLinkId, PidTagAttachExtension, PidTagAttachFilename, PidTagAttachLongFilename, PidTagAttachContentId, PidTagAttachMimeTag, PidTagDisplayName, PidTagCreationTime, PidTagLastModificationTime, PidTagAttachDataBinary, PidTagAttachmentContactPhoto, PidTagRenderingPosition, PidTagRecordKey, PidTagExceptionStartTime, PidTagExceptionEndTime, PidTagExceptionReplaceTime };
	static const int	prop_count = sizeof(prop_tags) / sizeof (enum MAPITAGS);
	struct SPropTagArray	query_props;
	uint32_t		i, method, contextID;
	enum MAPISTATUS		*retvals;
	void			**data_pointers, *attachment_object;
	enum mapistore_error	ret;

	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagFXDelProp);
	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagMessageAttachments);

	table_object = emsmdbp_object_message_open_attachment_table(NULL, emsmdbp_ctx, message_object);
	if (table_object && table_object->object.table->denominator > 0) {
		table_object->object.table->properties = prop_tags;
		table_object->object.table->prop_count = prop_count;
		contextID = emsmdbp_get_contextID(table_object);
		if (emsmdbp_is_mapistore(table_object)) {
			ret = mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, contextID,
							  table_object->backend_object, prop_count, prop_tags);
			if (ret != MAPISTORE_SUCCESS) {
				OC_DEBUG(0, "table_set_columns failed with %s", mapistore_errstr(ret));
				talloc_free(table_object);
				return;
			}
		}
		for (i = 0; i < table_object->object.table->denominator; i++) {
			mem_ctx = talloc_zero(NULL, void);
			data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, table_object, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
			if (data_pointers) {
				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, NewAttach);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, PidTagAttachNumber);
				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, i);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
				query_props.cValues = prop_count;
				query_props.aulPropTag = prop_tags;
				oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, &query_props, data_pointers, (enum MAPISTATUS *) retvals);

				if (retvals[0] == MAPI_E_SUCCESS) {
					method = *((uint32_t *) data_pointers[0]);
					if (method == afEmbeddedMessage) {
						mapistore_message_open_attachment(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object, mem_ctx, i, &attachment_object);
						oxcfxics_push_messageChange_attachment_embedded_message(emsmdbp_ctx, contextID, synccontext, sync_data, attachment_object);
					}
				}

				ndr_push_uint32(sync_data->ndr, NDR_SCALARS, EndAttach);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
				ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			}
			else {
				OC_DEBUG(5, "no data returned for attachment row %d\n", i);
				abort();
			}
			talloc_free(mem_ctx);
		}
	}

	talloc_free(table_object);
}

static void oxcfxics_table_set_cn_restriction(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, const char *owner, struct idset *cnset_seen)
{
	struct mapi_SRestriction cn_restriction;
	struct idset *local_cnset;
	uint16_t repl_id;
	uint8_t state;

	if (!emsmdbp_is_mapistore(table_object)) {
		OC_DEBUG(5, "table restrictions not supported by non-mapistore tables\n");
		return;
	}

	local_cnset = cnset_seen;
	while (local_cnset && (emsmdbp_guid_to_replid(emsmdbp_ctx, owner, &local_cnset->repl.guid, &repl_id) != MAPI_E_SUCCESS || repl_id != 1)) {
		local_cnset = local_cnset->next;
	}

	if (!local_cnset) {
		OC_DEBUG(5, "no change set available -> no table restrictions\n");
		return;
	}
	if (local_cnset->range_count != 1) {
		OC_DEBUG(5, "no valid change set available (range_count = %d) -> no table restrictions\n", local_cnset->range_count);
		return;
	}

	cn_restriction.rt = RES_PROPERTY;
	cn_restriction.res.resProperty.relop = RELOP_GT;
	cn_restriction.res.resProperty.ulPropTag = PidTagChangeNumber;
	cn_restriction.res.resProperty.lpProp.ulPropTag = PidTagChangeNumber;
	cn_restriction.res.resProperty.lpProp.value.d = (cnset_seen->ranges[0].high << 16) | repl_id;

	mapistore_table_set_restrictions(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(table_object), table_object->backend_object, &cn_restriction, &state);
}

static bool oxcfxics_push_messageChange(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object_synccontext *synccontext, const char *owner, struct oxcfxics_sync_data *sync_data, struct emsmdbp_object *folder_object)
{
	TALLOC_CTX			*mem_ctx, *msg_ctx;
	bool				folder_is_mapistore, end_of_table, changed_prop_index = false;
	struct emsmdbp_object		*table_object, *message_object;
	uint32_t			i;
	static enum MAPITAGS		mid_property = PidTagMid;
	enum MAPISTATUS			*retvals, *header_retvals, retval_msg_class = MAPI_E_NOT_FOUND;
	void				**data_pointers, **header_data_pointers;
	struct FILETIME			*lm_time;
	NTTIME				nt_time;
	uint32_t			unix_time, contextID, message_class_prop_index;
	struct UI8Array_r		preload_mids;
	enum mapistore_table_type	mstore_type;
	struct SPropTagArray		query_props;
	struct Binary_r			predecessors_data;
	struct Binary_r			*bin_data;
	uint64_t			eid, cn;
	struct mapistore_message	*msg;
	struct GUID			replica_guid;
	struct idset			*original_cnset_seen;
	struct UI8Array_r		*deleted_eids;
	struct SPropTagArray		*msg_properties, *properties, *sharing_properties;
	struct oxcfxics_message_sync_data	*message_sync_data;
	struct SSortOrderSet		lpSortCriteria;
	uint8_t				status;
	struct oxcfxics_prop_index	msg_prop_index;

	mem_ctx = talloc_zero(NULL, void);

	if (sync_data->table_type == MAPISTORE_FAI_TABLE) {
		original_cnset_seen = synccontext->cnset_seen_fai;
		properties = &synccontext->fai_properties;
		mstore_type = MAPISTORE_FAI_TABLE;
	}
	else {
		original_cnset_seen = synccontext->cnset_seen;
		properties = &synccontext->properties;
		mstore_type = MAPISTORE_MESSAGE_TABLE;
	}

	msg_prop_index = sync_data->prop_index;

	if (sync_data->message_sync_data) {
		message_sync_data = sync_data->message_sync_data;
	}
	else {
		message_sync_data = talloc_zero(NULL, struct oxcfxics_message_sync_data);
		sync_data->message_sync_data = message_sync_data;

		/* we only push "messageChangeFull" since we don't handle property-based changes */
		/* messageChangeFull = IncrSyncChg messageChangeHeader IncrSyncMessage propList messageChildren */

		table_object = emsmdbp_folder_open_table(mem_ctx, folder_object, sync_data->table_type, 0);
		if (!table_object) {
			OC_DEBUG(5, "could not open folder table\n");
			abort();
		}

		table_object->object.table->prop_count = 1;
		table_object->object.table->properties = &mid_property;

		oxcfxics_table_set_cn_restriction(emsmdbp_ctx, table_object, owner, original_cnset_seen);
		message_sync_data->max = 0;
		if (emsmdbp_is_mapistore(table_object)) {
			contextID = emsmdbp_get_contextID(folder_object);
			mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, table_object->object.table->prop_count, table_object->object.table->properties);
			mapistore_table_get_row_count(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, MAPISTORE_PREFILTERED_QUERY, &table_object->object.table->denominator);
			synccontext->total_objects += table_object->object.table->denominator;

			OC_DEBUG(5, "push_messageChange: %d objects in table\n", table_object->object.table->denominator);
			/* fetch maching mids */
			message_sync_data->mids = talloc_array(message_sync_data, uint64_t, table_object->object.table->denominator);

			/* sort messages: most recent delivery time first when available */
			lpSortCriteria.cSorts = 1;
			lpSortCriteria.cCategories = 0;
			lpSortCriteria.cExpanded = 0;
			lpSortCriteria.aSort = talloc_array(mem_ctx, struct SSortOrder, 1);
			lpSortCriteria.aSort[0].ulPropTag = PidTagMessageDeliveryTime;
			lpSortCriteria.aSort[0].ulOrder = TABLE_SORT_DESCEND;
			mapistore_table_set_sort_order(emsmdbp_ctx->mstore_ctx, contextID,
						       table_object->backend_object,
						       &lpSortCriteria, &status);
			talloc_free(lpSortCriteria.aSort);

			for (i = 0; i < table_object->object.table->denominator; i++) {
				data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, table_object, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
				if (data_pointers) {
					if (retvals[0] == MAPI_E_SUCCESS) {
						message_sync_data->mids[message_sync_data->max] = *(uint64_t *) data_pointers[0];
						message_sync_data->max++;
					}
				}
			}
		}

		message_sync_data->count = 0;
	}

	folder_is_mapistore = emsmdbp_is_mapistore(folder_object);
	if (folder_is_mapistore) {
		contextID = emsmdbp_get_contextID(folder_object);
	}

	/* find where PidTagMessageClass is located */
	if (mstore_type == MAPISTORE_MESSAGE_TABLE) {
		retval_msg_class = SPropTagArray_find(*properties, PidTagMessageClass, &message_class_prop_index);
	}

	/* open each message and fetch properties */
	for (; sync_data->ndr->offset < max_message_sync_size && message_sync_data->count < message_sync_data->max; message_sync_data->count++) {
		msg_ctx = talloc_new(NULL);
		msg_properties = properties;

		if (folder_is_mapistore && (message_sync_data->count % message_preload_interval) == 0) {
			preload_mids.lpui8 = message_sync_data->mids + message_sync_data->count;
			if ((message_sync_data->count + message_preload_interval) < message_sync_data->max) {
				preload_mids.cValues = message_preload_interval;
			}
			else {
				preload_mids.cValues = message_sync_data->max - message_sync_data->count;
			}
			mapistore_folder_preload_message_bodies(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, mstore_type, &preload_mids);
		}

		eid = *(message_sync_data->mids + message_sync_data->count);
		if (eid == 0x7fffffffffffffffLL) {
			OC_DEBUG(0, "message without a valid eid\n");
			goto end_row;
		}

		if (emsmdbp_object_message_open(msg_ctx, emsmdbp_ctx, folder_object, folder_object->object.folder->folderID, eid, false, &message_object, &msg) != MAPISTORE_SUCCESS) {
			OC_DEBUG(5, "message '%.16"PRIx64"' could not be open, skipped\n", eid);
			goto end_row;
		}

		data_pointers = emsmdbp_object_get_properties(msg_ctx, emsmdbp_ctx, message_object, msg_properties, &retvals);
		if (!data_pointers) {
			OC_DEBUG(5, "message '%.16"PRIx64"' returned no value, skipped\n", eid);
			goto end_row;
		}
                /* Check if the message is a sharing object message to
                   retrieve additional sharing properties for this
                   message. This could be extensible and more
                   generic in the future. */
		if (retval_msg_class == MAPI_E_SUCCESS && retvals[message_class_prop_index] == MAPI_E_SUCCESS
		    && strncmp((char *)data_pointers[message_class_prop_index], "IPM.Sharing", strlen("IPM.Sharing")) == 0) {
			/* Get available properties from this specific message */
			if (emsmdbp_object_get_available_properties(msg_ctx, emsmdbp_ctx, message_object,
								    &sharing_properties) == MAPISTORE_SUCCESS) {
				msg_properties = sharing_properties;
				data_pointers = emsmdbp_object_get_properties(msg_ctx, emsmdbp_ctx,
									      message_object, msg_properties, &retvals);
				if (!data_pointers) {
					OC_DEBUG(5, "message '%.16"PRIx64"' returned no value, skipped\n", eid);
					goto end_row;
				}
				/* Update prop index for fixed header props */
				changed_prop_index = true;
				SPropTagArray_find(*msg_properties, PidTagMid, &msg_prop_index.eid);
				SPropTagArray_find(*msg_properties, PidTagChangeNumber, &msg_prop_index.change_number);
				SPropTagArray_find(*msg_properties, PidTagChangeKey, &msg_prop_index.change_key);
				SPropTagArray_find(*msg_properties, PidTagPredecessorChangeList, &msg_prop_index.predecessor_change_list);
				SPropTagArray_find(*msg_properties, PidTagLastModificationTime, &msg_prop_index.last_modification_time);
				SPropTagArray_find(*msg_properties, PidTagAssociated, &msg_prop_index.associated);
				SPropTagArray_find(*msg_properties, PidTagMessageSize, &msg_prop_index.message_size);
			}
		}


		oxcfxics_ndr_check(sync_data->ndr, "sync_data->ndr");
		oxcfxics_ndr_check(sync_data->cutmarks_ndr, "sync_data->cutmarks_ndr");

		/** fixed header props */
		header_data_pointers = talloc_array(data_pointers, void *, 9);
		header_retvals = talloc_array(header_data_pointers, enum MAPISTATUS, 9);
		memset(header_retvals, 0, 9 * sizeof(uint32_t));
		query_props.aulPropTag = talloc_array(header_data_pointers, enum MAPITAGS, 9);

		i = 0;

		/* source key */
		emsmdbp_replid_to_guid(emsmdbp_ctx, owner, eid & 0xffff, &replica_guid);
		RAWIDSET_push_guid_glob(sync_data->eid_set, &replica_guid, (eid >> 16) & 0x0000ffffffffffff);

		/* bin_data = oxcfxics_make_gid(header_data_pointers, &sync_data->replica_guid, eid >> 16); */
		emsmdbp_source_key_from_fmid(header_data_pointers, emsmdbp_ctx, owner, eid, &bin_data);
		query_props.aulPropTag[i] = PidTagSourceKey;
		header_data_pointers[i] = bin_data;
		i++;

		/* last modification time */
		if (retvals[msg_prop_index.last_modification_time]) {
			unix_time = oc_version_time;
			unix_to_nt_time(&nt_time, unix_time);
			lm_time = talloc_zero(header_data_pointers, struct FILETIME);
			lm_time->dwLowDateTime = (nt_time & 0xffffffff);
			lm_time->dwHighDateTime = nt_time >> 32;
		}
		else {
			lm_time = (struct FILETIME *) data_pointers[msg_prop_index.last_modification_time];
			nt_time = ((uint64_t) lm_time->dwHighDateTime << 32) | lm_time->dwLowDateTime;
			unix_time = nt_time_to_unix(nt_time);
		}
		query_props.aulPropTag[i] = PidTagLastModificationTime;
		header_data_pointers[i] = lm_time;
		i++;

		if (retvals[msg_prop_index.change_number]) {
			OC_DEBUG(5, "mandatory property PidTagChangeNumber not returned for message\n");
			abort();
		}
		cn = ((*(uint64_t *) data_pointers[msg_prop_index.change_number]) >> 16) & 0x0000ffffffffffff;
		if (IDSET_includes_guid_glob(original_cnset_seen, &sync_data->replica_guid, cn)) {
			synccontext->skipped_objects++;
			OC_DEBUG(5, "message changes: cn %.16"PRIx64" already present\n", cn);
			goto end_row;
		}
		/* The "cnset_seen" range is going to be merged later with the one from synccontext since the ids are not sorted */
		RAWIDSET_push_guid_glob(sync_data->cnset_seen, &sync_data->replica_guid, cn);

		/* change key */
		/* bin_data = oxcfxics_make_gid(header_data_pointers, &sync_data->replica_guid, cn); */
		if (retvals[msg_prop_index.change_key]) {
			OC_DEBUG(5, "mandatory property PidTagChangeKey not returned for message\n");
			abort();
		}
		query_props.aulPropTag[i] = PidTagChangeKey;
		bin_data = data_pointers[msg_prop_index.change_key];
		header_data_pointers[i] = bin_data;
		i++;

		/* predecessor change list */
		query_props.aulPropTag[i] = PidTagPredecessorChangeList;
		if (retvals[msg_prop_index.predecessor_change_list]) {
			OC_DEBUG(5, "mandatory property PidTagPredecessorChangeList not returned for message\n");
			/* abort(); */

			predecessors_data.cb = bin_data->cb + 1;
			predecessors_data.lpb = talloc_array(header_data_pointers, uint8_t, predecessors_data.cb);
			*predecessors_data.lpb = bin_data->cb & 0xff;
			memcpy(predecessors_data.lpb + 1, bin_data->lpb, bin_data->cb);
			header_data_pointers[i] = &predecessors_data;
		}
		else {
			bin_data = data_pointers[msg_prop_index.predecessor_change_list];
			header_data_pointers[i] = bin_data;
		}
		i++;

		/* associated (could be based on table type ) */
		query_props.aulPropTag[i] = PidTagAssociated;
		if (retvals[msg_prop_index.associated]) {
			header_data_pointers[i] = talloc_zero(header_data_pointers, uint8_t);
		}
		else {
			header_data_pointers[i] = data_pointers[msg_prop_index.associated];
		}
		i++;

		/* message id (conditional) */
		if (synccontext->request.request_eid) {
			query_props.aulPropTag[i] = PidTagMid;
			header_data_pointers[i] = &eid;
			i++;
		}

		/* message size (conditional) */
		if (synccontext->request.request_message_size) {
			query_props.aulPropTag[i] = PidTagMessageSize;
			header_data_pointers[i] = data_pointers[msg_prop_index.message_size];
			if (retvals[msg_prop_index.parent_fid]) {
				header_data_pointers[i] = talloc_zero(header_data_pointers, uint32_t);
			}
			else {
				header_data_pointers[i] = data_pointers[msg_prop_index.message_size];
			}
			i++;
		}

		/* cn (conditional) */
		if (synccontext->request.request_cn) {
			query_props.aulPropTag[i] = PidTagChangeNumber;
			header_data_pointers[i] = talloc_zero(header_data_pointers, uint64_t);
			*(uint64_t *) header_data_pointers[i] = (cn << 16) | (eid & 0xffff);
			i++;
		}

		query_props.cValues = i;

		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncChg);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
		oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, &query_props, header_data_pointers, (enum MAPISTATUS *) header_retvals);

		/** remaining props */
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncMessage);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

		/* we shift the number of remaining properties to the amount of properties explicitly requested in RopSyncConfigure that were used above */
		if (msg_properties->cValues > message_properties_shift) {
			query_props.cValues = msg_properties->cValues - message_properties_shift;
			query_props.aulPropTag = msg_properties->aulPropTag + message_properties_shift;
			oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, &query_props, data_pointers + message_properties_shift, (enum MAPISTATUS *) retvals + message_properties_shift);
		}

		/* messageChildren:
		   [ PidTagFXDelProp ] [ *(StartRecip propList EndToRecip) ] [ PidTagFXDelProp ] [ *(NewAttach propList [embeddedMessage] EndAttach) ]
		   embeddedMessage:
		   StartEmbed messageContent EndEmbed */

		oxcfxics_push_messageChange_recipients(emsmdbp_ctx, sync_data, message_object, msg);
		oxcfxics_push_messageChange_attachments(emsmdbp_ctx, synccontext, sync_data, message_object);

		synccontext->sent_objects++;
		if (changed_prop_index) {
			msg_prop_index = sync_data->prop_index;
			changed_prop_index = false;
		}
	end_row:
		talloc_free(msg_ctx);
	}

	if (sync_data->ndr->offset >= max_message_sync_size) {
		OC_DEBUG(5, "reached max sync size: %u >= %zu\n", sync_data->ndr->offset, max_message_sync_size);
	}

	if (message_sync_data->count < message_sync_data->max) {
		end_of_table = false;
		OC_DEBUG(5, "table status: count: %"PRId64", max: %"PRId64"\n", message_sync_data->count, message_sync_data->max);
	}
	else {
		/* fetch deleted ids */
		if (folder_is_mapistore) {
			if (original_cnset_seen && original_cnset_seen->range_count > 0) {
				cn = (original_cnset_seen->ranges[0].high << 16) | 0x0001;
			}
			else {
				cn = 0;
			}
			if (!mapistore_folder_get_deleted_fmids(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, mem_ctx, sync_data->table_type, cn, &deleted_eids, &cn)) {
				for (i = 0; i < deleted_eids->cValues; i++) {
					RAWIDSET_push_guid_glob(sync_data->deleted_eid_set, &sync_data->replica_guid, (deleted_eids->lpui8[i] >> 16) & 0x0000ffffffffffff);
				}
				if (deleted_eids->cValues > 0) {
					RAWIDSET_push_guid_glob(sync_data->cnset_seen, &sync_data->replica_guid, (cn >> 16) & 0x0000ffffffffffff);
				}
			}
			preload_mids.cValues = 0;
			mapistore_folder_preload_message_bodies(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, mstore_type, &preload_mids);
		}
		OC_DEBUG(5, "end of table reached: count: %"PRId64", max: %"PRId64"\n", message_sync_data->count, message_sync_data->max);
		talloc_free(message_sync_data);
		sync_data->message_sync_data = NULL;
		end_of_table = true;
	}

	talloc_free(mem_ctx);

	return end_of_table;
}

static void oxcfxics_fill_synccontext_with_messageChange(struct emsmdbp_object_synccontext *synccontext, TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, const char *owner, struct emsmdbp_object *parent_object)
{
	struct oxcfxics_sync_data	*sync_data;
	struct idset			*new_idset, *old_idset;
	
	/* contentsSync = [progressTotal] *( [progressPerMessage] messageChange ) [deletions] [readStateChanges] state IncrSyncEnd */

	if (synccontext->sync_stage == 0) {
		/* 1. we setup the mandatory properties indexes */
		sync_data = talloc_zero(NULL, struct oxcfxics_sync_data);
		openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, owner, NULL, &sync_data->replica_guid);
		SPropTagArray_find(synccontext->properties, PidTagMid, &sync_data->prop_index.eid);
		SPropTagArray_find(synccontext->properties, PidTagChangeNumber, &sync_data->prop_index.change_number);
		SPropTagArray_find(synccontext->properties, PidTagChangeKey, &sync_data->prop_index.change_key);
		SPropTagArray_find(synccontext->properties, PidTagPredecessorChangeList, &sync_data->prop_index.predecessor_change_list);
		SPropTagArray_find(synccontext->properties, PidTagLastModificationTime, &sync_data->prop_index.last_modification_time);
		SPropTagArray_find(synccontext->properties, PidTagAssociated, &sync_data->prop_index.associated);
		SPropTagArray_find(synccontext->properties, PidTagMessageSize, &sync_data->prop_index.message_size);

		sync_data->cnset_read = RAWIDSET_make(sync_data, false, true);
		sync_data->eid_set = RAWIDSET_make(sync_data, false, false);
		sync_data->deleted_eid_set = RAWIDSET_make(sync_data, false, false);

		synccontext->sync_data = sync_data;
		synccontext->sync_stage = 1;
	}
	else {
		sync_data = synccontext->sync_data;
		talloc_free(sync_data->ndr);
		talloc_free(sync_data->cutmarks_ndr);
	}
	sync_data->ndr = ndr_push_init_ctx(sync_data);
	ndr_set_flags(&sync_data->ndr->flags, LIBNDR_FLAG_NOALIGN);
	sync_data->ndr->offset = 0;
	sync_data->cutmarks_ndr = ndr_push_init_ctx(sync_data);
	ndr_set_flags(&sync_data->cutmarks_ndr->flags, LIBNDR_FLAG_NOALIGN);
	sync_data->cutmarks_ndr->offset = 0;

	if (synccontext->sync_stage == 1) {
		/* 2a. we build the message stream (normal messages) */
		if (synccontext->request.normal) {
			if (!sync_data->message_sync_data) {
				sync_data->cnset_seen = RAWIDSET_make(NULL, false, true);
				sync_data->table_type = MAPISTORE_MESSAGE_TABLE;
			}

			if (oxcfxics_push_messageChange(emsmdbp_ctx, synccontext, owner, sync_data, parent_object)) {
				new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->cnset_seen);
				old_idset = synccontext->cnset_seen;
				/* IDSET_dump (synccontext->cnset_seen, "initial cnset_seen"); */
				synccontext->cnset_seen = IDSET_merge_idsets(synccontext, old_idset, new_idset);
				/* IDSET_dump (synccontext->cnset_seen, "merged cnset_seen"); */
				talloc_free(old_idset);
				talloc_free(new_idset);
				talloc_free(sync_data->cnset_seen);
				synccontext->sync_stage = 2;
			}
		}
		else {
			synccontext->sync_stage = 2;
		}
	}

	if (synccontext->sync_stage == 2) {
		/* 2b. we build the message stream (FAI messages) */
		if (synccontext->request.fai) {
			if (!sync_data->message_sync_data) {
				sync_data->cnset_seen = RAWIDSET_make(NULL, false, true);
				sync_data->table_type = MAPISTORE_FAI_TABLE;
			}

			if (oxcfxics_push_messageChange(emsmdbp_ctx, synccontext, owner, sync_data, parent_object)) {
				new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->cnset_seen);
				old_idset = synccontext->cnset_seen_fai;
				/* IDSET_dump (synccontext->cnset_seen, "initial cnset_seen_fai"); */
				synccontext->cnset_seen_fai = IDSET_merge_idsets(synccontext, old_idset, new_idset);
				/* IDSET_dump (synccontext->cnset_seen, "merged cnset_seen_fai"); */
				talloc_free(old_idset);
				talloc_free(new_idset);
				talloc_free(sync_data->cnset_seen);
				synccontext->sync_stage = 3;
			}
		}
		else {
			synccontext->sync_stage = 3;
		}
	}

	if (synccontext->sync_stage == 3) {
		/* deletions */
		if (sync_data->deleted_eid_set->count > 0 && !synccontext->request.no_deletions) {
			IDSET_remove_rawidset(synccontext->idset_given, sync_data->deleted_eid_set);
			new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->deleted_eid_set);
			/* FIXME: we "convert" the idset hackishly */
			new_idset->idbased = true;
			new_idset->repl.id = 1;
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncDel);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagIdsetDeleted);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			ndr_push_idset(sync_data->ndr, new_idset);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			/* IDSET_dump (new_idset, "cnset_deleted"); */
			talloc_free(new_idset);
		}

		/* state */
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncStateBegin);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

		new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->eid_set);
		old_idset = synccontext->idset_given;
		/* IDSET_dump (synccontext->idset_given, "initial idset_given"); */
		synccontext->idset_given = IDSET_merge_idsets(synccontext, old_idset, new_idset);
		/* IDSET_dump (synccontext->idset_given, "merged idset_given"); */
		talloc_free(old_idset);
		talloc_free(new_idset);

		IDSET_dump (synccontext->cnset_seen, "cnset_seen");
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagCnsetSeen);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
		ndr_push_idset(sync_data->ndr, synccontext->cnset_seen);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

		if (synccontext->request.fai) {
			IDSET_dump (synccontext->cnset_seen_fai, "cnset_seen_fai");
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagCnsetSeenFAI);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			ndr_push_idset(sync_data->ndr, synccontext->cnset_seen_fai);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
		}
		IDSET_dump (synccontext->idset_given, "idset_given");
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagIdsetGiven);
		ndr_push_idset(sync_data->ndr, synccontext->idset_given);
		if (synccontext->request.read_state) {
			IDSET_dump (synccontext->cnset_read, "cnset_read");
			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagCnsetRead);
			ndr_push_idset(sync_data->ndr, synccontext->cnset_read);
		}
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncStateEnd);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

		/* end of stream */
		ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncEnd);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
		ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

		synccontext->sync_stage = 4;
	}

	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0xffffffff);

	synccontext->cutmarks = (uint32_t *) sync_data->cutmarks_ndr->data;
	synccontext->next_cutmark_idx = 1;
	synccontext->stream.position = 0;
	synccontext->stream.buffer.data = sync_data->ndr->data;
	synccontext->stream.buffer.length = sync_data->ndr->offset;

	if (synccontext->sync_stage == 4) {
		(void) talloc_reference(synccontext, sync_data->ndr->data);
		(void) talloc_reference(synccontext, sync_data->cutmarks_ndr->data);
		talloc_free(sync_data);
		synccontext->sync_data = NULL;
	}
}

static void oxcfxics_push_folderChange(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object_synccontext *synccontext, const char *owner, struct emsmdbp_object *topmost_folder_object, struct oxcfxics_sync_data *sync_data, struct emsmdbp_object *folder_object)
{
	TALLOC_CTX		*mem_ctx;
	struct emsmdbp_object	*table_object, *subfolder_object;
	uint64_t		eid, cn;
	struct Binary_r		predecessors_data;
	struct Binary_r		*bin_data;
	struct FILETIME		*lm_time;
	NTTIME			nt_time;
	int32_t			unix_time, contextID;
	uint32_t		i, j;
	enum MAPISTATUS		*retvals, *header_retvals;
	enum mapistore_error	retval;
	void			**data_pointers, **header_data_pointers;
	struct SPropTagArray	query_props;
	struct GUID		replica_guid;

	mem_ctx = talloc_zero(NULL, void);

	contextID = emsmdbp_get_contextID(folder_object);

	/* 2b. we build the stream */
	table_object = emsmdbp_folder_open_table(mem_ctx, folder_object, MAPISTORE_FOLDER_TABLE, 0); 
	if (!table_object) {
		OC_DEBUG(5, "folder does not handle hierarchy tables\n");
		return;
	}

	table_object->object.table->prop_count = synccontext->properties.cValues;
	table_object->object.table->properties = synccontext->properties.aulPropTag;
	oxcfxics_table_set_cn_restriction(emsmdbp_ctx, table_object, owner, synccontext->cnset_seen);
	if (emsmdbp_is_mapistore(table_object)) {
		mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, contextID,
					    table_object->backend_object, synccontext->properties.cValues, synccontext->properties.aulPropTag);
		mapistore_table_get_row_count(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, MAPISTORE_PREFILTERED_QUERY, &table_object->object.table->denominator);
		synccontext->total_objects += table_object->object.table->denominator;
	}

	for (i = 0; i < table_object->object.table->denominator; i++) {
		data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, table_object, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
		if (data_pointers) {
			/** fixed header props */
			header_data_pointers = talloc_array(NULL, void *, 8);
			header_retvals = talloc_array(header_data_pointers, enum MAPISTATUS, 8);
			memset(header_retvals, 0, 8 * sizeof(uint32_t));
			query_props.aulPropTag = talloc_array(header_data_pointers, enum MAPITAGS, 8);

			j = 0;

			/* parent source key */
			if (folder_object == topmost_folder_object) {
				/* No parent source key at the first hierarchy level */
				bin_data = talloc_zero(header_data_pointers, struct Binary_r);
				bin_data->lpb = (uint8_t *) "";
			}
			else {
				emsmdbp_source_key_from_fmid(header_data_pointers, emsmdbp_ctx, owner, *(uint64_t *) data_pointers[sync_data->prop_index.parent_fid], &bin_data);
			}
			query_props.aulPropTag[j] = PidTagParentSourceKey;
			header_data_pointers[j] = bin_data;
			j++;

			/* source key */
			eid = *(uint64_t *) data_pointers[sync_data->prop_index.eid];
			if (eid == 0x7fffffffffffffffLL) {
				OC_DEBUG(0, "folder without a valid eid\n");
				talloc_free(header_data_pointers);
				continue;
			}
			emsmdbp_replid_to_guid(emsmdbp_ctx, owner, eid & 0xffff, &replica_guid);
			RAWIDSET_push_guid_glob(sync_data->eid_set, &replica_guid, (eid >> 16) & 0x0000ffffffffffff);

			/* bin_data = oxcfxics_make_gid(header_data_pointers, &sync_data->replica_guid, eid >> 16); */
			emsmdbp_source_key_from_fmid(header_data_pointers, emsmdbp_ctx, owner, eid, &bin_data);
			query_props.aulPropTag[j] = PidTagSourceKey;
			header_data_pointers[j] = bin_data;
			j++;

			/* last modification time */
			if (retvals[sync_data->prop_index.last_modification_time]) {
				unix_time = oc_version_time;
				unix_to_nt_time(&nt_time, unix_time);
				lm_time = talloc_zero(header_data_pointers, struct FILETIME);
				lm_time->dwLowDateTime = (nt_time & 0xffffffff);
				lm_time->dwHighDateTime = nt_time >> 32;
			}
			else {
				lm_time = (struct FILETIME *) data_pointers[sync_data->prop_index.last_modification_time];
				nt_time = ((uint64_t) lm_time->dwHighDateTime << 32) | lm_time->dwLowDateTime;
				unix_time = nt_time_to_unix(nt_time);
			}
			query_props.aulPropTag[j] = PidTagLastModificationTime;
			header_data_pointers[j] = lm_time;
			j++;

			if (retvals[sync_data->prop_index.change_number]) {
				OC_DEBUG(5, "mandatory property PidTagChangeNumber not returned for folder\n");
				abort();
			}
			cn = ((*(uint64_t *) data_pointers[sync_data->prop_index.change_number]) >> 16) & 0x0000ffffffffffff;
			if (IDSET_includes_guid_glob(synccontext->cnset_seen, &sync_data->replica_guid, cn)) {
				synccontext->skipped_objects++;
				OC_DEBUG(5, "folder changes: cn %.16"PRIx64" already present\n", cn);
				if (retvals[sync_data->prop_index.change_key] == MAPI_E_SUCCESS) {
					goto end_row;
				}
			}
			RAWIDSET_push_guid_glob(sync_data->cnset_seen, &sync_data->replica_guid, cn);

			/* change key */
			/* work-around an issue in SOGo that generates an empty predecessor change list blob */
			if ((retvals[sync_data->prop_index.change_key] != MAPI_E_SUCCESS) ||
			    ((retvals[sync_data->prop_index.change_key] == MAPI_E_SUCCESS) &&
			     (retvals[sync_data->prop_index.predecessor_change_list] != MAPI_E_SUCCESS))) {
				bin_data = oxcfxics_make_gid(header_data_pointers, &sync_data->replica_guid, cn);
			} else {
				bin_data = data_pointers[sync_data->prop_index.change_key];
			}


			query_props.aulPropTag[j] = PidTagChangeKey;
			header_data_pointers[j] = bin_data;
			j++;

			/* predecessor... (already computed) */
			query_props.aulPropTag[j] = PidTagPredecessorChangeList;
			if (retvals[sync_data->prop_index.predecessor_change_list] != MAPI_E_SUCCESS) {
				predecessors_data.cb = bin_data->cb + 1;
				predecessors_data.lpb = talloc_array(header_data_pointers, uint8_t, predecessors_data.cb);
				*predecessors_data.lpb = bin_data->cb & 0xff;
				memcpy(predecessors_data.lpb + 1, bin_data->lpb, bin_data->cb);
				header_data_pointers[j] = &predecessors_data;
			}
			else {
				bin_data = data_pointers[sync_data->prop_index.predecessor_change_list];
				header_data_pointers[j] = bin_data;
			}
			j++;
					
			/* display name */
			query_props.aulPropTag[j] = PidTagDisplayName;
			if (retvals[sync_data->prop_index.display_name]) {
				header_data_pointers[j] = "";
			}
			else {
				header_data_pointers[j] = data_pointers[sync_data->prop_index.display_name];
			}
			j++;
			
			/* folder id (conditional) */
			if (synccontext->request.request_eid) {
				query_props.aulPropTag[j] = PidTagFolderId;
				header_data_pointers[j] = data_pointers[sync_data->prop_index.eid];
				j++;
			}

			/* parent folder id (conditional) */
			if (synccontext->request.no_foreign_identifiers) {
				query_props.aulPropTag[j] = PidTagParentFolderId;
				header_data_pointers[j] = data_pointers[sync_data->prop_index.parent_fid];
				if (retvals[sync_data->prop_index.parent_fid]) {
					header_data_pointers[j] = talloc_zero(header_data_pointers, uint64_t);
				}
				else {
					header_data_pointers[j] = data_pointers[sync_data->prop_index.parent_fid];
				}
				j++;
			}
			
			query_props.cValues = j;

			ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncChg);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
			ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
			oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, &query_props, header_data_pointers, (enum MAPISTATUS *) header_retvals);

			/** remaining props */
			if (table_object->object.table->prop_count > folder_properties_shift) {
				query_props.cValues = table_object->object.table->prop_count - folder_properties_shift;
				query_props.aulPropTag = table_object->object.table->properties + folder_properties_shift;
				oxcfxics_ndr_push_properties(sync_data->ndr, sync_data->cutmarks_ndr, emsmdbp_ctx->mstore_ctx->nprops_ctx, &query_props, data_pointers + folder_properties_shift, (enum MAPISTATUS *) retvals + folder_properties_shift);
			}

			synccontext->sent_objects++;
		end_row:
			talloc_free(header_data_pointers);
			talloc_free(data_pointers);
			talloc_free(retvals);

			retval = emsmdbp_object_open_folder(NULL, emsmdbp_ctx, folder_object, eid, &subfolder_object);
			if (retval == MAPISTORE_SUCCESS) {
				oxcfxics_push_folderChange(emsmdbp_ctx, synccontext, owner, topmost_folder_object, sync_data, subfolder_object);
				talloc_free(subfolder_object);
			} else {
				OC_DEBUG(5, "[oxcfxics] Fail open folder %"PRIu64" from parent folder %"PRIu64" (retval %d)", eid, folder_object->object.folder->folderID, retval);
			}
		}
	}

	talloc_free(mem_ctx);
}

static void oxcfxics_prepare_synccontext_with_folderChange(struct emsmdbp_object_synccontext *synccontext, TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, const char *owner, struct emsmdbp_object *parent_object)
{
	struct oxcfxics_sync_data		*sync_data;
	struct idset				*new_idset, *old_idset;

	/* 1b. we setup context data */
	sync_data = talloc_zero(NULL, struct oxcfxics_sync_data);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, owner, NULL, &sync_data->replica_guid);
	SPropTagArray_find(synccontext->properties, PidTagParentFolderId, &sync_data->prop_index.parent_fid);
	SPropTagArray_find(synccontext->properties, PidTagFolderId, &sync_data->prop_index.eid);
	SPropTagArray_find(synccontext->properties, PidTagChangeNumber, &sync_data->prop_index.change_number);
	SPropTagArray_find(synccontext->properties, PidTagChangeKey, &sync_data->prop_index.change_key);
	SPropTagArray_find(synccontext->properties, PidTagPredecessorChangeList, &sync_data->prop_index.predecessor_change_list);
	SPropTagArray_find(synccontext->properties, PidTagLastModificationTime, &sync_data->prop_index.last_modification_time);
	SPropTagArray_find(synccontext->properties, PidTagDisplayName, &sync_data->prop_index.display_name);
	sync_data->ndr = ndr_push_init_ctx(sync_data);
	ndr_set_flags(&sync_data->ndr->flags, LIBNDR_FLAG_NOALIGN);
	sync_data->ndr->offset = 0;
	sync_data->cutmarks_ndr = ndr_push_init_ctx(sync_data);
	ndr_set_flags(&sync_data->cutmarks_ndr->flags, LIBNDR_FLAG_NOALIGN);
	sync_data->cutmarks_ndr->offset = 0;
	sync_data->cnset_seen = RAWIDSET_make(sync_data, false, true);
	sync_data->eid_set = RAWIDSET_make(sync_data, false, false);

	oxcfxics_push_folderChange(emsmdbp_ctx, synccontext, owner, parent_object, sync_data, parent_object);

	/* deletions (mapistore v2) */

	/* state */
	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncStateBegin);

	new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->cnset_seen);
	old_idset = synccontext->cnset_seen;
	/* IDSET_dump (synccontext->cnset_seen, "initial cnset_seen (folder change)"); */
	synccontext->cnset_seen = IDSET_merge_idsets(synccontext, old_idset, new_idset);
	/* IDSET_dump (synccontext->cnset_seen, "merged cnset_seen (folder change)"); */
	talloc_free(old_idset);
	talloc_free(new_idset);

	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagCnsetSeen);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
	ndr_push_idset(sync_data->ndr, synccontext->cnset_seen);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

	new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->eid_set);
	old_idset = synccontext->idset_given;
	/* IDSET_dump (synccontext->idset_given, "initial idset_given (folder change)"); */
	synccontext->idset_given = IDSET_merge_idsets(synccontext, old_idset, new_idset);
	/* IDSET_dump (synccontext->idset_given, "merged idset_given (folder change)"); */
	talloc_free(old_idset);
	talloc_free(new_idset);

	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, MetaTagIdsetGiven);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
	ndr_push_idset(sync_data->ndr, synccontext->idset_given);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncStateEnd);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);

	/* end of stream */
	ndr_push_uint32(sync_data->ndr, NDR_SCALARS, IncrSyncEnd);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, sync_data->ndr->offset);
	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0);

	ndr_push_uint32(sync_data->cutmarks_ndr, NDR_SCALARS, 0xffffffff);

	synccontext->cutmarks = (uint32_t *) sync_data->cutmarks_ndr->data;
	synccontext->next_cutmark_idx = 1;
	synccontext->stream.position = 0;
	synccontext->stream.buffer.data = sync_data->ndr->data;
	synccontext->stream.buffer.length = sync_data->ndr->offset;

	(void) talloc_reference(synccontext, sync_data->ndr->data);
	(void) talloc_reference(synccontext, sync_data->cutmarks_ndr->data);
	talloc_free(sync_data);
}

static inline void oxcfxics_fill_ftcontext_fasttransfer_response(struct FastTransferSourceGetBuffer_repl *response, uint32_t request_buffer_size, TALLOC_CTX *mem_ctx, struct emsmdbp_object_ftcontext *ftcontext, struct emsmdbp_context *emsmdbp_ctx)
{
	uint32_t buffer_size, min_value_buffer, mark_idx, max_cutmark;

	buffer_size = request_buffer_size;

	if (ftcontext->stream.position == 0) {
		ftcontext->steps = 0;
		ftcontext->total_steps = (ftcontext->stream.buffer.length / request_buffer_size) + 1;
		ftcontext->next_cutmark_idx = 1;
		oxcfxics_check_cutmark_buffer(ftcontext->cutmarks, &ftcontext->stream.buffer);
		OC_DEBUG(5, "fast transfer buffer is %d bytes long\n", (uint32_t) ftcontext->stream.buffer.length);
	}
	ftcontext->steps += 1;

	if (ftcontext->stream.position + request_buffer_size < ftcontext->stream.buffer.length) {
		mark_idx = ftcontext->next_cutmark_idx;
		max_cutmark = ftcontext->stream.position + request_buffer_size;
		/* FIXME: cutmark lookups would be faster using a binary search */
		while (ftcontext->cutmarks[mark_idx] != 0xffffffff && ftcontext->cutmarks[mark_idx] < max_cutmark) {
			buffer_size = ftcontext->cutmarks[mark_idx] - ftcontext->stream.position;
			mark_idx += 2;
		}
		if (buffer_size < request_buffer_size && ftcontext->cutmarks[mark_idx] != 0xffffffff) {
			min_value_buffer = ftcontext->cutmarks[mark_idx-1];
			if (min_value_buffer && (request_buffer_size - buffer_size > min_value_buffer)) {
				buffer_size = request_buffer_size;
			}
		}
		ftcontext->next_cutmark_idx = mark_idx;
	}
	
	response->TransferBuffer = emsmdbp_stream_read_buffer(&ftcontext->stream, buffer_size);
	response->TotalStepCount = ftcontext->total_steps;
	if (ftcontext->stream.position == ftcontext->stream.buffer.length) {
		response->TransferStatus = TransferStatus_Done;
		response->InProgressCount = response->TotalStepCount;
	}
	else {
		response->TransferStatus = TransferStatus_Partial;
		response->InProgressCount = ftcontext->steps;
	}
}

static uint32_t oxcfxics_advance_cutmarks(struct emsmdbp_object_synccontext *synccontext, uint32_t request_buffer_size)
{
	uint32_t buffer_size, min_value_buffer, mark_idx, max_cutmark;

	buffer_size = request_buffer_size;

	mark_idx = synccontext->next_cutmark_idx;
	max_cutmark = synccontext->stream.position + request_buffer_size;
	/* FIXME: cutmark lookups would be faster using a binary search */
	while (synccontext->cutmarks[mark_idx] != 0xffffffff && synccontext->cutmarks[mark_idx] < max_cutmark) {
		buffer_size = synccontext->cutmarks[mark_idx] - synccontext->stream.position;
		mark_idx += 2;
	}
	if (buffer_size < request_buffer_size && synccontext->cutmarks[mark_idx] != 0xffffffff) {
		min_value_buffer = synccontext->cutmarks[mark_idx-1];
		if (min_value_buffer && (request_buffer_size - buffer_size > min_value_buffer)) {
			buffer_size = request_buffer_size;
		}
	}
	synccontext->next_cutmark_idx = mark_idx;

	return buffer_size;
}


static inline void oxcfxics_fill_synccontext_fasttransfer_response(struct FastTransferSourceGetBuffer_repl *response, uint32_t request_buffer_size, TALLOC_CTX *mem_ctx, struct emsmdbp_object_synccontext *synccontext, struct emsmdbp_object *parent_object)
{
	char		*owner;
	size_t		old_chunk_size, new_chunk_size;
	uint32_t	buffer_size;
	bool		end_of_buffer = false;
	DATA_BLOB	joint_buffer;

	owner = emsmdbp_get_owner(parent_object);

	OC_DEBUG(5, "start syncstream: position = %zu, size = %zu\n", synccontext->stream.position, synccontext->stream.buffer.length);
	if (synccontext->stream.position + request_buffer_size < synccontext->stream.buffer.length) {
		/* the current chunk has not been "emptied" yet */
		buffer_size = oxcfxics_advance_cutmarks(synccontext, request_buffer_size);
		response->TransferBuffer = emsmdbp_stream_read_buffer(&synccontext->stream, buffer_size);
	}
	else {
		/* the current chunk not does exist or we reached its end */
		if (synccontext->request.contents_mode) {
			OC_DEBUG(5, "content mode, stage %d\n", synccontext->sync_stage);
			if (synccontext->sync_stage == 4) {
				/* the last chunk was the last one */
				end_of_buffer = true;
				response->TransferBuffer = emsmdbp_stream_read_buffer(&synccontext->stream, request_buffer_size);
			}
			else if (synccontext->sync_stage == 0) {
				/* no chunk sent yet, so we create a new one */
				oxcfxics_fill_synccontext_with_messageChange(synccontext, mem_ctx, parent_object->emsmdbp_ctx, owner, parent_object);
				oxcfxics_check_cutmark_buffer(synccontext->cutmarks, &synccontext->stream.buffer);
				if (request_buffer_size < synccontext->stream.buffer.length) {
					buffer_size = oxcfxics_advance_cutmarks(synccontext, request_buffer_size);
				}
				else {
					buffer_size = request_buffer_size;
					if (synccontext->sync_stage == 4) {
						end_of_buffer = true;
					}
					else {
						abort();
					}
				}
				response->TransferBuffer = emsmdbp_stream_read_buffer(&synccontext->stream, buffer_size);
			}
			else {
				/* we have reached the end of a middle chunk, we must thus finish it and complete the buffer with the content of the next chunk */
				old_chunk_size = synccontext->stream.buffer.length - synccontext->stream.position;

				if (old_chunk_size > 0) {
					joint_buffer.length = old_chunk_size;
					joint_buffer.data = talloc_memdup(mem_ctx, synccontext->stream.buffer.data + synccontext->stream.position, joint_buffer.length);
				}

				oxcfxics_fill_synccontext_with_messageChange(synccontext, mem_ctx, parent_object->emsmdbp_ctx, owner, parent_object);
				oxcfxics_check_cutmark_buffer(synccontext->cutmarks, &synccontext->stream.buffer);

				new_chunk_size = request_buffer_size - old_chunk_size;
				if (synccontext->stream.buffer.length < new_chunk_size) {
					new_chunk_size = synccontext->stream.buffer.length;
					if (synccontext->sync_stage == 4) {
						end_of_buffer = true;
					}
					else {
						abort();
					}
				}
				else {
					new_chunk_size = oxcfxics_advance_cutmarks(synccontext, new_chunk_size);
				}

				if (new_chunk_size > 0) {
					if (old_chunk_size) {
						joint_buffer.length += new_chunk_size;
						joint_buffer.data = talloc_realloc(mem_ctx, joint_buffer.data, uint8_t, joint_buffer.length);
						memcpy(joint_buffer.data + old_chunk_size, synccontext->stream.buffer.data, new_chunk_size);
						synccontext->stream.position += new_chunk_size;
					}
					else {
						joint_buffer = emsmdbp_stream_read_buffer(&synccontext->stream, new_chunk_size);
					}
				}
				response->TransferBuffer = joint_buffer;


				OC_DEBUG(5, "joint buffers of sizes %zu and %zu\n", old_chunk_size, new_chunk_size);
			}
		}
		else {
			buffer_size = request_buffer_size;
			if (synccontext->stream.buffer.data) {
				end_of_buffer = true;
			}
			else {
				oxcfxics_prepare_synccontext_with_folderChange(synccontext, mem_ctx, parent_object->emsmdbp_ctx, owner, parent_object);
				oxcfxics_check_cutmark_buffer(synccontext->cutmarks, &synccontext->stream.buffer);
				OC_DEBUG(5, "synccontext buffer is %u bytes long\n", (uint32_t) synccontext->stream.buffer.length);
			}
			response->TransferBuffer = emsmdbp_stream_read_buffer(&synccontext->stream, buffer_size);

			if (synccontext->stream.position == synccontext->stream.buffer.length) {
				end_of_buffer = true;
			}
		}
	}

	response->TotalStepCount = synccontext->total_steps;
	if (end_of_buffer) {
		response->TransferStatus = TransferStatus_Done;
		response->InProgressCount = response->TotalStepCount;
	}
	else {
		response->TransferStatus = TransferStatus_Partial;
		response->InProgressCount = synccontext->steps;
	}
	OC_DEBUG(5, "  end syncstream: position = %zu, size = %zu", synccontext->stream.position, synccontext->stream.buffer.length);
}


/**
   \details EcDoRpc EcDoRpc_RopFastTransferSourceGetBuffer (0x4e) Rop. This operation downloads the next portion of a FastTransfer stream that is produced by a previously configured download operation.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the FastTransferSourceGetBuffer EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the FastTransferSourceGetBuffer EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFastTransferSourceGetBuffer(TALLOC_CTX *mem_ctx,
								struct emsmdbp_context *emsmdbp_ctx,
								struct EcDoRpc_MAPI_REQ *mapi_req,
								struct EcDoRpc_MAPI_REPL *mapi_repl,
								uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	uint32_t				handle_id;
	struct mapi_handles			*object_handle = NULL;
	struct emsmdbp_object			*object = NULL;
	struct FastTransferSourceGetBuffer_req	 *request;
	struct FastTransferSourceGetBuffer_repl	 *response;
	uint32_t				request_buffer_size;
	void					*data;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] FastTransferSourceGetBuffer (0x4e)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve object handle */
	handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle_id, &object_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle_id, mapi_req->handle_idx);
		goto end;
	}

	/* Step 2. Check whether the parent object supports fetching properties */
	mapi_handles_get_private_data(object_handle, &data);
	object = (struct emsmdbp_object *) data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object not found\n");
		goto end;
	}

	request = &mapi_req->u.mapi_FastTransferSourceGetBuffer;
	response = &mapi_repl->u.mapi_FastTransferSourceGetBuffer;

	request_buffer_size = request->BufferSize;
	if (request_buffer_size == 0xBABE) {
		request_buffer_size = request->MaximumBufferSize.MaximumBufferSize;
	}

	/* Step 3. Perform the read operation */
	switch (object->type) {
	case EMSMDBP_OBJECT_FTCONTEXT:
		oxcfxics_fill_ftcontext_fasttransfer_response(response, request_buffer_size, mem_ctx, object->object.ftcontext, emsmdbp_ctx);
		break;
	case EMSMDBP_OBJECT_SYNCCONTEXT:
		oxcfxics_fill_synccontext_fasttransfer_response(response, request_buffer_size, mem_ctx, object->object.synccontext, object->parent_object);
		break;
	default:
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  object type %d not supported\n", object->type);
		goto end;
	}

	response->TransferBufferSize = response->TransferBuffer.length;
	response->Reserved = 0;

end:
	*size += libmapiserver_RopFastTransferSourceGetBuffer_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncConfigure (0x70) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncConfigure EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncConfigure EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncConfigure(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	struct SyncConfigure_req		*request;
	uint32_t				folder_handle;
	struct mapi_handles			*folder_rec;
	struct mapi_handles			*synccontext_rec;
	struct emsmdbp_object			*folder_object;
	struct emsmdbp_object			*synccontext_object;
	struct emsmdbp_object			*table_object;
        struct emsmdbp_object_synccontext	*synccontext;
	enum MAPISTATUS				retval;
	bool					*properties_exclusion;
	bool					include_props;
	uint16_t				i, j;
	void					*data = NULL;
	struct SPropTagArray			*available_properties;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncConfigure (0x70)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_SyncConfigure;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
        mapi_repl->handle_idx = request->handle_idx;

	folder_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, folder_handle, &folder_rec);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", folder_handle, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(folder_rec, &data);
	folder_object = (struct emsmdbp_object *)data;
	if (!folder_object || folder_object->type != EMSMDBP_OBJECT_FOLDER) {
		OC_DEBUG(5, "  object not found or not a folder\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

        synccontext_object = emsmdbp_object_synccontext_init(NULL, emsmdbp_ctx, folder_object);
        synccontext = synccontext_object->object.synccontext;

	gettimeofday(&synccontext->request_start, NULL);

	/* SynchronizationType */
	synccontext->request.is_collector = false;
	synccontext->request.contents_mode = (request->SynchronizationType == Contents);

	/* SendOptions */
	synccontext->request.unicode = (request->SendOptions & FastTransfer_Unicode);
	synccontext->request.use_cpid = (request->SendOptions & FastTransfer_UseCpid);
	synccontext->request.recover_mode = (request->SendOptions & FastTransfer_RecoverMode);
	synccontext->request.force_unicode = (request->SendOptions & FastTransfer_ForceUnicode);
	synccontext->request.partial_item = (request->SendOptions & FastTransfer_PartialItem);

	/* SynchronizationFlag */
	if (synccontext->request.unicode && !(request->SynchronizationFlag & SynchronizationFlag_Unicode)) {
		OC_DEBUG(4, "unhandled value for SynchronizationType: %d\n", request->SynchronizationType);
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
		talloc_free(synccontext_object);
		goto end;
	}
	synccontext->request.no_deletions = (request->SynchronizationFlag & SynchronizationFlag_NoDeletions);
	synccontext->request.no_soft_deletions = (request->SynchronizationFlag & SynchronizationFlag_NoSoftDeletions);
	synccontext->request.ignore_no_longer_in_scope = (request->SynchronizationFlag & SynchronizationFlag_NoSoftDeletions);
	synccontext->request.read_state = (request->SynchronizationFlag & SynchronizationFlag_ReadState);
	synccontext->request.fai = (request->SynchronizationFlag & SynchronizationFlag_FAI);
	synccontext->request.normal = (request->SynchronizationFlag & SynchronizationFlag_Normal);
	synccontext->request.no_foreign_identifiers = (request->SynchronizationFlag & SynchronizationFlag_NoForeignIdentifiers);
	synccontext->request.best_body = (request->SynchronizationFlag & SynchronizationFlag_BestBody);
	synccontext->request.ignored_specified_on_fai = (request->SynchronizationFlag & SynchronizationFlag_IgnoreSpecifiedOnFAI);
	synccontext->request.progress = (request->SynchronizationFlag & SynchronizationFlag_Progress);

	/* SynchronizationExtraFlag */
	synccontext->request.request_eid = (request->SynchronizationExtraFlags & Eid);
	synccontext->request.request_message_size = (request->SynchronizationExtraFlags & MessageSize);
	synccontext->request.request_cn = (request->SynchronizationExtraFlags & Cn);
	synccontext->request.order_by_delivery_time = (request->SynchronizationExtraFlags & OrderByDeliveryTime);

	/* Building the real properties array... */
	properties_exclusion = talloc_array(NULL, bool, 65536);
	memset(properties_exclusion, 0, 65536 * sizeof(bool));

	synccontext->properties.cValues = 0;
	synccontext->properties.aulPropTag = talloc_zero(synccontext, enum MAPITAGS);
	if (synccontext->request.contents_mode) {	/* keyword: messageChangeHeader */
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagMid); /* PidTagSourceKey */
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagAssociated);
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagMessageSize);
	}
	else {						/* keyword: folderChange */
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagParentFolderId); /* PidTagParentSourceKey */
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagFolderId); /* PidTagSourceKey */
		properties_exclusion[PidTagMessageClass >> 16] = false;
	}
	SPropTagArray_add(synccontext, &synccontext->properties, PidTagChangeNumber);
	SPropTagArray_add(synccontext, &synccontext->properties, PidTagChangeKey);
	SPropTagArray_add(synccontext, &synccontext->properties, PidTagPredecessorChangeList);
	SPropTagArray_add(synccontext, &synccontext->properties, PidTagLastModificationTime);
	SPropTagArray_add(synccontext, &synccontext->properties, PidTagDisplayName);

	if (!synccontext->request.contents_mode) {
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagRights);
		SPropTagArray_add(synccontext, &synccontext->properties, PidTagAccessLevel);
	}

	for (j = 0; j < synccontext->properties.cValues; j++) {
		i = (synccontext->properties.aulPropTag[j] & 0xffff0000) >> 16;
		properties_exclusion[i] = true;
	}

	/* Explicit exclusions */
	properties_exclusion[(uint16_t) (PidTagRowType >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagInstanceKey >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagInstanceNum >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagInstID >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagFolderId >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagMid >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagSourceKey >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagParentSourceKey >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagParentFolderId >> 16)] = true;

	/* Include or exclude specified properties passed in array */
	include_props = ((request->SynchronizationFlag & SynchronizationFlag_OnlySpecifiedProperties));
	for (j = 0; j < request->PropertyTags.cValues; j++) {
		i = (uint16_t) (request->PropertyTags.aulPropTag[j] >> 16);
		if (!properties_exclusion[i]) {
			properties_exclusion[i] = true; /* avoid including the same prop twice */
			if (include_props) {
				SPropTagArray_add(synccontext, &synccontext->properties, request->PropertyTags.aulPropTag[j]);
			}
		}
	}

	/* When "best body" is requested and one of the required properties is excluded, we include it back */
	if (!include_props && ((request->SynchronizationFlag & SynchronizationFlag_BestBody))) {
		properties_exclusion[PidTagBodyHtml >> 16] = false;
		properties_exclusion[PidTagBody >> 16] = false;
	}

	/* we instantiate a table object that will help us retrieve the list of available properties */
	/* FIXME: the table_get_available_properties operations should actually be replaced with per-object requests, since not all message/folder types return the same available properties */
	if (!include_props && synccontext->request.contents_mode && synccontext->request.normal) {
		table_object = emsmdbp_folder_open_table(NULL, folder_object, MAPISTORE_MESSAGE_TABLE, 0);
		if (!table_object) {
			OC_DEBUG(5, "could not open message table\n");
			abort();
		}
		if (emsmdbp_object_table_get_available_properties(mem_ctx, emsmdbp_ctx, table_object, &available_properties) == MAPISTORE_SUCCESS) {
			for (j = 0; j < available_properties->cValues; j++) {
				i = (available_properties->aulPropTag[j] & 0xffff0000) >> 16;
				if (!properties_exclusion[i]) {
					properties_exclusion[i] = true;
					SPropTagArray_add(synccontext, &synccontext->properties, available_properties->aulPropTag[j]);
				}
			}
			talloc_free(available_properties->aulPropTag);
			talloc_free(available_properties);
		}
		talloc_free(table_object);
	}

	if (synccontext->request.contents_mode && synccontext->request.fai) {
		synccontext->fai_properties.cValues = synccontext->properties.cValues;
		synccontext->fai_properties.aulPropTag = talloc_memdup(synccontext, synccontext->properties.aulPropTag, synccontext->properties.cValues * sizeof (enum MAPITAGS));

		if (!include_props || synccontext->request.ignored_specified_on_fai) {
			table_object = emsmdbp_folder_open_table(NULL, folder_object, MAPISTORE_FAI_TABLE, 0);
			if (!table_object) {
				OC_DEBUG(5, "could not open FAI table\n");
				abort();
			}
			if (emsmdbp_object_table_get_available_properties(mem_ctx, emsmdbp_ctx, table_object, &available_properties) == MAPISTORE_SUCCESS) {
				for (j = 0; j < available_properties->cValues; j++) {
					i = (available_properties->aulPropTag[j] & 0xffff0000) >> 16;
					if (!properties_exclusion[i]) {
						properties_exclusion[i] = true;
						SPropTagArray_add(synccontext, &synccontext->fai_properties, available_properties->aulPropTag[j]);
					}
				}
				talloc_free(available_properties->aulPropTag);
				talloc_free(available_properties);
			}
			talloc_free(table_object);
		}
	}

	if (!include_props && ! synccontext->request.contents_mode) {
		table_object = emsmdbp_folder_open_table(NULL, folder_object, MAPISTORE_FOLDER_TABLE, 0);
		if (!table_object) {
			OC_DEBUG(5, "could not open folder table\n");
			abort();
		}
		if (emsmdbp_object_table_get_available_properties(mem_ctx, emsmdbp_ctx, table_object, &available_properties) == MAPISTORE_SUCCESS) {
			for (j = 0; j < available_properties->cValues; j++) {
				i = (available_properties->aulPropTag[j] & 0xffff0000) >> 16;
				if (!properties_exclusion[i]) {
					properties_exclusion[i] = true;
					SPropTagArray_add(synccontext, &synccontext->properties, available_properties->aulPropTag[j]);
				}
			}
			talloc_free(available_properties->aulPropTag);
			talloc_free(available_properties);
		}
		talloc_free(table_object);
	}
	talloc_free(properties_exclusion);

	/* TODO: handle restrictions */

	/* The properties array is now ready and further processing must occur in the first FastTransferSource_GetBuffer since we need to wait to receive the state streams in order to build it. */

        retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, folder_handle, &synccontext_rec);
	(void) talloc_reference(synccontext_rec, synccontext_object);
        mapi_handles_set_private_data(synccontext_rec, synccontext_object);
	talloc_free(synccontext_object);
        handles[mapi_repl->handle_idx] = synccontext_rec->handle;
end:
	*size += libmapiserver_RopSyncConfigure_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncImportMessageChange (0x72) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncImportMessageChange EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncImportMessageChange EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportMessageChange(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    struct EcDoRpc_MAPI_REQ *mapi_req,
							    struct EcDoRpc_MAPI_REPL *mapi_repl,
							    uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	enum mapistore_error			ret;
	struct mapi_handles			*synccontext_object_handle = NULL, *message_object_handle;
	struct emsmdbp_object			*synccontext_object = NULL, *message_object;
	uint32_t				synccontext_handle_id;
	void					*data;
	struct SyncImportMessageChange_req	*request;
	struct SyncImportMessageChange_repl	*response;
	char					*owner;
	uint64_t				folderID, messageID;
	struct GUID				replica_guid;
	uint16_t				repl_id, i;
	struct mapistore_message		*msg;
	struct SRow				aRow;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncImportMessageChange (0x72)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_SyncImportMessageChange;
	response = &mapi_repl->u.mapi_SyncImportMessageChange;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = request->handle_idx;

	/* Step 1. Retrieve object handle */
	synccontext_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle_id, &synccontext_object_handle);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle_id, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(synccontext_object_handle, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	if (!emsmdbp_is_mapistore(synccontext_object->parent_object)) {
		OC_DEBUG(5, "  cannot create message on non-mapistore object\n");
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	/* The type of messages handled in the collector contexts must be specified in order for the transfer state operations to work properly */
	if (request->ImportFlag & ImportFlag_Associated) {
		synccontext_object->object.synccontext->request.fai = true;
	}
	else {
		synccontext_object->object.synccontext->request.normal = true;
	}

	folderID = synccontext_object->parent_object->object.folder->folderID;
	owner = emsmdbp_get_owner(synccontext_object);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, owner, &repl_id, &replica_guid);
	if (oxcfxics_fmid_from_source_key(emsmdbp_ctx, owner, &request->PropertyValues.lpProps[0].value.bin, &messageID)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	/* Initialize Message object */
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &message_object_handle);
	handles[mapi_repl->handle_idx] = message_object_handle->handle;

	ret = emsmdbp_object_message_open(message_object_handle, emsmdbp_ctx, synccontext_object->parent_object, folderID, messageID, true, &message_object, &msg);
	if (ret == MAPISTORE_ERR_NOT_FOUND) {
		message_object = emsmdbp_object_message_init(message_object_handle, emsmdbp_ctx, messageID, synccontext_object->parent_object);
		if (mapistore_folder_create_message(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(synccontext_object->parent_object), synccontext_object->parent_object->backend_object, message_object, messageID, (request->ImportFlag & ImportFlag_Associated), &message_object->backend_object)) {
			mapi_handles_delete(emsmdbp_ctx->handles_ctx, message_object_handle->handle);
			OC_DEBUG(5, "could not open nor create mapistore message\n");
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
			goto end;
		}
		message_object->object.message->read_write = true;
	}
	else if (ret != MAPISTORE_SUCCESS) {
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, message_object_handle->handle);
		if (ret == MAPISTORE_ERR_DENIED) {
			mapi_repl->error_code = MAPI_E_NO_ACCESS;
		}
		else {
			mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		}
		goto end;
	}

	mapi_handles_set_private_data(message_object_handle, message_object);

	response->MessageId = 0; /* Must be set to 0 */

	aRow.cValues = request->PropertyValues.cValues;
	aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, aRow.cValues + 2);
	for (i = 0; i < request->PropertyValues.cValues; i++) {
		cast_SPropValue(aRow.lpProps, &request->PropertyValues.lpProps[i],
				&(aRow.lpProps[i]));
	}
	emsmdbp_object_set_properties(emsmdbp_ctx, message_object, &aRow);

end:
	*size += libmapiserver_RopSyncImportMessageChange_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncImportHierarchyChange (0x73) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncImportHierarchyChange EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncImportHierarchyChange EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportHierarchyChange(TALLOC_CTX *mem_ctx,
							      struct emsmdbp_context *emsmdbp_ctx,
							      struct EcDoRpc_MAPI_REQ *mapi_req,
							      struct EcDoRpc_MAPI_REPL *mapi_repl,
							      uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	enum mapistore_error			ret;
	struct mapi_handles			*synccontext_object_handle = NULL;
	struct emsmdbp_object			*synccontext_object = NULL, *folder_object = NULL, *parent_folder = NULL;
	uint32_t				synccontext_handle_id;
	void					*data;
	struct SyncImportHierarchyChange_req	*request;
	struct SyncImportHierarchyChange_repl	*response;
	char					*owner;
	const char				*new_folder_name;
	uint64_t				parentFolderID;
	uint64_t				folderID, cn;
	struct GUID				replica_guid;
	uint16_t				repl_id;
	uint32_t				i;
	struct SRow				aRow;
	bool					folder_was_open = true;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncImportHierarchyChange (0x73)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve object handle */
	synccontext_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle_id, &synccontext_object_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle_id, mapi_req->handle_idx);
		goto end;
	}

	mapi_handles_get_private_data(synccontext_object_handle, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	request = &mapi_req->u.mapi_SyncImportHierarchyChange;
	response = &mapi_repl->u.mapi_SyncImportHierarchyChange;

	owner = emsmdbp_get_owner(synccontext_object);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, owner, &repl_id, &replica_guid);

	/* deduce the parent folder id (fixed position 0). */
	if (oxcfxics_fmid_from_source_key(emsmdbp_ctx, owner, &request->HierarchyValues.lpProps[0].value.bin, &parentFolderID)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	/* deduce the folder id (fixed position 1) */
	if (oxcfxics_fmid_from_source_key(emsmdbp_ctx, owner, &request->HierarchyValues.lpProps[1].value.bin, &folderID)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	aRow.cValues = request->HierarchyValues.cValues + request->PropertyValues.cValues;
	aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, aRow.cValues + 3);
	for (i = 0; i < request->HierarchyValues.cValues; i++) {
		cast_SPropValue(aRow.lpProps, request->HierarchyValues.lpProps + i, aRow.lpProps + i);
	}
	for (i = 0; i < request->PropertyValues.cValues; i++) {
		cast_SPropValue(aRow.lpProps, request->PropertyValues.lpProps + i, aRow.lpProps + request->HierarchyValues.cValues + i);
	}

	/* Initialize folder object */
	if (synccontext_object->parent_object->object.folder->folderID == parentFolderID) {
		parent_folder = synccontext_object->parent_object;
		folder_was_open = true;
	}
	else {
		retval = emsmdbp_object_open_folder_by_fid(NULL, emsmdbp_ctx, synccontext_object->parent_object, parentFolderID, &parent_folder);
		if (retval != MAPI_E_SUCCESS) {
			OC_DEBUG(0, "Failed to open parent folder with FID=[0x%016"PRIx64"]: %s\n", parentFolderID, mapi_get_errstr(retval));
			mapi_repl->error_code = retval;
			goto end;
		}
		folder_was_open = false;
	}

	retval = emsmdbp_object_open_folder_by_fid(NULL, emsmdbp_ctx, parent_folder, folderID, &folder_object);
	if (retval != MAPI_E_SUCCESS) {
		retval = openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, &cn);
		if (retval) {
			OC_DEBUG(5, "unable to obtain a change number\n");
			folder_object = NULL;
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
			goto end;
		}
		aRow.lpProps[aRow.cValues].ulPropTag = PidTagChangeNumber;
		aRow.lpProps[aRow.cValues].value.d = cn;
		aRow.cValues++;
		retval = emsmdbp_object_create_folder(emsmdbp_ctx, parent_folder, NULL,
						      folderID, &aRow, false, &folder_object);
		if (retval) {
			mapi_repl->error_code = retval;
			OC_DEBUG(5, "folder creation failed\n");
			folder_object = NULL;
			goto end;
		}
	}

	if (folder_object->parent_object->object.folder->folderID != parent_folder->object.folder->folderID) {
		/* a move was requested */
		new_folder_name = find_SPropValue_data(&aRow, PidTagDisplayName);
		ret = emsmdbp_folder_move_folder(emsmdbp_ctx, folder_object, parent_folder, mem_ctx, new_folder_name);
		if (ret != MAPISTORE_SUCCESS) {
			mapi_repl->error_code = mapistore_error_to_mapi(ret);
			goto end;
		}
	}

	/* Set properties on folder object */
	retval = emsmdbp_object_set_properties(emsmdbp_ctx, folder_object, &aRow);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}
	response->FolderId = 0; /* Must be set to 0 */

end:
	if (folder_object) {
		talloc_free(folder_object);
	}
	if (!folder_was_open) {
		talloc_free(parent_folder);
	}

	*size += libmapiserver_RopSyncImportHierarchyChange_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc SyncImportDeletes (0x74) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncImportDeletes EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SyncImportDeletes EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportDeletes(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS				retval;
	struct mapi_handles			*synccontext_object_handle = NULL;
	struct emsmdbp_object			*synccontext_object = NULL;
	uint32_t				synccontext_handle_id;
	void					*data;
	struct SyncImportDeletes_req		*request;
	uint32_t				contextID;
	uint64_t				objectID;
	char					*owner;
	struct GUID				replica_guid;
	uint16_t				repl_id;
	struct mapi_SBinaryArray		*object_array;
	uint8_t					delete_type;
	uint32_t				i;
	int						ret;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] SyncImportDeletes (0x74)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve object handle */
	synccontext_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle_id, &synccontext_object_handle);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle_id, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(synccontext_object_handle, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	request = &mapi_req->u.mapi_SyncImportDeletes;

	if (request->Flags & SyncImportDeletes_HardDelete) {
		delete_type = MAPISTORE_PERMANENT_DELETE;
	}
	else {
		delete_type = MAPISTORE_SOFT_DELETE;
	}

	owner = emsmdbp_get_owner(synccontext_object);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, owner, &repl_id, &replica_guid);

	if (request->Flags & SyncImportDeletes_Hierarchy) {
		object_array = &request->PropertyValues.lpProps[0].value.MVbin;
		for (i = 0; i < object_array->cValues; i++) {
			ret = oxcfxics_fmid_from_source_key(emsmdbp_ctx, owner, object_array->bin + i, &objectID);
			if (ret == MAPISTORE_SUCCESS) {
				emsmdbp_folder_delete(emsmdbp_ctx, synccontext_object->parent_object, objectID, 0xff);
			}
		}
	}
	else {
		if (!emsmdbp_is_mapistore(synccontext_object)) {
			OC_DEBUG(5, "  no message deletes on non-mapistore store\n");
			mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
			goto end;
		}

		contextID = emsmdbp_get_contextID(synccontext_object);

		object_array = &request->PropertyValues.lpProps[0].value.MVbin;
		for (i = 0; i < object_array->cValues; i++) {
			ret = oxcfxics_fmid_from_source_key(emsmdbp_ctx, owner, object_array->bin + i, &objectID);
			if (ret == MAPISTORE_SUCCESS) {
				ret = mapistore_folder_delete_message(emsmdbp_ctx->mstore_ctx, contextID, synccontext_object->parent_object->backend_object, objectID, delete_type);
				if (ret != MAPISTORE_SUCCESS) {
					OC_DEBUG(5, "message deletion failed for fmid: 0x%.16"PRIx64"\n", objectID);
				}
				ret = mapistore_indexing_record_del_mid(emsmdbp_ctx->mstore_ctx, contextID, owner, objectID, delete_type);
				if (ret != MAPISTORE_SUCCESS) {
					OC_DEBUG(5, "message deletion of index record failed for fmid: 0x%.16"PRIx64"\n", objectID);
				}
			}
		}
	}

end:
	*size += libmapiserver_RopSyncImportDeletes_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamBegin (0x75) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncUploadStateStreamBegin EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SyncUploadStateStreamBegin EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamBegin(TALLOC_CTX *mem_ctx,
							       struct emsmdbp_context *emsmdbp_ctx,
							       struct EcDoRpc_MAPI_REQ *mapi_req,
							       struct EcDoRpc_MAPI_REPL *mapi_repl,
							       uint32_t *handles, uint16_t *size)
{
 	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	enum StateProperty	property;
	void			*data = NULL;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamBegin (0x75)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	synccontext_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle, &synccontext_rec);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(synccontext_rec, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	if (synccontext_object->object.synccontext->state_property != 0) {
		OC_DEBUG(5, "  stream already in pending state\n");
		mapi_repl->error_code = MAPI_E_NOT_INITIALIZED;
		goto end;
	}

	property = mapi_req->u.mapi_SyncUploadStateStreamBegin.StateProperty;
	if (!(property == MetaTagIdsetGiven || property == MetaTagCnsetSeen || property == MetaTagCnsetSeenFAI || property == MetaTagCnsetRead)) {
		OC_DEBUG(5, "  state property is invalid\n");
		mapi_repl->error_code = MAPI_E_INVALID_PARAMETER;
		goto end;
	}

	synccontext_object->object.synccontext->state_property = property;
	memset(&synccontext_object->object.synccontext->state_stream, 0, sizeof(struct emsmdbp_stream));
	synccontext_object->object.synccontext->state_stream.buffer.data = talloc_zero(synccontext_object->object.synccontext, uint8_t);

end:
	*size += libmapiserver_RopSyncUploadStateStreamBegin_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamContinue (0x76) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncUploadStateStreamContinue EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncUploadStateStreamContinue EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamContinue(TALLOC_CTX *mem_ctx,
								  struct emsmdbp_context *emsmdbp_ctx,
								  struct EcDoRpc_MAPI_REQ *mapi_req,
								  struct EcDoRpc_MAPI_REPL *mapi_repl,
								  uint32_t *handles, uint16_t *size)
{
 	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;
	struct SyncUploadStateStreamContinue_req *request;
	DATA_BLOB		new_data;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamContinue (0x76)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	synccontext_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle, &synccontext_rec);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(synccontext_rec, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	if (synccontext_object->object.synccontext->state_property == 0) {
		OC_DEBUG(5, "  attempt to feed an idle stream\n");
		mapi_repl->error_code = MAPI_E_NOT_INITIALIZED;
		goto end;
	}

	request = &mapi_req->u.mapi_SyncUploadStateStreamContinue;
	new_data.length = request->StreamDataSize;
	new_data.data = request->StreamData;
	emsmdbp_stream_write_buffer(synccontext_object->object.synccontext,
				    &synccontext_object->object.synccontext->state_stream,
				    new_data);

end:
	*size += libmapiserver_RopSyncUploadStateStreamContinue_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details Checks that change numbers referenced in parsed_idset are valid

   \param oc_ctx pointer to openchangedb context
   \param username mailbox name of the current logged user
   \param parsed_idset pointer to the struct idset to be checked
   \param label A name to identify where the verification is being done, only
   used for logging in case the verification fails
   \returns MAPI_E_SUCCESS if check succeeded
 */
static enum MAPISTATUS oxcfxics_check_cnset(struct openchangedb_context *oc_ctx,
					    const char *username,
					    struct idset *parsed_idset,
					    const char *label)
{
	uint64_t	next_cn, high_cn;
	enum MAPISTATUS	retval;

	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!label, MAPI_E_INVALID_PARAMETER, NULL);

	// Perform check only if parsed_idset is defined
	OPENCHANGE_RETVAL_IF(!parsed_idset, MAPI_E_SUCCESS, NULL);

	retval = openchangedb_get_next_changeNumber(oc_ctx, username, &next_cn);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);
	next_cn = exchange_globcnt(next_cn >> 16);
	high_cn = exchange_globcnt(parsed_idset->ranges->high);
	if (high_cn >= next_cn) {
		OC_DEBUG(0, "inconsistency: idset range for '%s' is referencing a change number "
			  "that has not been issued yet: %"PRIx64" >= %"PRIx64" \n",
			  label, high_cn, next_cn);
		return MAPI_E_BAD_VALUE;

	}
	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamEnd (0x77) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncUploadStateStreamEnd EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncUploadStateStreamEnd EcDoRpc_MAPI_REPL  structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamEnd(TALLOC_CTX *mem_ctx,
							     struct emsmdbp_context *emsmdbp_ctx,
							     struct EcDoRpc_MAPI_REQ *mapi_req,
							     struct EcDoRpc_MAPI_REPL *mapi_repl,
							     uint32_t *handles, uint16_t *size)
{
 	uint32_t				synccontext_handle;
	struct mapi_handles			*synccontext_rec;
	struct emsmdbp_object			*synccontext_object;
	struct emsmdbp_object_synccontext	*synccontext;
	struct idset				*parsed_idset, *old_idset = NULL;
	enum MAPISTATUS				retval;
	void					*data = NULL;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamEnd (0x77)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	synccontext_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle, &synccontext_rec);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(synccontext_rec, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;	
		goto end;
	}

	if (synccontext_object->object.synccontext->state_property == 0) {
		OC_DEBUG(5, "  attempt to end an idle stream\n");
		mapi_repl->error_code = MAPI_E_NOT_INITIALIZED;
		goto end;
	}

	if (synccontext_object->object.synccontext->request.is_collector) {
		OC_DEBUG(5, "  synccontext is collector\n");
	}

	/* parse IDSET */
	synccontext = synccontext_object->object.synccontext;
	parsed_idset = IDSET_parse(synccontext, synccontext->state_stream.buffer, false);

	retval = IDSET_check_ranges(parsed_idset);
	if (retval != MAPI_E_SUCCESS) {
		mapi_repl->error_code = retval;
		goto reset;
	}

	switch (synccontext->state_property) {
	case MetaTagIdsetGiven:
		if (parsed_idset && parsed_idset->range_count == 0) {
			OC_DEBUG(5, "empty idset, ignored\n");
		}
		old_idset = synccontext->idset_given;
		synccontext->idset_given = parsed_idset;
		break;
	case MetaTagCnsetSeen:
		if (parsed_idset) {
			parsed_idset->single = true;
		}
		retval = oxcfxics_check_cnset(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, parsed_idset, "cnset_seen");
		if (retval != MAPI_E_SUCCESS) {
			mapi_repl->error_code = retval;
			goto reset;
		}
		old_idset = synccontext->cnset_seen;
		synccontext->cnset_seen = parsed_idset;
		break;
	case MetaTagCnsetSeenFAI:
		if (parsed_idset) {
			parsed_idset->single = true;
		}
		retval = oxcfxics_check_cnset(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, parsed_idset, "cnset_seen_fai");
		if (retval != MAPI_E_SUCCESS) {
			mapi_repl->error_code = retval;
			goto reset;
		}
		old_idset = synccontext->cnset_seen_fai;
		synccontext->cnset_seen_fai = parsed_idset;
		break;
	case MetaTagCnsetRead:
		if (parsed_idset) {
			parsed_idset->single = true;
		}
		retval = oxcfxics_check_cnset(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, parsed_idset, "cnset_seen_read");
		if (retval != MAPI_E_SUCCESS) {
			mapi_repl->error_code = retval;
			goto reset;
		}
		old_idset = synccontext->cnset_read;
		synccontext->cnset_read = parsed_idset;
		break;
	}
	if (old_idset) {
		talloc_free(old_idset);
	}

reset:
	/* reset synccontext state */
	if (synccontext->state_stream.buffer.length > 0) {
		talloc_free(synccontext->state_stream.buffer.data);
		synccontext->state_stream.buffer.data = talloc_zero(synccontext, uint8_t);
		synccontext->state_stream.buffer.length = 0;
	}

	synccontext->state_property = 0;

end:
	*size += libmapiserver_RopSyncUploadStateStreamEnd_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

static bool convertIdToFMID(const struct GUID *replica_guid, uint8_t *data, uint32_t size, uint64_t *fmidP)
{
	struct GUID	guid;
	uint64_t	base, fmid;
	uint32_t	i;

	if (size < 17) {
		return false;
	}

	GUID_from_string((char *) data, &guid);
	if (!GUID_equal(replica_guid, &guid)) {
		return false;
	}

	fmid = 0;
	base = 1;
	for (i = 16; i < size; i++) {
		fmid |= (uint64_t) data[i] * base;
		base <<= 8;
	}
	fmid <<= 16;
	fmid |= 1;
	*fmidP = fmid;

	return true;
}

/**
   \details EcDoRpc SyncImportMessageMove (0x78) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncImportMessageMove EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SyncImportMessageMove EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportMessageMove(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct EcDoRpc_MAPI_REQ *mapi_req,
							  struct EcDoRpc_MAPI_REPL *mapi_repl,
							  uint32_t *handles, uint16_t *size)
{
	struct SyncImportMessageMove_req	*request;
	struct SyncImportMessageMove_repl	*response;
	struct GUID				replica_guid;
	uint64_t				sourceFID, sourceMID, destMID;
	struct Binary_r				*change_key;
	uint32_t				contextID, synccontext_handle;
	void					*data;
	struct mapi_handles			*synccontext_rec;
	struct emsmdbp_object			*synccontext_object;
	struct emsmdbp_object			*source_folder_object;
	char					*owner;
	enum MAPISTATUS				retval;
	bool					mapistore;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] SyncImportMessageMove (0x78)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* Step 1. Retrieve object handle */
	synccontext_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle, &synccontext_rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle, mapi_req->handle_idx);
		goto end;
	}

	mapi_handles_get_private_data(synccontext_rec, &data);
	synccontext_object = (struct emsmdbp_object *) data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	request = &mapi_req->u.mapi_SyncImportMessageMove;

	/* FIXME: we consider the local replica to always have id 1. This is correct for now but might pose problems if the local replica handling changes. */
	owner = emsmdbp_get_owner(synccontext_object);
	emsmdbp_replid_to_guid(emsmdbp_ctx, owner, 1, &replica_guid);
	if (!convertIdToFMID(&replica_guid, request->SourceFolderId, request->SourceFolderIdSize, &sourceFID)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}
	if (!convertIdToFMID(&replica_guid, request->SourceMessageId, request->SourceMessageIdSize, &sourceMID)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}
	if (!convertIdToFMID(&replica_guid, request->DestinationMessageId, request->DestinationMessageIdSize, &destMID)) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	retval = emsmdbp_object_open_folder_by_fid(NULL, emsmdbp_ctx, synccontext_object, sourceFID, &source_folder_object);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(0, "Failed to open source folder with FID=[0x%016"PRIx64"]: %s\n",
			  sourceFID, mapi_get_errstr(retval));
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	contextID = emsmdbp_get_contextID(synccontext_object);
	mapistore = emsmdbp_is_mapistore(synccontext_object) && emsmdbp_is_mapistore(source_folder_object);

	change_key = talloc_zero(mem_ctx, struct Binary_r);
	change_key->cb = request->ChangeNumberSize;
	change_key->lpb = request->ChangeNumber;
	if (mapistore) {
		/* We invoke the backend method */
		mapistore_folder_move_copy_messages(emsmdbp_ctx->mstore_ctx, contextID, synccontext_object->parent_object->backend_object, source_folder_object->backend_object, mem_ctx, 1, &sourceMID, &destMID, &change_key, false);
	}
	else {
		OC_DEBUG(0, "mapistore support not implemented yet - shouldn't occur\n");
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
	}

	talloc_free(source_folder_object);

	response = &mapi_repl->u.mapi_SyncImportMessageMove;
	response->MessageId = 0;

end:
	*size += libmapiserver_RopSyncImportMessageMove_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncOpenCollector (0x7e) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncOpenCollector EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncOpenCollector EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncOpenCollector(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	uint32_t		folder_handle;
	struct mapi_handles	*folder_rec;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*folder_object;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncOpenCollector (0x7e)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_SyncOpenCollector.handle_idx;

	folder_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, folder_handle, &folder_rec);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", folder_handle, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(folder_rec, &data);
	folder_object = (struct emsmdbp_object *)data;
	if (!folder_object || folder_object->type != EMSMDBP_OBJECT_FOLDER) {
		OC_DEBUG(5, "  object not found or not a folder\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;	
		goto end;
	}

	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, folder_handle, &synccontext_rec);

	synccontext_object = emsmdbp_object_synccontext_init((TALLOC_CTX *)synccontext_rec, emsmdbp_ctx, folder_object);
	synccontext_object->object.synccontext->request.is_collector = true;

	talloc_steal(synccontext_rec, synccontext_object);
	retval = mapi_handles_set_private_data(synccontext_rec, synccontext_object);
	synccontext_object->object.synccontext->request.contents_mode = (mapi_req->u.mapi_SyncOpenCollector.IsContentsCollector != 0);
	handles[mapi_repl->handle_idx] = synccontext_rec->handle;

end:
	*size += libmapiserver_RopSyncOpenCollector_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopGetLocalReplicaIds (0x7f) Rop. This operation reserves a range of IDs to be used by a local replica.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetLocalReplicaIds EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the GetLocalReplicaIds EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetLocalReplicaIds(TALLOC_CTX *mem_ctx,
                                                       struct emsmdbp_context *emsmdbp_ctx,
                                                       struct EcDoRpc_MAPI_REQ *mapi_req,
                                                       struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                       uint32_t *handles, uint16_t *size)
{
	struct GetLocalReplicaIds_req	*request;
	struct mapi_handles		*object_handle;
	uint32_t			handle_id;
	uint64_t			new_id;
	uint8_t				i;
	void				*data;
	int				retval;
	struct emsmdbp_object		*mailbox_object;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopGetLocalReplicaIds (0x7f)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Step 1. Retrieve object handle */
	handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle_id, &object_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle_id, mapi_req->handle_idx);
		goto end;
	}

	/* Step 2. Check whether the parent object supports fetching properties */
	mapi_handles_get_private_data(object_handle, &data);
	mailbox_object = (struct emsmdbp_object *) data;
	if (!mailbox_object || mailbox_object->type != EMSMDBP_OBJECT_MAILBOX) {
		OC_DEBUG(5, "  object not found or not a folder\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	request = &mapi_req->u.mapi_GetLocalReplicaIds;

	emsmdbp_replid_to_guid(emsmdbp_ctx, mailbox_object->object.mailbox->owner_username, 0x0001, &mapi_repl->u.mapi_GetLocalReplicaIds.ReplGuid);
	mapistore_indexing_reserve_fmid_range(emsmdbp_ctx->mstore_ctx, request->IdCount, &new_id);
	new_id >>= 16;
	for (i = 0; i < 6 ; i++) {
		mapi_repl->u.mapi_GetLocalReplicaIds.GlobalCount[i] = new_id & 0xff;
		new_id >>= 8;
	}

end:
	*size += libmapiserver_RopGetLocalReplicaIds_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve a MessageReadState structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw MessageReadState data

   \return Allocated MessageReadState structure on success, otherwise NULL

   \note Developers must free the allocated MessageReadState when finished.
 */
static struct MessageReadState *get_MessageReadState(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct MessageReadState	*message_read_states = NULL;
	struct ndr_pull			*ndr;
	enum ndr_err_code		ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	message_read_states = talloc_zero(mem_ctx, struct MessageReadState);
	ndr_err_code = ndr_pull_MessageReadState(ndr, NDR_SCALARS, message_read_states);

	/* talloc_free(ndr); */

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(message_read_states);
		return NULL;
	}

	return message_read_states;
}

/**
   \details EcDoRpc SyncImportReadStateChanges (0x80) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncImportReadStateChanges EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SyncImportReadStateChanges EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportReadStateChanges(TALLOC_CTX *mem_ctx,
							       struct emsmdbp_context *emsmdbp_ctx,
							       struct EcDoRpc_MAPI_REQ *mapi_req,
							       struct EcDoRpc_MAPI_REPL *mapi_repl,
							       uint32_t *handles, uint16_t *size)
{
	struct SyncImportReadStateChanges_req	*request;
	uint32_t				contextID, synccontext_handle;
	void					*data;
	struct mapi_handles			*synccontext_rec;
	struct emsmdbp_object			*synccontext_object, *folder_object, *message_object;
	enum MAPISTATUS				retval;
	enum mapistore_error			ret;
	struct MessageReadState			*read_states;
	uint32_t				read_states_size;
	struct Binary_r				*bin_data;
	char					*owner;
	uint64_t				mid, base;
	uint16_t				replid;
	int					i;
	struct mapistore_message		*msg;
	struct GUID				guid;
	DATA_BLOB				guid_blob = { .length = 16, .data = NULL };
	uint8_t					flag;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] SyncImportReadStateChanges (0x80)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* Step 1. Retrieve object handle */
	synccontext_handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle, &synccontext_rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle, mapi_req->handle_idx);
		goto end;
	}

	mapi_handles_get_private_data(synccontext_rec, &data);
	synccontext_object = (struct emsmdbp_object *) data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	request = &mapi_req->u.mapi_SyncImportReadStateChanges;

	folder_object = synccontext_object->parent_object;
	if (emsmdbp_is_mapistore(folder_object)) {
		contextID = emsmdbp_get_contextID(folder_object);
		bin_data = talloc_zero(mem_ctx, struct Binary_r);
		bin_data->cb = request->MessageReadStates.length;
		bin_data->lpb = request->MessageReadStates.data;
		while (bin_data->cb) {
			read_states = get_MessageReadState(mem_ctx, bin_data);
			read_states_size = read_states->MessageIdSize + 3;

			bin_data->cb -= read_states_size;
			bin_data->lpb += read_states_size;

			guid_blob.data = read_states->MessageId;
			if (NT_STATUS_IS_ERR(GUID_from_data_blob(&guid_blob, &guid))) {
				continue;
			}
			owner = emsmdbp_get_owner(synccontext_object);
			if (emsmdbp_guid_to_replid(emsmdbp_ctx, owner, &guid, &replid)) {
				continue;
			}

			mid = 0;
			base = 1;
			for (i = 16; i < read_states->MessageIdSize; i++) {
				mid |= (uint64_t) read_states->MessageId[i] * base;
				base <<= 8;
			}
			mid <<= 16;
			mid |= replid;

			if (read_states->MarkAsRead) {
				flag = SUPPRESS_RECEIPT | CLEAR_RN_PENDING;
			}
			else {
				flag = CLEAR_READ_FLAG | CLEAR_NRN_PENDING;
			}

			ret = emsmdbp_object_message_open(NULL, emsmdbp_ctx, folder_object, folder_object->object.folder->folderID, mid, true, &message_object, &msg);
			if (ret == MAPISTORE_SUCCESS) {
				mapistore_message_set_read_flag(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object, flag);
				talloc_free(message_object);
			} else {
				OC_DEBUG(5, "[oxcfxics]: Failed to open message 0x%"PRIx64": %s\n", mid, mapistore_errstr(ret));
			}
		}
	}
	else {
		OC_DEBUG(0, "operation not supported on non-mapistore objects\n");
	}

end:
	*size += libmapiserver_RopSyncImportReadStateChanges_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS oxcfxics_fill_transfer_state_arrays(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
							   struct emsmdbp_object_synccontext *synccontext,
							   const char *owner, struct oxcfxics_sync_data *sync_data,
							   struct emsmdbp_object *folder_object)
{
	struct SPropTagArray		*count_query_props;
	uint64_t			eid, cn;
	uint32_t			i, nr_eid;
	void				**data_pointers;
	enum MAPISTATUS			*retvals;
	struct emsmdbp_object		*table_object, *subfolder_object;
	struct emsmdbp_object_table	*table;
	uint32_t			unix_time;
	struct FILETIME			*lm_time;
	NTTIME				nt_time;
	TALLOC_CTX			*local_mem_ctx;
	struct GUID			replica_guid;
	enum mapistore_error		ret;
	enum MAPISTATUS			retval;
	
	local_mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(local_mem_ctx == NULL, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* Query the amount of rows and update sync_data structure */
	count_query_props = talloc_zero(local_mem_ctx, struct SPropTagArray);
	count_query_props->cValues = 1;
	count_query_props->aulPropTag = talloc_zero(count_query_props, enum MAPITAGS);
	switch (sync_data->table_type) {
	case MAPISTORE_FOLDER_TABLE:
		count_query_props->aulPropTag[0] = PidTagFolderChildCount;
		break;
	case MAPISTORE_MESSAGE_TABLE:
		count_query_props->aulPropTag[0] = PidTagContentCount;
		break;
	case MAPISTORE_FAI_TABLE:
		count_query_props->aulPropTag[0] = PidTagAssociatedContentCount;
		break;
	default:
		abort();
	}
	data_pointers = emsmdbp_object_get_properties(local_mem_ctx, emsmdbp_ctx, folder_object, count_query_props, (enum MAPISTATUS **) &retvals);
	if (data_pointers && !retvals[0]) {
		nr_eid = *(uint32_t *) data_pointers[0];
		/* No content, return */
		OPENCHANGE_RETVAL_IF(!nr_eid, MAPI_E_SUCCESS, local_mem_ctx);
	} else {
		OC_DEBUG(1, "ERROR: could not retrieve number of rows in table\n");
		abort();
	}

	/* Fetch the actual table data */
 	table_object = emsmdbp_folder_open_table(local_mem_ctx, folder_object, sync_data->table_type, 0); 
	if (!table_object) {
		OC_DEBUG(5, "could not open folder table\n");
		abort();
	}
	table_object->object.table->prop_count = synccontext->properties.cValues;
	table_object->object.table->properties = synccontext->properties.aulPropTag;
	if (emsmdbp_is_mapistore(table_object)) {
		ret = mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx,
						  emsmdbp_get_contextID(table_object), table_object->backend_object,
						  synccontext->properties.cValues, synccontext->properties.aulPropTag);
		OPENCHANGE_RETVAL_IF(ret != MAPISTORE_SUCCESS, mapistore_error_to_mapi(ret), local_mem_ctx);
	}
	table = table_object->object.table;
	for (i = 0; i < table->denominator; i++) {
		data_pointers = emsmdbp_object_table_get_row_props(NULL, emsmdbp_ctx, table_object, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
		if (data_pointers) {
			eid = *(uint64_t *) data_pointers[0];
			emsmdbp_replid_to_guid(emsmdbp_ctx, owner, eid & 0xffff, &replica_guid);
			RAWIDSET_push_guid_glob(sync_data->eid_set, &replica_guid, (eid >> 16) & 0x0000ffffffffffff);
			
			if (retvals[1]) {
				unix_time = oc_version_time;
			}
			else {
				lm_time = (struct FILETIME *) data_pointers[1];
				nt_time = ((uint64_t) lm_time->dwHighDateTime << 32) | lm_time->dwLowDateTime;
				unix_time = nt_time_to_unix(nt_time);
			}

			if (unix_time < oc_version_time) {
				unix_time = oc_version_time;
			}

			if (retvals[sync_data->prop_index.change_number]) {
				OC_DEBUG(5, "mandatory property PidTagChangeNumber not returned for message\n");
				abort();
			}
			cn = ((*(uint64_t *) data_pointers[sync_data->prop_index.change_number]) >> 16) & 0x0000ffffffffffff;
			RAWIDSET_push_guid_glob(sync_data->cnset_seen, &sync_data->replica_guid, cn);
			
			talloc_free(retvals);
			talloc_free(data_pointers);

			if (sync_data->table_type == MAPISTORE_FOLDER_TABLE) {
				ret = emsmdbp_object_open_folder(local_mem_ctx, emsmdbp_ctx, folder_object, eid, &subfolder_object);
				OPENCHANGE_RETVAL_IF(ret != MAPISTORE_SUCCESS, mapistore_error_to_mapi(ret), local_mem_ctx);

				/*
				 * FIXME: be careful with following - it has a comment
				 * to check it further, so problems might be expected
				 * For now, errors are just reported, so we preserve
				 * existing behavior - I am unable to test failure
				 * branches at the moment.
				 */
				retval = oxcfxics_fill_transfer_state_arrays(mem_ctx, emsmdbp_ctx, synccontext, owner, sync_data, subfolder_object);
				talloc_free(subfolder_object);
				if (retval != MAPI_E_SUCCESS) {
					OC_DEBUG(0, "ERROR: oxcfxics_fill_transfer_state_arrays has failed - %s."
						  " Execution will continue to preserve previous behavior\n",
						  mapi_get_errstr(retval));
					continue;
				}
			}
		}
	}

	talloc_free(local_mem_ctx);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS oxcfxics_ndr_push_transfer_state(struct ndr_push *ndr, const char *owner, struct emsmdbp_object *synccontext_object)
{
	void					*mem_ctx;
	struct idset				*new_idset, *old_idset;
	struct oxcfxics_sync_data		*sync_data;
	struct emsmdbp_context			*emsmdbp_ctx;
	struct emsmdbp_object_synccontext	*synccontext;
	enum MAPISTATUS				retval;

	emsmdbp_ctx = synccontext_object->emsmdbp_ctx;
	synccontext = synccontext_object->object.synccontext;
	ndr_push_uint32(ndr, NDR_SCALARS, IncrSyncStateBegin);

	mem_ctx = talloc_zero(NULL, void);

	sync_data = talloc_zero(mem_ctx, struct oxcfxics_sync_data);
	OPENCHANGE_RETVAL_IF(!sync_data, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	retval = openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, owner, NULL, &sync_data->replica_guid);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, mem_ctx);
	sync_data->prop_index.eid = 0;
	sync_data->prop_index.change_number = 1;
	synccontext->properties.cValues = 2;
	synccontext->properties.aulPropTag = talloc_array(synccontext, enum MAPITAGS, 2);
	OPENCHANGE_RETVAL_IF(!synccontext->properties.aulPropTag, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	synccontext->properties.aulPropTag[1] = PidTagChangeNumber;
	sync_data->ndr = ndr;
	sync_data->cutmarks_ndr = ndr_push_init_ctx(sync_data);
	ndr_set_flags(&sync_data->cutmarks_ndr->flags, LIBNDR_FLAG_NOALIGN);
	sync_data->cutmarks_ndr->offset = 0;
	sync_data->cnset_seen = RAWIDSET_make(sync_data, false, true);
	sync_data->eid_set = RAWIDSET_make(sync_data, false, false);

	if (synccontext->request.contents_mode) {
		synccontext->properties.aulPropTag[0] = PidTagMid;

		if (synccontext->request.normal) {
			sync_data->table_type = MAPISTORE_MESSAGE_TABLE;
			oxcfxics_fill_transfer_state_arrays(mem_ctx, emsmdbp_ctx, synccontext, owner, sync_data, synccontext_object->parent_object);
		}

		if (synccontext->request.fai) {
			sync_data->table_type = MAPISTORE_FAI_TABLE;
			oxcfxics_fill_transfer_state_arrays(mem_ctx, emsmdbp_ctx, synccontext, owner, sync_data, synccontext_object->parent_object);
		}
	}
	else {
		synccontext->properties.aulPropTag[0] = PidTagFolderId;
		sync_data->table_type = MAPISTORE_FOLDER_TABLE;

		oxcfxics_fill_transfer_state_arrays(mem_ctx, emsmdbp_ctx, synccontext, owner, sync_data, synccontext_object->parent_object);
	}

	/* for some reason, Exchange returns the same range for MetaTagCnsetSeen, MetaTagCnsetSeenFAI and MetaTagCnsetRead */

	new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->cnset_seen);
	old_idset = synccontext->cnset_seen;
	synccontext->cnset_seen = IDSET_merge_idsets(synccontext, old_idset, new_idset);
	retval = oxcfxics_check_cnset(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, synccontext->cnset_seen, "cnset_seen");
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, mem_ctx);
	talloc_free(old_idset);
	talloc_free(new_idset);

	ndr_push_uint32(ndr, NDR_SCALARS, MetaTagCnsetSeen);
	ndr_push_idset(ndr, synccontext->cnset_seen);
	if (synccontext->request.contents_mode && synccontext->request.fai) {
		ndr_push_uint32(ndr, NDR_SCALARS, MetaTagCnsetSeenFAI);
		ndr_push_idset(ndr, synccontext->cnset_seen);
	}

	new_idset = RAWIDSET_convert_to_idset(NULL, sync_data->eid_set);
	old_idset = synccontext->idset_given;
	synccontext->idset_given = IDSET_merge_idsets(synccontext, old_idset, new_idset);
	talloc_free(old_idset);
	talloc_free(new_idset);

	ndr_push_uint32(ndr, NDR_SCALARS, MetaTagIdsetGiven);
	ndr_push_idset(ndr, synccontext->idset_given);

	if (synccontext->request.contents_mode && synccontext->request.read_state) {
		ndr_push_uint32(ndr, NDR_SCALARS, MetaTagCnsetRead);
		ndr_push_idset(ndr, synccontext->cnset_seen);
	}

	talloc_free(mem_ctx);

	ndr_push_uint32(ndr, NDR_SCALARS, IncrSyncStateEnd);
	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncGetTransferState (0x82) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SyncGetTransferState EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the SyncGetTransferState EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncGetTransferState(TALLOC_CTX *mem_ctx,
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct EcDoRpc_MAPI_REQ *mapi_req,
							 struct EcDoRpc_MAPI_REPL *mapi_repl,
							 uint32_t *handles, uint16_t *size)
{
	uint32_t				synccontext_handle_id;
	struct mapi_handles			*synccontext_handle, *ftcontext_handle;
	struct emsmdbp_object			*synccontext_object, *ftcontext_object;
	struct emsmdbp_object_ftcontext		*ftcontext;
	enum MAPISTATUS				retval;
	void					*data = NULL;
	struct ndr_push				*ndr;
	char					*owner;

	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] RopSyncGetTransferState (0x82)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_SyncGetTransferState.handle_idx;

	synccontext_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, synccontext_handle_id, &synccontext_handle);
	if (retval) {
		OC_DEBUG(5, "  handle (%x) not found: %x\n", synccontext_handle_id, mapi_req->handle_idx);
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	mapi_handles_get_private_data(synccontext_handle, &data);
	synccontext_object = (struct emsmdbp_object *)data;
	if (!synccontext_object || synccontext_object->type != EMSMDBP_OBJECT_SYNCCONTEXT) {
		OC_DEBUG(5, "  object not found or not a synccontext\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;	
		goto end;
	}

	ndr = ndr_push_init_ctx(NULL);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;
	
	owner = emsmdbp_get_owner(synccontext_object);
	retval = oxcfxics_ndr_push_transfer_state(ndr, owner, synccontext_object);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(5, "ndr_push_transfer_state failed\n");
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, synccontext_handle_id, &ftcontext_handle);
	ftcontext_object = emsmdbp_object_ftcontext_init((TALLOC_CTX *)ftcontext_handle, emsmdbp_ctx, synccontext_object);
	mapi_handles_set_private_data(ftcontext_handle, ftcontext_object);
	handles[mapi_repl->handle_idx] = ftcontext_handle->handle;

	ftcontext = ftcontext_object->object.ftcontext;
	(void) talloc_reference(ftcontext, ndr->data);
	ftcontext->stream.buffer.data = ndr->data;
	ftcontext->stream.buffer.length = ndr->offset;

	talloc_free(ndr);

	/* cutmarks */
	ndr = ndr_push_init_ctx(ftcontext);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;
	ndr_push_uint32(ndr, NDR_SCALARS, 0);
	ndr_push_uint32(ndr, NDR_SCALARS, 0xffffffff);

	ftcontext->cutmarks = (uint32_t *) ndr->data;
	(void) talloc_reference(ftcontext, ndr->data);
	talloc_free(ndr);

end:
	*size += libmapiserver_RopSyncGetTransferState_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc SetLocalReplicaMidsetDeleted (0x93) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetLocalReplicaMidsetDeleted EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SetLocalReplicaMidsetDeleted EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetLocalReplicaMidsetDeleted(TALLOC_CTX *mem_ctx,
								 struct emsmdbp_context *emsmdbp_ctx,
								 struct EcDoRpc_MAPI_REQ *mapi_req,
								 struct EcDoRpc_MAPI_REPL *mapi_repl,
								 uint32_t *handles, uint16_t *size)
{
	OC_DEBUG(4, "exchange_emsmdb: [OXCFXICS] SetLocalReplicaMidsetDeleted (0x93) - stub\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO effective work here */

	*size += libmapiserver_RopSetLocalReplicaMidsetDeleted_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}
