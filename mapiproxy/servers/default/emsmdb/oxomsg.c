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
	uint64_t		messageID;
	uint32_t		contextID;
	uint8_t			flags;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SubmitMessage (0x32)\n"));

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
	switch (mapistore) {
	case false:
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
		messageID = object->object.message->messageID;
		contextID = emsmdbp_get_contextID(object);
		flags = mapi_req->u.mapi_SubmitMessage.SubmitFlags;
		mapistore_message_submit(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, flags);
		mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, messageID);
		break;
	}

 end:
	*size += libmapiserver_RopSubmitMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/* Get the organisation name (like "First Organization") as a DN. */
static bool mapiserver_get_org_dn(struct emsmdbp_context *emsmdbp_ctx,
					    struct ldb_dn **basedn)
{
	int			ret;
	struct ldb_result	*res = NULL;

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_config_basedn(emsmdbp_ctx->samdb_ctx),
                         LDB_SCOPE_SUBTREE, NULL,
			 "(|(objectClass=msExchOrganizationContainer))");

	/* If the search failed */
        if (ret != LDB_SUCCESS) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] mapiserver_get_org_dn ldb_search failure.\n"));
		return false;
        }
        /* If we didn't get the expected entry */
	if (res->count != 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] mapiserver_get_org_dn unexpected entry count: %i (expected 1).\n", res->count));
		return false;
	}
	
	*basedn = ldb_dn_new(emsmdbp_ctx, emsmdbp_ctx->samdb_ctx,
			     ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	return true;
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
	DEBUG(4, ("exchange_emsmdb: [OXOMSG] SetSpooler (0x47)\n"));

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
	
	DEBUG(4, ("exchange_emsmdb: [OXOMSG] AddressTypes (0x49)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapiserver_get_org_dn(emsmdbp_ctx, &basedn);
	ldb_dn_add_child_fmt(basedn, "CN=ADDRESSING");
	ldb_dn_add_child_fmt(basedn, "CN=ADDRESS-TEMPLATES");

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res, basedn,
                         LDB_SCOPE_SUBTREE, attrs, "CN=%x", emsmdbp_ctx->userLanguage);
        /* If the search failed */
        if (ret != LDB_SUCCESS) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes ldb_search failure.\n"));
		return MAPI_E_CORRUPT_STORE;
        }
        /* If we didn't get the expected entry */
	if (res->count != 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes unexpected entry count: %i (expected 1).\n", res->count));
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get the expected number of elements in our record */
	if (res->msgs[0]->num_elements != 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes unexpected element count: %i (expected 1).\n", res->msgs[0]->num_elements));
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get at least one address type, things are probably bad. It _could_ be allowable though. */
	if (res->msgs[0]->elements[0].num_values < 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes unexpected values count: %i (expected 1).\n", res->msgs[0]->num_elements));
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
	struct TransportSend_req	 *request;
	struct TransportSend_repl	 *response;
	enum MAPISTATUS			retval;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	void				*private_data;
	bool				mapistore = false;
	struct emsmdbp_object		*object;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] TransportSend (0x4a)\n"));

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

	request = &mapi_req->u.mapi_TransportSend;
	response = &mapi_repl->u.mapi_TransportSend;

	mapistore = emsmdbp_is_mapistore(object);
	switch (mapistore) {
	case false:
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
		mapistore_message_submit(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, 0);
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

	DEBUG(4, ("exchange_emsmdb: [OXOMSG] OptionsData (0x6f)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;
	mapi_repl->u.mapi_OptionsData.Reserved = 0x00;
	mapi_repl->u.mapi_OptionsData.OptionsInfo.cb = 0x0000;
	mapi_repl->u.mapi_OptionsData.OptionsInfo.lpb = talloc_array(mem_ctx, uint8_t, mapi_repl->u.mapi_OptionsData.OptionsInfo.cb);
	mapi_repl->u.mapi_OptionsData.HelpFileSize = 0x0000;
	mapi_repl->u.mapi_OptionsData.HelpFile = talloc_array(mem_ctx, uint8_t, mapi_repl->u.mapi_OptionsData.HelpFileSize);

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

 	DEBUG(4, ("exchange_emsmdb: [OXOMSG] GetTransportFolder (0x6d)\n"));

	/* Step 1. Ensure the referring MAPI handle is mailbox one */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	retval = mapi_handles_get_private_data(rec, (void **)&private_data);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);
	object = (struct emsmdbp_object *) private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MAILBOX) {
		mapi_repl->error_code = ecNullObject;
		DEBUG(5, ("  invalid object\n"));
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
