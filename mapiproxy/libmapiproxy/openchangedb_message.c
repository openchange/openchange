/*
   OpenChange Server implementation

   OpenChangeDB message object implementation

   Copyright (C) Julien Kerihuel 2011

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
   \file openchangedb_message.c

   \brief OpenChange Dispatcher database message routines
 */

#include <inttypes.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

/**
   \details Initialize and create a message object

   \param mem_ctx pointer to the memory context to use for allocation
   \param ldb_ctx pointer to the ldb context
   \param messageID the identifier of the message to create
   \param folderID the identifier of the folder where the message is created
   \param message_object pointer on pointer to the message object to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_message_create(TALLOC_CTX *mem_ctx, struct ldb_context *ldb_ctx,
						     uint64_t messageID, uint64_t folderID, bool fai,
						     void **message_object)
{
	enum MAPISTATUS			retval;
	struct openchangedb_message	*msg;
	struct ldb_dn			*basedn;
	char				*dn;
	char				*parentDN;
	char				*mailboxDN;
	int				i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);
	
	/* Retrieve distinguishedName of parent folder */
	retval = openchangedb_get_distinguishedName(mem_ctx, ldb_ctx, folderID, &parentDN);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Retrieve mailboxDN of parent folder */
	retval = openchangedb_get_mailboxDN(mem_ctx, ldb_ctx, folderID, &mailboxDN);
	if (retval) {
		mailboxDN = NULL;
	}
	
	dn = talloc_asprintf(mem_ctx, "CN=%"PRIu64",%s", messageID, parentDN);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	basedn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	talloc_free(dn);

	OPENCHANGE_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, NULL);

	msg = talloc_zero(mem_ctx, struct openchangedb_message);
	OPENCHANGE_RETVAL_IF(!msg, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	msg->status = OPENCHANGEDB_MESSAGE_CREATE;
	msg->folderID = folderID;
	msg->messageID = messageID;
	msg->ldb_ctx = ldb_ctx;
	msg->msg = NULL;
	msg->res = NULL;

	msg->msg = ldb_msg_new((TALLOC_CTX *)msg);
	OPENCHANGE_RETVAL_IF(!msg->msg, MAPI_E_NOT_ENOUGH_MEMORY, msg);

	msg->msg->dn = ldb_dn_copy((TALLOC_CTX *)msg->msg, basedn);

	/* Add openchangedb required attributes */
	ldb_msg_add_string(msg->msg, "objectClass", fai ? "faiMessage" : "systemMessage");
	ldb_msg_add_fmt(msg->msg, "cn", "%"PRIu64, messageID);
	ldb_msg_add_fmt(msg->msg, "PidTagParentFolderId", "%"PRIu64, folderID);
	ldb_msg_add_fmt(msg->msg, "PidTagMessageId", "%"PRIu64, messageID);
	ldb_msg_add_fmt(msg->msg, "distinguishedName", "%s", ldb_dn_get_linearized(msg->msg->dn));
	if (mailboxDN) {
		ldb_msg_add_string(msg->msg, "mailboxDN", mailboxDN);
	}

	/* Add required properties as described in [MS_OXCMSG] 3.2.5.2 */
	ldb_msg_add_string(msg->msg, "PidTagDisplayBcc", "");
	ldb_msg_add_string(msg->msg, "PidTagDisplayCc", "");
	ldb_msg_add_string(msg->msg, "PidTagDisplayTo", "");
	/* PidTagMessageSize */
	/* PidTagSecurityDescriptor */
	/* ldb_msg_add_string(msg->msg, "PidTagCreationTime", ""); */
	/* ldb_msg_add_string(msg->msg, "PidTagLastModificationTime", ""); */
	/* PidTagSearchKey */
	/* PidTagMessageLocalId */
	/* PidTagCreatorName */
	/* PidTagCreatorEntryId */
	ldb_msg_add_fmt(msg->msg, "PidTagHasNamedProperties", "%d", 0x0);
	/* PidTagLocaleId same as PidTagMessageLocaleId */
	/* PidTagLocalCommitTime same as PidTagCreationTime */

	/* Set LDB flag */
	for (i = 0; i < msg->msg->num_elements; i++) {
		msg->msg->elements[i].flags = LDB_FLAG_MOD_ADD;
	}

	*message_object = (void *)msg;

	return MAPI_E_SUCCESS;
}

/**
   \details Save (commit) message in openchangedb database

   \param msg the message object
   \param SaveFlags flags associated to the save operation

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_message_save(void *_msg, uint8_t SaveFlags)
{
	struct openchangedb_message *msg = (struct openchangedb_message *)_msg;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!msg, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!msg->ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	switch (msg->status) {
	case OPENCHANGEDB_MESSAGE_CREATE:
		OPENCHANGE_RETVAL_IF(!msg->msg, MAPI_E_NOT_INITIALIZED, NULL);
		if (ldb_add(msg->ldb_ctx, msg->msg) != LDB_SUCCESS) {
			return MAPI_E_CALL_FAILED;
		}
		break;
	case OPENCHANGEDB_MESSAGE_OPEN:
		if (ldb_modify(msg->ldb_ctx, msg->res->msgs[0]) != LDB_SUCCESS) {
			return MAPI_E_CALL_FAILED;
		}
		break;
	}

	/* FIXME: Deal with SaveFlags */

	return MAPI_E_SUCCESS;
}

/**
   \details Initialize and open a message object

   \param mem_ctx pointer to the memory context to use for allocation
   \param ldb_ctx pointer to the ldb context
   \param messageID the identifier of the message to open
   \param folderID the identifier of the folder where the message is stored
   \param message_object pointer on pointer to the message object to return
   \param msgp pointer on pointer to the mapistore message to return

   \return MAPI_E_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_message_open(TALLOC_CTX *mem_ctx, struct ldb_context *ldb_ctx,
						   uint64_t messageID, uint64_t folderID, 
						   void **message_object, void **msgp)
{
	struct mapistore_message	*mmsg;
	struct openchangedb_message	*msg;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	char				*ldb_filter;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);

	msg = talloc_zero(mem_ctx, struct openchangedb_message);
	if (!msg) {
		return MAPI_E_NOT_ENOUGH_MEMORY;
	}
	printf("openchangedb_message_open: folderID=%"PRIu64" messageID=%"PRIu64"\n", folderID, messageID);

	msg->status = OPENCHANGEDB_MESSAGE_OPEN;
	msg->folderID = folderID;
	msg->messageID = messageID;
	msg->ldb_ctx = ldb_ctx;
	msg->msg = NULL;
	msg->res = NULL;

	/* Open the message and load results */
	ldb_filter = talloc_asprintf(mem_ctx, "(&(PidTagParentFolderId=%"PRIu64")(PidTagMessageId=%"PRIu64"))", folderID, messageID);
	ret = ldb_search(ldb_ctx, (TALLOC_CTX *)msg, &msg->res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter, NULL);
	printf("We have found: %d messages for ldb_filter = %s\n", msg->res->count, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !msg->res->count, MAPI_E_NOT_FOUND, msg);
	*message_object = (void *)msg;

	if (msgp) {
		mmsg = talloc_zero(mem_ctx, struct mapistore_message);
		mmsg->subject_prefix = NULL;
		mmsg->normalized_subject = (char *)ldb_msg_find_attr_as_string(msg->res->msgs[0], "PidTagNormalizedSubject", NULL);
		mmsg->columns = NULL;
		mmsg->recipients_count = 0;
		mmsg->recipients = NULL;
		*msgp = (void *)mmsg;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve a property on a LDB message

   \param mem_ctx pointer to the memory context
   \param message_object the openchangedb message to retrieve data from
   \param proptag the MAPI property tag to lookup
   \param data pointer on pointer to the data to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_message_get_property(TALLOC_CTX *mem_ctx, 
							   void *message_object, 
							   uint32_t proptag, void **data)
{
	struct openchangedb_message	*msg;
	struct ldb_message		*message;
	char				*PidTagAttr = NULL;
	bool				tofree = false;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!data, MAPI_E_NOT_INITIALIZED, NULL);

	msg = (struct openchangedb_message *)message_object;

	/* Get results from correct location */
	switch (msg->status) {
	case OPENCHANGEDB_MESSAGE_CREATE:
		OPENCHANGE_RETVAL_IF(!msg->msg, MAPI_E_NOT_INITIALIZED, NULL);
		message = msg->msg;
		break;
	case OPENCHANGEDB_MESSAGE_OPEN:
		OPENCHANGE_RETVAL_IF(!msg->res, MAPI_E_NOT_INITIALIZED, NULL);
		OPENCHANGE_RETVAL_IF(!msg->res->count, MAPI_E_NOT_INITIALIZED, NULL);
		message = msg->res->msgs[0];
		break;
	}


	/* Turn property into PidTagAttr */
	PidTagAttr = (char *) openchangedb_property_get_attribute(proptag);
	if (!PidTagAttr) {
		tofree = true;
		PidTagAttr = talloc_asprintf(mem_ctx, "%.8x", proptag);
	}

	/* Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(message, PidTagAttr), MAPI_E_NOT_FOUND, (tofree == true) ? PidTagAttr : NULL);

	/* Retrieve data */
	*data = openchangedb_get_property_data_message(mem_ctx, message, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, (tofree == true) ? PidTagAttr : NULL);

	if (tofree == true) {
		talloc_free(PidTagAttr);
	}

	return MAPI_E_NOT_FOUND;
}

/**
   \details Set a list of properties on a message

   \param mem_ctx pointer to the memory context
   \param message_object pointer to the openchangedb message object
   \param row pointer to the SRow structure holding the array of
   properties to set on the message

   \return MAPI_E_SUCCESS on success, otherwise MAPI errors.
 */
_PUBLIC_ enum MAPISTATUS openchangedb_message_set_properties(TALLOC_CTX *mem_ctx,
							     void *message_object, 
							     struct SRow *row)
{
	struct openchangedb_message	*msg = (struct openchangedb_message *) message_object;
	struct ldb_message		*message;
	struct ldb_message_element	*element;
	char				*PidTagAttr = NULL;
	struct SPropValue		value;
	char				*str_value;
	int				i;
	bool				tofree = false;

	DEBUG(5, ("openchangedb_message_set_properties\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!msg, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!row, MAPI_E_NOT_INITIALIZED, NULL);

	/* Get results from correct location */
	switch (msg->status) {
	case OPENCHANGEDB_MESSAGE_CREATE:
		OPENCHANGE_RETVAL_IF(!msg->msg, MAPI_E_NOT_INITIALIZED, NULL);
		message = msg->msg;
		break;
	case OPENCHANGEDB_MESSAGE_OPEN:
		OPENCHANGE_RETVAL_IF(!msg->res, MAPI_E_NOT_INITIALIZED, NULL);
		OPENCHANGE_RETVAL_IF(!msg->res->count, MAPI_E_NOT_INITIALIZED, NULL);
		message = msg->res->msgs[0];
		break;
	}

	for (i = 0; i < row->cValues; i++) {
		tofree = false;
		value = row->lpProps[i];
		switch (value.ulPropTag) {
		case PR_DEPTH:
		case PR_SOURCE_KEY:
		case PR_PARENT_SOURCE_KEY:
		case PR_CHANGE_KEY:
			DEBUG(5, ("Ignored attempt to set handled property %.8x\n", value.ulPropTag));
			break;
		default:
			/* Convert proptag into PidTag attribute */
			PidTagAttr = (char *) openchangedb_property_get_attribute(value.ulPropTag);
			if (!PidTagAttr) {
				PidTagAttr = talloc_asprintf(mem_ctx, "%.8x", value.ulPropTag);
				tofree = true;
			}

			str_value = openchangedb_set_folder_property_data((TALLOC_CTX *)message, &value);
			if (!str_value) {
				DEBUG(5, ("Ignored property of unhandled type %.4x\n", (value.ulPropTag & 0xFFFF)));
				continue;
			} else {
				element = ldb_msg_find_element(message, PidTagAttr);
				if (!element) {
				/* Case where the element doesn't exist */
					ldb_msg_add_string(message, PidTagAttr, str_value);
					message->elements[message->num_elements - 1].flags = LDB_FLAG_MOD_ADD;
				} else {
					switch (msg->status) {
					case OPENCHANGEDB_MESSAGE_CREATE:
						ldb_msg_remove_element(message, element);
						ldb_msg_add_string(message, PidTagAttr, str_value);
						message->elements[message->num_elements - 1].flags = LDB_FLAG_MOD_ADD;
						break;
					case OPENCHANGEDB_MESSAGE_OPEN:
						if (element->flags == LDB_FLAG_MOD_ADD) {
							ldb_msg_remove_element(message, element);
							ldb_msg_add_string(message, PidTagAttr, str_value);
							message->elements[message->num_elements - 1].flags = LDB_FLAG_MOD_ADD;
						} else {
							ldb_msg_remove_element(message, element);
							ldb_msg_add_string(message, PidTagAttr, str_value);
							message->elements[message->num_elements - 1].flags = LDB_FLAG_MOD_REPLACE;							
						}
						break;
					}
}
			}
			if (tofree == true) {
				talloc_free((char *)PidTagAttr);
			}
			break;
		}
	}

	DEBUG(5, ("openchangedb_message_set_properties end\n"));
	return MAPI_E_SUCCESS;
}
