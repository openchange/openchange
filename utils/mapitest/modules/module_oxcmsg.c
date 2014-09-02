/*
   Stand-alone MAPI testsuite

   OpenChange Project - E-MAIL OBJECT PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Brad Hards <bradh@openchange.org> 2009

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

#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_oxcmsg.c

   \brief Message and Attachment Object Protocol test suite
*/


/**
   \details Test the CreateMessage (0x6) operation

   This function:
	-# Log on the user private mailbox
	-# Open the Outbox folder
	-# Create the message
	-# Delete the message

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_CreateMessage(struct mapitest *mt)
{
	bool			ret;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_id_t		id_msgs[1];

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, olFolderOutbox);
	if (ret == false) return ret;

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Delete the message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}

#define	OXCMSG_SETREADFLAGS	"[OXCMSG] SetMessageReadFlag"

/**
   \details Test the SetMessageReadFlag (0x11) operation

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder
	-# Create a tmp message
	-# Play with SetMessageReadFlag
	-# Delete the message
	
   Note: We can test either SetMessageReadFlag was effective by checking its
   old/new value with GetProps on PR_MESSAGE_FLAGS property.


   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_SetMessageReadFlag(struct mapitest *mt)
{
	bool			ret;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	mapi_id_t		id_msgs[1];
	uint32_t		status= 0;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Step 2. Open Outbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, olFolderOutbox);
	if (ret == false) return ret;

	/* Step 3. Create the tmp message and save it */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, OXCMSG_SETREADFLAGS);
	if (ret == false) return ret;

	SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadWrite);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Play with SetMessageReadFlag */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_MID, PR_MESSAGE_FLAGS);
	ret = true;

	/* 1. Retrieve and Save the original PR_MESSAGE_FLAGS value */
	GetProps(&obj_message, 0, SPropTagArray, &lpProps, &cValues);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	if (cValues > 1) {
		status = lpProps[1].value.l;
	}
	MAPIFreeBuffer(lpProps);
	
	/* Set message flags as read */
	SetMessageReadFlag(&obj_folder, &obj_message, MSGFLAG_READ);
	mapitest_print_retval_fmt(mt, "SetMessageReadFlag", "(%s)", "MSGFLAG_READ");

	/* Check if the operation was successful */
	GetProps(&obj_message, 0, SPropTagArray, &lpProps, &cValues);
	mapitest_print_retval(mt, "GetProps");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	if (cValues > 1 && status != lpProps[1].value.l) {
		mapitest_print(mt, "* %-35s: PR_MESSAGE_FLAGS changed\n", "SetMessageReadFlag");
		status = lpProps[1].value.l;
	} else {
		mapitest_print(mt, "* %-35s: PR_MESSAGE_FLAGS failed\n", "SetMessageReadFlag");
		return false;
	}
	MAPIFreeBuffer(lpProps);
		
	/* Set the message flags as submitted */
	SetMessageReadFlag(&obj_folder, &obj_message, MSGFLAG_SUBMIT);
	mapitest_print_retval_fmt(mt, "SetMessageReadFlag", "(%s)", "MSGFLAG_SUBMIT");
	
	/* Check if the operation was successful */
	GetProps(&obj_message, 0, SPropTagArray, &lpProps, &cValues);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	if (cValues > 1 && status != lpProps[1].value.l) {
		mapitest_print(mt, "* %-35s: PR_MESSAGE_FLAGS changed\n", "SetMessageReadFlag");
		status = lpProps[1].value.l;
	} else {
		mapitest_print(mt, "* %-35s: PR_MESSAGE_FLAGS failed\n", "SetMessageReadFlag");
		return false;
	}

	/* Step 5. Delete the message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
	
	/* Release */
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	MAPIFreeBuffer(SPropTagArray);

	return true;
}

/**
   \details Test the ModifyRecipients (0xe) operation

   This function:
   -# Log on the user private mailbox
   -# Open the Outbox folder
   -# Create the message
   -# Resolve recipients names
   -# Call ModifyRecipients operation for MAPI_TO, MAPI_CC, MAPI_BCC
   -# Delete the message

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_ModifyRecipients(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_message;
	mapi_id_t			id_folder;
	char				**username = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyValue_r		value;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct SRowSet			*SRowSet = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	mapi_id_t			id_msgs[1];

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}


	/* Step 4. Resolve the recipients and call ModifyRecipients */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0xA,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_SEND_RICH_INFO,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);

	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = (char *)mt->profile->mailbox;
	username[1] = NULL;

	retval = ResolveNames(mapi_object_get_session(&obj_message), 
			      (const char **)username, SPropTagArray, 
			      &RowSet, &flaglist, MAPI_UNICODE);
	mapitest_print_retval_clean(mt, "ResolveNames", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	if (!RowSet) {
		mapitest_print(mt, "Null RowSet\n");
		return false;
	}
	if (!RowSet->cRows) {
		mapitest_print(mt, "No values in RowSet\n");
		MAPIFreeBuffer(RowSet);
		return false;
	}

	value.ulPropTag = PR_SEND_INTERNET_ENCODING;
	value.value.l = 0;
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);

	SRowSet = talloc_zero(RowSet, struct SRowSet);
	cast_PropertyRowSet_to_SRowSet(SRowSet, RowSet, SRowSet);

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	mapitest_print_retval(mt, "SetRecipientType");
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_TO");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_CC);
	mapitest_print_retval(mt, "SetRecipientType");
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_CC");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}


	SetRecipientType(&(SRowSet->aRow[0]), MAPI_BCC);
	mapitest_print_retval(mt, "SetRecipientType");
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_BCC");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Step 5. Delete the message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}
	/* Release */
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(flaglist);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the RemoveAllRecipients (0xd) operation

   This function:
   -# Log on the use private mailbox
   -# Open the Outbox folder
   -# Create the message, set recipients
   -# Save the message
   -# Remove all recipients
   -# Delete the message

   \param mt point on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_RemoveAllRecipients(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	bool				ret = false;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_message;
	mapi_id_t			id_folder;
	char				**username = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyValue_r		value;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct SRowSet			*SRowSet = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	mapi_id_t			id_msgs[1];

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0xA,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_SEND_RICH_INFO,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);

	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = (char *)mt->profile->mailbox;
	username[1] = NULL;

	retval = ResolveNames(mapi_object_get_session(&obj_message),
			      (const char **)username, SPropTagArray, 
			      &RowSet, &flaglist, MAPI_UNICODE);
	mapitest_print_retval_clean(mt, "ResolveNames", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	if (!RowSet) {
		mapitest_print(mt, "Null RowSet\n");
		return false;
	}
	if (!RowSet->cRows) {
		mapitest_print(mt, "No values in RowSet\n");
		MAPIFreeBuffer(RowSet);
		return false;
	}

	value.ulPropTag = PR_SEND_INTERNET_ENCODING;
	value.value.l = 0;
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);

	SRowSet = talloc_zero(RowSet, struct SRowSet);
	cast_PropertyRowSet_to_SRowSet(SRowSet, RowSet, SRowSet);

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_TO");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_CC);
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_CC");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}


	SetRecipientType(&(SRowSet->aRow[0]), MAPI_BCC);
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_BCC");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	ret = true;

	/* Step 4. Remove all recipients */
	retval = RemoveAllRecipients(&obj_message);
	mapitest_print_retval(mt, "RemoveAllRecipients");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		ret = false;
	}

	/* Step 5. Save the message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Step 6. Delete the message */
	errno = 0;
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		ret = false;
	}

	/* Release */
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(flaglist);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the ReadRecipients (0xf) operation

      This function:
      -# Log on the use private mailbox
      -# Open the Outbox folder
      -# Create the message, set recipients
      -# Save the message
      -# Read message recipients
      -# Delete the message

   \param mt point on the top-level mapitest structure
   
   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_ReadRecipients(struct mapitest *mt)
{
	enum MAPISTATUS			retval;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_message;
	mapi_id_t			id_folder;
	char				**username = NULL;
	struct SPropTagArray		*SPropTagArray = NULL;
	struct PropertyValue_r		value;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct SRowSet			*SRowSet = NULL;
	struct PropertyTagArray_r   	*flaglist = NULL;
	struct ReadRecipientRow		*RecipientRows;
	uint8_t				count;
	mapi_id_t			id_msgs[1];

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0xA,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_SEND_RICH_INFO,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);

	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = (char *)mt->profile->mailbox;
	username[1] = NULL;

	retval = ResolveNames(mapi_object_get_session(&obj_message),
			      (const char **)username, SPropTagArray, 
			      &RowSet, &flaglist, MAPI_UNICODE);
	mapitest_print_retval_clean(mt, "ResolveNames", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	if (!RowSet) {
		mapitest_print(mt, "Null RowSet\n");
		return false;
	}
	if (!RowSet->cRows) {
		mapitest_print(mt, "No values in RowSet\n");
		MAPIFreeBuffer(RowSet);
		return false;
	}

	value.ulPropTag = PR_SEND_INTERNET_ENCODING;
	value.value.l = 0;
	PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);

	SRowSet = talloc_zero(RowSet, struct SRowSet);
	cast_PropertyRowSet_to_SRowSet(SRowSet, RowSet, SRowSet);

	retval = SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	mapitest_print_retval_clean(mt, "SetRecipientType", retval);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_TO");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_CC);
	mapitest_print_retval(mt, "SetRecipientType");
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_CC");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	SetRecipientType(&(SRowSet->aRow[0]), MAPI_BCC);
	mapitest_print_retval(mt, "SetRecipientType");
	retval = ModifyRecipients(&obj_message, SRowSet);
	mapitest_print_retval_fmt(mt, "ModifyRecipients", "(%s)", "MAPI_BCC");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Step 4. Save the message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Step 5. Read recipients */
	RecipientRows = talloc_zero(mt->mem_ctx, struct ReadRecipientRow);
	retval = ReadRecipients(&obj_message, 0, &count, &RecipientRows);
	mapitest_print_retval(mt, "ReadRecipients");
	MAPIFreeBuffer(RecipientRows);
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Step 6. Delete the message */
	errno = 0;
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		MAPIFreeBuffer(RowSet);
		MAPIFreeBuffer(flaglist);
		return false;
	}

	/* Release */
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(flaglist);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the SaveChangesMessage (0xc) operation

   This function:
   -# Log on the user private mailbox
   -# Open the Outbox folder
   -# Create the message
   -# Save the message
   -# Delete the message

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_SaveChangesMessage(struct mapitest *mt)
{
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_id_t		id_folder;
	mapi_id_t		id_msgs[1];

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_folder);
	OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	mapi_object_init(&obj_message);
	CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Save the message */
	SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Delete the saved message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetMessageStatus (0x1f) operation

   This function:
   -# Log on the user private mailbox
   -# Open the outbox folder
   -# Create the message
   -# Save the message
   -# Get outbox contents table
   -# Get messages status
   -# Delete the message

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_GetMessageStatus(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_ctable;
	mapi_id_t		id_folder;
	mapi_id_t		id_msgs[1];
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	uint32_t		status;
	uint32_t		i;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_message);
	mapi_object_init(&obj_ctable);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_clean(mt, "OpenMsgStore", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	mapitest_print_retval_clean(mt, "GetDefaultFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval_clean(mt, "OpenFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the message */
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval_clean(mt, "CreateMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Save the message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Get outbox contents table */
	retval = GetContentsTable(&obj_folder, &obj_ctable, 0, &count);
	mapitest_print_retval_clean(mt, "GetContentsTable", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_MID, PR_MSG_STATUS);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	mapitest_print_retval_clean(mt, "GetMessageStatus", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 6. Get Message Status */
	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		count -= SRowSet.cRows;
		for (i = 0; i < SRowSet.cRows; i++) {
			retval = GetMessageStatus(&obj_folder, SRowSet.aRow[i].lpProps[0].value.d, &status);
			mapitest_print_retval_clean(mt, "GetMessageStatus", retval);
			if (retval != MAPI_E_SUCCESS) {
				return false;
			}
		}
	}

	/* Step 7. Delete the saved message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval_clean(mt, "DeleteMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


struct msgstatus {
	uint32_t	status;
	const char	*name;
};

static struct msgstatus msgstatus[] = {
	{MSGSTATUS_HIDDEN,		"MSGSTATUS_HIDDEN"},
	{MSGSTATUS_HIGHLIGHTED,		"MSGSTATUS_HIGHLIGHTED"},
	{MSGSTATUS_TAGGED,		"MSGSTATUS_TAGGED"},
	{MSGSTATUS_REMOTE_DOWNLOAD,	"MSGSTATUS_REMOTE_DOWNLOAD"},
	{MSGSTATUS_REMOTE_DELETE,	"MSGSTATUS_REMOTE_DELETE"},
	{MSGSTATUS_DELMARKED,		"MSGSTATUS_DELMARKED"},
	{0,				NULL}
};

/**
   \details Test the GetMessageStatus (0x1f) operation

   This function:
   -# Log on the user private mailbox
   -# Open the outbox folder
   -# Create the message
   -# Save the message
   -# Get outbox contents table
   -# Set different messages status, then get them and compare values
   -# Delete the message

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_SetMessageStatus(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		obj_ctable;
	mapi_id_t		id_folder;
	mapi_id_t		id_msgs[1];
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	uint32_t		i;
	uint32_t		ulOldStatus = 0;
	uint32_t		ulOldStatus2 = 0;
	bool			ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_message);
	mapi_object_init(&obj_ctable);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_clean(mt, "OpenMsgStore", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	mapitest_print_retval_clean(mt, "GetDefaultFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval_clean(mt, "OpenFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 3. Create the message */
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval_clean(mt, "CreateMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 4. Save the message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 5. Get outbox contents table */
	retval = GetContentsTable(&obj_folder, &obj_ctable, 0, &count);
	mapitest_print_retval_clean(mt, "GetContentsTable", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_MID, PR_MSG_STATUS);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	mapitest_print_retval_clean(mt, "SetColumns", retval);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Fetch the first email */
	retval = QueryRows(&obj_ctable, 1, TBL_NOADVANCE, &SRowSet);
	mapitest_print_retval_clean(mt, "QueryRows", retval);
	if (retval != MAPI_E_SUCCESS || SRowSet.cRows == 0) {
		ret = false;
		goto release;
	}

	/* Step 6. SetMessageStatus + GetMessageStatus + Comparison */
	for (i = 0; msgstatus[i].name; i++) {
		retval = SetMessageStatus(&obj_folder, SRowSet.aRow[0].lpProps[0].value.d,
					  msgstatus[i].status, msgstatus[i].status, &ulOldStatus2);
		mapitest_print_retval_clean(mt, "SetMessageStatus", retval);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			goto release;
		}

		retval = GetMessageStatus(&obj_folder, SRowSet.aRow[0].lpProps[0].value.d, &ulOldStatus);
		mapitest_print_retval_clean(mt, "GetMessageStatus", retval);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			goto release;
		}

		if ((ulOldStatus != ulOldStatus2) && (ulOldStatus & msgstatus[i].status)) {
			errno = 0;
			mapitest_print(mt, "* %-35s: %s 0x%.8x\n", "Comparison", msgstatus[i].name, GetLastError());
		}
	}

	/* Step 7. Delete the saved message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

release:
	/* Release */
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test the SetReadFlags (0x66) operation

   This function:
   -# Opens the Inbox folder and creates some test content
   -# Checks that the PR_MESSAGE_FLAGS property on each message is 0x0
   -# Apply SetReadFlags() on every second messages
   -# Check the results are as expected
   -# Apply SetReadFlags() again
   -# Check the results are as expected
   -# Cleanup
	
   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_SetReadFlags(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_htable;
	mapi_object_t		obj_test_folder;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	struct mt_common_tf_ctx	*context;
	int			i;
	int64_t			messageIds[5];

	/* Step 1. Logon */
	if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
		return false;
	}

	/* Fetch the contents table for the test folder */
	context = mt->priv;
	mapi_object_init(&(obj_test_folder));
	retval = GetContentsTable(&(context->obj_test_folder), &(obj_test_folder), 0, NULL);
	mapitest_print_retval_clean(mt, "GetContentsTable", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2, PR_MID, PR_MESSAGE_FLAGS);
	retval = SetColumns(&obj_test_folder, SPropTagArray);
	mapitest_print_retval_clean(mt, "SetColumns", retval);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = QueryRows(&obj_test_folder, 10, TBL_NOADVANCE, &SRowSet);
	mapitest_print_retval_clean(mt, "QueryRows", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	/* 0x0400 is mfEverRead - the message has been read at least once. We see this on Exchange 2010 */
	for (i = 0; i < 10; ++i) {
		if ((*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i].lpProps[1])) & ~0x0400) != 0x0) {
			mapitest_print(mt, "* %-35s: unexpected flag at %i 0x%x\n", "QueryRows", i,
				       (*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i].lpProps[1]))));
			ret = false;
			goto cleanup;
		}
	}	

	for (i = 0; i < 5; ++i) {
		messageIds[i] = (*(const int64_t *)get_SPropValue_data(&(SRowSet.aRow[i*2].lpProps[0])));
	}

	retval = SetReadFlags(&(context->obj_test_folder), 0x0, 5, messageIds);
	mapitest_print_retval_clean(mt, "SetReadFlags", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = QueryRows(&obj_test_folder, 10, TBL_NOADVANCE, &SRowSet);
	mapitest_print_retval_clean(mt, "QueryRows", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	for (i = 0; i < 10; i+=2) {
		if ((*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i].lpProps[1])) & ~0x0400) != MSGFLAG_READ) {
			mapitest_print(mt, "* %-35s: unexpected flag (0) at %i 0x%x\n", "QueryRows", i,
				       (*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i].lpProps[1]))));
			ret = false;
			goto cleanup;
		}
		if ((*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i+1].lpProps[1])) & ~0x0400) != 0x0) {
			mapitest_print(mt, "* %-35s: unexpected flag (1) at %i 0x%x\n", "QueryRows", i,
				       (*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i+1].lpProps[1]))));
			ret = false;
			goto cleanup;
		}
	}	

	SetReadFlags(&(context->obj_test_folder), CLEAR_READ_FLAG, 5, messageIds);

	retval = QueryRows(&obj_test_folder, 10, TBL_NOADVANCE, &SRowSet);
	mapitest_print_retval_clean(mt, "QueryRows", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	for (i = 0; i < 10; ++i) {
		if ((*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i].lpProps[1])) & ~0x0400) != 0x0) {
			mapitest_print(mt, "* %-35s: unexpected flag (3) at %i 0x%x\n", "QueryRows", i,
				       (*(const uint32_t *)get_SPropValue_data(&(SRowSet.aRow[i].lpProps[1]))));
			ret = false;
			goto cleanup;
		}
	}

 cleanup:
	/* Cleanup and release */
	talloc_free(SRowSet.aRow);
	mapi_object_release(&obj_htable);
	mapitest_common_cleanup(mt);

	return ret;
}

/**
   \details Test the OpenEmbeddedMessage (0x46) and CreateAttach (0x23) operations

   This function:
        -# Logs on the user private mailbox
        -# Open the Inbox folder         
        -# Create a test message          
        -# Embed a message in the test message
        -# Delete the test message                  

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */                                        
_PUBLIC_ bool mapitest_oxcmsg_OpenEmbeddedMessage(struct mapitest *mt)
{                                                                     
	enum MAPISTATUS		retval;                               
	bool			ret;                                  
	mapi_object_t		obj_store;                            
	mapi_object_t		obj_folder;                           
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach;
	mapi_object_t		obj_embeddedmsg;
	mapi_id_t		id_msgs[1];                           
	struct SPropValue	attach[2];
	struct SRowSet		SRowSet;
	struct SPropTagArray	SPropTagArray;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_store);
		return false;                          
	}

	/* Step 2. Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, olFolderInbox);
	if (ret == false) {
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* Step 3. Create the tmp message and save it */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, OXCMSG_SETREADFLAGS);
	if (ret == false) return ret;

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadWrite);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* Step 4. Embed another message in the message */
	mapi_object_init(&obj_attach);
	retval = CreateAttach(&obj_message, &obj_attach);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateAttach", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* use SetProps() to set the attachment up */
	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_EMBEDDED_MSG;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	retval = SetProps(&obj_attach, 0, attach, 2);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetProps", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	mapi_object_init(&obj_embeddedmsg);
	retval = OpenEmbeddedMessage(&obj_attach, &obj_embeddedmsg, MAPI_CREATE);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenEmbeddedMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	ret = mapitest_common_message_fill(mt, &obj_embeddedmsg, "MT EmbeddedMessage");
	if (ret == false) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	// Save the changes to the embedded message
	retval = SaveChangesMessage(&obj_message, &obj_embeddedmsg, KeepOpenReadOnly);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}
	// Save the changes to the attachment and then the message
	retval = SaveChangesAttachment(&obj_message, &obj_attach, KeepOpenReadOnly);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChangesAttachment", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* Step 5. Open the embedded message */
	mapi_object_init(&obj_attach);
	retval = OpenAttach(&obj_message, 0, &obj_attach);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenAttach", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}
	retval = OpenEmbeddedMessage(&obj_attach, &obj_embeddedmsg, MAPI_READONLY);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenEmbeddedMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_embeddedmsg);
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* Step 6. Get the recipient table */
	retval = GetRecipientTable(&obj_embeddedmsg, &SRowSet, &SPropTagArray);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetRecipientTable", retval);

	mapitest_print_SRowSet(mt, &SRowSet, "\t * ");
	mapi_object_release(&obj_embeddedmsg);

	/* Step 7. Delete the message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "DeleteMessage", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_attach);
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* Release */
	mapi_object_release(&obj_attach);
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetValidAttachments (0x52) and CreateAttach (0x23) operations

   This function:
	-# Logs on the user private mailbox
	-# Open the Inbox folder         
	-# Create a test message          
	-# Check the number of valid attachments is zero
	-# Create two attachments
	-# Check the number of valid attachments is two (numbered 0 and 1)
	-# Delete the first attachment
	-# Check the number of valid attachments is one (numbered 1)
	-# Delete the test message                  

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */                                        
_PUBLIC_ bool mapitest_oxcmsg_GetValidAttachments(struct mapitest *mt)
{                                                                     
	enum MAPISTATUS		retval;                               
	bool			ret;                                  
	mapi_object_t		obj_store;                            
	mapi_object_t		obj_folder;                           
	mapi_object_t		obj_message;
	mapi_object_t		obj_attach0;
	mapi_object_t		obj_attach1;
	mapi_id_t		id_msgs[1];                           
	struct SPropValue	attach[3];
	uint16_t		numAttach;
	uint32_t		*attachmentIds;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_store);
		return false;                          
	}

	/* Step 2. Open Inbox folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_folder, olFolderInbox);
	if (ret == false) {
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	/* Step 3. Create the test message and save it */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, OXCMSG_SETREADFLAGS);
	if (ret == false) return ret;

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadWrite);
	mapitest_print_retval_clean(mt, "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_message);
		mapi_object_release(&obj_folder);
		mapi_object_release(&obj_store);
		return false;
	}

	ret = true;

	/* Step 4. Check the number of valid attachments */
	numAttach = 99;
	retval = GetValidAttach(&obj_message, &numAttach, &attachmentIds);
	mapitest_print_retval_fmt_clean(mt, "GetValidAttach", retval, "%i", numAttach);
	if (numAttach != 0) {
		ret = false;
		goto cleanup_wo_attach;
	}
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup_wo_attach;
	}

	/* Step 5. Create two attachments to the message */
	mapi_object_init(&obj_attach0);
	mapi_object_init(&obj_attach1);
	retval = CreateAttach(&obj_message, &obj_attach0);
	mapitest_print_retval_clean(mt, "CreateAttach", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* use SetProps() to set the attachment up */
	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = "Attachment 0";
	retval = SetProps(&obj_attach0, 0, attach, 3);
	mapitest_print_retval_clean(mt, "SetProps", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Save the changes to the attachment and then the message */
	retval = SaveChangesAttachment(&obj_message, &obj_attach0, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesAttachment", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = CreateAttach(&obj_message, &obj_attach1);
	mapitest_print_retval_clean(mt, "CreateAttach", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* use SetProps() to set the attachment up */
	attach[0].ulPropTag = PR_ATTACH_METHOD;
	attach[0].value.l = ATTACH_BY_VALUE;
	attach[1].ulPropTag = PR_RENDERING_POSITION;
	attach[1].value.l = 0;
	attach[2].ulPropTag = PR_ATTACH_FILENAME;
	attach[2].value.lpszA = "Attachment 1";
	retval = SetProps(&obj_attach1, 0, attach, 3);
	mapitest_print_retval_clean(mt, "SetProps", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Save the changes to the attachment and then the message */
	retval = SaveChangesAttachment(&obj_message, &obj_attach1, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesAttachment", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadWrite);
	mapitest_print_retval_clean(mt, "SaveChangesMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6. Check the number of valid attachments */
	numAttach = 99;
	retval = GetValidAttach(&obj_message, &numAttach, &attachmentIds);
	mapitest_print_retval_fmt_clean(mt, "GetValidAttach", retval, "%i", numAttach);
	if (numAttach != 2) {
		ret = false;
		goto cleanup;
	}
	mapitest_print(mt, "IDs: %d, %d\n", attachmentIds[0], attachmentIds[1]);
	if ( (attachmentIds[0] != 0) || (attachmentIds[1] != 1) ) {
		ret = false;
		goto cleanup;
	}
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 7. Delete the first attachment */
	retval = DeleteAttach(&obj_message, attachmentIds[0]);
	mapitest_print_retval_clean(mt, "DeleteAttach", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 8. Check the number of valid attachments */
	numAttach = 99;
	retval = GetValidAttach(&obj_message, &numAttach, &attachmentIds);
	mapitest_print_retval_fmt_clean(mt, "GetValidAttach", retval, "%i", numAttach);
	if (numAttach != 1) {
		ret = false;
		goto cleanup;
	}
	mapitest_print(mt, "IDs: %d\n", attachmentIds[0]);
	if ( (attachmentIds[0] != 1) ) {
		ret = false;
		goto cleanup;
	}
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 9. Delete the message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval_clean(mt, "DeleteMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Release */
	mapi_object_release(&obj_attach0);
	mapi_object_release(&obj_attach1);
cleanup_wo_attach:
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test the ReloadCachedInformation (0x10) operation

   This function:
   -# Logs on to the user private mailbox
   -# Open the outbox folder
   -# Create the message
   -# Save the message
   -# Reloads the cached message information
   -# Delete the message

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcmsg_ReloadCachedInformation(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_id_t		id_folder;
	mapi_id_t		id_msgs[1];
	bool			ret;

	ret = true;

	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_message);
	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Outbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderOutbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create the message */
	retval = CreateMessage(&obj_folder, &obj_message);
	mapitest_print_retval(mt, "CreateMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Save the message */
	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Reload cached information */
	retval = ReloadCachedInformation(&obj_message);
	mapitest_print_retval(mt, "ReloadCachedInformation");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6. Delete the saved message */
	id_msgs[0] = mapi_object_get_id(&obj_message);
	retval = DeleteMessage(&obj_folder, id_msgs, 1);
	mapitest_print_retval(mt, "DeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

cleanup:
	/* Release */
	mapi_object_release(&obj_message);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}
