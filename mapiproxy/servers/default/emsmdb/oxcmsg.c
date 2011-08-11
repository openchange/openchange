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
   \file oxcmsg.c

   \brief Message and Attachment object routines and Rops
 */

#include <sys/time.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details EcDoRpc OpenMessage (0x03) Rop. This operation opens an
   existing message in a mailbox.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOpenMessage(TALLOC_CTX *mem_ctx,
						struct emsmdbp_context *emsmdbp_ctx,
						struct EcDoRpc_MAPI_REQ *mapi_req,
						struct EcDoRpc_MAPI_REPL *mapi_repl,
						uint32_t *handles, uint16_t *size)
{
	struct OpenMessage_req		*request;
	struct OpenMessage_repl		*response;
	/* int				ret; */
	enum MAPISTATUS			retval;
	uint32_t			parent_handle_id;
	struct mapi_handles		*object_handle = NULL;
	struct mapi_handles		*context_object_handle = NULL;
	struct emsmdbp_object		*object = NULL;
	struct emsmdbp_object		*context_object = NULL;
	struct mapistore_message	*msg;
	void				*data;
	uint64_t			folderID;
	uint64_t			messageID = 0;
	/* uint32_t			contextID; */
	uint32_t			handle;
	/* bool				mapistore = false; */
	/* struct indexing_folders_list	*flist; */
	struct SPropTagArray		*SPropTagArray;
	char				*subject = NULL, *subject_prefix = NULL;
	int				i;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] OpenMessage (0x03)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_OpenMessage;
	response = &mapi_repl->u.mapi_OpenMessage;

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = request->handle_idx;

	parent_handle_id = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, parent_handle_id, &context_object_handle);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* With OpenMessage, the parent object may NOT BE the direct parent folder of the message */
	mapi_handles_get_private_data(context_object_handle, &data);
	context_object = (struct emsmdbp_object *)data;
	if (!context_object) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		*size += libmapiserver_RopOpenMessage_size(NULL);
		return MAPI_E_SUCCESS;
	}

	/* OpenMessage can only be called for mailbox/folder objects */
	if (!(context_object->type == EMSMDBP_OBJECT_MAILBOX || context_object->type == EMSMDBP_OBJECT_FOLDER)) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		*size += libmapiserver_RopOpenMessage_size(NULL);
		return MAPI_E_SUCCESS;
	}

	messageID = request->MessageId;
	folderID = request->FolderId;

	/* Initialize Message object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &object_handle);
	object = emsmdbp_object_message_open(object_handle, emsmdbp_ctx, context_object, folderID, messageID, &msg);
	if (!object) {
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, object_handle->handle);
		if ((request->OpenModeFlags & Create)) {
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		}
		else {
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
		}
		goto end;
	}
	handles[mapi_repl->handle_idx] = object_handle->handle;
	retval = mapi_handles_set_private_data(object_handle, object);

	/* Build the OpenMessage reply */
	response->HasNamedProperties = false;
				
	subject_prefix = (char *) find_SPropValue_data(msg->properties, PR_SUBJECT_PREFIX_UNICODE);
	if (subject_prefix && strlen(subject_prefix) > 0) {
		response->SubjectPrefix.StringType = StringType_UNICODE;
		response->SubjectPrefix.String.lpszW = talloc_strdup(mem_ctx, subject_prefix);
	}
	else {
		response->SubjectPrefix.StringType = StringType_EMPTY;
	}
	
	subject = (char *) find_SPropValue_data(msg->properties, PR_NORMALIZED_SUBJECT_UNICODE);
	if (subject && strlen(subject) > 0) {
		response->NormalizedSubject.StringType = StringType_UNICODE;
		response->NormalizedSubject.String.lpszW = talloc_strdup(mem_ctx, subject);
	}
	else {
		response->NormalizedSubject.StringType = StringType_EMPTY;
	}
				
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x4, PR_DISPLAY_TYPE, PR_OBJECT_TYPE, PR_7BIT_DISPLAY_NAME_UNICODE, PR_SMTP_ADDRESS_UNICODE);
	response->RecipientColumns.cValues = SPropTagArray->cValues;
	response->RecipientColumns.aulPropTag = SPropTagArray->aulPropTag;
	if (msg->recipients) {
		response->RecipientCount = msg->recipients->cRows;
		response->recipients = talloc_array(mem_ctx,
						    struct OpenMessage_recipients,
						    msg->recipients->cRows + 1);
		for (i = 0; i < msg->recipients->cRows; i++) {
			response->recipients[i].RecipClass = msg->recipients->aRow[i].lpProps[0].value.l;
			response->recipients[i].codepage = CP_USASCII;
			response->recipients[i].Reserved = 0;
			emsmdbp_resolve_recipient(mem_ctx, emsmdbp_ctx,
						  (char *)msg->recipients->aRow[i].lpProps[2].value.lpszA,
						  &(response->RecipientColumns),
						  &(response->recipients[i].RecipientRow));
		}
	}
	else {
		response->RecipientCount = 0;
	}
	response->RowCount = response->RecipientCount;

end:
	*size += libmapiserver_RopOpenMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc CreateMessage (0x06) Rop. This operation creates a
   message object in the mailbox.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the CreateMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the CreateMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopCreateMessage(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	struct mapi_handles		*context_handle = NULL;
	struct mapi_handles		*message_handle = NULL;
	struct emsmdbp_object		*context_object = NULL;
	struct emsmdbp_object		*folder_object = NULL;
	struct emsmdbp_object		*message_object = NULL;
	uint32_t			handle;
	uint64_t			folderID, messageID;
	uint32_t			contextID;
	bool				mapistore = false;
	void				*data;
	struct SRow			aRow;
	uint32_t			pt_long;
	bool				pt_boolean;
	struct timeval			tv;
	struct FILETIME			ft;
	NTTIME				time;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] CreateMessage (0x06)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_CreateMessage.handle_idx;
	mapi_repl->u.mapi_CreateMessage.HasMessageId = 0;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &context_handle);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}
	/* With CreateMessage, the parent object may NOT BE the direct parent folder of the message */
	retval = mapi_handles_get_private_data(context_handle, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}
	context_object = data;

	folderID = mapi_req->u.mapi_CreateMessage.FolderId;

	/* Step 1. Retrieve parent handle in the hierarchy */
	folder_object = emsmdbp_object_open_folder_by_fid(mem_ctx, emsmdbp_ctx, context_object, folderID);
	contextID = emsmdbp_get_contextID(folder_object);
	mapistore = emsmdbp_is_mapistore(folder_object);

	if (mapistore) {
		/* This should be handled differently here: temporary hack */
		retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &messageID);
		if (retval) {
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
			goto end;
		}
		mapi_repl->u.mapi_CreateMessage.HasMessageId = 1;
		mapi_repl->u.mapi_CreateMessage.MessageId.MessageId = messageID;

		/* Initialize Message object */
		handle = handles[mapi_req->handle_idx];
		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &message_handle);
		handles[mapi_repl->handle_idx] = message_handle->handle;

		message_object = emsmdbp_object_message_init((TALLOC_CTX *)message_handle, emsmdbp_ctx, messageID, folder_object);
		mapistore_folder_create_message(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, message_object, messageID, mapi_req->u.mapi_CreateMessage.AssociatedFlag, &message_object->backend_object);

		/* Add default properties to message MS-OXCMSG 3.2.5.2 */
		retval = mapi_handles_set_private_data(message_handle, message_object);

		/* Set default properties for message: MS-OXCMSG 3.2.5.2 */
		aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, 2);
		aRow.cValues = 0;

		pt_long = 0x1;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_IMPORTANCE, (const void *)&pt_long);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_MESSAGE_CLASS, (const void *)"IPM.Note");
		pt_long = 0x0;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_SENSITIVITY, (const void *)&pt_long);
		pt_long = 0x9;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_MESSAGE_FLAGS, (const void *)&pt_long);
		pt_boolean = false;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_HASATTACH, (const void *)&pt_boolean);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_URL_COMP_NAME_SET, (const void *)&pt_boolean);
		pt_long = 0x1;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_TRUST_SENDER, (const void *)&pt_long);
		pt_long = 0x3;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_ACCESS, (const void *)&pt_long);
		pt_long = 0x1;
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_ACCESS_LEVEL, (const void *)&pt_long);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_URL_COMP_NAME, (const void *)"No Subject.EML");

		gettimeofday(&tv, NULL);
		time = timeval_to_nttime(&tv);
		ft.dwLowDateTime = (time << 32) >> 32;
		ft.dwHighDateTime = time >> 32;		
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_CREATION_TIME, (const void *)&ft);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_LAST_MODIFICATION_TIME, (const void *)&ft);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_LOCAL_COMMIT_TIME, (const void *)&ft);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_MESSAGE_LOCALE_ID, (const void *)&mapi_req->u.mapi_CreateMessage.CodePageId);
		aRow.lpProps = add_SPropValue(mem_ctx, aRow.lpProps, &aRow.cValues, PR_LOCALE_ID, (const void *)&mapi_req->u.mapi_CreateMessage.CodePageId);

		/* TODO: some required properties are not set: PidTagSearchKey, PidTagCreatorName, ... */
		emsmdbp_object_set_properties(emsmdbp_ctx, message_object, &aRow);
	}
	else {
		/* system/special folder */
		DEBUG(0, ("[%s] - not implemented yet - shouldn't occur\n", __location__));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	DEBUG(0, ("CreateMessage: 0x%.16"PRIx64": mapistore = %s\n", folderID, 
		  emsmdbp_is_mapistore(folder_object) == true ? "true" : "false"));

end:

	*size += libmapiserver_RopCreateMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SaveChangesMessage (0x0c) Rop. This operation
   operation commits the changes made to a message.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SaveChangesMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SaveChangesMessage
   EcDoRpc_MAPI_REPL structure

   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSaveChangesMessage(TALLOC_CTX *mem_ctx,
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

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SaveChangesMessage (0x0c)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	handle = handles[mapi_req->u.mapi_SaveChangesMessage.handle_idx];
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
		DEBUG(0, ("Not implement yet - shouldn't occur\n"));
		break;
	case true:
                contextID = emsmdbp_get_contextID(object);
		messageID = object->object.message->messageID;
		flags = mapi_req->u.mapi_SaveChangesMessage.SaveFlags;
		mapistore_message_save(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object);
                mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, messageID);
		break;
	}

	mapi_repl->u.mapi_SaveChangesMessage.handle_idx = mapi_req->u.mapi_SaveChangesMessage.handle_idx;
	mapi_repl->u.mapi_SaveChangesMessage.MessageId = object->object.message->messageID;

end:
	*size += libmapiserver_RopSaveChangesMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc RemoveAllRecipients (0x0d) Rop. This operation removes all
   recipients from a message.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the RemoveAllRecipients EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the RemoveAllRecipients EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopRemoveAllRecipients(TALLOC_CTX *mem_ctx,
							struct emsmdbp_context *emsmdbp_ctx,
							struct EcDoRpc_MAPI_REQ *mapi_req,
							struct EcDoRpc_MAPI_REPL *mapi_repl,
							uint32_t *handles, uint16_t *size)
{
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object;
	enum MAPISTATUS		retval;
	uint32_t		handle;
	void			*private_data;
	bool			mapistore = false;
	uint32_t		contextID;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] RemoveAllRecipients (0x0d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	mapi_repl->handle_idx = mapi_req->handle_idx;

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MESSAGE) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(object);
	if (mapistore) {
		contextID = emsmdbp_get_contextID(object);
		mapistore_message_modify_recipients(emsmdbp_ctx->mstore_ctx, contextID,
						    object->backend_object, NULL, 0);
	}
	else {
		DEBUG(0, ("Not implement yet - shouldn't occur\n"));
	}

end:
	*size += libmapiserver_RopRemoveAllRecipients_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc ModifyRecipients (0x0e) Rop. This operation modifies an
   existing message to add recipients (TO, CC, BCC).

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the ModifyRecipients EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the ModifyRecipients EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopModifyRecipients(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*object;
	enum MAPISTATUS		retval;
	uint32_t		handle;
	void			*private_data;
	bool			mapistore = false;
	uint32_t		contextID;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] ModifyRecipients (0x0e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	mapi_repl->handle_idx = mapi_req->handle_idx;

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MESSAGE) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(object);
	if (mapistore) {
		contextID = emsmdbp_get_contextID(object);
		mapistore_message_modify_recipients(emsmdbp_ctx->mstore_ctx, contextID,
						    object->backend_object, mapi_req->u.mapi_ModifyRecipients.RecipientRow, mapi_req->u.mapi_ModifyRecipients.cValues);
		/* mapistore_savechangesmessage(emsmdbp_ctx->mstore_ctx, contextID, messageID, flags); */
	}
	else {
		DEBUG(0, ("Not implement yet - shouldn't occur\n"));
	}

end:
	*size += libmapiserver_RopModifyRecipients_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc ReloadCachedInformation (0x10) Rop. This operation
   gets message and recipient information from a message.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the ReloadCachedInformation
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the ReloadCachedInformation
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopReloadCachedInformation(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    struct EcDoRpc_MAPI_REQ *mapi_req,
							    struct EcDoRpc_MAPI_REPL *mapi_repl,
							    uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS			retval;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	void				*private_data;
	bool				mapistore = false;
	struct mapistore_message	*msg;
	struct emsmdbp_object		*object;
	void				*backend_object;
	uint64_t			messageID;
	uint32_t			contextID;
	struct SPropTagArray		*SPropTagArray;
	char				*subject = NULL;
	int				i;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] ReloadCachedInformation (0x10)\n"));

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

		if (mapistore_folder_open_message(emsmdbp_ctx->mstore_ctx, contextID, object->parent_object->backend_object, mem_ctx, messageID, &backend_object, &msg)) {
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
			goto end;
		}

		/* Build the ReloadCachedInformation reply */
		subject = (char *) find_SPropValue_data(msg->properties, PR_SUBJECT);
		mapi_repl->u.mapi_ReloadCachedInformation.HasNamedProperties = false;
		mapi_repl->u.mapi_ReloadCachedInformation.SubjectPrefix.StringType = StringType_EMPTY;
		if (subject) {
			mapi_repl->u.mapi_ReloadCachedInformation.NormalizedSubject.StringType = StringType_UNICODE;
			mapi_repl->u.mapi_ReloadCachedInformation.NormalizedSubject.String.lpszW = talloc_strdup(mem_ctx, subject);
		} else {
			mapi_repl->u.mapi_ReloadCachedInformation.NormalizedSubject.StringType = StringType_EMPTY;
		}

		SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
						  PR_DISPLAY_TYPE,
						  PR_OBJECT_TYPE,
						  PR_7BIT_DISPLAY_NAME_UNICODE,
						  PR_SMTP_ADDRESS_UNICODE);
		mapi_repl->u.mapi_ReloadCachedInformation.RecipientColumns.cValues = SPropTagArray->cValues;
		mapi_repl->u.mapi_ReloadCachedInformation.RecipientColumns.aulPropTag = SPropTagArray->aulPropTag;
		if (msg->recipients) {
			mapi_repl->u.mapi_ReloadCachedInformation.RowCount = msg->recipients->cRows;
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientCount = msg->recipients->cRows;
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows = talloc_array(mem_ctx, 
											       struct OpenRecipientRow, 
											       msg->recipients->cRows + 1);
			for (i = 0; i < msg->recipients->cRows; i++) {
				mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows[i].RecipientType = msg->recipients->aRow[i].lpProps[0].value.l;
				mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows[i].CodePageId = CP_USASCII;
				mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows[i].Reserved = 0;
				emsmdbp_resolve_recipient(mem_ctx, emsmdbp_ctx, 
							  (char *)msg->recipients->aRow[i].lpProps[2].value.lpszA,
							  &(mapi_repl->u.mapi_ReloadCachedInformation.RecipientColumns),
							  &(mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows[i].RecipientRow));
			}
		}
		else {
			mapi_repl->u.mapi_ReloadCachedInformation.RowCount = 0;
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientCount = 0;
		}
		break;
	}

end:
	*size += libmapiserver_RopReloadCachedInformation_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc SetMessageReadFlag (0x11) Rop. This operation sets
   or clears the message read flag.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SetMessageReadFlag EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SetMessageReadFlag
   EcDoRpc_MAPI_REPL structure

   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSetMessageReadFlag(TALLOC_CTX *mem_ctx,
						       struct emsmdbp_context *emsmdbp_ctx,
						       struct EcDoRpc_MAPI_REQ *mapi_req,
						       struct EcDoRpc_MAPI_REPL *mapi_repl,
						       uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SetMessageReadFlag (0x11) -- stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->u.mapi_SetMessageReadFlag.ReadStatusChanged = false;

	*size += libmapiserver_RopSetMessageReadFlag_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetAttachmentTable (0x21) Rop. This operation gets
   the attachment table of a message.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetAttachmentTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetAttachmentTable
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetAttachmentTable(TALLOC_CTX *mem_ctx,
						       struct emsmdbp_context *emsmdbp_ctx,
						       struct EcDoRpc_MAPI_REQ *mapi_req,
						       struct EcDoRpc_MAPI_REPL *mapi_repl,
						       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	struct mapi_handles		*rec = NULL;
	struct mapi_handles		*table_rec = NULL;
	struct emsmdbp_object		*message_object = NULL;
	struct emsmdbp_object		*table_object = NULL;
	void				*data;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] GetAttachmentTable (0x21)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetAttachmentTable.handle_idx;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	message_object = (struct emsmdbp_object *) data;
	if (!message_object || message_object->type != EMSMDBP_OBJECT_MESSAGE) {
		DEBUG(5, ("  no object or object is not a message\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &table_rec);
	handles[mapi_repl->handle_idx] = table_rec->handle;

	table_object = emsmdbp_object_message_open_attachment_table(table_rec, emsmdbp_ctx, message_object);
	if (!table_object) {
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, table_rec->handle);
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}
	mapi_handles_set_private_data(table_rec, table_object);
	
 end:
	*size += libmapiserver_RopGetAttachmentTable_size(mapi_repl);

	return MAPI_E_SUCCESS;	
}

/**
   \details EcDoRpc OpenAttach (0x22) Rop. This operation open an attachment
   from the message handle.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenAttach EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenAttach
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOpenAttach(TALLOC_CTX *mem_ctx,
                                               struct emsmdbp_context *emsmdbp_ctx,
                                               struct EcDoRpc_MAPI_REQ *mapi_req,
                                               struct EcDoRpc_MAPI_REPL *mapi_repl,
                                               uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	uint32_t		attachmentID;
	uint32_t		contextID;
	struct mapi_handles		*rec = NULL;
	struct mapi_handles		*attachment_rec = NULL;
	struct emsmdbp_object		*message_object = NULL;
	struct emsmdbp_object		*attachment_object = NULL;
	void				*data;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] OpenAttach (0x22)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_OpenAttach.handle_idx;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	message_object = (struct emsmdbp_object *) data;
	if (!message_object || message_object->type != EMSMDBP_OBJECT_MESSAGE) {
		DEBUG(5, ("  no object or object is not a message\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	switch (emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
                contextID = emsmdbp_get_contextID(message_object);
                attachmentID = mapi_req->u.mapi_OpenAttach.AttachmentID;

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &attachment_rec);
		handles[mapi_repl->handle_idx] = attachment_rec->handle;

		attachment_object = emsmdbp_object_attachment_init((TALLOC_CTX *)attachment_rec, emsmdbp_ctx,
								   message_object->object.message->messageID, message_object);
		if (attachment_object) {
			retval = mapistore_message_open_attachment(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object,
								   attachment_object, attachmentID, &attachment_object->backend_object);
			if (retval) {
				mapi_handles_delete(emsmdbp_ctx->handles_ctx, attachment_rec->handle);
				DEBUG(5, ("could not open nor create mapistore message\n"));
				mapi_repl->error_code = MAPI_E_NOT_FOUND;
			}
			retval = mapi_handles_set_private_data(attachment_rec, attachment_object);
		}
        }

 end:
	*size += libmapiserver_RopOpenAttach_size(mapi_repl);

	return MAPI_E_SUCCESS;	
}

/**
   \details EcDoRpc CreateAttach (0x23) Rop. This operation open an attachment
   from the message handle.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the CreateAttach EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the CreateAttach
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopCreateAttach(TALLOC_CTX *mem_ctx,
                                                 struct emsmdbp_context *emsmdbp_ctx,
                                                 struct EcDoRpc_MAPI_REQ *mapi_req,
                                                 struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	uint32_t		contextID;
	uint64_t		messageID;
	struct mapi_handles		*rec = NULL;
	struct mapi_handles		*attachment_rec = NULL;
	struct emsmdbp_object		*message_object = NULL;
	struct emsmdbp_object		*attachment_object = NULL;
	void				*data;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] CreateAttach (0x23)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_CreateAttach.handle_idx;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("  handle data not found, idx = %x\n", mapi_req->handle_idx));
		goto end;
	}

	message_object = (struct emsmdbp_object *) data;
	if (!message_object || message_object->type != EMSMDBP_OBJECT_MESSAGE) {
		DEBUG(5, ("  no object or object is not a message\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	switch (emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
                messageID = message_object->object.message->messageID;
                contextID = emsmdbp_get_contextID(message_object);

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &attachment_rec);
		handles[mapi_repl->handle_idx] = attachment_rec->handle;
		
		attachment_object = emsmdbp_object_attachment_init((TALLOC_CTX *)attachment_rec, emsmdbp_ctx,
								   messageID, message_object);
		if (attachment_object) {
			retval = mapistore_message_create_attachment(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object,
								     attachment_object, &attachment_object->backend_object, &mapi_repl->u.mapi_CreateAttach.AttachmentID);
			if (retval) {
				mapi_handles_delete(emsmdbp_ctx->handles_ctx, attachment_rec->handle);
				DEBUG(5, ("could not open nor create mapistore message\n"));
				mapi_repl->error_code = MAPI_E_NOT_FOUND;
			}
			retval = mapi_handles_set_private_data(attachment_rec, attachment_object);
		}
        }

 end:
	*size += libmapiserver_RopCreateAttach_size(mapi_repl);

	return MAPI_E_SUCCESS;	
}

/**
   \details EcDoRpc SaveChangesAttachment (0x25) Rop. This operation open an attachment
   from the message handle.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SaveChangesAttachment EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SaveChangesAttachment
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSaveChangesAttachment(TALLOC_CTX *mem_ctx,
                                                          struct emsmdbp_context *emsmdbp_ctx,
                                                          struct EcDoRpc_MAPI_REQ *mapi_req,
                                                          struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                          uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SaveChangesAttachment (0x25) -- valid stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_SaveChangesAttachment.handle_idx;

	*size += libmapiserver_RopSaveChangesAttachment_size(mapi_repl);

	return MAPI_E_SUCCESS;	
}

/**
   \details EcDoRpc OpenEmbeddedMessage (0x46) Rop. This operation open an attachment
   from the message handle.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenEmbeddedMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenEmbeddedMessage
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOpenEmbeddedMessage(TALLOC_CTX *mem_ctx,
                                                        struct emsmdbp_context *emsmdbp_ctx,
                                                        struct EcDoRpc_MAPI_REQ *mapi_req,
                                                        struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                        uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS                 retval;
        uint32_t                        handle;
        uint32_t                        contextID;
        uint64_t                        messageID;
	struct mapi_handles		*attachment_rec = NULL;
	struct mapi_handles		*message_rec = NULL;
        struct mapistore_message	*msg;
        void                            *backend_attachment_message;
	struct emsmdbp_object           *attachment_object = NULL;
	struct emsmdbp_object           *message_object = NULL;
        bool                            mapistore;
	struct SPropTagArray		*SPropTagArray;
	char				*subject = NULL, *subject_prefix = NULL;
        uint16_t                        i;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] OpenEmbeddedMessage (0x46)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_OpenEmbeddedMessage.handle_idx;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &attachment_rec);
	if (retval) {
		DEBUG(5, ("  handle (%x) not found: %x\n", handle, mapi_req->handle_idx));
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	retval = mapi_handles_get_private_data(attachment_rec, (void *) &attachment_object);
	if (!attachment_object || attachment_object->type != EMSMDBP_OBJECT_ATTACHMENT) {
		DEBUG(5, ("  no object or object is not an attachment\n"));
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

        memset(&mapi_repl->u.mapi_OpenEmbeddedMessage, 0, sizeof(struct OpenEmbeddedMessage_repl));

	mapistore = emsmdbp_is_mapistore(attachment_object);
	switch (mapistore) {
	case false:
		DEBUG(0, ("Not implemented - shouldn't occur\n"));
		break;
	case true:
                if (mapi_req->u.mapi_OpenEmbeddedMessage.OpenModeFlags == MAPI_CREATE) {
                        retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &messageID);
                        if (retval) {
                                mapi_repl->error_code = MAPI_E_NO_SUPPORT;
                                goto end;
                        }
                }

		contextID = emsmdbp_get_contextID(attachment_object);
		retval = mapistore_message_attachment_open_embedded_message(emsmdbp_ctx->mstore_ctx, contextID, attachment_object->backend_object,
									    NULL, &backend_attachment_message,
									    &messageID,
									    &msg);
                if (retval != MAPISTORE_SUCCESS) {
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
			goto end;
                }

                mapi_repl->u.mapi_OpenEmbeddedMessage.MessageId = messageID;

                mapi_repl->u.mapi_OpenEmbeddedMessage.SubjectPrefix.StringType = StringType_EMPTY;
                mapi_repl->u.mapi_OpenEmbeddedMessage.NormalizedSubject.StringType = StringType_EMPTY;
                if (msg->properties != NULL) {
                        subject_prefix = (char *) find_SPropValue_data(msg->properties, PR_SUBJECT_PREFIX_UNICODE);
                        if (subject_prefix && strlen(subject_prefix) > 0) {
                                mapi_repl->u.mapi_OpenEmbeddedMessage.SubjectPrefix.StringType = StringType_UNICODE;
                                mapi_repl->u.mapi_OpenEmbeddedMessage.SubjectPrefix.String.lpszW = talloc_strdup(mem_ctx, subject_prefix);
                        }
                
                        subject = (char *) find_SPropValue_data(msg->properties, PR_NORMALIZED_SUBJECT_UNICODE);
                        if (subject && strlen(subject) > 0) {
                                mapi_repl->u.mapi_OpenEmbeddedMessage.NormalizedSubject.StringType = StringType_UNICODE;
                                mapi_repl->u.mapi_OpenEmbeddedMessage.NormalizedSubject.String.lpszW = talloc_strdup(mem_ctx, subject);
                        }
                }

                SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
                                                  PR_DISPLAY_TYPE,
                                                  PR_OBJECT_TYPE,
                                                  PR_7BIT_DISPLAY_NAME_UNICODE,
                                                  PR_SMTP_ADDRESS_UNICODE);
                mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientColumns.cValues = SPropTagArray->cValues;
                mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientColumns.aulPropTag = SPropTagArray->aulPropTag;
                if (msg->recipients != NULL) {
                        mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientCount = msg->recipients->cRows;
                        mapi_repl->u.mapi_OpenEmbeddedMessage.RowCount = msg->recipients->cRows;
                        mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientRows = talloc_array(mem_ctx, 
                                                                                           struct OpenRecipientRow, 
                                                                                           msg->recipients->cRows + 1);
                        for (i = 0; i < msg->recipients->cRows; i++) {
                                mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientRows[i].RecipientType = msg->recipients->aRow[i].lpProps[0].value.l;
                                mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientRows[i].CodePageId = CP_USASCII;
                                mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientRows[i].Reserved = 0;
                                emsmdbp_resolve_recipient(mem_ctx, emsmdbp_ctx,
                                                          (char *)msg->recipients->aRow[i].lpProps[2].value.lpszA,
                                                          &(mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientColumns),
                                                          &(mapi_repl->u.mapi_OpenEmbeddedMessage.RecipientRows[i].RecipientRow));
                        }
                }

                /* Initialize Message object */
                handle = handles[mapi_req->handle_idx];
                retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &message_rec);
                handles[mapi_repl->handle_idx] = message_rec->handle;

                message_object = emsmdbp_object_message_init((TALLOC_CTX *)message_rec, emsmdbp_ctx, messageID, attachment_object);
                message_object->backend_object = backend_attachment_message;
		talloc_reference(message_object, backend_attachment_message);
		talloc_free(backend_attachment_message);
		talloc_free(msg);
                retval = mapi_handles_set_private_data(message_rec, message_object);

		break;
	}

 end:
	*size += libmapiserver_RopOpenEmbeddedMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;	
}
