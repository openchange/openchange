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

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("CreateAttach");

	size = 0;
	/* Fill the CreateAttach operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_CreateAttach;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_set_handle(obj_attach, mapi_response->handles[1]);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Delete an attachment from a message

   This function deletes one attachment from a message. The attachment
   to be deleted is specified by its PR_ATTACH_NUM

   \param obj_message the message to operate on
   \param num_attach the attachment number

   \return MAPI_E_SUCCESS on success, otherwise -1.

      \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, GetAttachmentTable, GetLastError
 */
_PUBLIC_ enum MAPISTATUS DeleteAttach(mapi_object_t *obj_message, uint32_t num_attach)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct DeleteAttach_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("DeleteAttach");

	size = 0;
	/* Fill the DeleteAttach operation */
	request.num_attach = num_attach;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_DeleteAttach;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the attachment table for a message

   \param obj_message the message
   \param obj_tb_attch the attachment table for the message

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, OpenMessage, CreateAttach, OpenAttach, GetLastError
*/
_PUBLIC_ enum MAPISTATUS GetAttachmentTable(mapi_object_t *obj_message, 
					    mapi_object_t *obj_tb_attch)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct GetAttachmentTable_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	TALLOC_CTX			*mem_ctx;
	uint32_t			size = 0;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("GetAttachmentTable");

	/* Fill the GetAttachmentTable operation */
	request.handle_idx = 1;
	request.unknown = 0x0;
	size += sizeof(request.handle_idx) + sizeof(request.unknown);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_GetAttachmentTable;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_set_handle(obj_tb_attch, mapi_response->handles[mapi_response->mapi_repl->handle_idx]);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Open an attachment to a message

   This function opens one attachment from a message. The attachment
   to be opened is specified by its PR_ATTACH_NUM.

   \param obj_message the message to operate on
   \param num_attach the attachment number
   \param obj_attach the resulting attachment object

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, CreateAttach, GetAttachmentTable, GetLastError
*/
_PUBLIC_ enum MAPISTATUS OpenAttach(mapi_object_t *obj_message, uint32_t num_attach,
				    mapi_object_t *obj_attach)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct OpenAttach_req	request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("OpenAttach");

	size = 0;

	/* Fill the OpenAttach operation */
	request.handle_idx = 0x1;
	size += sizeof(uint8_t);
	request.unknown = 0x0;
	size += sizeof(uint8_t);
	request.num_attach = num_attach;
	size += sizeof(uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_OpenAttach;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_set_handle(obj_attach, mapi_response->handles[1]);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Set the type of a recipient

   The function sets the recipient type (RecipClass) in the aRow
   parameter.  ResolveNames should be used to fill the SRow structure.

   \param aRow the row to set
   \param RecipClass the type of recipient to set on the specified row

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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
	MAPI_RETVAL_IF(retval, retval, NULL);

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

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct ModifyRecipients_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	unsigned long		i_prop, j;
	unsigned long		i_recip;
	mapi_ctx_t		*mapi_ctx;
	uint32_t		count;


	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!SRowSet, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!SRowSet->aRow, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("ModifyRecipients");

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
		MAPI_RETVAL_IF(!RecipClass, MAPI_E_INVALID_PARAMETER, mem_ctx);
		request.RecipientRow[i_recip].RecipClass = (uint8_t) *RecipClass;
		size += sizeof(uint8_t);
		
		RecipientRow->RecipientFlags = mapi_recipients_bitmask(aRow);
		
		/* recipient type EXCHANGE or SMTP */
		switch (RecipientRow->RecipientFlags & 0xB) {
		case 0x1: 
			RecipientRow->type.EXCHANGE.organization_length = mapi_recipients_get_org_length(mapi_ctx->session->profile);
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
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Read Recipients from a message

   \param obj_message the message we want to read recipients from
   \param RowId the row index we start reading recipients from
   \param RowCount pointer on the number of recipients
   \param RecipientRows pointer on the recipients array

   \return MAPI_E_SUCCESS on success, otherwise -1.

  \note Developers should call GetLastError() to retrieve the last
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
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(RowId < 0, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("ReadRecipients");

	size = 0;

	/* Fill the ReadRecipients operation */
	request.RowId = RowId;
	size += sizeof (uint32_t);

	request.ulReserved = 0;
	size += sizeof (uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_ReadRecipients;
	mapi_req->logon_id = 0;
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
	
	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* Retrieve the recipients */
	*RowCount = mapi_response->mapi_repl->u.mapi_ReadRecipients.RowCount;
	*RecipientRows = mapi_response->mapi_repl->u.mapi_ReadRecipients.RecipientRows;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Deletes all recipients from a message

   \param obj_message the message we want to remove all recipients from

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("RemoveAllRecipients");

	size = 0;

	/* Fill the RemoveAllRecipients operation */
	request.ulReserved = 0;
	size += sizeof (uint32_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_RemoveAllRecipients;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

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

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa CreateMessage, SetProps, ModifyRecipients, SetRecipientType,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS SubmitMessage(mapi_object_t *obj_message)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SubmitMessage_req request;
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	
	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SubmitMessage");

	size = 0;

	/* Fill the SubmitMessage operation */
	request.SubmitFlags = 0x0;
	size += sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SubmitMessage;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Aborts a previous message submission.

   \param obj_store the store object
   \param obj_folder the folder object where the message has been
   submitted
   \param obj_message the submitted message object

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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
	NTSTATUS		status;
	enum MAPISTATUS		retval;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
	mapi_ctx_t		*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!obj_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!obj_message, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("ABortSubmit");

	size = 0;

	/* Fill the AbortSubmit operation */
	request.FolderId = mapi_object_get_id(obj_folder);
	size += sizeof (uint64_t);

	request.MessageId = mapi_object_get_id(obj_message);
	size += sizeof (uint64_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_AbortSubmit;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


/**
   \details Saves all changes to the message

   \param parent the parent object for the message
   \param obj_message the message to save

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa SetProps, ModifyRecipients, GetLastError
 */
_PUBLIC_ enum MAPISTATUS SaveChangesMessage(mapi_object_t *parent,
					    mapi_object_t *obj_message)
{
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct SaveChangesMessage_req	request;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	uint32_t			size = 0;
	TALLOC_CTX			*mem_ctx;
	mapi_ctx_t			*mapi_ctx;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	
	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("SaveChangesMessage");

	size = 0;

	/* Fill the SaveChangesMessage operation */
	request.handle_idx = 0x1;
	request.prop_nb = 0xa;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SaveChangesMessage;
	mapi_req->logon_id = 0;
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

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* store the message_id */
	mapi_object_set_id(obj_message, mapi_response->mapi_repl->u.mapi_SaveChangesMessage.message_id);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Returns the message recipient table

   \param obj_message the message to receive recipients from
   \param SRowSet pointer to the recipient table
   \param SPropTagArray pointer to the array of properties listed in
   the recipient table

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \note Developers should call GetLastError() to retrieve the last
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

	MAPI_RETVAL_IF(!obj_message, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!message, MAPI_E_NOT_INITIALIZED, NULL);

	*SRowSet = message->SRowSet;
	*SPropTagArray = message->SPropTagArray;

	return MAPI_E_SUCCESS;
}
