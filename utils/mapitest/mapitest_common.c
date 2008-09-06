/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>

#include <fcntl.h>

/**
 * Opens a default folder
 */
_PUBLIC_ bool mapitest_common_folder_open(struct mapitest *mt,
					  mapi_object_t *obj_parent, 
					  mapi_object_t *obj_child,
					  uint32_t olNum)
{
	enum MAPISTATUS	retval;
	mapi_id_t	id_child;

	retval = GetDefaultFolder(obj_parent, &id_child, olNum);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetDefaultFolder", GetLastError());
		return false;
	}

	retval = OpenFolder(obj_parent, id_child, obj_child);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "OpenFolder", GetLastError());
		return false;
	}

	return true;
}


/**
 * This convenient function searches for an obj_message given the
 * specified subject. Note: this function doesn't use the Restrict
 * call so we can respect the ring policy we defined.
 */
_PUBLIC_ bool mapitest_common_message_find_subject(struct mapitest *mt,
						   mapi_object_t *obj_folder,
						   mapi_object_t *obj_message,
						   uint8_t mode,
						   const char *subject,
						   const char *call,
						   uint32_t *index)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_ctable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		SRowSet;
	uint32_t		count;
	uint32_t		count2 = 0;
	uint32_t		attempt = 0;
	const char		*msubject = NULL;
	uint32_t		i;

	/* Sanity checks */
	if (index == NULL) return false;
	if (call == NULL) return false;
	if (subject == NULL) return false;

	/* Retrieve the contents table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(obj_folder, &obj_ctable, 0, &count);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "GetContentsTable", GetLastError());
		mapi_object_release(&obj_ctable);
		return false;
	}

	/* Customize the content table view */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x3,
					  PR_FID,
					  PR_MID,
					  PR_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetColumns", GetLastError());
		mapi_object_release(&obj_ctable);
		return false;
	}

	/* Position the table at the latest know position */
	retval = SeekRow(&obj_ctable, BOOKMARK_BEGINNING, *index, &count2);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "SeekRow", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapi_object_release(&obj_ctable);
		return false;
	}

	/* Substract current position from the total */
	count -= count2;

	while (((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &SRowSet)) != MAPI_E_NOT_FOUND) &&
	       SRowSet.cRows) {
		for (i = 0; i < SRowSet.cRows; i++) {
		retry:
			errno = 0;
			mapi_object_init(obj_message);
			retval = OpenMessage(obj_folder,
					     SRowSet.aRow[i].lpProps[0].value.d,
					     SRowSet.aRow[i].lpProps[1].value.d,
					     obj_message, mode);
			if (GetLastError() != MAPI_E_SUCCESS && attempt < 3) {
				sleep(1);
				attempt++;
				mapi_object_release(obj_message);
				goto retry;
			}
			attempt = 0;

			if (retval == MAPI_E_SUCCESS) {
				msubject = SRowSet.aRow[i].lpProps[2].value.lpszA;
				if (msubject && !strncmp(subject, msubject, strlen(subject))) {
					*index += i + 1;
					mapi_object_release(&obj_ctable);
					return true;
				}
			}
			mapi_object_release(obj_message);
		}
		*index += SRowSet.cRows;
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
	const char		*tmp;
	char			*newname;
	const uint64_t		*fid;
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

	while ((retval = QueryRows(&obj_htable, count, TBL_ADVANCE, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			tmp = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME);

			newname = windows_to_utf8(mt->mem_ctx, tmp);
			if (newname && fid && !strcmp(newname, name)) {
				retval = OpenFolder(obj_parent, *fid, obj_child);
				mapi_object_release(&obj_htable);
				MAPIFreeBuffer(newname);
				return true;
			}
			MAPIFreeBuffer(newname);
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
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	struct SPropValue	SPropValue;
	struct SPropValue	lpProps[2];
	const char	       	*username[2];
	uint32_t		msgflag;

	/* Sanity checks */
	if (subject == NULL) return false;

	/* Create the mesage */
	retval = CreateMessage(obj_folder, obj_message);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateMessage", GetLastError());
		return false;
	}

	/* Resolve recipients */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	username[0] = (const char *)mt->info.username;
	username[1] = NULL;

	SRowSet = talloc_zero(mt->mem_ctx, struct SRowSet);
	flaglist = talloc_zero(mt->mem_ctx, struct SPropTagArray);

	retval = ResolveNames(username, SPropTagArray, &SRowSet, &flaglist, 0);
	MAPIFreeBuffer(SPropTagArray);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "ResolveNames", GetLastError());
		talloc_free(SRowSet);
		talloc_free(SPropTagArray);
		talloc_free(flaglist);
		return false;
	}

	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mt->mem_ctx, SRowSet, SPropValue);

	/* Set Recipients */
	SetRecipientType(&(SRowSet->aRow[0]), MAPI_TO);
	retval = ModifyRecipients(obj_message, SRowSet);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "ModifyRecipients", GetLastError());
		return false;
	}

	/* Set message properties */
	msgflag = MSGFLAG_SUBMIT;
	set_SPropValue_proptag(&lpProps[0], PR_SUBJECT, (const void *) subject);
	set_SPropValue_proptag(&lpProps[1], PR_MESSAGE_FLAGS, (const void *)&msgflag);
	retval = SetProps(obj_message, lpProps, 2);
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "SetProps", GetLastError());
		return false;
	}

	errno = 0;
	return true;
}


/**
   Generate a random blob of readable data
 */
_PUBLIC_ char *mapitest_common_genblob(TALLOC_CTX *mem_ctx, size_t len)
{
	int		fd;
	int		ret;
	int		i;
	char		*retstr;
	const char	*list = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_-#.,";
	int		list_len = strlen(list);

	/* Sanity check */
	if (!mem_ctx || (len < 0)) {
		return NULL;
	}

	fd = open("/dev/urandom", O_RDONLY, 0);
	if (fd == -1) {
		return NULL;
	}

	retstr = talloc_array(mem_ctx, char, len + 1);
	if ((ret = read(fd, retstr, len)) == -1) {
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

	return retstr;
}

_PUBLIC_ bool mapitest_common_create_filled_test_folder(struct mapitest *mt)
{
	struct mt_common_tf_ctx	*context;
	enum MAPISTATUS		retval;
	const char		*from = NULL;
	const char		*subject = NULL;
	const char		*body = NULL;
	struct SPropValue	lpProp[2];
	int 			i;

	context = mt->priv;

	/* Create test folder */
	mapi_object_init(&(context->obj_test_folder));
        retval = CreateFolder(&(context->obj_top_folder), FOLDER_GENERIC,
			      MT_DIRNAME_TEST, NULL,
                              OPEN_IF_EXISTS, &(context->obj_test_folder));
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Create the test folder", GetLastError());
		return false;
	}

	/* Create 5 test messages in the test folder with the same subject */
	for (i = 0; i < 5; ++i) {
		mapi_object_init(&(context->obj_test_msg[i]));
		retval = mapitest_common_message_create(mt, &(context->obj_test_folder),
							&(context->obj_test_msg[i]), MT_MAIL_SUBJECT);
		if (GetLastError() != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Create test message", GetLastError());
			mapi_object_release(&(context->obj_test_folder));
			return false;
		}

		from = talloc_asprintf(mt->mem_ctx, "[MT] Dummy%i", i);
		set_SPropValue_proptag(&lpProp[0], PR_SENDER_NAME, (const void *)from);
		body = talloc_asprintf(mt->mem_ctx, "Body of message %i", i);
		set_SPropValue_proptag(&lpProp[1], PR_BODY, (const void *)body);
		retval = SetProps(&(context->obj_test_msg[i]), lpProp, 2);
		MAPIFreeBuffer((void *)from);
		MAPIFreeBuffer((void *)body);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Set props on message", GetLastError());
			mapi_object_release(&(context->obj_test_folder));
			return false;
		}
		retval = SaveChangesMessage(&(context->obj_test_folder), &(context->obj_test_msg[i]));
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Save changes to  message", GetLastError());
			mapi_object_release(&(context->obj_test_folder));
			return false;
		}
	}

	/* Create 5 test messages in the test folder with the same sender */
	for (i = 5; i < 10; ++i) {
		mapi_object_init(&(context->obj_test_msg[i]));
		subject = talloc_asprintf(mt->mem_ctx, "[MT] Subject%i", i);
		retval = mapitest_common_message_create(mt, &(context->obj_test_folder),
							&(context->obj_test_msg[i]), subject);
		if (GetLastError() != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Create test message", GetLastError());
			return false;
		}

		from = talloc_asprintf(mt->mem_ctx, "[MT] Dummy From");
		set_SPropValue_proptag(&lpProp[0], PR_SENDER_NAME, (const void *)from);
		body = talloc_asprintf(mt->mem_ctx, "Body of message %i", i);
		set_SPropValue_proptag(&lpProp[1], PR_BODY, (const void *)body);
		retval = SetProps(&(context->obj_test_msg[i]), lpProp, 2);
		MAPIFreeBuffer((void *)from);
		MAPIFreeBuffer((void *)body);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_print(mt, "* %-35s: 0x%.8x\n", "Set props on message", GetLastError());
			return false;
		}
		retval = SaveChangesMessage(&(context->obj_test_folder), &(context->obj_test_msg[i]));
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
	}

	return true;
};

/**
   Convenience function to login to the server

   This functions logs into the server, gets the top level store, and
   gets the hierachy table for the top level store (which is returned as
   obj_htable). It also creates a test folder with 10 test messages.

   \param mt pointer to the top-level mapitest structure
   \param obj_htable the hierachy table for the top level store
   \param count the number of rows in the top level hierarchy table

   \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_common_setup(struct mapitest *mt, mapi_object_t *obj_htable, uint32_t *count)
{
	bool			ret = false;
	struct mt_common_tf_ctx	*context;

	context = talloc(mt->mem_ctx, struct mt_common_tf_ctx);
	mt->priv = context;

	mapi_object_init(&(context->obj_store));
	OpenMsgStore(&(context->obj_store));
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&(context->obj_top_folder));
	ret = mapitest_common_folder_open(mt, &(context->obj_store), &(context->obj_top_folder), 
					  olFolderTopInformationStore);
	if (ret == false) {
		return false;
	}

	/* We do this before getting the hierachy table, because otherwise the new
	   test folder will be omitted, and the count will be wrong */
	ret = mapitest_common_create_filled_test_folder(mt);
	if (ret == false) {
		return false;
	}

	mapi_object_init(obj_htable);
	GetHierarchyTable(&(context->obj_top_folder), obj_htable, 0, count);
	if (GetLastError() != MAPI_E_SUCCESS) {
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
	int			i;

	context = mt->priv;

	for (i = 0; i<10; ++i) {
		mapi_object_release(&(context->obj_test_msg[i]));
	}

	EmptyFolder(&(context->obj_test_folder));
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Empty test folder", GetLastError());
	}

	DeleteFolder(&(context->obj_top_folder), mapi_object_get_id(&(context->obj_test_folder)));
	if (GetLastError() != MAPI_E_SUCCESS) {
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "Delete test folder", GetLastError());
	}

	mapi_object_release(&(context->obj_test_folder));
	mapi_object_release(&(context->obj_top_folder));
	mapi_object_release(&(context->obj_store));

	talloc_free(mt->priv);
}

