/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008-2013

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

#include <fcntl.h>

/**
	\file
	Support functions for %mapitest modules

	These functions implement commonly needed functionality that
	would otherwise be copied into each module implementation
*/

/**
     Opens a default folder

     This function opens one of the default (standard) folders,
     returning the folder as obj_child. olNum may be one of:
	- olFolderTopInformationStore
	- olFolderDeletedItems
	- olFolderOutbox
	- olFolderSentMail
	- olFolderInbox
	- olFolderCalendar
	- olFolderContacts
	- olFolderJournal
	- olFolderNotes
	- olFolderTasks
	- olFolderDrafts

     \param mt pointer to the top level mapitest structure
     \param obj_parent parent folder (usually the message store, must be opened)
     \param obj_child the folder that has been opened
     \param olNum the folder identifier (see list above)

     \return true on success, false on failure
 */
_PUBLIC_ bool mapitest_common_folder_open(struct mapitest *mt,
					  mapi_object_t *obj_parent, 
					  mapi_object_t *obj_child,
					  uint32_t olNum)
{
	mapi_id_t	id_child;

	GetDefaultFolder(obj_parent, &id_child, olNum);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetDefaultFolder", GetLastError());
		return false;
	}

	OpenFolder(obj_parent, id_child, obj_child);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenFolder", GetLastError());
		return false;
	}

	return true;
}

/**
   This function deletes messages in a folder, based on matching the subject
   name. This is meant to clean up a folder after a test has been run.

   \param mt pointer to the top level mapitest structure
   \param obj_folder the folder to search through
   \param subject the message subject to match
*/
_PUBLIC_ bool mapitest_common_message_delete_by_subject(struct mapitest *mt,
							mapi_object_t *obj_folder,
							const char *subject)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_ctable;
	mapi_id_t		msgids[1];
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	const char		*msubject = NULL;
	uint32_t		i;

	/* Sanity checks */
	if (subject == NULL) return false;

	/* Retrieve the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(obj_folder, &obj_ctable, 0, &count);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetContentsTable", GetLastError());
		mapi_object_release(&obj_ctable);
		return false;
	}

	/* Customize the content table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_MID,
					  PR_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns", GetLastError());
		mapi_object_release(&obj_ctable);
		return false;
	}

	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) && !retval && SRowSet.cRows) {
		for (i = 0; i < SRowSet.cRows; i++) {
			if (retval == MAPI_E_SUCCESS) {
				msgids[0] = SRowSet.aRow[i].lpProps[0].value.d;
				msubject = (const char *)find_SPropValue_data(&SRowSet.aRow[i], PR_SUBJECT);
				if (msubject && !strncmp(subject, msubject, strlen(subject))) {
					retval = DeleteMessage(obj_folder, msgids, 1);
					if (retval != MAPI_E_SUCCESS) {
						mapitest_print_retval(mt, "DeleteMessage");
						mapi_object_release(&obj_ctable);
						return false;
					}
				}
			}
		}
	}
	mapi_object_release(&obj_ctable);
	return false;
}


/**
   Find a folder within a container
 */
_PUBLIC_ bool mapitest_common_find_folder(struct mapitest *mt,
					  mapi_object_t *obj_parent,
					  mapi_object_t *obj_child,
					  const char *name)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	mapi_object_t		obj_htable;
	const char		*newname;
	const int64_t		*fid;
	uint32_t		count;
	uint32_t		index;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(obj_parent, &obj_htable, 0, &count);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_htable);
		return false;
	}

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x2,
					  PR_DISPLAY_NAME,
					  PR_FID);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_htable);
		return false;
	}

	while (((retval = QueryRows(&obj_htable, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const int64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			newname = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME);

			if (newname && fid && !strcmp(newname, name)) {
				retval = OpenFolder(obj_parent, *fid, obj_child);
				mapi_object_release(&obj_htable);
				return true;
			}
		}
	}

	mapi_object_release(&obj_htable);
	return false;
}


/**
 * Create a message ready to submit
 */
_PUBLIC_ bool mapitest_common_message_create(struct mapitest *mt,
					     mapi_object_t *obj_folder, 
					     mapi_object_t *obj_message,
					     const char *subject)
{
	enum MAPISTATUS		retval;

	/* Create the message */
	retval = CreateMessage(obj_folder, obj_message);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "(Common) CreateMessage");
		return false;
	}

	return mapitest_common_message_fill(mt, obj_message, subject);
}

/**
 * Create a message ready to submit
 */
_PUBLIC_ bool mapitest_common_message_fill(struct mapitest *mt,
					   mapi_object_t *obj_message,
					   const char *subject)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct PropertyRowSet_r		*RowSet = NULL;
	struct SRowSet			*SRowSet = NULL;
	struct PropertyTagArray_r	*flaglist = NULL;
	struct PropertyValue_r		value;
	struct SPropValue		lpProps[4];
	const char			*username[2];
	const char			*body;
	uint32_t			msgflag;
	uint32_t			format;
	uint32_t			ret;

	/* Sanity checks */
	if (subject == NULL) return false;

	/* Resolve recipients */
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

	username[0] = (char *)mt->profile->mailbox;
	username[1] = NULL;

	RowSet = talloc_zero(mt->mem_ctx, struct PropertyRowSet_r);
	flaglist = talloc_zero(mt->mem_ctx, struct PropertyTagArray_r);

	retval = ResolveNames(mapi_object_get_session(obj_message), username, SPropTagArray, 
			      &RowSet, &flaglist, MAPI_UNICODE);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "(Common) ResolveNames");
		talloc_free(RowSet);
		talloc_free(flaglist);
		return false;
	}

	value.ulPropTag = PR_SEND_INTERNET_ENCODING;
	value.value.l = 0;
	ret = PropertyRowSet_propcpy(mt->mem_ctx, RowSet, value);
	if (ret) return false;

	SRowSet = talloc_zero(RowSet, struct SRowSet);
	cast_PropertyRowSet_to_SRowSet(SRowSet, RowSet, SRowSet);

	/* Set Recipients */
	SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	retval = ModifyRecipients(obj_message, SRowSet);
	MAPIFreeBuffer(RowSet);
	MAPIFreeBuffer(flaglist);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "(Common) ModifyRecipients");
		return false;
	}

	/* Set message properties */
	msgflag = MSGFLAG_SUBMIT;
	set_SPropValue_proptag(&lpProps[0], PR_SUBJECT, (const void *) subject);
	set_SPropValue_proptag(&lpProps[1], PR_MESSAGE_FLAGS, (const void *)&msgflag);
	body = talloc_asprintf(mt->mem_ctx, "Body of message with subject: %s", subject);
	set_SPropValue_proptag(&lpProps[2], PR_BODY, (const void *)body);
	format = EDITOR_FORMAT_PLAINTEXT;
	set_SPropValue_proptag(&lpProps[3], PR_MSG_EDITOR_FORMAT, (const void *)&format);

	retval = SetProps(obj_message, 0, lpProps, 4);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "(Common) SetProps");
		return false;
	}

	errno = 0;
	return true;
}


/**
   Generate a random blob of readable data

   \param mem_ctx the talloc memory context to create the blob in
   \param len the length of the blob to create

   \return random blob of readable data, of length len bytes, with a
           null terminator.

   \note the data is from 0..len, and the null terminator is at position
         len+1. So the returned array is actually len+1 bytes in total.

 */
_PUBLIC_ char *mapitest_common_genblob(TALLOC_CTX *mem_ctx, size_t len)
{
	int		fd;
	int		ret;
	unsigned int	i;
	char		*retstr;
	const char	*list = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_-#.,";
	int		list_len = strlen(list);

	/* Sanity check */
	if (!mem_ctx || (len == 0)) {
		return NULL;
	}

	fd = open("/dev/urandom", O_RDONLY, 0);
	if (fd == -1) {
		return NULL;
	}

	retstr = talloc_array(mem_ctx, char, len + 1);
	if ((ret = read(fd, retstr, len)) == -1) {
		close(fd);
		talloc_free(retstr);
		return NULL;
	}

	for (i = 0; i < len; i++) {
		retstr[i] = list[retstr[i] % list_len];
		if (!retstr[i]) {
			retstr[i] = 'X';
		}
	}
	retstr[i] = '\0';

	close(fd);
	return retstr;
}

/**
	Create a test folder, and fill with 10 sample messages

	This function creates a test folder (name set by the MT_DIRNAME_TEST define),
	and fills it with 5 messages with the same subject and 5 messages with the
	same sender.

	\param mt pointer to the mapitest context
*/
_PUBLIC_ bool mapitest_common_create_filled_test_folder(struct mapitest *mt)
{
	struct mt_common_tf_ctx	*context;
	enum MAPISTATUS		retval;
	const char		*from = NULL;
	const char		*subject = NULL;
	const char		*body = NULL;
	struct SPropValue	lpProp[3];
	int 			i;
	uint32_t		format;
	bool			ret;

	context = mt->priv;

	/* Create test folder */
	mapi_object_init(&(context->obj_test_folder));
        retval = CreateFolder(&(context->obj_top_folder), FOLDER_GENERIC,
			      MT_DIRNAME_TEST, NULL,
                              OPEN_IF_EXISTS, &(context->obj_test_folder));
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "Create the test folder");
		return false;
	}

	/* Create 5 test messages in the test folder with the same subject */
	for (i = 0; i < 5; ++i) {
		mapi_object_init(&(context->obj_test_msg[i]));
		ret = mapitest_common_message_create(mt, &(context->obj_test_folder),
							&(context->obj_test_msg[i]), MT_MAIL_SUBJECT);
		if (! ret) {
			mapitest_print(mt, "* %-35s\n", "Failed to create test message");
			return false;
		}

		from = talloc_asprintf(mt->mem_ctx, "MT Dummy%i", i);
		set_SPropValue_proptag(&lpProp[0], PR_SENDER_NAME, (const void *)from);
		body = talloc_asprintf(mt->mem_ctx, "Body of message %i", i);
		set_SPropValue_proptag(&lpProp[1], PR_BODY, (const void *)body);
		format = EDITOR_FORMAT_PLAINTEXT;
		set_SPropValue_proptag(&lpProp[2], PR_MSG_EDITOR_FORMAT, (const void *)&format);
		retval = SetProps(&(context->obj_test_msg[i]), 0, lpProp, 3);
		MAPIFreeBuffer((void *)from);
		MAPIFreeBuffer((void *)body);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Set props on message", GetLastError());
			return false;
		}
		retval = SaveChangesMessage(&(context->obj_test_folder), &(context->obj_test_msg[i]), KeepOpenReadWrite);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Save changes to  message", GetLastError());
			return false;
		}
	}

	/* Create 5 test messages in the test folder with the same sender */
	for (i = 5; i < 10; ++i) {
		mapi_object_init(&(context->obj_test_msg[i]));
		subject = talloc_asprintf(mt->mem_ctx, "MT Subject%i", i);
		ret = mapitest_common_message_create(mt, &(context->obj_test_folder),
							&(context->obj_test_msg[i]), subject);
		if (! ret){
			mapitest_print(mt, "* %-35s\n", "Failed to create test message");
			return false;
		}

		from = talloc_asprintf(mt->mem_ctx, "MT Dummy From");
		set_SPropValue_proptag(&lpProp[0], PR_SENDER_NAME, (const void *)from);
		body = talloc_asprintf(mt->mem_ctx, "Body of message %i", i);
		set_SPropValue_proptag(&lpProp[1], PR_BODY, (const void *)body);
		format = EDITOR_FORMAT_PLAINTEXT;
		set_SPropValue_proptag(&lpProp[2], PR_MSG_EDITOR_FORMAT, (const void *)&format);
		retval = SetProps(&(context->obj_test_msg[i]), 0, lpProp, 3);
		MAPIFreeBuffer((void *)from);
		MAPIFreeBuffer((void *)body);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Set props on message", GetLastError());
			return false;
		}
		retval = SaveChangesMessage(&(context->obj_test_folder), &(context->obj_test_msg[i]), KeepOpenReadWrite);
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	return true;
}

/**
   Convenience function to login to the server

   This functions logs into the server, gets the top level store, and
   gets the hierarchy table for the top level store (which is returned as
   obj_htable). It also creates a test folder with 10 test messages.

   \param mt pointer to the top-level mapitest structure
   \param obj_htable the hierarchy table for the top level store
   \param count the number of rows in the top level hierarchy table

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_common_setup(struct mapitest *mt, mapi_object_t *obj_htable, uint32_t *count)
{
	bool			ret = false;
	struct mt_common_tf_ctx	*context;
	enum MAPISTATUS		retval;

	context = talloc(mt->mem_ctx, struct mt_common_tf_ctx);
	mt->priv = context;

	mapi_object_init(&(context->obj_store));
	retval = OpenMsgStore(mt->session, &(context->obj_store));
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "Failed OpenMsgStore");
		return false;
	}

	mapi_object_init(&(context->obj_top_folder));
	ret = mapitest_common_folder_open(mt, &(context->obj_store), &(context->obj_top_folder), 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* We do this before getting the hierarchy table, because otherwise the new
	   test folder will be omitted, and the count will be wrong */
	ret = mapitest_common_create_filled_test_folder(mt);
	if (ret == false) {
		return false;
	}

	mapi_object_init(obj_htable);
	retval = GetHierarchyTable(&(context->obj_top_folder), obj_htable, 0, count);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print_retval(mt, "Failed GetHierarchyTable");
		return false;
	}

	return true;
}

/**
   Convenience function to clean up after logging into the server

   This functions cleans up after a mapitest_common_setup() call

   \param mt pointer to the top-level mapitest structure
*/
_PUBLIC_ void mapitest_common_cleanup(struct mapitest *mt)
{
	struct mt_common_tf_ctx	*context;
	enum MAPISTATUS		retval;
	int			i;

	context = mt->priv;

	for (i = 0; i<10; ++i) {
		mapi_object_release(&(context->obj_test_msg[i]));
	}

	retval = EmptyFolder(&(context->obj_test_folder));
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Empty test folder", GetLastError());
	}

	retval = DeleteFolder(&(context->obj_top_folder), mapi_object_get_id(&(context->obj_test_folder)),
			      DEL_FOLDERS | DEL_MESSAGES | DELETE_HARD_DELETE, NULL);
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Delete test folder", GetLastError());
	}

	mapi_object_release(&(context->obj_test_folder));
	mapi_object_release(&(context->obj_top_folder));
	mapi_object_release(&(context->obj_store));

	talloc_free(mt->priv);
}

