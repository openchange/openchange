/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Brad Hards <bradh@openchange.org> 2010

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
   \file oxomsg.c

   \brief Server-side message routines and Rops
 */

#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

static void oxomsg_mapistore_handle_message_relocation(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *old_message_object)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPITAGS			properties[] = { PidTagTargetEntryId, PidTagSentMailSvrEID };
	uint32_t			properties_count = sizeof(properties) / sizeof(enum MAPITAGS);
	struct mapistore_property_data	*property_data;
	enum MAPITAGS			ex_properties[] = { PidTagTargetEntryId, PidTagChangeKey, PidTagPredecessorChangeList };
	struct SPropTagArray		excluded_tags = { sizeof(ex_properties) / sizeof(enum MAPITAGS), ex_properties };
	struct Binary_r			*bin_data;
	struct MessageEntryId		*entryID;
	struct PtypServerId		*folderSvrID;
	uint32_t			contextID;
	uint64_t			folderID;
	uint64_t			messageID;
	uint16_t			replID;
	int				ret, i;
	char				*owner;
	struct emsmdbp_object		*folder_object;
	struct emsmdbp_object		*message_object;
	enum MAPISTATUS			retval;

	mem_ctx = talloc_new(NULL);

	property_data = talloc_array(mem_ctx, struct mapistore_property_data, properties_count);

	contextID = emsmdbp_get_contextID(old_message_object);

	mapistore_properties_get_properties(emsmdbp_ctx->mstore_ctx, contextID, old_message_object->backend_object, mem_ctx, properties_count, properties, property_data);
	for (i = 0; i < properties_count; i++) { 
		if (property_data[i].error) {
			continue;
		}

		/* OC_DEBUG(5, (__location__": old message fid: %.16"PRIx64"\n", old_message_object->parent_object->object.folder->folderID)); */
		/* OC_DEBUG(5, (__location__": old message mid: %.16"PRIx64"\n", old_message_object->object.message->messageID)); */

		owner = emsmdbp_get_owner(old_message_object);
		bin_data = property_data[i].data;

		switch (properties[i]) {
		case PidTagTargetEntryId:
			entryID = get_MessageEntryId(mem_ctx, bin_data);
			if (!entryID) {
				OC_DEBUG(5, "invalid entryID\n");
				continue;
			}

			ret = emsmdbp_guid_to_replid(emsmdbp_ctx, owner, &entryID->FolderDatabaseGuid, &replID);
			if (ret) {
				OC_DEBUG(5, "unable to deduce folder replID\n");
				continue;
			}
			folderID = (entryID->FolderGlobalCounter.value << 16) | replID;
			/* OC_DEBUG(5, (__location__": dest folder id: %.16"PRIx64"\n", folderID)); */

			ret = emsmdbp_guid_to_replid(emsmdbp_ctx, owner, &entryID->MessageDatabaseGuid, &replID);
			if (ret) {
				OC_DEBUG(5, "unable to deduce message replID\n");
				continue;
			}
			messageID = (entryID->MessageGlobalCounter.value << 16) | replID;
			/* OC_DEBUG(5, (__location__": dest message id: %.16"PRIx64"\n", messageID)); */
			break;
		case PidTagSentMailSvrEID:
			folderSvrID = get_PtypServerId(mem_ctx, bin_data);
			if (!folderSvrID) {
				OC_DEBUG(5, "invalid folderSvrID\n");
				continue;
			}

			folderID = folderSvrID->FolderId;
			mapistore_indexing_get_new_folderID(emsmdbp_ctx->mstore_ctx, &messageID);

			/* OC_DEBUG(5, (__location__": dest folder id: %.16"PRIx64"\n", folderID)); */
			break;
		default:
			OC_DEBUG(5, "invalid entryid property: %.8x\n", properties[i]);
			continue;
		}

		retval = emsmdbp_object_open_folder_by_fid(mem_ctx, emsmdbp_ctx, old_message_object, folderID, &folder_object);
		if (retval != MAPI_E_SUCCESS) {
			OC_DEBUG(5, "Failed to open parent folder with FID=[0x%016"PRIx64"]: %s",
				  folderID, mapi_get_errstr(retval));
			continue;
		}

		message_object = emsmdbp_object_message_init(mem_ctx, emsmdbp_ctx, messageID, folder_object);
		if (mapistore_folder_create_message(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, message_object, messageID, false, &message_object->backend_object)) {
			OC_DEBUG(5, "unable to create message in backend\n");
			continue;
		}

		/* FIXME: (from oxomsg 3.2.5.1) PidTagMessageFlags: mfUnsent and mfRead must be cleared.
		   Note: Property not managed by any current mapistore backend */
		/* Copy PidTagChangeKey / PidTagPredecessorChangeList */
		emsmdbp_object_copy_properties_submit(emsmdbp_ctx, old_message_object, message_object, &excluded_tags, true);

		mapistore_message_save(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object, mem_ctx);
		mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, owner, messageID);
	}

	talloc_free(mem_ctx);
}

/**
   \details EcDoRpc SubmitMessage (0x32) Rop. This operation marks a message
   as being ready to send (subject to some flags).

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SubmitMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SubmitMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSubmitMessage(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	void			*private_data;
	bool			mapistore = false;
	struct emsmdbp_object	*object;
	char			*owner;
	uint64_t		messageID;
	uint32_t		contextID;
	uint8_t			flags;

	OC_DEBUG(4, "exchange_emsmdb: [OXOMSG] SubmitMessage (0x32)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MESSAGE) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(object);
	switch ((int)mapistore) {
	case false:
		OC_DEBUG(0, "Not implemented yet - shouldn't occur\n");
		break;
	case true:
		/* Check if we still have uncommitted streams attached to this message */
		{
			struct mapi_handles 	*handles;

			for (handles = emsmdbp_ctx->handles_ctx->handles; handles; handles = handles->next) {
				if (handles->parent_handle == rec->handle) {
					struct emsmdbp_object	*object2 = NULL;
					void			*private_data2;
					struct mapi_handles	*rec2 = NULL;

					retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handles->handle, &rec2);
					if (retval) {
						continue;
					}
					retval = mapi_handles_get_private_data(rec2, &private_data2);
					object2 = (struct emsmdbp_object *)private_data2;
					if (object2->type == EMSMDBP_OBJECT_STREAM) {
						emsmdbp_object_stream_commit(object2);
					}
				}
			}
		}

		messageID = object->object.message->messageID;
		contextID = emsmdbp_get_contextID(object);
		flags = mapi_req->u.mapi_SubmitMessage.SubmitFlags;
		owner = emsmdbp_get_owner(object);
		mapistore_message_submit(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, flags);
		oxomsg_mapistore_handle_message_relocation(emsmdbp_ctx, object);
		mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, owner, messageID);
		break;
	}

 end:
	*size += libmapiserver_RopSubmitMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SetSpooler (0x47) Rop. This operation informs the
   server that the client intends to act as a mail spooler
   
   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdbp provider context
   \param mapi_req pointer to the SeSpooler EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the SetSpooler EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetSpooler(TALLOC_CTX *mem_ctx,
					       struct emsmdbp_context *emsmdbp_ctx,
					       struct EcDoRpc_MAPI_REQ *mapi_req,
					       struct EcDoRpc_MAPI_REPL *mapi_repl,
					       uint32_t *handles, uint16_t *size)
{
	OC_DEBUG(4, "exchange_emsmdb: [OXOMSG] SetSpooler (0x47)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement related server-side behavior */

	*size += libmapiserver_RopSetSpooler_size(NULL);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetAddressTypes (0x49) Rop. This operation gets
   the valid address types (e.g. "SMTP", "X400", "EX")

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the AddressTypes EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the AddressTypes EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetAddressTypes(TALLOC_CTX *mem_ctx,
						    struct emsmdbp_context *emsmdbp_ctx,
						    struct EcDoRpc_MAPI_REQ *mapi_req,
						    struct EcDoRpc_MAPI_REPL *mapi_repl,
						    uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;
	int			ret;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "msExchTemplateRDNs", NULL };
	uint32_t                j;
	struct ldb_dn 		*basedn = 0;

	OC_DEBUG(4, "exchange_emsmdb: [OXOMSG] AddressTypes (0x49)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	retval = emsmdbp_get_org_dn(emsmdbp_ctx, &basedn);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);

	ldb_dn_add_child_fmt(basedn, "CN=ADDRESSING");
	ldb_dn_add_child_fmt(basedn, "CN=ADDRESS-TEMPLATES");

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res, basedn,
                         LDB_SCOPE_SUBTREE, attrs, "CN=%x", emsmdbp_ctx->userLanguage);
	talloc_free(basedn);
	/* If the search failed */
	if (ret != LDB_SUCCESS) {
		OC_DEBUG(1, "exchange_emsmdb: [OXOMSG] AddressTypes ldb_search failure.\n");
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get the expected entry */
	if (res->count != 1) {
		OC_DEBUG(1, "exchange_emsmdb: [OXOMSG] AddressTypes unexpected entry count: %i (expected 1).\n", res->count);
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get the expected number of elements in our record */
	if (res->msgs[0]->num_elements != 1) {
	  	OC_DEBUG(1, "exchange_emsmdb: [OXOMSG] AddressTypes unexpected element count: %i (expected 1).\n", res->msgs[0]->num_elements);
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get at least one address type, things are probably bad. It _could_ be allowable though. */
	if (res->msgs[0]->elements[0].num_values < 1) {
		OC_DEBUG(1, "exchange_emsmdb: [OXOMSG] AddressTypes unexpected values count: %i (expected 1).\n", res->msgs[0]->num_elements);
	}

	/* If we got to here, it looks sane. Build the reply message. */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;
	mapi_repl->u.mapi_AddressTypes.cValues = res->msgs[0]->elements[0].num_values;
	mapi_repl->u.mapi_AddressTypes.size = 0;
	mapi_repl->u.mapi_AddressTypes.transport = talloc_array(mem_ctx, struct mapi_LPSTR, mapi_repl->u.mapi_AddressTypes.cValues);
	for (j = 0; j < mapi_repl->u.mapi_AddressTypes.cValues; ++j) {
		const char *addr_type;
		addr_type = (const char *)res->msgs[0]->elements[0].values[j].data;
		mapi_repl->u.mapi_AddressTypes.transport[j].lppszA = talloc_asprintf(mem_ctx, "%s", addr_type);
		mapi_repl->u.mapi_AddressTypes.size += (strlen(mapi_repl->u.mapi_AddressTypes.transport[j].lppszA) + 1);
	}
	*size += libmapiserver_RopGetAddressTypes_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}

/**
   \details EcDoRpc TransportSend (0x4a) Rop. This operation sends a message.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the TransportSend EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the TransportSend EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopTransportSend(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	struct TransportSend_repl	*response;
	enum MAPISTATUS			retval;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	void				*private_data;
	bool				mapistore = false;
	struct emsmdbp_object		*object;

	OC_DEBUG(4, "exchange_emsmdb: [OXOMSG] TransportSend (0x4a)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MESSAGE) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	response = &mapi_repl->u.mapi_TransportSend;

	mapistore = emsmdbp_is_mapistore(object);
	switch ((int)mapistore) {
	case false:
		OC_DEBUG(0, "Not implemented yet - shouldn't occur\n");
		break;
	case true:
		retval = emsmdbp_object_attach_sharing_metadata_XML_file(emsmdbp_ctx, object);
		if (retval != MAPI_E_SUCCESS) {
			OC_DEBUG(0, "Failing to create sharing metadata for a sharing object: %s\n", mapi_get_errstr(retval));
		}
		mapistore_message_submit(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, 0);

		oxomsg_mapistore_handle_message_relocation(emsmdbp_ctx, object);
		/* mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, messageID); */
		break;
	}

	response->NoPropertiesReturned = 1;

 end:
	*size += libmapiserver_RopTransportSend_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc OptionsData (0x6f) Rop. This doesn't really do anything,
   but could be used to provide HelpData if we wanted to do something like that
   later.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OptionsData EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the OptionsData EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOptionsData(TALLOC_CTX *mem_ctx,
						struct emsmdbp_context *emsmdbp_ctx,
						struct EcDoRpc_MAPI_REQ *mapi_req,
						struct EcDoRpc_MAPI_REPL *mapi_repl,
						uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;

	OC_DEBUG(4, "exchange_emsmdb: [OXOMSG] OptionsData (0x6f)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;
	mapi_repl->u.mapi_OptionsData.Reserved = 0x01; /* always 1, as specified in the doc */
	mapi_repl->u.mapi_OptionsData.OptionsInfo.cb = 0x0121; /* Outlook expects a 300 bytes response, full of 0s */
	mapi_repl->u.mapi_OptionsData.OptionsInfo.lpb = talloc_zero_array(mem_ctx, uint8_t, mapi_repl->u.mapi_OptionsData.OptionsInfo.cb);
	
	mapi_repl->u.mapi_OptionsData.HelpFileSize = 0x0000;
	mapi_repl->u.mapi_OptionsData.HelpFile = talloc_zero_array(mem_ctx, uint8_t, mapi_repl->u.mapi_OptionsData.HelpFileSize);

	*size += libmapiserver_RopOptionsData_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}

/**
   \details EcDoRpc GetTransportFolder (0x6d) ROP.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetTransportFolder EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetTransportFolder EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetTransportFolder(TALLOC_CTX *mem_ctx,
						       struct emsmdbp_context *emsmdbp_ctx,
						       struct EcDoRpc_MAPI_REQ *mapi_req,
						       struct EcDoRpc_MAPI_REPL *mapi_repl,
						       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object = NULL;
	void			*private_data = NULL;
	uint32_t		handle;

	OC_DEBUG(4, "exchange_emsmdb: [OXOMSG] GetTransportFolder (0x6d)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* Step 1. Ensure the referring MAPI handle is mailbox one */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(rec, (void **)&private_data);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MAILBOX) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  invalid object\n");
		goto end;
	}

	/* Step 2. Search for the specified MessageClass substring within user mailbox */
	retval = openchangedb_get_TransportFolder(emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username,
						  &mapi_repl->u.mapi_GetTransportFolder.FolderId);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
	}

end:
 	*size += libmapiserver_RopGetTransportFolder_size(mapi_repl);

 	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return MAPI_E_SUCCESS;
}
