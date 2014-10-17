/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2014

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

struct oxcmsg_prop_index {
	uint32_t display_name; /* PR_DISPLAY_NAME_UNICODE or PR_7BIT_DISPLAY_NAME_UNICODE or PR_RECIPIENT_DISPLAY_NAME_UNICODE */
	uint32_t email_address; /* PR_EMAIL_ADDRESS_UNICODE or PR_SMTP_ADDRESS_UNICODE */
};

static inline void oxcmsg_fill_prop_index(struct oxcmsg_prop_index *prop_index, struct SPropTagArray *properties)
{
	if (SPropTagArray_find(*properties, PR_DISPLAY_NAME_UNICODE, &prop_index->display_name) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_7BIT_DISPLAY_NAME_UNICODE, &prop_index->display_name) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_RECIPIENT_DISPLAY_NAME_UNICODE, &prop_index->display_name) == MAPI_E_NOT_FOUND) {
		prop_index->display_name = (uint32_t) -1;;
	}
	if (SPropTagArray_find(*properties, PR_EMAIL_ADDRESS_UNICODE, &prop_index->email_address) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_SMTP_ADDRESS_UNICODE, &prop_index->email_address) == MAPI_E_NOT_FOUND) {
		prop_index->email_address = (uint32_t) -1;;
	}
}

static void oxcmsg_fill_RecipientRow(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct RecipientRow *row, struct mapistore_message_recipient *recipient, struct oxcmsg_prop_index *prop_index)
{
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };
	int			ret;
	char			*full_name, *email_address, *simple_name, *legacyExchangeDN;

	if (!recipient->username) {
		goto smtp_recipient;
	}

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs,
			 "(&(objectClass=user)(sAMAccountName=*%s*)(!(objectClass=computer)))",
			 ldb_binary_encode_string(mem_ctx, recipient->username));
	/* If the search failed, build an external recipient: very basic for the moment */
	if (ret != LDB_SUCCESS || !res->count) {
		DEBUG(0, ("record not found for %s\n", recipient->username));
		goto smtp_recipient;
	}
	full_name = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "displayName", NULL);
	if (!full_name) {
		DEBUG(0, ("record found but displayName is missing for %s\n", recipient->username));
		goto smtp_recipient;
	}
	simple_name = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "mailNickname", NULL);
	if (!simple_name) {
		DEBUG(0, ("record found but mailNickname is missing for %s\n", recipient->username));
		goto smtp_recipient;
	}
	legacyExchangeDN = (char *) ldb_msg_find_attr_as_string(res->msgs[0], "legacyExchangeDN", NULL);
	if (!legacyExchangeDN) {
		DEBUG(0, ("record found but legacyExchangeDN is missing for %s\n", recipient->username));
		goto smtp_recipient;
	}

	row->RecipientFlags = 0x06d1;
	row->AddressPrefixUsed.prefix_size = strlen(legacyExchangeDN) - strlen(recipient->username);
	row->DisplayType.display_type = SINGLE_RECIPIENT;
	row->X500DN.recipient_x500name = talloc_strdup(mem_ctx, recipient->username);
	row->DisplayName.lpszW = talloc_strdup(mem_ctx, full_name);
	row->SimpleDisplayName.lpszW = talloc_strdup(mem_ctx, simple_name);

	return;

smtp_recipient:
	row->RecipientFlags = 0x303; /* type = SMTP, no rich text, unicode */
	row->RecipientFlags |= 0x80; /* from doc: a different transport is responsible for delivery to this recipient. */
	if (prop_index->display_name != (uint32_t) -1) {
		full_name = recipient->data[prop_index->display_name];
		if (full_name) {
			row->RecipientFlags |= 0x10;
			row->DisplayName.lpszW = talloc_strdup(mem_ctx, full_name);

			row->RecipientFlags |= 0x0400;
			row->SimpleDisplayName.lpszW = row->DisplayName.lpszW;
		}
	}
	if (prop_index->email_address != (uint32_t) -1) {
		email_address = recipient->data[prop_index->email_address];
		if (email_address) {
			row->RecipientFlags |= 0x08;
			row->EmailAddress.lpszW = talloc_strdup(mem_ctx, email_address);
		}
	}
}

static void oxcmsg_fill_RecipientRow_data(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct RecipientRow *row, struct SPropTagArray *properties, struct mapistore_message_recipient *recipient)
{
	uint32_t	i, retval;
	void		*data;
	enum MAPITAGS	property;

	row->prop_count = properties->cValues;
	row->prop_values.length = 0;
	row->layout = 0;
	for (i = 0; i < properties->cValues; i++) {
		if (recipient->data[i] == NULL) {
			row->layout = 1;
			break;
		}
	}

	for (i = 0; i < properties->cValues; i++) {
		property = properties->aulPropTag[i];
		data = recipient->data[i];
		if (data == NULL) {
			retval = MAPI_E_NOT_FOUND;
			property = (property & 0xffff0000) + PT_ERROR;
			data = (void *)&retval;
		}
		libmapiserver_push_property(mem_ctx,
					    property, (const void *)data, &row->prop_values, 
					    row->layout, 0, 0);
	}
}

static void oxcmsg_fill_OpenRecipientRow(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct OpenRecipientRow *row, struct SPropTagArray *properties, struct mapistore_message_recipient *recipient, struct oxcmsg_prop_index *prop_index)
{
	row->CodePageId = CP_USASCII;
	row->Reserved = 0;
	row->RecipientType = recipient->type;

	oxcmsg_fill_RecipientRow(mem_ctx, emsmdbp_ctx, &row->RecipientRow, recipient, prop_index);
	oxcmsg_fill_RecipientRow_data(mem_ctx, emsmdbp_ctx, &row->RecipientRow, properties, recipient);
}

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
	enum mapistore_error		ret;
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
	struct oxcmsg_prop_index	prop_index;
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
		goto end;
	}

	messageID = request->MessageId;
	folderID = request->FolderId;

	/* Initialize Message object */
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &object_handle);

	if (request->OpenModeFlags == ReadOnly) {
		ret = emsmdbp_object_message_open(object_handle, emsmdbp_ctx, context_object, folderID, messageID, false, &object, &msg);
	}
	else if (request->OpenModeFlags == OpenSoftDelete) {
		ret = MAPISTORE_ERROR;
	}
	else { /* ReadWrite/BestAccess */
		ret = emsmdbp_object_message_open(object_handle, emsmdbp_ctx, context_object, folderID, messageID, true, &object, &msg);
		if (ret == MAPISTORE_ERR_DENIED && request->OpenModeFlags == BestAccess) {
			ret = emsmdbp_object_message_open(object_handle, emsmdbp_ctx, context_object, folderID, messageID, false, &object, &msg);
		}
	}

	if (ret != MAPISTORE_SUCCESS) {
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, object_handle->handle);
		if (ret == MAPISTORE_ERR_DENIED) {
			mapi_repl->error_code = MAPI_E_NO_ACCESS;
		}
		else if (ret == MAPISTORE_ERR_NOT_FOUND) {
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
		}
		else {
			mapi_repl->error_code = MAPI_E_CALL_FAILED;
		}
		goto end;
	}

	handles[mapi_repl->handle_idx] = object_handle->handle;
	retval = mapi_handles_set_private_data(object_handle, object);

	/* Build the OpenMessage reply */
	response->HasNamedProperties = true;

	if (msg->subject_prefix && strlen(msg->subject_prefix) > 0) {
		response->SubjectPrefix.StringType = StringType_UNICODE;
		response->SubjectPrefix.String.lpszW = talloc_strdup(mem_ctx, msg->subject_prefix);
	}
	else {
		response->SubjectPrefix.StringType = StringType_EMPTY;
	}
	if (msg->normalized_subject && strlen(msg->normalized_subject) > 0) {
		response->NormalizedSubject.StringType = StringType_UNICODE;
		response->NormalizedSubject.String.lpszW = talloc_strdup(mem_ctx, msg->normalized_subject);
	}
	else {
		response->NormalizedSubject.StringType = StringType_EMPTY;
	}
	if (msg->columns) {
		response->RecipientColumns.cValues = msg->columns->cValues;
		response->RecipientColumns.aulPropTag = msg->columns->aulPropTag;
	}
	else {
		response->RecipientColumns.cValues = 0;
	}
	response->RecipientCount = msg->recipients_count;
	response->RowCount = msg->recipients_count;
	if (msg->recipients_count > 0) {
		response->RecipientRows = talloc_array(mem_ctx,
						       struct OpenRecipientRow,
						       msg->recipients_count + 1);
		oxcmsg_fill_prop_index(&prop_index, msg->columns);
		for (i = 0; i < msg->recipients_count; i++) {
			oxcmsg_fill_OpenRecipientRow(mem_ctx, emsmdbp_ctx, &(response->RecipientRows[i]), msg->columns, msg->recipients + i, &prop_index);
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
	enum mapistore_error		ret;
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
	struct SBinary_short		*pt_binary;
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
	ret = emsmdbp_object_open_folder_by_fid(mem_ctx, emsmdbp_ctx, context_object, folderID, &folder_object);
	if (ret != MAPISTORE_SUCCESS) {
		if (ret == MAPISTORE_ERR_DENIED) {
			mapi_repl->error_code = MAPI_E_NO_ACCESS;
		}
		else {
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		}
		goto end;
	}

	/* This should be handled differently here: temporary hack */
	ret = mapistore_indexing_get_new_folderID(emsmdbp_ctx->mstore_ctx, &messageID);
	if (ret) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}
	mapi_repl->u.mapi_CreateMessage.HasMessageId = 1;
	mapi_repl->u.mapi_CreateMessage.MessageId.MessageId = messageID;

	/* Initialize Message object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &message_handle);

	message_object = emsmdbp_object_message_init((TALLOC_CTX *)message_handle, emsmdbp_ctx, messageID, folder_object);
	message_object->object.message->read_write = true;

	contextID = emsmdbp_get_contextID(folder_object);
	mapistore = emsmdbp_is_mapistore(folder_object);
	switch ((int)mapistore) {
	case true:
		ret = mapistore_folder_create_message(emsmdbp_ctx->mstore_ctx, contextID, 
						      folder_object->backend_object, message_object, 
						      messageID, mapi_req->u.mapi_CreateMessage.AssociatedFlag, 
						      &message_object->backend_object);
		if (ret != MAPISTORE_SUCCESS) {
			if (ret == MAPISTORE_ERR_DENIED) {
				mapi_repl->error_code = MAPI_E_NO_ACCESS;
			}
			else if (ret == MAPISTORE_ERR_NOT_FOUND) {
				mapi_repl->error_code = MAPI_E_NOT_FOUND;
			}
			else {
				mapi_repl->error_code = MAPI_E_CALL_FAILED;
			}
			goto end;
		}
		break;
	case false:
		retval = openchangedb_message_create(emsmdbp_ctx->mstore_ctx, 
						     emsmdbp_ctx->oc_ctx,
						     emsmdbp_ctx->username,
						     messageID, folderID,
						     mapi_req->u.mapi_CreateMessage.AssociatedFlag,
						     &message_object->backend_object);
		DEBUG(5, ("openchangedb_create_message returned 0x%.8x\n", retval));
		break;
	}

	handles[mapi_repl->handle_idx] = message_handle->handle;

	/* Add default properties to message MS-OXCMSG 3.2.5.2 */
	retval = mapi_handles_set_private_data(message_handle, message_object);

	/* Set default properties for message: MS-OXCMSG 3.2.5.2 */
	aRow.ulAdrEntryPad = 0;
	aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, 23);
	aRow.cValues = 0;

	pt_long = 0x1;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_IMPORTANCE, (const void *)&pt_long);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_MESSAGE_CLASS_UNICODE, (const void *)"IPM.Note");
	aRow.cValues++;
	pt_long = 0x0;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_SENSITIVITY, (const void *)&pt_long);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_DISPLAY_TO_UNICODE, (const void *)"");
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_DISPLAY_CC_UNICODE, (const void *)"");
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_DISPLAY_BCC_UNICODE, (const void *)"");
	aRow.cValues++;
	pt_long = 0x9;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_MESSAGE_FLAGS, (const void *)&pt_long);
	aRow.cValues++;

	pt_boolean = false;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_HASATTACH, (const void *)&pt_boolean);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_HAS_NAMED_PROPERTIES, (const void *)&pt_boolean);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_URL_COMP_NAME_SET, (const void *)&pt_boolean);
	aRow.cValues++;

	pt_long = 0x1;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_TRUST_SENDER, (const void *)&pt_long);
	aRow.cValues++;
	pt_long = 0x3;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_ACCESS, (const void *)&pt_long);
	aRow.cValues++;
	pt_long = 0x1;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_ACCESS_LEVEL, (const void *)&pt_long);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PidTagUrlCompName, (const void *)"No Subject.EML");
	aRow.cValues++;

	gettimeofday(&tv, NULL);
	time = timeval_to_nttime(&tv);
	ft.dwLowDateTime = (time << 32) >> 32;
	ft.dwHighDateTime = time >> 32;		
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_CREATION_TIME, (const void *)&ft);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_LAST_MODIFICATION_TIME, (const void *)&ft);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_LOCAL_COMMIT_TIME, (const void *)&ft);
	aRow.cValues++;

	/* we copy CodePageId (uint16_t) into an uint32_t to avoid a buffer error */
	pt_long = mapi_req->u.mapi_CreateMessage.CodePageId;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_MESSAGE_LOCALE_ID, (const void *)&pt_long);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_LOCALE_ID, (const void *)&pt_long);
	aRow.cValues++;

	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_CREATOR_NAME_UNICODE, (const void *)emsmdbp_ctx->szDisplayName);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_LAST_MODIFIER_NAME_UNICODE, (const void *)emsmdbp_ctx->szDisplayName);
	aRow.cValues++;
	pt_binary = talloc_zero(mem_ctx, struct SBinary_short);
	entryid_set_AB_EntryID(pt_binary, emsmdbp_ctx->szUserDN, pt_binary);
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_CREATOR_ENTRYID, (const void *)pt_binary);
	aRow.cValues++;
	set_SPropValue_proptag(aRow.lpProps + aRow.cValues, PR_LAST_MODIFIER_ENTRYID, (const void *)pt_binary);
	aRow.cValues++;

	/* TODO: some required properties are not set: PidTagSearchKey, PidTagMessageSize, PidTagSecurityDescriptor */
	emsmdbp_object_set_properties(emsmdbp_ctx, message_object, &aRow);

	DEBUG(0, ("CreateMessage: 0x%.16"PRIx64": mapistore = %s\n", folderID, mapistore ? "true" : "false"));

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
	char			*owner;
	uint8_t			flags;
	enum mapistore_error	ret;

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
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &private_data);
	object = (struct emsmdbp_object *)private_data;
	if (!object || object->type != EMSMDBP_OBJECT_MESSAGE) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		goto end;
	}

	flags = mapi_req->u.mapi_SaveChangesMessage.SaveFlags;

	mapistore = emsmdbp_is_mapistore(object);
	switch ((int)mapistore) {
	case false:
		retval = openchangedb_message_save(emsmdbp_ctx->oc_ctx, object->backend_object, flags);
		DEBUG(0, ("[%s:%d]: openchangedb_save_message: retval = 0x%x\n", __FUNCTION__, __LINE__, retval));
		break;
	case true:
                contextID = emsmdbp_get_contextID(object);
		messageID = object->object.message->messageID;
		ret = mapistore_message_save(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, mem_ctx);
		if (ret == MAPISTORE_ERR_DENIED) {
			mapi_repl->error_code = MAPI_E_NO_ACCESS;
			goto end;
		}
		owner = emsmdbp_get_owner(object);
		mapistore_indexing_record_add_mid(emsmdbp_ctx->mstore_ctx, contextID, owner, messageID);
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
	struct SPropTagArray	columns;

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
		memset(&columns, 0, sizeof(struct SPropTagArray));
		mapistore_message_modify_recipients(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, &columns, 0, NULL);
	}
	else {
		DEBUG(0, ("Not implement yet - shouldn't occur\n"));
	}

end:
	*size += libmapiserver_RopRemoveAllRecipients_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details Resolve partial username in x500dn format to a sAMAccountName
            for corresponding user. If no such user, then just returns
            reconstructed x500dn (legaxyExchangeDN)

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param prefix_size amount of DN Prefix to use
   \param x500name partial (or full) x500dn to resolve
   \param username_p pointe to buffer to store resolved user name

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS oxcmsg_resolve_partial_x500name(TALLOC_CTX *mem_ctx,
						       struct emsmdbp_context *emsmdbp_ctx,
						       uint8_t prefix_size, const char *x500name,
						       char **username_p)
{
	char			*x500dn;
	char			*username;
	const char * const	attrs[] = { "sAMAccountName", NULL };
	int			ret;
	struct ldb_result	*res = NULL;

	OPENCHANGE_RETVAL_IF(!username_p, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 0. Restore full x500dn */
	if (prefix_size != 0) {
		/* sanity check for DNPrefix valid length */
		OPENCHANGE_RETVAL_IF(!emsmdbp_ctx->szDNPrefix, MAPI_E_INVALID_PARAMETER, NULL);
		if (prefix_size > strlen(emsmdbp_ctx->szDNPrefix)) {
			DEBUG(0, ("Requested x500name prefix is beyond what we have for DNPrefix=[%s]\n",
					emsmdbp_ctx->szDNPrefix));
			return MAPI_E_INVALID_PARAMETER;
		}

		/* concatenate x500name suffix with szDNPrefix we are using at the moment */
		size_t name_len = prefix_size + strlen(x500name) + 1;
		x500dn = talloc_zero_array(mem_ctx, char, name_len);
		OPENCHANGE_RETVAL_IF(!x500dn, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

		strncpy(x500dn, emsmdbp_ctx->szDNPrefix, prefix_size);
		strcpy(x500dn + prefix_size, x500name);
	} else {
		x500dn = discard_const_p(char, x500name);
	}

	/* Step 1. Try to find out an account with x500dn we have */
	ret = ldb_search(emsmdbp_ctx->samdb_ctx, mem_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx), LDB_SCOPE_SUBTREE,
			 attrs, "legacyExchangeDN=%s", x500dn);
	if (ret != LDB_SUCCESS || res->count != 1) {
		/* no such user, just pass what we have */
		username = x500dn;
	} else {
		username = discard_const_p(char, ldb_msg_find_attr_as_string(res->msgs[0], "sAMAccountName", x500dn));
	}

	*username_p = username;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS oxcmsg_parse_ModifyRecipientRow(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
						       struct ModifyRecipientRow *recipient_row,
						       uint16_t prop_count, enum MAPITAGS *properties,
						       struct mapistore_message_recipient *recipient)
{
	int			i;
	uint32_t		data_pos;
	const void		*prop_value;
	enum MAPISTATUS		retval;

	recipient->type = recipient_row->RecipClass;

	recipient->username = NULL;
	if ((recipient_row->RecipientRow.RecipientFlags & 0x07) == 1) {
		retval = oxcmsg_resolve_partial_x500name(mem_ctx, emsmdbp_ctx,
							 recipient_row->RecipientRow.AddressPrefixUsed.prefix_size,
							 recipient_row->RecipientRow.X500DN.recipient_x500name,
							 &recipient->username);
		OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);
	}

	recipient->data = talloc_array(mem_ctx, void *, prop_count + 2);

	/* PR_DISPLAY_NAME_UNICODE */
	switch ((recipient_row->RecipientRow.RecipientFlags & 0x210)) {
	case 0x10:
		recipient->data[0] = (void *) recipient_row->RecipientRow.DisplayName.lpszA;
		break;
	case 0x210:
		recipient->data[0] = (void *) recipient_row->RecipientRow.DisplayName.lpszW;
		break;
	default:
		recipient->data[0] = NULL;
	}

	/* PR_EMAIL_ADDRESS_UNICODE */
	switch ((recipient_row->RecipientRow.RecipientFlags & 0x208)) {
	case 0x08:
		recipient->data[1] = (void *) recipient_row->RecipientRow.EmailAddress.lpszA;
		break;
	case 0x208:
		recipient->data[1] = (void *) recipient_row->RecipientRow.EmailAddress.lpszW;
		break;
	default:
		recipient->data[1] = NULL;
	}

	data_pos = 0;
	for (i = 0; i < prop_count; i++) {

		if (recipient_row->RecipientRow.layout) {
			data_pos++;
			if (recipient_row->RecipientRow.prop_values.data[data_pos] != 0) {
				recipient->data[i+2] = NULL;
				if (recipient_row->RecipientRow.prop_values.data[data_pos] == 0xa) {
					data_pos += 4;
				}
				continue;
			}
		}

		prop_value = pull_emsmdb_property(recipient->data, &data_pos, properties[i],
						  &recipient_row->RecipientRow.prop_values);
		if (prop_value == NULL) {
			DEBUG(0, ("%s: Failed to convert RecipientProperty with tag [%s]. "
				  "We are going to save it as-is.\n",
				  __PRETTY_FUNCTION__, get_proptag_name(properties[i])));
			TALLOC_FREE(recipient->data);
			return MAPI_E_TYPE_NO_SUPPORT;
		}
		recipient->data[i+2] = discard_const(prop_value);
	}

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
	struct mapi_handles			*rec = NULL;
	struct emsmdbp_object			*object;
	enum MAPISTATUS				retval;
	uint32_t				handle;
	void					*private_data;
	bool					mapistore = false;
	uint32_t				contextID;
	struct mapistore_message_recipient	*recipients;
	struct SPropTagArray			*columns;
	int					i;

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
		/* Convention: we somewhat flatten the row and provide PR_DISPLAY_NAME_UNICODE and PR_EMAIL_ADDRESS_UNICODE to the backend */
		contextID = emsmdbp_get_contextID(object);
		columns = talloc_zero(mem_ctx, struct SPropTagArray);
		columns->cValues = mapi_req->u.mapi_ModifyRecipients.prop_count + 2;
		columns->aulPropTag = talloc_array(columns, enum MAPITAGS, columns->cValues);
		columns->aulPropTag[0] = PR_DISPLAY_NAME_UNICODE;
		columns->aulPropTag[1] = PR_EMAIL_ADDRESS_UNICODE;
		memcpy(columns->aulPropTag + 2, mapi_req->u.mapi_ModifyRecipients.properties, mapi_req->u.mapi_ModifyRecipients.prop_count * sizeof(enum MAPITAGS));

		recipients = talloc_array(mem_ctx, struct mapistore_message_recipient, mapi_req->u.mapi_ModifyRecipients.cValues);
		for (i = 0; i < mapi_req->u.mapi_ModifyRecipients.cValues; i++) {
			retval = oxcmsg_parse_ModifyRecipientRow(recipients, emsmdbp_ctx,
								 mapi_req->u.mapi_ModifyRecipients.RecipientRow + i,
								 mapi_req->u.mapi_ModifyRecipients.prop_count,
								 mapi_req->u.mapi_ModifyRecipients.properties,
								 recipients + i);
			if (retval != MAPI_E_SUCCESS) {
				DEBUG(0, ("Failed to parse RecipientRow. [%s]\n", mapi_get_errstr(retval)));
				mapi_repl->error_code = retval;
				goto end;
			}
		}
		mapistore_message_modify_recipients(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, columns, mapi_req->u.mapi_ModifyRecipients.cValues, recipients);
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
	uint32_t			contextID;
	struct oxcmsg_prop_index	prop_index;
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
	switch ((int)mapistore) {
	case false:
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
		contextID = emsmdbp_get_contextID(object);

		if (mapistore_message_get_message_data(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, mem_ctx, &msg) != MAPISTORE_SUCCESS) {
			mapi_repl->error_code = MAPI_E_NOT_FOUND;
			goto end;
		}

		/* Build the ReloadCachedInformation reply */
		if (msg->subject_prefix && strlen(msg->subject_prefix) > 0) {
			mapi_repl->u.mapi_ReloadCachedInformation.SubjectPrefix.StringType = StringType_UNICODE;
			mapi_repl->u.mapi_ReloadCachedInformation.SubjectPrefix.String.lpszW = talloc_strdup(mem_ctx, msg->subject_prefix);
		}
		else {
			mapi_repl->u.mapi_ReloadCachedInformation.SubjectPrefix.StringType = StringType_EMPTY;
		}
		if (msg->normalized_subject && strlen(msg->normalized_subject) > 0) {
			mapi_repl->u.mapi_ReloadCachedInformation.NormalizedSubject.StringType = StringType_UNICODE;
			mapi_repl->u.mapi_ReloadCachedInformation.NormalizedSubject.String.lpszW = talloc_strdup(mem_ctx, msg->normalized_subject);
		}
		else {
			mapi_repl->u.mapi_ReloadCachedInformation.NormalizedSubject.StringType = StringType_EMPTY;
		}
		if (msg->columns) {
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientColumns.cValues = msg->columns->cValues;
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientColumns.aulPropTag = msg->columns->aulPropTag;
		}
		else {
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientColumns.cValues = 0;
		}
		mapi_repl->u.mapi_ReloadCachedInformation.RecipientCount = msg->recipients_count;
		mapi_repl->u.mapi_ReloadCachedInformation.RowCount = msg->recipients_count;
		if (msg->recipients_count > 0) {
			mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows = talloc_array(mem_ctx, 
											       struct OpenRecipientRow, 
											       msg->recipients_count + 1);
			oxcmsg_fill_prop_index(&prop_index, msg->columns);
			for (i = 0; i < msg->recipients_count; i++) {
				oxcmsg_fill_OpenRecipientRow(mem_ctx, emsmdbp_ctx, &(mapi_repl->u.mapi_ReloadCachedInformation.RecipientRows[i]), msg->columns, msg->recipients + i, &prop_index);
			}
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
	struct SetMessageReadFlag_req	*request;
	/* struct SetMessageReadFlag_repl	*response; */
	enum MAPISTATUS			retval;
	uint32_t			handle;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*message_object = NULL;
	uint32_t			contextID;
	void				*data;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SetMessageReadFlag (0x11)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_SetMessageReadFlag;
	/* response = &mapi_repl->u.mapi_SetMessageReadFlag; */

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	handle = handles[request->handle_idx];
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

	switch ((int)emsmdbp_is_mapistore(message_object)) {
	case false:
		DEBUG(0, ("Not implemented yet\n"));
		break;
	case true:
                contextID = emsmdbp_get_contextID(message_object);
		mapistore_message_set_read_flag(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object, request->flags);
		break;
	}

	/* TODO: public folders */
	mapi_repl->u.mapi_SetMessageReadFlag.ReadStatusChanged = false;

end:
	*size += libmapiserver_RopSetMessageReadFlag_size(mapi_repl);

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc GetMessageStatus (0x1c) Rop. This operation
   returns the status of a message in a folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetMessageStatus EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetMessageStatus EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \todo Replace Stub implementation with mapistore calls

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetMessageStatus(TALLOC_CTX *mem_ctx,
						     struct emsmdbp_context *emsmdbp_ctx,
						     struct EcDoRpc_MAPI_REQ *mapi_req,
						     struct EcDoRpc_MAPI_REPL *mapi_repl,
						     uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	uint32_t		handle;
	struct mapi_handles	*rec = NULL;
	struct emsmdbp_object	*folder_object = NULL;
	void			*folder_private_data;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] GetMessageStatus (0x1c)\n"));

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
		mapi_repl->error_code = ecNullObject;
		DEBUG(5, ("[ERR][RopGetMessageStatus]: handle 0x%x not found\n", handle));
		goto end;
	}

	retval = mapi_handles_get_private_data(rec, &folder_private_data);
	if (retval) {
		mapi_repl->error_code = retval;
		DEBUG(5, ("[ERR][RopGetMessageStatus]: data associated to handle 0x%x not found\n", handle));
		goto end;
	}

	folder_object = (struct emsmdbp_object *) folder_private_data;
	if (!folder_object || folder_object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = ecNullObject;
		DEBUG(5, ("[ERR][RopGetMessageStatus]: Invalid or NULL folder object\n"));
		goto end;
	}

	switch ((int)emsmdbp_is_mapistore(folder_object)) {
	case false:
		DEBUG(0, ("[WARN][GetMessageStatus]: Not implemented\n"));
		mapi_repl->error_code = ecNullObject;
		break;
	case true:
		/* TODO: Retrieve the PidTagMessageStatus property from the message */
		mapi_repl->u.mapi_GetMessageStatus.MessageStatusFlags = 0x0;
		break;
	}

end:
	*size += libmapiserver_RopGetMessageStatus_size(mapi_repl);
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
	enum mapistore_error	mretval;
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

	switch ((int)emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
                contextID = emsmdbp_get_contextID(message_object);
                attachmentID = mapi_req->u.mapi_OpenAttach.AttachmentID;

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &attachment_rec);
		handles[mapi_repl->handle_idx] = attachment_rec->handle;

		attachment_object = emsmdbp_object_attachment_init((TALLOC_CTX *)attachment_rec, emsmdbp_ctx,
								   message_object->object.message->messageID, message_object);
		if (attachment_object) {
			mretval = mapistore_message_open_attachment(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object,
								    attachment_object, attachmentID, &attachment_object->backend_object);
			if (mretval) {
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
	enum mapistore_error	mretval;
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

	if (!message_object->object.message->read_write) {
		DEBUG(5, ("  parent message object is not open read/write\n"));
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}

	switch ((int)emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
                messageID = message_object->object.message->messageID;
                contextID = emsmdbp_get_contextID(message_object);

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &attachment_rec);
		handles[mapi_repl->handle_idx] = attachment_rec->handle;
		
		attachment_object = emsmdbp_object_attachment_init((TALLOC_CTX *)attachment_rec, emsmdbp_ctx,
								   messageID, message_object);
		if (attachment_object) {
			mretval = mapistore_message_create_attachment(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object,
								      attachment_object, &attachment_object->backend_object, &mapi_repl->u.mapi_CreateAttach.AttachmentID);
			if (mretval) {
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
   \details EcDoRpc DeleteAttach (0x24) Rop. This operation deletes an attachment
   from the message handle.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the DeleteAttach EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the DeleteAttach
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopDeleteAttach(TALLOC_CTX *mem_ctx,
                                                 struct emsmdbp_context *emsmdbp_ctx,
                                                 struct EcDoRpc_MAPI_REQ *mapi_req,
                                                 struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                 uint32_t *handles, uint16_t *size)
{
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*message_object = NULL;
	void				*data;
	enum MAPISTATUS			retval;
	enum mapistore_error		mretval;
	uint32_t			handle;
	uint32_t			contextID;
	uint32_t			attachmentID;
	bool				mapistore;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] DeleteAttach (0x24)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Initialise default empty DeleteAttach reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* Convert the handle index into a handle, and then get the message object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &rec);
	if (retval != MAPI_E_SUCCESS) {
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

	if (!message_object->object.message->read_write) {
		DEBUG(5, ("  parent message object is not open read/write\n"));
		mapi_repl->error_code = MAPI_E_NO_ACCESS;
		goto end;
	}

	/* retrieve the context and attachment IDs and perform the operation */
	mapistore = emsmdbp_is_mapistore(message_object);
	if (mapistore) {
                contextID = emsmdbp_get_contextID(message_object);

        	attachmentID = mapi_req->u.mapi_DeleteAttach.AttachmentID;

        	mretval = mapistore_message_delete_attachment(emsmdbp_ctx->mstore_ctx,
        			contextID, message_object->backend_object, attachmentID);
		if (mretval != MAPISTORE_SUCCESS) {
			DEBUG(5, ("Could not delete attachment\n"));
			mapi_repl->error_code = mapistore_error_to_mapi(mretval);
		}
	}
	else {
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
	}
end:
	*size += libmapiserver_RopDeleteAttach_size(mapi_repl);
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
	struct OpenEmbeddedMessage_req	*request;
	struct OpenEmbeddedMessage_repl	*response;
	enum mapistore_error		ret;
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
	struct oxcmsg_prop_index	prop_index;
	int				i;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] OpenEmbeddedMessage (0x46)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	request = &mapi_req->u.mapi_OpenEmbeddedMessage;
	response = &mapi_repl->u.mapi_OpenEmbeddedMessage;

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

        memset(response, 0, sizeof(struct OpenEmbeddedMessage_repl));

	mapistore = emsmdbp_is_mapistore(attachment_object);
	switch ((int)mapistore) {
	case false:
		DEBUG(0, ("Not implemented - shouldn't occur\n"));
		break;
	case true:
		contextID = emsmdbp_get_contextID(attachment_object);
                if (request->OpenModeFlags == MAPI_CREATE) {
                        ret = mapistore_indexing_get_new_folderID(emsmdbp_ctx->mstore_ctx, &messageID);
                        if (ret) {
                                mapi_repl->error_code = MAPI_E_NO_SUPPORT;
                                goto end;
                        }

			ret = mapistore_message_attachment_create_embedded_message(emsmdbp_ctx->mstore_ctx, contextID, attachment_object->backend_object,
										   NULL, &backend_attachment_message, &msg);
			if (ret != MAPISTORE_SUCCESS) {
				mapi_repl->error_code = MAPI_E_NOT_FOUND;
				goto end;
			}
                }
		else {
			ret = mapistore_message_attachment_open_embedded_message(emsmdbp_ctx->mstore_ctx, contextID, attachment_object->backend_object,
										 NULL, &backend_attachment_message, &messageID, &msg);
			if (ret != MAPISTORE_SUCCESS) {
				mapi_repl->error_code = MAPI_E_NOT_FOUND;
				goto end;
			}
		}

                response->MessageId = messageID;

		if (msg->subject_prefix && strlen(msg->subject_prefix) > 0) {
			response->SubjectPrefix.StringType = StringType_UNICODE;
			response->SubjectPrefix.String.lpszW = talloc_strdup(mem_ctx, msg->subject_prefix);
		}
		else {
			response->SubjectPrefix.StringType = StringType_EMPTY;
		}
		if (msg->normalized_subject && strlen(msg->normalized_subject) > 0) {
			response->NormalizedSubject.StringType = StringType_UNICODE;
			response->NormalizedSubject.String.lpszW = talloc_strdup(mem_ctx, msg->normalized_subject);
		}
		else {
			response->NormalizedSubject.StringType = StringType_EMPTY;
		}
		if (msg->columns) {
			response->RecipientColumns.cValues = msg->columns->cValues;
			response->RecipientColumns.aulPropTag = msg->columns->aulPropTag;
		}
		else {
			response->RecipientColumns.cValues = 0;
		}

		response->RecipientCount = msg->recipients_count;
		response->RowCount = msg->recipients_count;
		if (msg->recipients_count > 0) {
                        response->RecipientRows = talloc_array(mem_ctx, struct OpenRecipientRow, msg->recipients_count + 1);
			oxcmsg_fill_prop_index(&prop_index, msg->columns);
			for (i = 0; i < msg->recipients_count; i++) {
				oxcmsg_fill_OpenRecipientRow(mem_ctx, emsmdbp_ctx, &(response->RecipientRows[i]), msg->columns, msg->recipients + i, &prop_index);
			}
                }

                /* Initialize Message object */
                handle = handles[mapi_req->handle_idx];
                retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, 0, &message_rec);
                handles[mapi_repl->handle_idx] = message_rec->handle;

                message_object = emsmdbp_object_message_init((TALLOC_CTX *)message_rec, emsmdbp_ctx, messageID, attachment_object);
                message_object->backend_object = backend_attachment_message;
		message_object->object.message->read_write = (request->OpenModeFlags != MAPI_READONLY);
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
