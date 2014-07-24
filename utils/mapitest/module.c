/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008-2014

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

_PUBLIC_ uint32_t mapitest_register_modules(struct mapitest *mt)
{
	uint32_t	ret;

	ret = module_oxcstor_init(mt);
	ret += module_oxcfold_init(mt);
	ret += module_oxctable_init(mt);
	ret += module_oxomsg_init(mt);
	ret += module_oxcmsg_init(mt);
	ret += module_oxcprpt_init(mt);
	ret += module_oxorule_init(mt);
	ret += module_oxcnotif_init(mt);
	ret += module_oxcfxics_init(mt);
	ret += module_oxcperm_init(mt);
	ret += module_nspi_init(mt);
	ret += module_noserver_init(mt);
	ret += module_errorchecks_init(mt);
	ret += module_lcid_init(mt);
	ret += module_mapidump_init(mt);
	ret += module_lzxpress_init(mt);
	ret += module_zentyal_init(mt);

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
	mapitest_suite_add_test(suite, "PUBLICFOLDER-ISGHOSTED", "Determine if a public folder is ghosted", mapitest_oxcstor_PublicFolderIsGhosted);
	mapitest_suite_add_test(suite, "GETOWNINGSERVERS", "Get the list of servers that host replicas of a given public folder", mapitest_oxcstor_GetOwningServers);
	mapitest_suite_add_test(suite, "LONGTERMID", "Map to / from a Long Term ID", mapitest_oxcstor_LongTermId);
	mapitest_suite_add_test_flagged(suite, "GETSTORESTATE", "Retrieve the store state", mapitest_oxcstor_GetStoreState, NotInExchange2010);
	mapitest_suite_add_test(suite, "ISMAILBOXFOLDER", "Get the standard folder for a given folder ID", mapitest_oxcstor_IsMailboxFolder);

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
	mapitest_suite_add_test(suite, "CREATE-DELETE", "Do a basic create / delete cycle on a folder", mapitest_oxcfold_CreateDeleteFolder);
	mapitest_suite_add_test(suite, "CREATE", "Create a folder", mapitest_oxcfold_CreateFolder);
	mapitest_suite_add_test(suite, "CREATE-VARIANTS", "More folder creation variations", mapitest_oxcfold_CreateFolderVariants);
	mapitest_suite_add_test(suite, "GET-HIERARCHY-TABLE", "Retrieve the hierarchy table", mapitest_oxcfold_GetHierarchyTable);
	mapitest_suite_add_test(suite, "GET-CONTENTS-TABLE", "Retrieve the contents table", mapitest_oxcfold_GetContentsTable);
	mapitest_suite_add_test(suite, "SET-SEARCHCRITERIA", "Set a search criteria on a container", mapitest_oxcfold_SetSearchCriteria);
	mapitest_suite_add_test(suite, "GET-SEARCHCRITERIA", "Retrieve a search criteria associated to a container", mapitest_oxcfold_GetSearchCriteria);
	mapitest_suite_add_test(suite, "MOVECOPY-MESSAGES", "Move or copy messages from a source to destination folder", mapitest_oxcfold_MoveCopyMessages);
	mapitest_suite_add_test(suite, "MOVEFOLDER", "Move folder from source to destination", mapitest_oxcfold_MoveFolder);
	mapitest_suite_add_test(suite, "COPYFOLDER", "Copy folder from source to destination", mapitest_oxcfold_CopyFolder);
	mapitest_suite_add_test(suite, "HARDDELETEMESSAGES", "Hard delete messages", mapitest_oxcfold_HardDeleteMessages);
	mapitest_suite_add_test(suite, "HARDDELETEMESSAGESANDSUBFOLDERS", "Hard delete messages and subfolders", mapitest_oxcfold_HardDeleteMessagesAndSubfolders);
	mapitest_suite_add_test(suite, "DELETEMESSAGES", "Soft delete messages", mapitest_oxcfold_DeleteMessages);

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
	mapitest_suite_add_test(suite, "SPOOLER-LOCK-MESSAGE", "Lock the specified message for spooling", mapitest_oxomsg_SpoolerLockMessage);
	mapitest_suite_add_test(suite, "TRANSPORT-SEND", "Sends the specified message object out for message delivery", mapitest_oxomsg_TransportSend);
	mapitest_suite_add_test(suite, "TRANSPORT-NEW-MAIL", "Submit a new message for processing", mapitest_oxomsg_TransportNewMail);
	mapitest_suite_add_test(suite, "GET-TRANSPORT-FOLDER", "Retrieve the temporary transport folder ID", mapitest_oxomsg_GetTransportFolder);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Register the Message and Attachment Object Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcmsg_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite =  mapitest_suite_init(mt, "OXCMSG", "Message and Attachment Object Protocol", true);

	mapitest_suite_add_test(suite, "CREATE-MESSAGE", "Create message", mapitest_oxcmsg_CreateMessage);
	mapitest_suite_add_test(suite, "SET-MESSAGE-READ-FLAGS", "Set message read flag", mapitest_oxcmsg_SetMessageReadFlag);
	mapitest_suite_add_test(suite, "SET-READ-FLAGS", "Set read flag on multiple messages", mapitest_oxcmsg_SetReadFlags);
	mapitest_suite_add_test(suite, "MODIFY-RECIPIENTS", "Add new recipients", mapitest_oxcmsg_ModifyRecipients);
	mapitest_suite_add_test(suite, "READ-RECIPIENTS", "Read recipients from a message", mapitest_oxcmsg_ReadRecipients);
	mapitest_suite_add_test(suite, "REMOVE-ALL-RECIPIENTS", "Remove all recipients from a message", mapitest_oxcmsg_RemoveAllRecipients);
	mapitest_suite_add_test(suite, "SAVE-CHANGES-MESSAGE", "Save changes on message", mapitest_oxcmsg_SaveChangesMessage);
	mapitest_suite_add_test(suite, "GET-MESSAGE-STATUS", "Get message status", mapitest_oxcmsg_GetMessageStatus);
	mapitest_suite_add_test(suite, "SET-MESSAGE-STATUS", "Set message status", mapitest_oxcmsg_SetMessageStatus);
	mapitest_suite_add_test(suite, "OPEN-EMBEDDED-MESSAGE", "Open a message embedded in another message", mapitest_oxcmsg_OpenEmbeddedMessage);
	mapitest_suite_add_test_flagged(suite, "GET-VALID-ATTACHMENTS", "Get valid attachment IDs for a message", mapitest_oxcmsg_GetValidAttachments, NotInExchange2010);
	mapitest_suite_add_test(suite, "RELOAD-CACHED-INFORMATION", "Reload cached information for a message", mapitest_oxcmsg_ReloadCachedInformation);

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
	mapitest_suite_add_test_flagged(suite, "GETSTATUS", "Get Table Status", mapitest_oxctable_GetStatus, NotInExchange2010);
	mapitest_suite_add_test(suite, "SEEKROW", "Seek a row", mapitest_oxctable_SeekRow);
	mapitest_suite_add_test(suite, "RESTRICT", "Apply filters to a table", mapitest_oxctable_Restrict);
	mapitest_suite_add_test_flagged(suite, "SEEKROW-APPROX", "Seek an approximate row", mapitest_oxctable_SeekRowApprox, NotInExchange2010);
	mapitest_suite_add_test(suite, "CREATE-BOOKMARK", "Create a table bookmark", mapitest_oxctable_CreateBookmark);
	mapitest_suite_add_test(suite, "SEEKROW-BOOKMARK", "Seek a row given a bookmark", mapitest_oxctable_SeekRowBookmark);
	mapitest_suite_add_test(suite, "CATEGORY", "Expand/collapse category rows", mapitest_oxctable_Category);

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
	mapitest_suite_add_test(suite, "DELETE-PROPS", "Delete a specific set of properties", mapitest_oxcprpt_DeleteProps);
	mapitest_suite_add_test(suite, "PROPS-NOREPLICATE", "Set / delete a specific set of properties (no replicate)", mapitest_oxcprpt_NoReplicate);
	mapitest_suite_add_test(suite, "COPY-PROPS", "Copy a specified set of properties", mapitest_oxcprpt_CopyProps);
	mapitest_suite_add_test(suite, "STREAM", "Test stream operations", mapitest_oxcprpt_Stream);
	mapitest_suite_add_test(suite, "COPYTO", "Copy or move properties", mapitest_oxcprpt_CopyTo);
	mapitest_suite_add_test_flagged(suite, "WRITE-COMMIT-STREAM", "Test atomic Write / Commit operation", mapitest_oxcprpt_WriteAndCommitStream, NotInExchange2010);
	mapitest_suite_add_test_flagged(suite, "COPYTO-STREAM", "Copy stream from source to destination stream", mapitest_oxcprpt_CopyToStream, NotInExchange2010SP0);
	mapitest_suite_add_test(suite, "NAME-ID", "Convert between Names and IDs", mapitest_oxcprpt_NameId);
	mapitest_suite_add_test(suite, "PSMAPI-NAME-ID", "Convert between Names and IDs for PS_MAPI namespace", mapitest_oxcprpt_NameId_PSMAPI);

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
   \details Register the Core Notification Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcnotif_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCNOTIF", "Core Notification Protocol", true);

	// mapitest_suite_add_test(suite, "REGISTER-NOTIFICATION", "Subscribe to notifications", mapitest_oxcnotif_RegisterNotification);
	mapitest_suite_add_test_flagged(suite, "SYNC-OPEN-ADVISOR", "Test the SyncOpenAdvisor ROP", mapitest_oxcnotif_SyncOpenAdvisor, NotInExchange2010);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Register the Bulk Data Transfer Protocol test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcfxics_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCFXICS", "Bulk Data Transfer Protocol", true);
	
	mapitest_suite_add_test(suite, "GET-LOCAL-REPLICA-IDS", "Reserve a range of IDs for local replica", mapitest_oxcfxics_GetLocalReplicaIds);
	mapitest_suite_add_test(suite, "COPYFOLDER", "Test CopyFolder operation", mapitest_oxcfxics_CopyFolder);
	mapitest_suite_add_test(suite, "COPYMESSAGES", "Test CopyMessages operation", mapitest_oxcfxics_CopyMessages);
	mapitest_suite_add_test(suite, "COPYTO", "Test CopyTo operation", mapitest_oxcfxics_CopyTo);
	mapitest_suite_add_test(suite, "COPYPROPS", "Test CopyProperties operation", mapitest_oxcfxics_CopyProperties);
	mapitest_suite_add_test(suite, "DEST-CONFIGURE", "Test Destination Configure operation", mapitest_oxcfxics_DestConfigure);
	mapitest_suite_add_test(suite, "SYNC-CONFIGURE", "Configure ICS context for download", mapitest_oxcfxics_SyncConfigure);
	mapitest_suite_add_test(suite, "SET-LOCAL-REPLICA-MIDSET-DELETED", "Reserve a range of local replica IDs", mapitest_oxcfxics_SetLocalReplicaMidsetDeleted);
	mapitest_suite_add_test(suite, "SYNC-OPEN-COLLECTOR", "Test opening ICS upload collector", mapitest_oxcfxics_SyncOpenCollector);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Register the Permissions Protocol test suite

   \param mt pointer to the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_oxcperm_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "OXCPERM", "Permissions Protocol", true);
	
	mapitest_suite_add_test(suite, "GET-PERMISSIONS-TABLE", "Get handle to the Permissions table", mapitest_oxcperm_GetPermissionsTable);
	mapitest_suite_add_test(suite, "MODIFY-PERMISSIONS", "Modify access permissions on a folder", mapitest_oxcperm_ModifyPermissions);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}

/**
   \details Register the NSPI test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_nspi_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "NSPI", "Name Service Provider Interface", true);
	
	mapitest_suite_add_test(suite, "UPDATESTAT", "Update the STAT structure", mapitest_nspi_UpdateStat);
	mapitest_suite_add_test(suite, "QUERYROWS", "Returns a number of rows from a specified table", mapitest_nspi_QueryRows);
	mapitest_suite_add_test(suite, "SEEKENTRIES", "Searches for and sets the logical position in a specific table", mapitest_nspi_SeekEntries);
	mapitest_suite_add_test(suite, "GETMATCHES", "Returns an explicit table", mapitest_nspi_GetMatches);
	mapitest_suite_add_test(suite, "RESORTRESTRICTION", "Apply a sort order to the objects in a restricted address book container", mapitest_nspi_ResortRestriction);
	mapitest_suite_add_test(suite, "DNTOMID", "Maps a set of DN to a set of MId", mapitest_nspi_DNToMId);
	mapitest_suite_add_test(suite, "GETPROPLIST", "Retrieve the list of properties associated to an object", mapitest_nspi_GetPropList);
	mapitest_suite_add_test(suite, "GETPROPS", "Returns a row containing a set of the properties and values", mapitest_nspi_GetProps);
	mapitest_suite_add_test(suite, "COMPAREMIDS", "Compare the position in an AB container of two objects", mapitest_nspi_CompareMIds);
	mapitest_suite_add_test(suite, "MODPROPS", "Modify an address book object", mapitest_nspi_ModProps);
	mapitest_suite_add_test(suite, "GETSPECIALTABLE", "Returns the rows of a special table to the client", mapitest_nspi_GetSpecialTable);
	mapitest_suite_add_test(suite, "GETTEMPLATEINFO", "Returns information about template objects", mapitest_nspi_GetTemplateInfo);
	mapitest_suite_add_test(suite, "MODLINKATT", "Modifies the values of a specific property of a specific row", mapitest_nspi_ModLinkAtt);
	mapitest_suite_add_test(suite, "QUERYCOLUMNS", "Returns a list of all the properties the NSPI server is aware of", mapitest_nspi_QueryColumns);
	mapitest_suite_add_test(suite, "GETNAMESFROMIDS", "Returns a list of property names for a set of proptags", mapitest_nspi_GetNamesFromIDs);
	mapitest_suite_add_test(suite, "GETIDSFROMNAMES", "Returns the property IDs associated with property names", mapitest_nspi_GetIDsFromNames);
	mapitest_suite_add_test(suite, "RESOLVENAMES", "Resolve usernames", mapitest_nspi_ResolveNames);
	mapitest_suite_add_test(suite, "GETGALTABLE", "Fetches the Global Address List", mapitest_nspi_GetGALTable);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}

/**
   \details Register the Functional Testing NSPI test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_zentyal_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "ZENTYAL", "Functional Testing for Zentyal", true);

	mapitest_suite_add_test(suite, "NSPI-1876", "Test pStat overflow in QueryRows", mapitest_zentyal_1876);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}

/**
   \details Return the no server test suite

   \param mt pointer on the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_noserver_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "NOSERVER", "No server operations", false);

	mapitest_suite_add_test(suite, "LZFU-DECOMPRESS", "Test Compressed RTF decompression operations", mapitest_noserver_lzfu);
	mapitest_suite_add_test(suite, "LZFU-COMPRESS", "Test Compressed RTF compression operations", mapitest_noserver_rtfcp);
	mapitest_suite_add_test(suite, "LZFU-COMPRESS-LARGE", "Test RTF (de)compression operations on larger file", mapitest_noserver_rtfcp_large);
	mapitest_suite_add_test(suite, "SROWSET", "Test SRowSet parsing", mapitest_noserver_srowset);
	mapitest_suite_add_test(suite, "GETSETPROPS", "Test Property handling", mapitest_noserver_properties);
	mapitest_suite_add_test(suite, "MAPIPROPS", "Test MAPI Property handling", mapitest_noserver_mapi_properties);
	mapitest_suite_add_test(suite, "PROPTAGVALUE", "Test MAPI PropTag value handling", mapitest_noserver_proptagvalue);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}

/**
   \details Initialise the error / sanity-check test suite

   \param mt pointer to the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_errorchecks_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "ERRORCHECKS", "Error / sanity-check operations", false);

	mapitest_suite_add_test(suite, "SIMPLEMAPI", "Test failure paths for simplemapi.c", mapitest_errorchecks_simplemapi_c);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}

/**
   \details Initialise the language code / ID test suite

   \param mt pointer to the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_lcid_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "LCID", "Language code / ID operations", false);

	mapitest_suite_add_test(suite, "CODE2TAG", "Tests for lcid_langcode2langtag", mapitest_lcid_langcode2langtag);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}

/**
   \details Initialise the mapidump test suite

   \param mt pointer to the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_mapidump_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "MAPIDUMP", "mapidump test suite", false);

	mapitest_suite_add_test(suite, "SPROPVALUE", "Test dump of SPropValue", mapitest_mapidump_spropvalue);
	mapitest_suite_add_test(suite, "SPROPTAGARRAY", "Test dump of SPropTagArray", mapitest_mapidump_sproptagarray);
	mapitest_suite_add_test(suite, "SROWSET", "Test dump of SRowSet", mapitest_mapidump_srowset);
	mapitest_suite_add_test(suite, "PABENTRY", "Test dump of PAB Entry", mapitest_mapidump_pabentry);
	mapitest_suite_add_test(suite, "NOTE", "Test dump of a note message", mapitest_mapidump_note);
	mapitest_suite_add_test(suite, "TASK", "Test dump of a task message", mapitest_mapidump_task);
	mapitest_suite_add_test(suite, "CONTACT", "Test dump of a contact message", mapitest_mapidump_contact);
	mapitest_suite_add_test(suite, "APPOINTMENT", "Test dump of an appointment message", mapitest_mapidump_appointment);
	mapitest_suite_add_test(suite, "MESSAGE", "Test dump of an email message", mapitest_mapidump_message);
	mapitest_suite_add_test(suite, "NEWMAIL", "Test dump of a new mail notification", mapitest_mapidump_newmail);
	mapitest_suite_add_test(suite, "FREEBUSY", "Test dump of a free/busy event", mapitest_mapidump_freebusy);
	mapitest_suite_add_test(suite, "RECIPIENTS", "Test dump of a free/busy event", mapitest_mapidump_recipients);
	mapitest_suite_add_test(suite, "FOLDERCREATEDNOTIF", "Test dump of a folder created notification", mapitest_mapidump_foldercreated);
	mapitest_suite_add_test(suite, "FOLDERDELETEDNOTIF", "Test dump of a folder deleted notification", mapitest_mapidump_folderdeleted);
	mapitest_suite_add_test(suite, "FOLDERMOVEDNOTIF", "Test dump of a folder move notification", mapitest_mapidump_foldermoved);
	mapitest_suite_add_test(suite, "FOLDERCOPYNOTIF", "Test dump of a folder copy notification", mapitest_mapidump_foldercopied);
	mapitest_suite_add_test(suite, "MESSAGECREATEDNOTIF", "Test dump of a message created notification", mapitest_mapidump_messagecreated);
	mapitest_suite_add_test(suite, "MESSAGEDELETEDNOTIF", "Test dump of a message deleted notification", mapitest_mapidump_messagedeleted);
	mapitest_suite_add_test(suite, "MESSAGEMOVEDNOTIF", "Test dump of a message move notification", mapitest_mapidump_messagemoved);
	mapitest_suite_add_test(suite, "MESSAGECOPYNOTIF", "Test dump of a message copy notification", mapitest_mapidump_messagecopied);
	mapitest_suite_add_test(suite, "MESSAGEMODIFIEDNOTIF", "Test dump of a message modification notification", mapitest_mapidump_messagemodified);
	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}


/**
   \details Initialise the language code / ID test suite

   \param mt pointer to the top-level mapitest structure

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t module_lzxpress_init(struct mapitest *mt)
{
	struct mapitest_suite	*suite = NULL;

	suite = mapitest_suite_init(mt, "LZXPRESS", "lzxpress algorithm test suite", false);

	mapitest_suite_add_test_flagged(suite, "VALIDATE-001", "Validate LZXPRESS implementation using sample file 001", mapitest_lzxpress_validate_test_001, ExpectedFail);

	mapitest_suite_register(mt, suite);

	return MAPITEST_SUCCESS;
}
