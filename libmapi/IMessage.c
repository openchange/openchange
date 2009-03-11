/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.
   Copyright (C) Fabien Le Mentec 2007.

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
#include <param.h>


/**
   \file IMessage.c

   \brief Operations on messages
 */


/**
   \details Create a new attachment

   This function creates a new attachment to an existing message.

   \param obj_message the message to attach to
   \param obj_attach the attachment

   Both objects need to exist before you call this message. obj_message
   should be a valid message on the server. obj_attach needs to be 
   initialised.

   \code
   enum MAPISTATUS         retval;
   mapi_object_t           obj_message;
   mapi_object_t           obj_attach;

   ... open or create the obj_message ...

   mapi_object_init(&obj_attach);
   retval = CreateAttach(&obj_message, &obj_attach);
   ... check the return value ...

   ... use SetProps() to set the attachment up ...
   ... perhaps OpenStream() / WriteStream() / CommitStream() on obj_attach ...

   // Save the changes to the attachment and then the message
   retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
   ... check the return value ...
   retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
   ... check the return value ...
   \endcode

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, GetAttachmentTable, OpenAttach, GetLastError
*/
_PUBLIC_ enum MAPISTATUS CreateAttach(mapi_object_t *obj_message, 
				      mapi_object_t *obj_attach)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct CreateAttach_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	logon_id = mapi_object_get_logon_id(obj_message);

	mem_ctx = talloc_named(NULL, 0, "CreateAttach");
	size = 0;

	/* Fill the CreateAttach operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateAttach;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_CreateAttach = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + (sizeof (uint32_t) * 2);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_attach, session);
	mapi_object_set_handle(obj_attach, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_attach, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Delete an attachment from a message

   This function deletes one attachment from a message. The attachment
   to be deleted is specified by its PR_ATTACH_NUM

   \param obj_message the message to operate on
   \param AttachmentID the attachment number

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

      \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, GetAttachmentTable, GetLastError
 */
_PUBLIC_ enum MAPISTATUS DeleteAttach(mapi_object_t *obj_message, uint32_t AttachmentID)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteAttach_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "DeleteAttach");
	size = 0;

	/* Fill the DeleteAttach operation */
	request.AttachmentID = AttachmentID;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteAttach;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_message);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_DeleteAttach = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the attachment table for a message

   \param obj_message the message
   \param obj_table the attachment table for the message

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, OpenMessage, CreateAttach, OpenAttach, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetAttachmentTable(mapi_object_t *obj_message, 
					    mapi_object_t *obj_table)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetAttachmentTable_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	uint32_t			size = 0;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_table, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	logon_id = mapi_object_get_logon_id(obj_message);

	mem_ctx = talloc_named(NULL, 0, "GetAttachmentTable");
	size = 0;

	/* Fill the GetAttachmentTable operation */
	request.handle_idx = 1;
	size += sizeof (uint8_t);

	request.TableFlags = 0x0;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetAttachmentTable;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_GetAttachmentTable = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_table, session);
	mapi_object_set_handle(obj_table, mapi_response->handles[mapi_response->mapi_repl->handle_idx]);
	mapi_object_set_logon_id(obj_table, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Open an attachment to a message

   This function opens one attachment from a message. The attachment
   to be opened is specified by its PR_ATTACH_NUM.

   \param obj_message the message to operate on
   \param AttachmentID the attachment number
   \param obj_attach the resulting attachment object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, CreateAttach, GetAttachmentTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS OpenAttach(mapi_object_t *obj_message, uint32_t AttachmentID,
				    mapi_object_t *obj_attach)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenAttach_req	request;
	struct mapi_session	*session;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	uint8_t			logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	logon_id = mapi_object_get_logon_id(obj_message);

	mem_ctx = talloc_named(NULL, 0, "OpenAttach");
	size = 0;

	/* Fill the OpenAttach operation */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.OpenAttachmentFlags = OpenAttachmentFlags_ReadOnly;
	size += sizeof(uint8_t);
	request.AttachmentID = AttachmentID;
	size += sizeof(uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenAttach;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OpenAttach = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_attach, session);
	mapi_object_set_handle(obj_attach, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_attach, logon_id);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Set the type of a recipient

   The function sets the recipient type (RecipClass) in the aRow
   parameter.  ResolveNames should be used to fill the SRow structure.

   \param aRow the row to set
   \param RecipClass the type of recipient to set on the specified row

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_INVALID_PARAMETER: The aRow parameter was not set
     properly.

   \sa ResolveNames, ModifyRecipients, GetLastError
*/
_PUBLIC_ enum MAPISTATUS SetRecipientType(struct SRow *aRow, enum ulRecipClass RecipClass)
{
	enum MAPISTATUS		retval;
	struct SPropValue	lpProp;

	lpProp.ulPropTag = PR_RECIPIENT_TYPE;
	lpProp.value.l = RecipClass;

	retval = SRow_addprop(aRow, lpProp);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	return MAPI_E_SUCCESS;
}


/*
 * retrieve the organization length for Exchange recipients
 */
uint8_t mapi_recipients_get_org_length(struct mapi_profile *profile)
{
	if (profile->mailbox && profile->username)
		return (strlen(profile->mailbox) - strlen(profile->username));
	return 0;
}


/*
 * EXPERIMENTAL:
 * bitmask calculation for recipients headers structures
 */
uint16_t mapi_recipients_bitmask(struct SRow *aRow)
{
	uint16_t		bitmask;
	struct SPropValue	*lpProp = NULL;

	bitmask = 0;

	/* recipient type: EXCHANGE or SMTP */
	lpProp = get_SPropValue_SRow(aRow, PR_ADDRTYPE);
	if (lpProp && lpProp->value.lpszA) {
		if (!strcmp("SMTP", lpProp->value.lpszA)) {
			bitmask |= 0xB;
		}
	} else {
		/* (Bit 8) doesn't seem to work if unset */
		bitmask |= 0x81;
	}

	/* Bit 15: DISPLAY_NAME */
	lpProp = get_SPropValue_SRow(aRow, PR_DISPLAY_NAME);
	if (lpProp && lpProp->value.lpszA) {
		bitmask |= 0x400;
	}

	lpProp = get_SPropValue_SRow(aRow, PR_DISPLAY_NAME_UNICODE);
	if (lpProp && lpProp->value.lpszW) {
		bitmask |= 0x600;
	}

	/* Bit 6: PR_GIVEN_NAME */
	lpProp = get_SPropValue_SRow(aRow, PR_GIVEN_NAME);
	if (lpProp && lpProp->value.lpszA) {
		bitmask |= 0x20;
	}

	lpProp = get_SPropValue_SRow(aRow, PR_GIVEN_NAME_UNICODE);
	if (lpProp && lpProp->value.lpszW) {
		bitmask |= 0x220;
	}

	/* Bit 5. PR_7BIT_DISPLAY_NAME */
	lpProp = get_SPropValue_SRow(aRow, PR_7BIT_DISPLAY_NAME);
	if (lpProp && lpProp->value.lpszA) {
		bitmask |= 0x10;
	}

	lpProp = get_SPropValue_SRow(aRow, PR_7BIT_DISPLAY_NAME_UNICODE);
	if (lpProp && lpProp->value.lpszW) {
		bitmask |= 0x210;
	}

	/* Bit 4: PR_EMAIL_ADDRESS */
	if (bitmask & 0xA) {
		lpProp = get_SPropValue_SRow(aRow, PR_SMTP_ADDRESS);
		if (lpProp && lpProp->value.lpszA) {
			bitmask |= 0x8;
		}
		
		lpProp = get_SPropValue_SRow(aRow, PR_SMTP_ADDRESS_UNICODE);
		if (lpProp && lpProp->value.lpszW) {
			bitmask |= 0x208;
		}
	}

	return bitmask;
}


/**
   \details Adds, deletes or modifies message recipients

   \param obj_message the message to change the recipients for
   \param SRowSet the recipients to add

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \bug ModifyRecipients can only add recipients.

   \sa CreateMessage, ResolveNames, SetRecipientType, GetLastError

*/
_PUBLIC_ enum MAPISTATUS ModifyRecipients(mapi_object_t *obj_message, 
					  struct SRowSet *SRowSet)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct ModifyRecipients_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	unsigned long			i_prop, j;
	unsigned long			i_recip;
	uint32_t			count;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SRowSet, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!SRowSet->aRow, MAPI_E_NOT_INITIALIZED, NULL);

	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "ModifyRecipients");
	size = 0;

	/* Fill the ModifyRecipients operation */
	request.prop_count = SRowSet->aRow[0].cValues;
	size += sizeof(uint16_t);

	/* 
	 * append here property tags that can be fetched with
	 * ResolveNames but shouldn't be included in ModifyRecipients rows
	 */
	request.properties = get_MAPITAGS_SRow(mem_ctx, &SRowSet->aRow[0]);
	count = SRowSet->aRow[0].cValues - 1;
 	request.prop_count = MAPITAGS_delete_entries(request.properties, count, 7,
						     PR_DISPLAY_NAME,
						     PR_DISPLAY_NAME_UNICODE,
						     PR_GIVEN_NAME,
						     PR_GIVEN_NAME_UNICODE,
						     PR_GIVEN_NAME_ERROR,
						     PR_RECIPIENT_TYPE,
						     PR_ADDRTYPE);
	size += request.prop_count * sizeof(uint32_t);

	request.cValues = SRowSet->cRows;
	size += sizeof(uint16_t);
	request.RecipientRow = talloc_array(mem_ctx, struct ModifyRecipientRow, request.cValues);

	for (i_recip = 0; i_recip < request.cValues; i_recip++) {
		struct SRow			*aRow;
		struct RecipientRow		*RecipientRow;
		struct ndr_push			*ndr;
		struct mapi_SPropValue		mapi_sprop;
		const uint32_t			*RecipClass = 0;

		ndr = talloc_zero(mem_ctx, struct ndr_push);
		ndr->iconv_convenience = smb_iconv_convenience_init(mem_ctx, "CP850", "UTF8", true);

		aRow = &(SRowSet->aRow[i_recip]);
		RecipientRow = &(request.RecipientRow[i_recip].RecipientRow);
		
		request.RecipientRow[i_recip].idx = i_recip;
		size += sizeof(uint32_t);

		RecipClass = (const uint32_t *) find_SPropValue_data(aRow, PR_RECIPIENT_TYPE);
		OPENCHANGE_RETVAL_IF(!RecipClass, MAPI_E_INVALID_PARAMETER, mem_ctx);
		request.RecipientRow[i_recip].RecipClass = (uint8_t) *RecipClass;
		size += sizeof(uint8_t);
		
		RecipientRow->RecipientFlags = mapi_recipients_bitmask(aRow);
		
		/* recipient type EXCHANGE or SMTP */
		switch (RecipientRow->RecipientFlags & 0xB) {
		case 0x1: 
			RecipientRow->type.EXCHANGE.organization_length = mapi_recipients_get_org_length(session->profile);
			RecipientRow->type.EXCHANGE.addr_type = 0;
			RecipientRow->type.EXCHANGE.username = (const char *) find_SPropValue_data(aRow, PR_7BIT_DISPLAY_NAME);
			size += sizeof(uint32_t) + strlen(RecipientRow->type.EXCHANGE.username) + 1;
			break;
		case 0xB:
			size += sizeof(uint16_t);
			break;
		}
		
		/* Bit 15: PR_DISPLAY_NAME */
		switch(RecipientRow->RecipientFlags & 0x600) {
		case (0x400):
			RecipientRow->SimpleDisplayName.lpszA = (const char *) find_SPropValue_data(aRow, PR_DISPLAY_NAME);
			size += strlen(RecipientRow->SimpleDisplayName.lpszA) + 1;
			break;
		case (0x600):
			RecipientRow->SimpleDisplayName.lpszW = (const char *) find_SPropValue_data(aRow, PR_DISPLAY_NAME_UNICODE);
			size += strlen(RecipientRow->SimpleDisplayName.lpszW) * 2 + 2;
			break;
		default:
			break;
		}

		/* Bit 6: PR_GIVEN_NAME */
		switch (RecipientRow->RecipientFlags & 0x220) {
		case (0x20):
			RecipientRow->TransmittableDisplayName.lpszA = (const char *) find_SPropValue_data(aRow, PR_GIVEN_NAME);
			size += strlen(RecipientRow->TransmittableDisplayName.lpszA) + 1;
			break;
		case (0x220):
			RecipientRow->TransmittableDisplayName.lpszW = (const char *) find_SPropValue_data(aRow, PR_GIVEN_NAME_UNICODE);
			size += strlen(RecipientRow->TransmittableDisplayName.lpszW) * 2 + 2;
			break;
		default:
			break;
		}

		/* Bit 5: PR_7BIT_DISPLAY_NAME */
		switch (RecipientRow->RecipientFlags & 0x210) {
		case (0x10):
			RecipientRow->DisplayName.lpszA = (const char *) find_SPropValue_data(aRow, PR_7BIT_DISPLAY_NAME);
			size += strlen(RecipientRow->DisplayName.lpszA) + 1;
			break;
		case (0x210):
			RecipientRow->DisplayName.lpszW = (const char *) find_SPropValue_data(aRow, PR_7BIT_DISPLAY_NAME_UNICODE);
			size += strlen(RecipientRow->DisplayName.lpszW) * 2 + 2;
			break;
		default:
			break;
		}

		/* Bit 4: PR_SMTP_ADDRESS */
		switch (RecipientRow->RecipientFlags & 0x208) {
		case (0x8):
			RecipientRow->EmailAddress.lpszA = (const char *) find_SPropValue_data(aRow, PR_SMTP_ADDRESS);
			size += strlen(RecipientRow->EmailAddress.lpszA) + 1;
			break;
		case (0x208):
			RecipientRow->EmailAddress.lpszW = (const char *) find_SPropValue_data(aRow, PR_SMTP_ADDRESS_UNICODE);
			size += strlen(RecipientRow->EmailAddress.lpszW) * 2 + 2;
			break;
		default:
			break;
		}
		
		RecipientRow->prop_count = request.prop_count;
		size += sizeof(uint16_t);
		RecipientRow->layout = 0;
		size += sizeof(uint8_t);
		
		/* for each property, we set the switch values and ndr_flags then call mapi_SPropValue_CTR */
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		for (i_prop = 0; i_prop < request.prop_count; i_prop++) {
			for (j = 0; j < aRow->cValues; j++) {
				if (aRow->lpProps[j].ulPropTag == request.properties[i_prop]) {
					ndr_push_set_switch_value(ndr, &mapi_sprop.value, 
								  (aRow->lpProps[j].ulPropTag & 0xFFFF));
					cast_mapi_SPropValue(&mapi_sprop, &aRow->lpProps[j]);
					ndr_push_mapi_SPropValue_CTR(ndr, NDR_SCALARS, &mapi_sprop.value);
				}
			}
		}
		
		RecipientRow->prop_values.length = ndr->offset;
		RecipientRow->prop_values.data = ndr->data;
		/* add the blob size field */
		size += sizeof(uint16_t);
		size += RecipientRow->prop_values.length;
	}
	
	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ModifyRecipients;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_message);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_ModifyRecipients = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof(uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	errno = 0;
	return MAPI_E_SUCCESS;
}


/**
   \details Read Recipients from a message

   \param obj_message the message we want to read recipients from
   \param RowId the row index we start reading recipients from
   \param RowCount pointer on the number of recipients
   \param RecipientRows pointer on the recipients array

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

  \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa ModifyRecipients, RemoveAllRecipients, GetRecipientTable,
   OpenMessage
 */
_PUBLIC_ enum MAPISTATUS ReadRecipients(mapi_object_t *obj_message, 
					uint32_t RowId, uint8_t *RowCount,
					struct ReadRecipientRow **RecipientRows)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct ReadRecipients_req	request;
	struct ReadRecipients_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "ReadRecipients");
	size = 0;

	/* Fill the ReadRecipients operation */
	request.RowId = RowId;
	size += sizeof (uint32_t);

	request.ulReserved = 0;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ReadRecipients;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_message);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_ReadRecipients = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);
	
	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Retrieve the recipients */
	reply = &mapi_response->mapi_repl->u.mapi_ReadRecipients;
	*RowCount = reply->RowCount;
	*RecipientRows = talloc_steal((TALLOC_CTX *)session, reply->RecipientRows);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Deletes all recipients from a message

   \param obj_message the message we want to remove all recipients from

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa ModifyRecipients, ReadRecipients
*/
_PUBLIC_ enum MAPISTATUS RemoveAllRecipients(mapi_object_t *obj_message)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct RemoveAllRecipients_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "RemoveAllRecipients");
	size = 0;

	/* Fill the RemoveAllRecipients operation */
	request.ulReserved = 0;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_RemoveAllRecipients;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_message);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_RemoveAllRecipients = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Saves all changes to the message and marks it as ready for
   sending.

   This function saves all changes made to a message and marks it
   ready to be sent.

   \param obj_message the message to mark complete

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, SetProps, ModifyRecipients, SetRecipientType,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS SubmitMessage(mapi_object_t *obj_message)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SubmitMessage_req	request;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "SubmitMessage");
	size = 0;

	/* Fill the SubmitMessage operation */
	request.SubmitFlags = 0x0;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SubmitMessage;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_message);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SubmitMessage = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Aborts a previous message submission.

   \param obj_store the store object
   \param obj_folder the folder object where the message has been
   submitted
   \param obj_message the submitted message object

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
   - MAPI_E_UNABLE_TO_ABORT: The operation can not be aborted
   - MAPI_E_NOT_IN_QUEUE: The message is no longer in the message store's
     spooler queue
   - MAPI_E_NO_SUPPORT: the server object associated with the input
     handle index in the server object table is not of type Logon or
     the current logon session is a public logon.

    \sa SubmitMessage
 */
_PUBLIC_ enum MAPISTATUS AbortSubmit(mapi_object_t *obj_store,
				     mapi_object_t *obj_folder, 
				     mapi_object_t *obj_message)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct AbortSubmit_req	request;
	struct mapi_session	*session[2];
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(obj_store);
	session[1] = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(session[0] != session[1], MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "AbortSubmit");
	size = 0;

	/* Fill the AbortSubmit operation */
	request.FolderId = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);

	request.MessageId = mapi_object_get_id(obj_message);
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_AbortSubmit;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_store);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_AbortSubmit = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_store);

	status = emsmdb_transaction(session[0]->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Saves all changes to the message

   \param parent the parent object for the message
   \param obj_message the message to save
   \param SaveFlags specify how the save operation behaves

   Possible value for SaveFlags:
   -# KeepReadOnly Keep the Message object open with read-only access
   -# KeepOpenReadWrite Keep the Message object open with read-write
      access
   -# ForceSave Commit the changes and keep the message object open
      with read-write access

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, ModifyRecipients, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SaveChangesMessage(mapi_object_t *parent,
					    mapi_object_t *obj_message,
					    uint8_t SaveFlags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SaveChangesMessage_req	request;
	struct mapi_session		*session[2];
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!parent, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF((SaveFlags != 0x9) && (SaveFlags != 0xA) && 
			     (SaveFlags != 0xC), MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(parent);
	session[1] = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "SaveChangesMessage");
	size = 0;

	/* Fill the SaveChangesMessage operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	request.SaveFlags = SaveFlags;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SaveChangesMessage;
	mapi_req->logon_id = mapi_object_get_logon_id(parent);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SaveChangesMessage = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + (2 * sizeof (uint32_t));
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(parent);
	mapi_request->handles[1] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session[0]->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* store the message_id */
	mapi_object_set_id(obj_message, mapi_response->mapi_repl->u.mapi_SaveChangesMessage.MessageId);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Sends the specified Message object out for message
   delivery.

*/
_PUBLIC_ enum MAPISTATUS TransportSend(mapi_object_t *obj_message, 
				       struct mapi_SPropValue_array *lpProps)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct TransportSend_repl	*reply;
	struct mapi_session		*session;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!lpProps, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj_message);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "TransportSend");
	size = 0;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_TransportSend;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_message);
	mapi_req->handle_idx = 0;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = mapi_object_get_handle(obj_message);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Retrieve reply parameters */
	reply = &mapi_response->mapi_repl->u.mapi_TransportSend;
	if (!reply->NoPropertiesReturned) {
		lpProps->cValues = reply->properties.lpProps.cValues;
		lpProps->lpProps = talloc_steal((TALLOC_CTX *)session, reply->properties.lpProps.lpProps);
	} else {
		lpProps = NULL;
	}

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns the message recipient table

   \param obj_message the message to receive recipients from
   \param SRowSet pointer to the recipient table
   \param SPropTagArray pointer to the array of properties listed in
   the recipient table

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized

     \sa OpenMessage
 */
_PUBLIC_ enum MAPISTATUS GetRecipientTable(mapi_object_t *obj_message, 
					   struct SRowSet *SRowSet,
					   struct SPropTagArray *SPropTagArray)
{
	mapi_object_message_t	*message;

	message = (mapi_object_message_t *)obj_message->private_data;

	OPENCHANGE_RETVAL_IF(!obj_message, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message, MAPI_E_NOT_INITIALIZED, NULL);

	*SRowSet = message->SRowSet;
	*SPropTagArray = message->SPropTagArray;

	return MAPI_E_SUCCESS;
}


/**
   \details Clear or set the MSGFLAG_READ flag for a given message

   This function clears or sets the MSGFLAG_READ flag in the
   PR_MESSAGE_FLAGS property of a given message.

   \param obj_folder the folder to operate in
   \param obj_child the message to set flags on
   \param flags the new flags (MSGFLAG_READ) value

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa OpenMessage, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SetMessageReadFlag(mapi_object_t *obj_folder, 
					    mapi_object_t *obj_child,
					    uint8_t flags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SetMessageReadFlag_req	request;
	struct mapi_session		*session[2];
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size;
	TALLOC_CTX			*mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_child, MAPI_E_INVALID_PARAMETER, NULL);

	session[0] = mapi_object_get_session(obj_folder);
	session[1] = mapi_object_get_session(obj_child);
	OPENCHANGE_RETVAL_IF(!session[0], MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session[1], MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "SetMessageReadFlags");
	size = 0;

	/* Fill the SetMessageReadFlags operation */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.flags = flags;
	size += sizeof(uint8_t);

	request.clientdata.length = 0;
	request.clientdata.data = NULL;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetMessageReadFlag;
	mapi_req->logon_id = mapi_object_get_logon_id(obj_folder);
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_SetMessageReadFlag = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_folder);
	mapi_request->handles[1] = mapi_object_get_handle(obj_child);

	status = emsmdb_transaction(session[0]->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Opens an embedded message object and retrieves a MAPI object that
   can be used to get or set properties on the embedded message.

   This function essentially takes an attachment and gives you back
   a message.

   \param obj_attach the attachment object
   \param obj_embeddedmsg the embedded message
   \param ulFlags access rights on the embedded message

   Possible ulFlags values:
   - 0x0: read only access
   - 0x1: Read / Write access
   - 0x2: Create 

   \code
	... assume we have a message - obj_message ...
	// Initialise the attachment object
	mapi_object_init(&obj_attach);

	// Create an attachment to the message
	retval = CreateAttach(&obj_message, &obj_attach);
	... check the return value ...

	// use SetProps() to set the attachment up, noting the special PR_ATTACHM_METHOD property
	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_EMBEDDED_MSG;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	retval = SetProps(&obj_attach, attach, 2);
	... check the return value ...

        // Initialise the embedded message object
	mapi_object_init(&obj_embeddedmsg);
	retval = OpenEmbeddedMessage(&obj_attach, &obj_embeddedmsg, MAPI_CREATE);
	... check the return value ...

	// Fill in the embedded message properties, just like any other message (e.g. resulting from CreateMessage())

	// Save the changes to the embedded message
	retval = SaveChangesMessage(&obj_message, &obj_embeddedmsg, KeepOpenReadOnly);
	... check the return value ...

	// Save the changes to the attachment
	retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
	... check the return value ...

	// Save the changes to the original message
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	... check the return value ...
   \endcode

   \return MAPI_E_SUCCESS on success, otherwise MAPI error. Possible
   MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: obj_store is undefined
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. 

   \sa CreateAttach, OpenMessage, GetLastError
*/
_PUBLIC_ enum MAPISTATUS OpenEmbeddedMessage(mapi_object_t *obj_attach,
					     mapi_object_t *obj_embeddedmsg,
					     enum OpenEmbeddedMessage_OpenModeFlags ulFlags)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct OpenEmbeddedMessage_req	request;
	struct OpenEmbeddedMessage_repl	*reply;
	struct mapi_session		*session;
	mapi_object_message_t		*message;
	struct SPropValue		lpProp;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	uint32_t			i = 0;
	uint8_t				logon_id;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!obj_attach, MAPI_E_INVALID_PARAMETER, NULL);
	session = mapi_object_get_session(obj_attach);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj_embeddedmsg, MAPI_E_INVALID_PARAMETER, NULL);

	logon_id = mapi_object_get_logon_id(obj_attach);

	mem_ctx = talloc_named(NULL, 0, "OpenEmbeddedMessage");

	/* Fill the OpenEmbeddedMessage request */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.CodePageId = 0xfff;
	size += sizeof(uint16_t);
	request.OpenModeFlags = ulFlags;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenEmbeddedMessage;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_OpenEmbeddedMessage = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj_attach);
	mapi_request->handles[1] = mapi_object_get_handle(obj_embeddedmsg);

	status = emsmdb_transaction(session->emsmdb->ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* Set object session and handle */
	mapi_object_set_session(obj_embeddedmsg, session);
	mapi_object_set_handle(obj_embeddedmsg, mapi_response->handles[1]);
	mapi_object_set_logon_id(obj_embeddedmsg, logon_id);

	/* Store OpenEmbeddedMessage reply data */
	reply = &mapi_response->mapi_repl->u.mapi_OpenEmbeddedMessage;

	message = talloc_zero((TALLOC_CTX *)session, mapi_object_message_t);
	message->cValues = reply->RecipientCount;
	message->SRowSet.cRows = reply->RowCount;
	message->SRowSet.aRow = talloc_array((TALLOC_CTX *)message, struct SRow, reply->RowCount + 1);

	message->SPropTagArray.cValues = reply->RecipientCount;
	message->SPropTagArray.aulPropTag = talloc_steal(message, reply->RecipientColumns);

	for (i = 0; i < reply->RowCount; i++) {
		emsmdb_get_SRow((TALLOC_CTX *)message, global_mapi_ctx->lp_ctx,
				&(message->SRowSet.aRow[i]), &message->SPropTagArray,
				reply->RecipientRows[i].RecipientRow.prop_count,
				&reply->RecipientRows[i].RecipientRow.prop_values,
				reply->RecipientRows[i].RecipientRow.layout, 1);

		lpProp.ulPropTag = PR_RECIPIENT_TYPE;
		lpProp.value.l = reply->RecipientRows[i].RecipientType;
		SRow_addprop(&(message->SRowSet.aRow[i]), lpProp);

		lpProp.ulPropTag = PR_INTERNET_CPID;
		lpProp.value.l = reply->RecipientRows[i].CodePageId;
		SRow_addprop(&(message->SRowSet.aRow[i]), lpProp);
	}

	/* add SPropTagArray elements we automatically append to SRow */
	SPropTagArray_add((TALLOC_CTX *)message, &message->SPropTagArray, PR_RECIPIENT_TYPE);
	SPropTagArray_add((TALLOC_CTX *)message, &message->SPropTagArray, PR_INTERNET_CPID);
	
	obj_embeddedmsg->private_data = (void *) message;

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

