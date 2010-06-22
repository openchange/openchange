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
	int				ret;
	enum MAPISTATUS			retval;
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*parent_handle = NULL;
	struct mapi_handles		*rec = NULL;
	struct emsmdbp_object		*object = NULL;
	struct emsmdbp_object		*parent_object = NULL;
	struct mapistore_message	msg;
	void				*data;
	uint64_t			folderID;
	uint64_t			messageID = 0;
	uint32_t			contextID;
	uint32_t			handle;
	bool				mapistore = false;
	struct indexing_folders_list	*flist;
	struct SPropTagArray		*SPropTagArray;
	char				*subject = NULL;
	int				i;


	DEBUG(4, ("exchange_emsmdb: [OXCMSG] OpenMessage (0x03)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->u.mapi_OpenMessage.handle_idx;
	messageID = mapi_req->u.mapi_OpenMessage.MessageId;
	folderID = mapi_req->u.mapi_OpenMessage.FolderId;

	/* OpenMessage can only be called for mailbox/folder objects */
	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *)data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		*size += libmapiserver_RopOpenMessage_size(NULL);
		return MAPI_E_SUCCESS;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		ret = mapistore_indexing_get_folder_list(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username,
							 messageID, &flist);
		if (ret || !flist->count) {
			DEBUG(0, ("No parent folder found for 0x%.16"PRIx64"\n", messageID));
		}
		/* If last element in the list doesn't match folderID, that's incorrect */
		if (folderID != flist->folderID[flist->count - 1]) {
			DEBUG(0, ("Last parent folder 0x%.16"PRIx64" doesn't match " \
				  "with expected 0x%.16"PRIx64"\n", 
				  flist->folderID[flist->count - 1], folderID));
		}

		/* Look if we have a parent folder already opened */
		for (i = flist->count - 1 ; i >= 0; i--) {
			parent_handle = emsmdbp_object_get_folder_handle_by_fid(emsmdbp_ctx->handles_ctx, 
										flist->folderID[i]);
			if (parent_handle) {
				break; 
			}
			
		}

		/* If we have a parent handle, we have a context_id
		 * and we can call subsequent OpenFolder - this will
		 * increment ref_count whereas needed */
		if (parent_handle) {
		recursive_open:
			for (i = i + 1; i < flist->count; i++) {
				mapi_handles_get_private_data(parent_handle, &data);
				parent_object = (struct emsmdbp_object *) data;
				folderID = parent_object->object.folder->folderID;
				contextID = parent_object->object.folder->contextID;
				retval = mapistore_opendir(emsmdbp_ctx->mstore_ctx, contextID, folderID,
							   flist->folderID[i]);
				mapi_handles_add(emsmdbp_ctx->handles_ctx, parent_handle->handle, &rec);
				object = emsmdbp_object_folder_init((TALLOC_CTX *)emsmdbp_ctx, emsmdbp_ctx,
								    flist->folderID[i], parent_handle);
				if (object) {
					retval = mapi_handles_set_private_data(rec, object);
				}

				parent_handle = rec;
				
			}
		} else {
			retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
			object = emsmdbp_object_folder_init((TALLOC_CTX *)emsmdbp_ctx, emsmdbp_ctx,
							    flist->folderID[0], parent);
			if (object) {
				retval = mapi_handles_set_private_data(rec, object);
			}
			parent_handle = rec;
			i = 0;
			/* now we have a context_id, we can use code above to open subfolders subsequently */
			goto recursive_open;
		}

		/* Add this stage our new parent_handle should point to the message */

		mapi_handles_get_private_data(parent_handle, &data);
		parent_object = (struct emsmdbp_object *) data;
		folderID = parent_object->object.folder->folderID;
		contextID = parent_object->object.folder->contextID;
		parent = parent_handle;
		break;
	case EMSMDBP_OBJECT_FOLDER:
		folderID = object->object.folder->folderID;
		contextID = object->object.folder->contextID;
		break;
	default:
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		*size = libmapiserver_RopGetHierarchyTable_size(NULL);
		return MAPI_E_SUCCESS;
	}

	mapistore = emsmdbp_is_mapistore(parent);
	switch (mapistore) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
		/* mapistore implementation goes here */
		mapistore_openmessage(emsmdbp_ctx->mstore_ctx, contextID, folderID, messageID, &msg);

		/* Build the OpenMessage reply */
		subject = (char *) find_SPropValue_data(msg.properties, PR_SUBJECT);

		mapi_repl->u.mapi_OpenMessage.HasNamedProperties = false;
		mapi_repl->u.mapi_OpenMessage.SubjectPrefix.StringType = StringType_EMPTY;
		mapi_repl->u.mapi_OpenMessage.NormalizedSubject.StringType = StringType_UNICODE_REDUCED;
		mapi_repl->u.mapi_OpenMessage.NormalizedSubject.String.lpszW_reduced = talloc_strdup(mem_ctx, subject);
		mapi_repl->u.mapi_OpenMessage.RecipientCount = msg.recipients->cRows;

		SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
						  PR_DISPLAY_TYPE,
						  PR_OBJECT_TYPE,
						  PR_7BIT_DISPLAY_NAME_UNICODE,
						  PR_SMTP_ADDRESS_UNICODE);
		mapi_repl->u.mapi_OpenMessage.RecipientColumns.cValues = SPropTagArray->cValues;
		mapi_repl->u.mapi_OpenMessage.RecipientColumns.aulPropTag = SPropTagArray->aulPropTag;
		mapi_repl->u.mapi_OpenMessage.RowCount = msg.recipients->cRows;
		mapi_repl->u.mapi_OpenMessage.recipients = talloc_array(mem_ctx, 
									struct OpenMessage_recipients, 
									msg.recipients->cRows + 1);
		for (i = 0; i < msg.recipients->cRows; i++) {
			mapi_repl->u.mapi_OpenMessage.recipients[i].RecipClass = msg.recipients->aRow[i].lpProps[0].value.l;
			mapi_repl->u.mapi_OpenMessage.recipients[i].codepage = CP_USASCII;
			mapi_repl->u.mapi_OpenMessage.recipients[i].Reserved = 0;
			emsmdbp_resolve_recipient(mem_ctx, emsmdbp_ctx, 
						  (char *)msg.recipients->aRow[i].lpProps[1].value.lpszA,
						  &(mapi_repl->u.mapi_OpenMessage.RecipientColumns),
						  &(mapi_repl->u.mapi_OpenMessage.recipients[i].RecipientRow));
		}

		break;
	}

	/* Initialize Message object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	if (messageID) {
		object = emsmdbp_object_message_init((TALLOC_CTX *)rec, emsmdbp_ctx, messageID, parent_handle);
		if (object) {
			retval = mapi_handles_set_private_data(rec, object);
		}
	}

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
	struct mapi_handles		*rec = NULL;
	struct mapi_handles		*parent = NULL;
	struct mapi_handles		*parent_handle = NULL;
	struct emsmdbp_object		*object = NULL;
	uint32_t			handle;
	uint64_t			folderID;
	uint64_t			messageID;
	uint32_t			contextID;
	bool				mapistore = false;
	void				*data;

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

	folderID = mapi_req->u.mapi_CreateMessage.FolderId;

	/* CreateMessage can only be called for a mailbox/folder object */
	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *)data;
	if (!object) {
		mapi_repl->error_code = MAPI_E_NO_SUPPORT;
		goto end;
	}

	/* FIXME: we can't assume the folder is already opened */
	parent_handle = emsmdbp_object_get_folder_handle_by_fid(emsmdbp_ctx->handles_ctx, folderID);
	if (!parent_handle) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}
	contextID = emsmdbp_get_contextID(parent_handle);
	mapistore = emsmdbp_is_mapistore(parent_handle);

	switch (mapistore) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
		/* This should be handled differently here: temporary hack */
		retval = openchangedb_get_new_folderID(emsmdbp_ctx->oc_ctx, &messageID);
		if (retval) {
			mapi_repl->error_code = MAPI_E_NO_SUPPORT;
			goto end;
		}
		mapi_repl->u.mapi_CreateMessage.HasMessageId = 1;
		mapi_repl->u.mapi_CreateMessage.MessageId.MessageId = messageID;
		mapistore_createmessage(emsmdbp_ctx->mstore_ctx, contextID, folderID, messageID);
		break;
	}

	DEBUG(0, ("CreateMessage: 0x%.16"PRIx64": mapistore = %s\n", folderID, 
		  emsmdbp_is_mapistore(parent_handle) == true ? "true" : "false"));

	/* Initialize Message object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	if (messageID) {
		object = emsmdbp_object_message_init((TALLOC_CTX *)rec, emsmdbp_ctx, messageID, parent_handle);
		if (object) {
			/* Add default properties to message MS-OXCMSG 3.2.5.2 */
			retval = mapi_handles_set_private_data(rec, object);
		}
	}

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
	
	mapi_repl->u.mapi_SaveChangesMessage.handle_idx = mapi_req->u.mapi_SaveChangesMessage.handle_idx;
	mapi_repl->u.mapi_SaveChangesMessage.MessageId = 0;

	*size += libmapiserver_RopSaveChangesMessage_size(mapi_repl);

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
	DEBUG(4, ("exchange_emsmdb: [OXCMSG] ModifyRecipients (0x0e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement this */

	*size += libmapiserver_RopModifyRecipients_size(mapi_repl);

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
	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SetMessageReadFlag (0x11)\n"));

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
	struct mapi_handles	*rec = NULL;
	uint32_t		handle;

	DEBUG(4, ("exchange_emsmdb: [OXCMSG] GetAttachmentTable (0x21)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement this */

	*size += libmapiserver_RopGetAttachmentTable_size(mapi_repl);

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	return MAPI_E_SUCCESS;	
}
