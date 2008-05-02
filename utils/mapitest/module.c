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
#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

_PUBLIC_ uint32_t mapitest_register_modules(struct mapitest *mt)
{
	uint32_t	ret;

	ret = module_oxcstor_init(mt);
	ret = module_oxcfold_init(mt);
	ret = module_oxctable_init(mt);
	ret = module_oxomsg_init(mt);
	ret = module_oxcmsg_init(mt);
	ret = module_oxcprpt_init(mt);
	ret = module_oxorule_init(mt);
	ret = module_noserver_init(mt);

	return ret;
}


/**
   \details Register the Store Object Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcstor_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCSTOR", "Store Object Protocol", true);

	mapitest_suite_add_test(suite, "LOGON", "Logon operation", mapitest_oxcstor_Logon);
	mapitest_suite_add_test(suite, "GET-RECEIVE-FOLDER", "Retrieve the receive folder ID", mapitest_oxcstor_GetReceiveFolder);
	mapitest_suite_add_test(suite, "SET-RECEIVE-FOLDER", "Set the receive folder", mapitest_oxcstor_SetReceiveFolder);
	mapitest_suite_add_test(suite, "GET-RECEIVE-FOLDER-TABLE", "Retrieve the Receive Folder Table", mapitest_oxcstor_GetReceiveFolderTable);

	mapitest_suite_register(mt, suite);
	
	return MAPITEST_SUCCESS;
}


/**
   \details Register the Folder Object Protocol test suite

   \param mt the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcfold_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCFOLD", "Folder Object Protocol", true);

	mapitest_suite_add_test(suite, "OPEN",   "Open a folder", mapitest_oxcfold_OpenFolder);
	mapitest_suite_add_test(suite, "CREATE", "Create a folder", mapitest_oxcfold_CreateFolder);
	mapitest_suite_add_test(suite, "GET-HIERARCHY-TABLE", "Retrieve the hierarchy table", mapitest_oxcfold_GetHierarchyTable);
	mapitest_suite_add_test(suite, "GET-CONTENTS-TABLE", "Retrieve the contents table", mapitest_oxcfold_GetContentsTable);
	mapitest_suite_add_test(suite, "MOVECOPY-MESSAGES", "Move or copy messages from a source to destination folder", mapitest_oxcfold_MoveCopyMessages);
	mapitest_suite_add_test(suite, "MOVEFOLDER", "Move folder from source to destination", mapitest_oxcfold_MoveFolder);
	mapitest_suite_add_test(suite, "COPYFOLDER", "Copy folder from source to destination", mapitest_oxcfold_CopyFolder);

	mapitest_suite_register(mt, suite);
	
	return MAPITEST_SUCCESS;
}


/**
   \details Register the E-mail Object Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxomsg_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXOMSG", "E-mail Object Protocol", true);
	
	mapitest_suite_add_test(suite, "ADDRESS-TYPES", "Address Types", mapitest_oxomsg_AddressTypes);
	mapitest_suite_add_test(suite, "SUBMIT-MESSAGE", "Submit message", mapitest_oxomsg_SubmitMessage);
	mapitest_suite_add_test(suite, "ABORT-SUBMIT", "Abort submitted message", mapitest_oxomsg_AbortSubmit);
	mapitest_suite_add_test(suite, "SET-SPOOLER", "Client intends to act as a mail spooler", mapitest_oxomsg_SetSpooler);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Register the Message and Attachment Object Protocol test suite

   \param mt pointer on the top-lvel mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcmsg_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite =  mapitest_suite_init(mt, "OXCMSG", "Message and Attachment Object Protocol", true);

	mapitest_suite_add_test(suite, "CREATE-MESSAGE", "Create message", mapitest_oxcmsg_CreateMessage);
	mapitest_suite_add_test(suite, "SET-READ-FLAGS", "Set message read flag", mapitest_oxcmsg_SetReadFlags);
	mapitest_suite_add_test(suite, "MODIFY-RECIPIENTS", "Add new recipients", mapitest_oxcmsg_ModifyRecipients);
	mapitest_suite_add_test(suite, "READ-RECIPIENTS", "Read recipients from a message", mapitest_oxcmsg_ReadRecipients);
	mapitest_suite_add_test(suite, "REMOVE-ALL-RECIPIENTS", "Remove all recipients from a message", mapitest_oxcmsg_RemoveAllRecipients);
	mapitest_suite_add_test(suite, "SAVE-CHANGES-MESSAGE", "Save changes on message", mapitest_oxcmsg_SaveChangesMessage);
	mapitest_suite_add_test(suite, "GET-MESSAGE-STATUS", "Get message status", mapitest_oxcmsg_GetMessageStatus);
	mapitest_suite_add_test(suite, "SET-MESSAGE-STATUS", "Set message status", mapitest_oxcmsg_SetMessageStatus);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Register the Table Object Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxctable_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCTABLE", "Table Object Protocol", true);
	
	mapitest_suite_add_test(suite, "SETCOLUMNS", "Set Table Columns", mapitest_oxctable_SetColumns);
	mapitest_suite_add_test(suite, "QUERYCOLUMNS", "Query Table Columns", mapitest_oxctable_QueryColumns);
	mapitest_suite_add_test(suite, "QUERYROWS", "Query Table Rows", mapitest_oxctable_QueryRows);
	mapitest_suite_add_test(suite, "GETSTATUS", "Get Table Status", mapitest_oxctable_GetStatus);
	mapitest_suite_add_test(suite, "SEEKROW", "Seek a row", mapitest_oxctable_SeekRow);
	mapitest_suite_add_test(suite, "SEEKROW-APPROX", "Seek an approximative row", mapitest_oxctable_SeekRowApprox);
	mapitest_suite_add_test(suite, "CREATE-BOOKMARK", "Create a table bookmark", mapitest_oxctable_CreateBookmark);
	mapitest_suite_add_test(suite, "SEEKROW-BOOKMARK", "Seek a row given a bookmark", mapitest_oxctable_SeekRowBookmark);

	mapitest_suite_register(mt, suite);
	
	return MAPITEST_SUCCESS;
}


/**
   \details Register the Property and Stream Object Protocol test
   suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcprpt_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCPRPT", "Property and Stream Object Protocol", true);

	mapitest_suite_add_test(suite, "GET-PROPS", "Retrieve a specific set of properties", mapitest_oxcprpt_GetProps);
	mapitest_suite_add_test(suite, "GET-PROPSALL", "Retrieve the whole property array", mapitest_oxcprpt_GetPropsAll);
	mapitest_suite_add_test(suite, "GET-PROPLIST", "Retrieve the property list", mapitest_oxcprpt_GetPropList);
	mapitest_suite_add_test(suite, "SET-PROPS", "Set a specific set of properties", mapitest_oxcprpt_SetProps);
	mapitest_suite_add_test(suite, "STREAM", "Test stream operations", mapitest_oxcprpt_Stream);
	mapitest_suite_add_test(suite, "COPYTO-STREAM", "Copy stream from source to desination stream", mapitest_oxcprpt_CopyToStream);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Register the E-Mail Rules Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxorule_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXORULE", "E-Mail Rules Protocol", true);

	mapitest_suite_add_test(suite, "GET-RULES-TABLE", "Retrieve the rules table associated to a folder", mapitest_oxorule_GetRulesTable);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   
 */
_PUBLIC_ uint32_t module_noserver_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "NOSERVER", "No server operations", false);

	mapitest_suite_add_test(suite, "LZFU", "Test LZFU operations", mapitest_noserver_lzfu);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}
