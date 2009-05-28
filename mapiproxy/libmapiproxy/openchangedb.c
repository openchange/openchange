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

#include <mapiproxy/dcesrv_mapiproxy.h>
#include "libmapiproxy.h"
#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <libmapi/defs_private.h>

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
   \details Retrieve the mapistore URI associated to a mailbox system
   folder.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *parent_ctx,
						       void *ldb_ctx,
						       uint64_t fid,
						       char **mapistoreURL)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=0x%.16"PRIx64")", fid);

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
	int				i;
	int				length;

	mem_ctx = talloc_named(NULL, 0, "get_ReceiveFolder");

	/* Step 1. Search Mailbox DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dnstr = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(res);

	/* Step 2. Search for MessageClass substring within user's mailbox */
	dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, 
			 "(PidTagMessageClass=%s*)", MessageClass);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	
	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0x0);

	/* Step 3. Find the longest ExplicitMessageClass matching MessageClass */
	ldb_element = ldb_msg_find_element(res->msgs[0], "PidTagMessageClass");
	for (i = 0, length = 0; i < ldb_element[0].num_values; i++) {
		if (MessageClass && !strncasecmp(MessageClass, (char *)ldb_element->values[i].data, 
						 strlen((char *)ldb_element->values[i].data)) &&
		    strlen((char *)ldb_element->values[i].data) > length) {

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
	OPENCHANGE_RETVAL_IF(!*ExplicitMessageClass, MAPI_E_NOT_FOUND, mem_ctx);

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
