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
   \file openchangedb.c

   \brief OpenChange Dispatcher database routines
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "libmapi/defs_private.h"

/**
   \details Retrieve the mailbox FolderID for given recipient from
   openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_SystemFolderID(void *ldb_ctx,
							 char *recipient, uint32_t SystemIdx,
							 uint64_t *FolderId)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	const char			*dn;
	struct ldb_dn			*ldb_dn = NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
	
	mem_ctx = talloc_named(NULL, 0, "get_SystemFolderID");

	/* Step 1. Search Mailbox Root DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. If Mailbox root folder, check for FolderID within current record */
	if (SystemIdx == 0x1) {
		*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
		OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

		talloc_free(mem_ctx);
		return MAPI_E_SUCCESS;
	}

	dn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 3. Search FolderID */
	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn, MAPI_E_CORRUPT_STORE, mem_ctx);
	talloc_free(res);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs, 
			 "(&(objectClass=systemfolder)(SystemIdx=%d))", SystemIdx);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
	OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the public folder FolderID (fid) for a given folder type

   \param ldb_ctx pointer to the OpenChange LDB context
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_PublicFolderID(void *ldb_ctx,
							 uint32_t SystemIdx,
							 uint64_t *FolderId)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_PublicFolderID");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(objectClass=publicfolder)(SystemIdx=%d))", SystemIdx);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);
	OPENCHANGE_RETVAL_IF(res->count != 1, MAPI_E_NOT_FOUND, mem_ctx);

	*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
	OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the distinguishedName associated to a mailbox
   system folder.

   \param parent_ctx pointer to the parent memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param distinguishedName pointer on pointer to the
   distinguishedName string the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_distinguishedName(TALLOC_CTX *parent_ctx, 
							    void *ldb_ctx, 
							    uint64_t fid, 
							    char **distinguishedName)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_distinguishedName");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=0x%.16"PRIx64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*distinguishedName = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mailbox GUID for given recipient from
   openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param MailboxGUID pointer to the mailbox GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxGuid(void *ldb_ctx,
						      char *recipient,
						      struct GUID *MailboxGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MailboxGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_MailboxGuid");

	/* Step 1. Search Mailbox DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	
	/* Step 2. Retrieve MailboxGUID attribute's value */
	guid = ldb_msg_find_attr_as_string(res->msgs[0], "MailboxGUID", NULL);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);

	GUID_from_string(guid, MailboxGUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mailbox replica identifier and GUID for given
   recipient from openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param ReplID pointer to the replica identifier the function returns
   \param ReplGUID pointer to the replica GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxReplica(void *ldb_ctx,
							 char *recipient, uint16_t *ReplID,
							 struct GUID *ReplGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_MailboxReplica");

	/* Step 1. Search Mailbox DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Retrieve ReplicaID attribute's value */
	*ReplID = ldb_msg_find_attr_as_int(res->msgs[0], "ReplicaID", 0);

	/* Step 3/ Retrieve ReplicaGUID attribute's value */
	guid = ldb_msg_find_attr_as_string(res->msgs[0], "ReplicaGUID", 0);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);

	GUID_from_string(guid, ReplGUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the public folder replica identifier and GUID
   from the openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param ReplID pointer to the replica identifier the function returns
   \param ReplGUID pointer to the replica GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_PublicFolderReplica(void *ldb_ctx,
							      uint16_t *ReplID,
							      struct GUID *GUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!ReplID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!GUID, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_PublicFolderReplica");

	/* Step 1. Search for public folder container */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=publicfolders");

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Retrieve ReplicaID attribute's value */
	*ReplID = ldb_msg_find_attr_as_int(res->msgs[0], "ReplicaID", 0);

	/* Step 3. Retrieve ReplicaGUID attribute's value */
	guid = ldb_msg_find_attr_as_string(res->msgs[0], "StoreGUID", 0);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);

	GUID_from_string(guid, GUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mapistore URI associated to a mailbox system
   folder.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *parent_ctx,
						       void *ldb_ctx,
						       uint64_t fid,
						       char **mapistoreURL,
						       bool mailboxstore)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	if (mailboxstore == true) {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=0x%.16"PRIx64")", fid);
	} else {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=0x%.16"PRIx64")", fid);
	}

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mapistoreURL = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "mapistore_uri", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the Explicit message class and Folder identifier
   associated to the MessageClass search pattern.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param recipient pointer to the mailbox's username
   \param MessageClass substring to search for
   \param fid pointer to the folder identifier the function returns
   \param ExplicitMessageClass pointer on pointer to the complete
   message class the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_ReceiveFolder(TALLOC_CTX *parent_ctx,
							void *ldb_ctx,
							const char *recipient,
							const char *MessageClass,
							uint64_t *fid,
							const char **ExplicitMessageClass)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	struct ldb_dn			*dn;
	struct ldb_message_element	*ldb_element;
	char				*dnstr;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	unsigned int			i, j;
	size_t				length;

	mem_ctx = talloc_named(NULL, 0, "get_ReceiveFolder");

	DEBUG(5, ("openchangedb_get_ReceiveFolder, recipient: %s\n", recipient));
	DEBUG(5, ("openchangedb_get_ReceiveFolder, MessageClass: %s\n", MessageClass));

	/* Step 1. Find mailbox DN for the recipient */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dnstr = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	DEBUG(5, ("openchangedb_get_ReceiveFolder, dnstr: %s\n", dnstr));

	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(res);

	dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	/* Step 2A. As a special case, find the "All" target */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs,
			 "(PidTagMessageClass=All)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || (res->count != 1), MAPI_E_NOT_FOUND, mem_ctx);
	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0x0);
	*ExplicitMessageClass = "";
	DEBUG(5, ("openchangedb_get_ReceiveFolder (All target), class: %s, fid: 0x%016"PRIx64"\n",
		  *ExplicitMessageClass, *fid));
	if (strcmp(MessageClass, "All") == 0) {
		/* we're done here */
		talloc_free(mem_ctx);
		return MAPI_E_SUCCESS;
	}

	/* Step 2B. Search for all MessageClasses within user's mailbox */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, 
			 "(PidTagMessageClass=*)");
	DEBUG(5, ("openchangedb_get_ReceiveFolder, res->count: %i\n", res->count));

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 3. Find the message class that has the longest matching string entry */
	for (j = 0; j < res->count; ++j) {
		ldb_element = ldb_msg_find_element(res->msgs[j], "PidTagMessageClass");
		DEBUG(6, ("openchangedb_get_ReceiveFolder, checking fid: 0x%016"PRIx64"\n",
			  ldb_msg_find_attr_as_uint64(res->msgs[j], "PidTagFolderId", 0x0)));
		for (i = 0, length = 0; i < ldb_element[j].num_values; i++) {
			DEBUG(6, ("openchangedb_get_ReceiveFolder, element %i, data: %s\n", i, (char *)ldb_element->values[i].data));
			if (MessageClass &&
			    !strncasecmp(MessageClass, (char *)ldb_element->values[i].data, strlen((char *)ldb_element->values[i].data)) &&
			    strlen((char *)ldb_element->values[i].data) > length) {
				*fid = ldb_msg_find_attr_as_uint64(res->msgs[j], "PidTagFolderId", 0x0);

				if (*ExplicitMessageClass && strcmp(*ExplicitMessageClass, "")) {
					talloc_free((char *)*ExplicitMessageClass);
				}

				if (MessageClass && !strcmp(MessageClass, "All")) {
					*ExplicitMessageClass = "";
				} else {
					*ExplicitMessageClass = talloc_strdup(parent_ctx, (char *)ldb_element->values[i].data);
				}
				length = strlen((char *)ldb_element->values[i].data);
			}
		}
	}
	OPENCHANGE_RETVAL_IF(!*ExplicitMessageClass, MAPI_E_NOT_FOUND, mem_ctx);
	DEBUG(5, ("openchangedb_get_ReceiveFolder, fid: 0x%016"PRIx64"\n", *fid));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the number of sub folders for a given fid

   \param ldb_ctx pointer to the openchange LDB context
   \param fid the folder identifier to use for the search
   \param RowCount pointer to the returned number of results

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_count(void *ldb_ctx,
						       uint64_t fid,
						       uint32_t *RowCount)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_folder_count");
	*RowCount = 0;

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, 
			 "(PidTagParentFolderId=0x%.16"PRIx64")(PidTagFolderId=*)", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	*RowCount = res->count;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Check if a property exists within an openchange dispatcher
   database record

   \param ldb_ctx pointer to the openchange LDB context
   \param proptag the MAPI property tag to lookup
   \param fid the record folder identifier

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_lookup_folder_property(void *ldb_ctx, 
							     uint32_t proptag, 
							     uint64_t fid)
{
	TALLOC_CTX	       	*mem_ctx;
	struct ldb_result      	*res = NULL;
	const char * const     	attrs[] = { "*", NULL };
	const char	       	*PidTagAttr = NULL;
	int		       	ret;

	mem_ctx = talloc_named(NULL, 0, "get_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=0x%.16"PRIx64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	OPENCHANGE_RETVAL_IF(!PidTagAttr, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 3. Search for attribute */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve a special MAPI property from a folder record

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param res pointer to the LDB result
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return pointer to valid data on success, otherwise NULL
 */
static void *openchangedb_get_folder_special_property(TALLOC_CTX *mem_ctx,
						      void *ldb_ctx,
						      char *recipient,
						      struct ldb_result *res,
						      uint32_t proptag,
						      const char *PidTagAttr)
{
	enum MAPISTATUS		retval;
	struct GUID		MailboxGUID;
	struct GUID		ReplGUID;
	uint16_t		ReplID;
	struct Binary_r		*bin;
	uint16_t		FolderType;
	uint64_t		FolderId;
	const char		*tmp;
	uint32_t		*l;

	switch (proptag) {
	case PR_IPM_APPOINTMENT_ENTRYID:
	case PR_IPM_CONTACT_ENTRYID:
	case PR_IPM_JOURNAL_ENTRYID:
	case PR_IPM_NOTE_ENTRYID:
	case PR_IPM_TASK_ENTRYID:
	case PR_REMINDERS_ONLINE_ENTRYID:
	case PR_IPM_DRAFTS_ENTRYID:
		retval = openchangedb_get_MailboxGuid(ldb_ctx, recipient, &MailboxGUID);
		retval = openchangedb_get_MailboxReplica(ldb_ctx, recipient, &ReplID, &ReplGUID);
		FolderType = (uint16_t) ldb_msg_find_attr_as_int(res->msgs[0], "FolderType", 0x1);

		tmp = ldb_msg_find_attr_as_string(res->msgs[0], PidTagAttr, NULL);
		FolderId = strtoul(tmp, NULL, 16);
		retval = entryid_set_folder_EntryID(mem_ctx, &MailboxGUID, &ReplGUID, FolderType, FolderId, &bin);
		return (void *)bin;
		break;
	case PR_DEPTH:
		l = talloc_zero(mem_ctx, uint32_t);
		*l = 0;
		return (void *)l;
	}

	return NULL;
}


/**
   \details Retrieve a MAPI property from a OpenChange LDB message

   \param mem_ctx pointer to the memory context
   \param res pointer to the LDB results
   \param pos the LDB result index
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return valid data pointer on success, otherwise NULL
 */
static void *openchangedb_get_folder_property_data(TALLOC_CTX *mem_ctx,
						   struct ldb_result *res,
						   uint32_t pos,
						   uint32_t proptag,
						   const char *PidTagAttr)
{
	void			*data;
	const char     		*str;
	uint64_t		*d;
	uint32_t		*l;
	int			*b;

	switch (proptag & 0xFFFF) {
	case PT_BOOLEAN:
		b = talloc_zero(mem_ctx, int);
		*b = ldb_msg_find_attr_as_bool(res->msgs[pos], PidTagAttr, 0x0);
		data = (void *)b;
		break;
	case PT_LONG:
		l = talloc_zero(mem_ctx, uint32_t);
		*l = ldb_msg_find_attr_as_int(res->msgs[pos], PidTagAttr, 0x0);
		data = (void *)l;
		break;
	case PT_I8:
		str = ldb_msg_find_attr_as_string(res->msgs[pos], PidTagAttr, 0x0);
		d = talloc_zero(mem_ctx, uint64_t);
		*d = strtoull(str, NULL, 16);
		data = (void *)d;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		str = ldb_msg_find_attr_as_string(res->msgs[pos], PidTagAttr, NULL);
		data = (char *) talloc_strdup(mem_ctx, str);
		break;
	default:
		talloc_free(mem_ctx);
		DEBUG(0, ("[%s:%d] Property Type 0x%.4x not supported\n", __FUNCTION__, __LINE__, (proptag & 0xFFFF)));
		return NULL;
	}

	return data;
}


/**
   \details Return the next available FolderID
   
   \param ldb_ctx pointer to the openchange LDB context
   \param fid pointer to the fid value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_folderID(void *ldb_ctx,
						       uint64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res = NULL;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };

	*fid = 0;

	mem_ctx = talloc_named(NULL, 0, "get_new_folderID");

	/* Step 1. Get the current GlobalCount */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "GlobalCount", 0);

	/* Step 2. Update GlobalCount value */
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "GlobalCount", "0x%"PRIx64, ((*fid) + 1));
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	*fid = (*fid << 16) + 1;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve a MAPI property value from a folder record

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param recipient the mailbox username
   \param proptag the MAPI property tag to retrieve value for
   \param fid the record folder identifier
   \param data pointer on pointer to the data the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_property(TALLOC_CTX *parent_ctx, 
							  void *ldb_ctx,
							  char *recipient,
							  uint32_t proptag,
							  uint64_t fid,
							  void **data)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	const char		*PidTagAttr = NULL;
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=0x%.16"PRIx64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	OPENCHANGE_RETVAL_IF(!PidTagAttr, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 3. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 4. Check if this is a "special property" */
	*data = openchangedb_get_folder_special_property(parent_ctx, ldb_ctx, recipient, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	/* Step 5. If this is not a "special property" */
	*data = openchangedb_get_folder_property_data(parent_ctx, res, 0, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_NOT_FOUND;
}


/**
   \details Retrieve a MAPI property from a table (ldb search results)

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param recipient the mailbox username
   \param ldb_filter the ldb search string
   \param proptag the MAPI property tag to retrieve value for
   \param pos the record position in search results
   \param data pointer on pointer to the data the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_table_property(TALLOC_CTX *parent_ctx,
							 void *ldb_ctx,
							 char *recipient,
							 char *ldb_filter,
							 uint32_t proptag,
							 uint32_t pos,
							 void **data)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	const char		*PidTagAttr = NULL;
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_table_property");

	/* Step 1. Fetch table results */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter, NULL);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_INVALID_OBJECT, mem_ctx);

	/* Step 2. Ensure position is within search results range */
	OPENCHANGE_RETVAL_IF(pos >= res->count, MAPI_E_INVALID_OBJECT, NULL);

	/* Step 3. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	OPENCHANGE_RETVAL_IF(!PidTagAttr, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 4. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[pos], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 5. Check if this is a "special property" */
	*data = openchangedb_get_folder_special_property(parent_ctx, ldb_ctx, recipient, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	/* Step 6. Check if this is not a "special property" */
	*data = openchangedb_get_folder_property_data(parent_ctx, res, pos, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_NOT_FOUND;
}

/**
   \details Retrieve the folder ID associated with a given folder name

   This function looks up the specified foldername (as a PidTagDisplayName)
   and returns the associated folder ID. Note that folder names are only
   unique in the context of a parent folder, so the parent folder needs to
   be provided.

   \param ldb_ctx pointer to the openchange LDB context
   \param parent_fid the folder ID of the parent folder 
   \param foldername the name to look up
   \param fid the folder ID for the folder with the specified name (0 if not found)

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_fid_by_name(void *ldb_ctx,
						      uint64_t parent_fid,
						      const char* foldername,
						      uint64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_fid_by_name");
	*fid = 0;

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(PidTagParentFolderId=0x%.16"PRIx64")(PidTagDisplayName=%s))",
			 parent_fid, foldername);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	/* We should only ever get 0 records or 1 record, but there is always a chance
	   that things got confused at some point, so just return one of the records */
	OPENCHANGE_RETVAL_IF(res->count < 1, MAPI_E_NOT_FOUND, mem_ctx);
	
	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Set the receive folder for a specific message class.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param recipient pointer to the mailbox's username
   \param MessageClass message class (e.g. IPM.whatever) to set
   \param fid folder identifier for the recipient folder for the message class

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_set_ReceiveFolder(TALLOC_CTX *parent_ctx,
							void *ldb_ctx,
							const char *recipient,
							const char *MessageClass,
							uint64_t fid)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	struct ldb_dn			*dn;
	char				*dnstr;
	const char * const		attrs[] = { "*", NULL };
	int				ret;


	mem_ctx = talloc_named(NULL, 0, "set_ReceiveFolder");

	DEBUG(5, ("openchangedb_set_ReceiveFolder, recipient: %s\n", recipient));
	DEBUG(5, ("openchangedb_set_ReceiveFolder, MessageClass: %s\n", MessageClass));
	DEBUG(5, ("openchangedb_set_ReceiveFolder, fid: 0x%016"PRIx64"\n", fid));
	
	/* Step 1. Find mailbox DN for the recipient */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dnstr = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	DEBUG(5, ("openchangedb_set_ReceiveFolder, dnstr: %s\n", dnstr));

	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(res);

	dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	/* Step 2. Search for the MessageClass within user's mailbox */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, 
			 "(PidTagMessageClass=%s)", MessageClass);
	DEBUG(5, ("openchangedb_get_ReceiveFolder, res->count: %i\n", res->count));

	/* We should never have more than one record with a specific MessageClass */
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || (res->count > 1), MAPI_E_CORRUPT_STORE, mem_ctx);
	
	/* Step 3. Delete the old entry if applicable */
	if (res->count) {
		/* we already have an entry for this message class, so delete it before creating the new one */
		enum MAPISTATUS		retval;
		char			*distinguishedName;
		struct ldb_message	*msg;

		uint64_t folderid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0x0);
		DEBUG(6, ("openchangedb_set_ReceiveFolder, fid to delete from: 0x%016"PRIx64"\n", folderid));

		retval = openchangedb_get_distinguishedName(parent_ctx, ldb_ctx, folderid, &distinguishedName);
		DEBUG(6, ("openchangedb_set_ReceiveFolder, dn to delete from: %s\n", distinguishedName));
		dn = ldb_dn_new(mem_ctx, ldb_ctx, distinguishedName);
		talloc_free(distinguishedName);

		msg = ldb_msg_new(mem_ctx);
		msg->dn = ldb_dn_copy(mem_ctx, dn);
		ldb_msg_add_string(msg, "PidTagMessageClass", MessageClass);
		msg->elements[0].flags = LDB_FLAG_MOD_DELETE;

		ret = ldb_modify(ldb_ctx, msg);
		if (ret != LDB_SUCCESS) {
			DEBUG(0, ("Failed to delete old message class entry: %s\n", ldb_strerror(ret)));
			talloc_free(mem_ctx);
			return MAPI_E_NO_SUPPORT;
		}
	}
	
	/* Step 4. Create the new entry if applicable */
	if (fid != 0x0) {
		enum MAPISTATUS		retval;
		char			*distinguishedName;
		struct ldb_message	*msg;

		retval = openchangedb_get_distinguishedName(parent_ctx, ldb_ctx, fid, &distinguishedName);
		DEBUG(6, ("openchangedb_set_ReceiveFolder, dn to create in: %s\n", distinguishedName));

		dn = ldb_dn_new(mem_ctx, ldb_ctx, distinguishedName);
		talloc_free(distinguishedName);

		msg = ldb_msg_new(mem_ctx);
		msg->dn = ldb_dn_copy(mem_ctx, dn);
		ldb_msg_add_string(msg, "PidTagMessageClass", MessageClass);
		msg->elements[0].flags = LDB_FLAG_MOD_ADD;

		ret = ldb_modify(ldb_ctx, msg);
		if (ret != LDB_SUCCESS) {
			DEBUG(0, ("Failed to add message class entry: %s\n", ldb_strerror(ret)));
			talloc_free(mem_ctx);
			return MAPI_E_NO_SUPPORT;
		}
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
