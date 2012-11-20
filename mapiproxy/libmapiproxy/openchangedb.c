/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2011

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

#include <inttypes.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

extern struct ldb_val ldb_binary_decode(TALLOC_CTX *, const char *);

const char *openchangedb_nil_string = "<nil>";

/**
   \details Retrieve the mailbox FolderID for given recipient from
   openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_SystemFolderID(struct ldb_context *ldb_ctx,
							 const char *recipient, uint32_t SystemIdx,
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
		*FolderId = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);
		OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

		talloc_free(mem_ctx);
		return MAPI_E_SUCCESS;
	}

	dn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 3. Search FolderID */
	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs, 
			 "(&(objectClass=systemfolder)(SystemIdx=%d))", SystemIdx);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*FolderId = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);
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
_PUBLIC_ enum MAPISTATUS openchangedb_get_PublicFolderID(struct ldb_context *ldb_ctx,
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

	*FolderId = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);
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
							    struct ldb_context *ldb_ctx, 
							    uint64_t fid, 
							    char **distinguishedName)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_distinguishedName");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*distinguishedName = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the mailboxDN associated to a mailbox system
   folder.

   \param parent_ctx pointer to the parent memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param fid the folder identifier to search for
   \param mailboxDN pointer on pointer to the mailboxDN string the
   function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mailboxDN(TALLOC_CTX *parent_ctx, 
						    struct ldb_context *ldb_ctx, 
						    uint64_t fid, 
						    char **mailboxDN)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mailboxDN");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mailboxDN = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "mailboxDN", NULL));

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
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxGuid(struct ldb_context *ldb_ctx,
						      const char *recipient,
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
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxReplica(struct ldb_context *ldb_ctx,
							 const char *recipient, uint16_t *ReplID,
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

	mem_ctx = talloc_named(NULL, 0, "get_MailboxReplica");

	/* Step 1. Search Mailbox DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Retrieve ReplicaID attribute's value */
        if (ReplID) {
		*ReplID = ldb_msg_find_attr_as_int(res->msgs[0], "ReplicaID", 0);
	}

	/* Step 3/ Retrieve ReplicaGUID attribute's value */
	if (ReplGUID) {
		guid = ldb_msg_find_attr_as_string(res->msgs[0], "ReplicaGUID", 0);
		OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);
		GUID_from_string(guid, ReplGUID);
	}

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
_PUBLIC_ enum MAPISTATUS openchangedb_get_PublicFolderReplica(struct ldb_context *ldb_ctx,
							      uint16_t *ReplID,
							      struct GUID *ReplGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_PublicFolderReplica");

	/* Step 1. Search for public folder container */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=publicfolders");

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Retrieve ReplicaID attribute's value */
	if (ReplID != NULL) {
		*ReplID = ldb_msg_find_attr_as_int(res->msgs[0], "ReplicaID", 0);
	}

	/* Step 3. Retrieve ReplicaGUID attribute's value */
	if (ReplGUID != NULL) {
		guid = ldb_msg_find_attr_as_string(res->msgs[0], "StoreGUID", 0);
		OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);
		GUID_from_string(guid, ReplGUID);
	}

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
   \param owner pointer on pointer to the owner of the parent mailbox
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *parent_ctx,
						       struct ldb_context *ldb_ctx,
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
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	} else {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	}

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mapistoreURL = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "MAPIStoreURI", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Store the mapistore URI associated to a mailbox system
   folder.

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns
   \param owner pointer on pointer to the owner of the parent mailbox
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_set_mapistoreURI(struct ldb_context *ldb_ctx,
						       uint64_t fid,
						       const char *mapistoreURL,
						       bool mailboxstore)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	if (mailboxstore == true) {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	} else {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	}

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_string(msg, "MAPIStoreURI", mapistoreURL);
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	return ret;
}

/**
   \details Retrieve the parent fid associated to a mailbox system
   folder.

   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param parent_fidp pointer to the parent_fid the function returns
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_parent_fid(struct ldb_context *ldb_ctx,
						     uint64_t fid,
						     uint64_t *parent_fidp,
						     bool mailboxstore)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_parent_fid");

	if (mailboxstore == true) {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	} else {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	}
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	*parent_fidp = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagParentFolderId", 0x0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the fid associated with a mapistore URI.

   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_fid(struct ldb_context *ldb_ctx, const char *mapistoreURL, uint64_t *fidp)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	char			*slashLessURL;
	size_t			len;
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "openchangedb_get_fid");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=%s)", mapistoreURL);
	if (ret != LDB_SUCCESS || !res->count) {
		len = strlen(mapistoreURL);
		if (mapistoreURL[len-1] == '/') {
			slashLessURL = talloc_strdup(mem_ctx, mapistoreURL);
			slashLessURL[len-1] = 0;
			ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=%s)", slashLessURL);
		}
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	}
	*fidp = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0x0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve a list of mapistore URI in use for a certain user

   \param ldb_ctx pointer to the openchange LDB context
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MAPIStoreURIs(struct ldb_context *ldb_ctx, const char *username, TALLOC_CTX *mem_ctx, struct StringArrayW_r **urisP)
{
	TALLOC_CTX		*local_mem_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_dn		*dn;
	const char * const	attrs[] = { "*", NULL };
	char			*dnstr;
	int			i, elements, ret;
	struct StringArrayW_r	*uris;

	local_mem_ctx = talloc_named(NULL, 0, "openchangedb_get_fid");

	/* fetch mailbox DN */
	ret = ldb_search(ldb_ctx, local_mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(cn=%s)(MailboxGUID=*))", username);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, local_mem_ctx);

	dnstr = talloc_strdup(local_mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, local_mem_ctx);
	dn = ldb_dn_new(local_mem_ctx, ldb_ctx, dnstr);

	uris = talloc_zero(mem_ctx, struct StringArrayW_r);
	uris->lppszW = talloc_zero(uris, const char *);
	*urisP = uris;

	elements = 0;

	/* search subfolders which have a non-null mapistore uri */
	ret = ldb_search(ldb_ctx, local_mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=*)");
	if (ret == LDB_SUCCESS) {
		for (i = 0; i < res->count; i++) {
			if ((uris->cValues + 1) > elements) {
				elements = uris->cValues + 16;
				uris->lppszW = talloc_realloc(uris, uris->lppszW, const char *, elements);
			}
			uris->lppszW[uris->cValues] = talloc_strdup(uris, ldb_msg_find_attr_as_string(res->msgs[i], "MAPIStoreURI", NULL));
			uris->cValues++;
		}
	}

	talloc_free(local_mem_ctx);

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
							struct ldb_context *ldb_ctx,
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
	DEBUG(5, ("openchangedb_get_ReceiveFolder (All target), class: %s, fid: %.16"PRIx64"\n",
		  *ExplicitMessageClass, *fid));
	if (strcmp(MessageClass, "All") == 0) {
		/* we're done here */
		talloc_free(mem_ctx);
		return MAPI_E_SUCCESS;
	}

	/* Step 2B. Search for all MessageClasses within user's mailbox */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs, 
			 "(PidTagMessageClass=*)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 3. Find the message class that has the longest matching string entry */
	for (j = 0; j < res->count; ++j) {
		ldb_element = ldb_msg_find_element(res->msgs[j], "PidTagMessageClass");
		DEBUG(6, ("openchangedb_get_ReceiveFolder, checking fid: %.16"PRIx64"\n",
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
	DEBUG(5, ("openchangedb_get_ReceiveFolder, fid: %.16"PRIx64"\n", *fid));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the Transport Folder FolderID for given recipient from openchange dispatcher database

   \param ldb_ctx pointer to the OpenChange LDB context
   \param recipient the mailbox username
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_TransportFolder(struct ldb_context *ldb_ctx, const char *recipient, uint64_t *FolderId)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	char				*dnstr;
	struct ldb_dn			*ldb_dn = NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
	
	mem_ctx = talloc_named(NULL, 0, "get_TransportFolder");

	/* Step 1. Find mailbox DN for the recipient */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", recipient);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dnstr = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(res);

	/* Step 2. Find "Outbox" in user mailbox */
	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs, "(PidTagDisplayName=%s)", "Outbox");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. If Mailbox root folder, check for FolderID within current record */
	*FolderId = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);
	OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

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
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_count(struct ldb_context *ldb_ctx, uint64_t fid, uint32_t *RowCount)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!RowCount, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_folder_count");
	*RowCount = 0;

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, 
			 "(PidTagParentFolderId=%"PRIu64")(PidTagFolderId=*)", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	*RowCount = res->count;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


static char *openchangedb_unknown_property(TALLOC_CTX *mem_ctx, uint32_t proptag)
{
	return talloc_asprintf(mem_ctx, "Unknown%.8x", proptag);
}

/**
   \details Check if a property exists within an openchange dispatcher
   database record

   \param ldb_ctx pointer to the openchange LDB context
   \param proptag the MAPI property tag to lookup
   \param fid the record folder identifier

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_lookup_folder_property(struct ldb_context *ldb_ctx, 
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
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	if (!PidTagAttr) {
		PidTagAttr = openchangedb_unknown_property(mem_ctx, proptag);
	}

	/* Step 3. Search for attribute */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve a special MAPI property from an openchangedb record

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer to the OpenChange LDB context
   \param res pointer to the LDB result
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return pointer to valid data on success, otherwise NULL
 */
void *openchangedb_get_special_property(TALLOC_CTX *mem_ctx,
					struct ldb_context *ldb_ctx,
					struct ldb_result *res,
					uint32_t proptag,
					const char *PidTagAttr)
{
	uint32_t		*l;

	switch (proptag) {
	case PR_DEPTH:
		l = talloc_zero(mem_ctx, uint32_t);
		*l = 0;
		return (void *)l;
	}

	return NULL;
}

static struct BinaryArray_r *decode_mv_binary(TALLOC_CTX *mem_ctx, const char *str)
{
	const char *start;
	char *tmp;
	size_t i, current, len;
	uint32_t j;
	struct BinaryArray_r *bin_array;

	bin_array = talloc_zero(mem_ctx, struct BinaryArray_r);

	start = str;
	len = strlen(str);
	i = 0;
	while (i < len && start[i] != ';') {
		i++;
	}
	if (i < len) {
		tmp = talloc_memdup(NULL, start, i + 1);
		tmp[i] = 0;
		bin_array->cValues = strtol(tmp, NULL, 16);
		bin_array->lpbin = talloc_array(bin_array, struct Binary_r, bin_array->cValues);
		talloc_free(tmp);

		i++;
		for (j = 0; j < bin_array->cValues; j++) {
			current = i;
			while (i < len && start[i] != ';') {
				i++;
			}

			tmp = talloc_memdup(bin_array, start + current, i - current + 1);
			tmp[i - current] = 0;
			i++;

			if (tmp[0] != 0 && strcmp(tmp, openchangedb_nil_string) != 0) {
				bin_array->lpbin[j].lpb = (uint8_t *) tmp;
				bin_array->lpbin[j].cb = ldb_base64_decode((char *) bin_array->lpbin[j].lpb);
			}
			else {
				bin_array->lpbin[j].lpb = talloc_zero(bin_array, uint8_t);
				bin_array->lpbin[j].cb = 0;
			}
		}
	}

	return bin_array;
}

static struct LongArray_r *decode_mv_long(TALLOC_CTX *mem_ctx, const char *str)
{
	const char *start;
	char *tmp;
	size_t i, current, len;
	uint32_t j;
	struct LongArray_r *long_array;

	long_array = talloc_zero(mem_ctx, struct LongArray_r);

	start = str;
	len = strlen(str);
	i = 0;
	while (i < len && start[i] != ';') {
		i++;
	}
	if (i < len) {
		tmp = talloc_memdup(NULL, start, i + 1);
		tmp[i] = 0;
		long_array->cValues = strtol(tmp, NULL, 16);
		long_array->lpl = talloc_array(long_array, uint32_t, long_array->cValues);
		talloc_free(tmp);

		i++;
		for (j = 0; j < long_array->cValues; j++) {
			current = i;
			while (i < len && start[i] != ';') {
				i++;
			}

			tmp = talloc_memdup(long_array, start + current, i - current + 1);
			tmp[i - current] = 0;
			i++;

			long_array->lpl[j] = strtol(tmp, NULL, 16);
			talloc_free(tmp);
		}
	}

	return long_array;
}

/**
   \details Retrieve a MAPI property from a OpenChange LDB result set

   \param mem_ctx pointer to the memory context
   \param res pointer to the LDB results
   \param pos the LDB result index
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return valid data pointer on success, otherwise NULL
 */
void *openchangedb_get_property_data(TALLOC_CTX *mem_ctx,
				     struct ldb_result *res,
				     uint32_t pos,
				     uint32_t proptag,
				     const char *PidTagAttr)
{
	return openchangedb_get_property_data_message(mem_ctx, res->msgs[pos], 
						      proptag, PidTagAttr);
}

/**
   \details Retrieve a MAPI property from an OpenChange LDB message

   \param mem_ctx pointer to the memory context
   \param msg pointer to the LDB message
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return valid data pointer on success, otherwise NULL
 */
void *openchangedb_get_property_data_message(TALLOC_CTX *mem_ctx,
					     struct ldb_message *msg,
					     uint32_t proptag,
					     const char *PidTagAttr)
{
	void			*data;
	const char     		*str;
	uint64_t		*ll;
	uint32_t		*l;
	int			*b;
	struct FILETIME		*ft;
	struct Binary_r		*bin;
	struct BinaryArray_r	*bin_array;
	struct LongArray_r	*long_array;
	struct ldb_val		val;
	TALLOC_CTX		*local_mem_ctx;

	switch (proptag & 0xFFFF) {
	case PT_BOOLEAN:
		b = talloc_zero(mem_ctx, int);
		*b = ldb_msg_find_attr_as_bool(msg, PidTagAttr, 0x0);
		data = (void *)b;
		break;
	case PT_LONG:
		l = talloc_zero(mem_ctx, uint32_t);
		*l = ldb_msg_find_attr_as_uint(msg, PidTagAttr, 0x0);
		data = (void *)l;
		break;
	case PT_I8:
		ll = talloc_zero(mem_ctx, uint64_t);
		*ll = ldb_msg_find_attr_as_uint64(msg, PidTagAttr, 0x0);
		data = (void *)ll;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		str = ldb_msg_find_attr_as_string(msg, PidTagAttr, NULL);
		local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
		val = ldb_binary_decode(local_mem_ctx, str);
		data = (char *) talloc_strndup(mem_ctx, (char *) val.data, val.length);
		talloc_free(local_mem_ctx);
		break;
	case PT_SYSTIME:
		ft = talloc_zero(mem_ctx, struct FILETIME);
		ll = talloc_zero(mem_ctx, uint64_t);
		*ll = ldb_msg_find_attr_as_uint64(msg, PidTagAttr, 0x0);
		ft->dwLowDateTime = (*ll & 0xffffffff);
		ft->dwHighDateTime = *ll >> 32;
		data = (void *)ft;
		talloc_free(ll);
		break;
	case PT_BINARY:
		str = ldb_msg_find_attr_as_string(msg, PidTagAttr, 0x0);
		bin = talloc_zero(mem_ctx, struct Binary_r);
		if (strcmp(str, openchangedb_nil_string) == 0) {
			bin->lpb = (uint8_t *) talloc_zero(mem_ctx, uint8_t);
			bin->cb = 0;
		}
		else {
			bin->lpb = (uint8_t *) talloc_strdup(mem_ctx, str);
			bin->cb = ldb_base64_decode((char *) bin->lpb);
		}
		data = (void *)bin;
		break;
	case PT_MV_BINARY:
		str = ldb_msg_find_attr_as_string(msg, PidTagAttr, NULL);
		bin_array = decode_mv_binary(mem_ctx, str);
		data = (void *)bin_array;
		break;
	case PT_MV_LONG:
		str = ldb_msg_find_attr_as_string(msg, PidTagAttr, NULL);
		long_array = decode_mv_long(mem_ctx, str);
		data = (void *)long_array;
		break;
	default:
		DEBUG(0, ("[%s:%d] Property Type 0x%.4x not supported\n", __FUNCTION__, __LINE__, (proptag & 0xFFFF)));
		abort();
		return NULL;
	}

	return data;
}

/**
   \details Build a MAPI property suitable for a OpenChange LDB message

   \param mem_ctx pointer to the memory context
   \param value the MAPI property

   \return valid string pointer on success, otherwise NULL
 */
_PUBLIC_ char *openchangedb_set_folder_property_data(TALLOC_CTX *mem_ctx, 
						     struct SPropValue *value)
{
	char			*data, *subdata;
	struct SPropValue	*subvalue;
	NTTIME			nt_time;
	uint32_t		i;
	size_t		        data_len, subdata_len;
	struct BinaryArray_r	*bin_array;

	switch (value->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		data = talloc_strdup(mem_ctx, value->value.b ? "TRUE" : "FALSE");
		break;
	case PT_LONG:
		data = talloc_asprintf(mem_ctx, "%d", value->value.l);
		break;
	case PT_I8:
		data = talloc_asprintf(mem_ctx, "%"PRIu64, value->value.d);
		break;
	case PT_STRING8:
		data = talloc_strdup(mem_ctx, value->value.lpszA);
		break;
	case PT_UNICODE:
		data = talloc_strdup(mem_ctx, value->value.lpszW);
		break;
	case PT_SYSTIME:
		nt_time = ((uint64_t) value->value.ft.dwHighDateTime << 32) | value->value.ft.dwLowDateTime;
		data = talloc_asprintf(mem_ctx, "%"PRIu64, nt_time);
		break;
	case PT_BINARY:
		/* ldb silently prevents empty strings from being added to the db */
		if (value->value.bin.cb > 0)
			data = ldb_base64_encode(mem_ctx, (char *) value->value.bin.lpb, value->value.bin.cb);
		else
			data = talloc_strdup(mem_ctx, openchangedb_nil_string);
		break;
	case PT_MV_BINARY:
		bin_array = &value->value.MVbin;
		data = talloc_asprintf(mem_ctx, "0x%.8x", bin_array->cValues);
		data_len = strlen(data);
		for (i = 0; i < bin_array->cValues; i++) {
			subvalue = talloc_zero(NULL, struct SPropValue);
			subvalue->ulPropTag = (value->ulPropTag & 0xffff0fff);
			subvalue->value.bin = bin_array->lpbin[i];
			subdata = openchangedb_set_folder_property_data(subvalue, subvalue);
			subdata_len = strlen(subdata);
			data = talloc_realloc(mem_ctx, data, char, data_len + subdata_len + 2);
			*(data + data_len) = ';';
			memcpy(data + data_len + 1, subdata, subdata_len);
			data_len += subdata_len + 1;
			talloc_free(subvalue);
		}
		*(data + data_len) = 0;
		break;
	default:
		DEBUG(0, ("[%s:%d] Property Type 0x%.4x not supported\n", __FUNCTION__, __LINE__, (value->ulPropTag & 0xFFFF)));
		data = NULL;
	}

	return data;
}

/**
   \details Allocates a new FolderID and returns it
   
   \param ldb_ctx pointer to the openchange LDB context
   \param fid pointer to the fid value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_folderID(struct ldb_context *ldb_ctx, uint64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };

	/* Get the current GlobalCount */
	mem_ctx = talloc_named(NULL, 0, "get_next_folderID");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "GlobalCount", 0);

	/* Update GlobalCount value */
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "GlobalCount", "%"PRIu64, ((*fid) + 1));
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	*fid = (exchange_globcnt(*fid) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

/**
   \details Allocates a batch of new folder ids and returns them
   
   \param ldb_ctx pointer to the openchange LDB context
   \param fid pointer to the fid value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_folderIDs(struct ldb_context *ldb_ctx, TALLOC_CTX *mem_ctx, uint64_t max, struct UI8Array_r **fids_p)
{
	TALLOC_CTX		*local_mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	uint64_t		fid, count;
	struct UI8Array_r	*fids;

	/* Get the current GlobalCount */
	local_mem_ctx = talloc_named(NULL, 0, "get_next_folderIDs");
	ret = ldb_search(ldb_ctx, local_mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, local_mem_ctx);

	fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "GlobalCount", 0);

	fids = talloc_zero(local_mem_ctx, struct UI8Array_r);
	fids->cValues = max;
	fids->lpui8 = talloc_array(fids, uint64_t, max);

	for (count = 0; count < max; count++) {
		fids->lpui8[count] = (exchange_globcnt(fid + count) << 16) | 0x0001;
	}

	/* Update GlobalCount value */
	msg = ldb_msg_new(local_mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, local_mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "GlobalCount", "%"PRIu64, ((fid) + max));
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, local_mem_ctx);

	*fids_p = fids;
	(void) talloc_reference(mem_ctx, fids);
	talloc_free(local_mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Allocates a new change number and returns it
   
   \param ldb_ctx pointer to the openchange LDB context
   \param cn pointer to the cn value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_changeNumber(struct ldb_context *ldb_ctx, uint64_t *cn)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };

	/* Get the current GlobalCount */
	mem_ctx = talloc_named(NULL, 0, "get_next_changeNumber");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*cn = ldb_msg_find_attr_as_uint64(res->msgs[0], "ChangeNumber", 1);

	/* Update GlobalCount value */
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "ChangeNumber", "%"PRIu64, ((*cn) + 1));
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	*cn = (exchange_globcnt(*cn) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

/**
   \details Allocates a batch of new change numbers and returns them
   
   \param ldb_ctx pointer to the openchange LDB context
   \param cn pointer to the cn value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_changeNumbers(struct ldb_context *ldb_ctx, TALLOC_CTX *mem_ctx, uint64_t max, struct UI8Array_r **cns_p)
{
	TALLOC_CTX		*local_mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	uint64_t		cn, count;
	struct UI8Array_r	*cns;

	/* Get the current GlobalCount */
	local_mem_ctx = talloc_named(NULL, 0, "get_new_changeNumber");
	ret = ldb_search(ldb_ctx, local_mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, local_mem_ctx);

	cn = ldb_msg_find_attr_as_uint64(res->msgs[0], "ChangeNumber", 1);

	cns = talloc_zero(local_mem_ctx, struct UI8Array_r);
	cns->cValues = max;
	cns->lpui8 = talloc_array(cns, uint64_t, max);

	for (count = 0; count < max; count++) {
		cns->lpui8[count] = (exchange_globcnt(cn + count) << 16) | 0x0001;
	}

	/* Update GlobalCount value */
	msg = ldb_msg_new(local_mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, local_mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "ChangeNumber", "%"PRIu64, (cn + max));
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, local_mem_ctx);

	*cns_p = cns;
	(void) talloc_reference(mem_ctx, cns);
	talloc_free(local_mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Returns the change number that will be allocated when openchangedb_get_new_changeNumber is next invoked
   
   \param ldb_ctx pointer to the openchange LDB context
   \param cn pointer to the cn value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_next_changeNumber(struct ldb_context *ldb_ctx, uint64_t *cn)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res;
	const char * const	attrs[] = { "ChangeNumber", NULL };

	/* Get the current GlobalCount */
	mem_ctx = talloc_named(NULL, 0, "get_next_changeNumber");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*cn = ldb_msg_find_attr_as_uint64(res->msgs[0], "ChangeNumber", 1);

	talloc_free(mem_ctx);

	*cn = (exchange_globcnt(*cn) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

/**
   \details Reserve a range of FMID
   
   \param ldb_ctx pointer to the openchange LDB context
   \param fid pointer to the fid value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_reserve_fmid_range(struct ldb_context *ldb_ctx,
							 uint64_t range_len,
							 uint64_t *first_fmidp)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	uint64_t		fmid;
	const char * const	attrs[] = { "*", NULL };

	/* Step 1. Get the current GlobalCount */
	mem_ctx = talloc_named(NULL, 0, "get_next_folderID");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	fmid = ldb_msg_find_attr_as_uint64(res->msgs[0], "GlobalCount", 0);

	/* Step 2. Update GlobalCount value */
	mem_ctx = talloc_zero(NULL, void);

	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));
	ldb_msg_add_fmt(msg, "GlobalCount", "%"PRIu64, (fmid + range_len));
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	*first_fmidp = (exchange_globcnt(fmid) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve a MAPI property value from a folder record

   \param parent_ctx pointer to the memory context
   \param ldb_ctx pointer to the openchange LDB context
   \param proptag the MAPI property tag to retrieve value for
   \param fid the record folder identifier
   \param data pointer on pointer to the data the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_property(TALLOC_CTX *parent_ctx, 
							  struct ldb_context *ldb_ctx,
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
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	if (!PidTagAttr) {
		PidTagAttr = openchangedb_unknown_property(mem_ctx, proptag);
	}

	/* Step 3. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 4. Check if this is a "special property" */
	*data = openchangedb_get_special_property(parent_ctx, ldb_ctx, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	/* Step 5. If this is not a "special property" */
	*data = openchangedb_get_property_data(parent_ctx, res, 0, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_NOT_FOUND;
}

_PUBLIC_ enum MAPISTATUS openchangedb_set_folder_properties(struct ldb_context *ldb_ctx, uint64_t fid, struct SRow *row)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	char			*PidTagAttr = NULL;
	struct SPropValue	*value;
	const char * const	attrs[] = { "*", NULL };
	struct ldb_message	*msg;
	char			*str_value;
	time_t			unix_time;
	NTTIME			nt_time;
	uint32_t		i;
	int			ret;

	unix_time = time(NULL);

	mem_ctx = talloc_named(NULL, 0, "set_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Update GlobalCount value */
	msg = ldb_msg_new(mem_ctx);
	msg->dn = ldb_dn_copy(msg, ldb_msg_find_attr_as_dn(ldb_ctx, mem_ctx, res->msgs[0], "distinguishedName"));

	for (i = 0; i < row->cValues; i++) {
		value = row->lpProps + i;

		switch (value->ulPropTag) {
		case PR_DEPTH:
		case PR_SOURCE_KEY:
		case PR_PARENT_SOURCE_KEY:
		case PR_CREATION_TIME:
		case PR_LAST_MODIFICATION_TIME:
			DEBUG(5, ("Ignored attempt to set handled property %.8x\n", value->ulPropTag));
			break;
		default:
			/* Step 2. Convert proptag into PidTag attribute */
			PidTagAttr = (char *) openchangedb_property_get_attribute(value->ulPropTag);
			if (!PidTagAttr) {
				PidTagAttr = openchangedb_unknown_property(mem_ctx, value->ulPropTag);
			}

			str_value = openchangedb_set_folder_property_data(mem_ctx, value);
			if (!str_value) {
				DEBUG(5, ("Ignored property of unhandled type %.4x\n", (value->ulPropTag & 0xffff)));
				continue;
			}

			ldb_msg_add_string(msg, PidTagAttr, str_value);
			msg->elements[msg->num_elements-1].flags = LDB_FLAG_MOD_REPLACE;
		}
	}

	/* We set the last modification time */
	value = talloc_zero(NULL, struct SPropValue);

	value->ulPropTag = PR_LAST_MODIFICATION_TIME;
	unix_to_nt_time(&nt_time, unix_time);
	value->value.ft.dwLowDateTime = nt_time & 0xffffffff;
	value->value.ft.dwHighDateTime = nt_time >> 32;
	str_value = openchangedb_set_folder_property_data(mem_ctx, value);
	ldb_msg_add_string(msg, "PidTagLastModificationTime", str_value);
	msg->elements[msg->num_elements-1].flags = LDB_FLAG_MOD_REPLACE;

	value->ulPropTag = PidTagChangeNumber;
	openchangedb_get_new_changeNumber(ldb_ctx, (uint64_t *) &value->value.d);
	str_value = openchangedb_set_folder_property_data(mem_ctx, value);
	ldb_msg_add_string(msg, "PidTagChangeNumber", str_value);
	msg->elements[msg->num_elements-1].flags = LDB_FLAG_MOD_REPLACE;

	talloc_free(value);

	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
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
							 struct ldb_context *ldb_ctx,
							 const char *ldb_filter,
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
	OPENCHANGE_RETVAL_IF(pos >= res->count, MAPI_E_INVALID_OBJECT, mem_ctx);

	/* Step 3. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	if (!PidTagAttr) {
		PidTagAttr = openchangedb_unknown_property(mem_ctx, proptag);
	}

	/* Step 4. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 5. Check if this is a "special property" */
	*data = openchangedb_get_special_property(parent_ctx, ldb_ctx, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	/* Step 6. Check if this is not a "special property" */
	*data = openchangedb_get_property_data(parent_ctx, res, pos, proptag, PidTagAttr);
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
_PUBLIC_ enum MAPISTATUS openchangedb_get_fid_by_name(struct ldb_context *ldb_ctx,
						      uint64_t parent_fid,
						      const char* foldername,
						      uint64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_fid_by_name");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(PidTagParentFolderId=%"PRIu64")(PidTagDisplayName=%s))",
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
   \details Retrieve the message ID associated with a given subject (normalized)

   \param ldb_ctx pointer to the openchange LDB context
   \param parent_fid the folder ID of the parent folder 
   \param subject the normalized subject to look up
   \param mid the message ID for the message (0 if not found)

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mid_by_subject(struct ldb_context *ldb_ctx, uint64_t parent_fid, const char *subject, bool mailboxstore, uint64_t *mid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	struct ldb_dn		*base_dn;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mid_by_subject");

	if (mailboxstore) {
		base_dn = ldb_get_default_basedn(ldb_ctx);
	} else {
		base_dn = ldb_get_root_basedn(ldb_ctx);
	}

	ret = ldb_search(ldb_ctx, mem_ctx, &res, base_dn,
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(PidTagParentFolderId=%"PRIu64")(PidTagNormalizedSubject=%s))",
			 parent_fid, subject);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	/* We should only ever get 0 records or 1 record, but there is always a chance
	   that things got confused at some point, so just return one of the records */
	OPENCHANGE_RETVAL_IF(res->count < 1, MAPI_E_NOT_FOUND, mem_ctx);
	
	*mid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagMessageId", 0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

_PUBLIC_ enum MAPISTATUS openchangedb_delete_folder(struct ldb_context *ldb_ctx, uint64_t fid)
{
	TALLOC_CTX	*mem_ctx;
	char		*dnstr;
	struct ldb_dn	*dn;
	int		retval;
	enum MAPISTATUS	ret;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	ret = openchangedb_get_distinguishedName(mem_ctx, ldb_ctx, fid, &dnstr);
	if (ret != MAPI_E_SUCCESS) {
		goto end;
	}

	dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	retval = ldb_delete(ldb_ctx, dn);
	if (retval == LDB_SUCCESS) {
		ret = MAPI_E_SUCCESS;
	}
	else {
		ret = MAPI_E_CORRUPT_STORE;
	}

end:
	talloc_free(mem_ctx);
	
	return ret;
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
_PUBLIC_ enum MAPISTATUS openchangedb_set_ReceiveFolder(struct ldb_context *ldb_ctx, const char *recipient, const char *MessageClass, uint64_t fid)
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
	DEBUG(5, ("openchangedb_set_ReceiveFolder, fid: 0x%.16"PRIx64"\n", fid));
	
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
		char			*distinguishedName;
		struct ldb_message	*msg;

		uint64_t folderid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0x0);
		DEBUG(6, ("openchangedb_set_ReceiveFolder, fid to delete from: 0x%.16"PRIx64"\n", folderid));

		openchangedb_get_distinguishedName(mem_ctx, ldb_ctx, folderid, &distinguishedName);
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
		char			*distinguishedName;
		struct ldb_message	*msg;

		openchangedb_get_distinguishedName(mem_ctx, ldb_ctx, fid, &distinguishedName);
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


_PUBLIC_ enum MAPISTATUS openchangedb_get_fid_from_partial_uri(struct ldb_context *ldb_ctx,
							       const char *partialURI,
							       uint64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_fid_from_partial_uri");

	/* Search mapistoreURI given partial URI */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=%s)", partialURI);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	OPENCHANGE_RETVAL_IF(res->count > 1, MAPI_E_COLLISION, mem_ctx);

	*fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);
	talloc_free(mem_ctx);
	
	return MAPI_E_SUCCESS;
}


_PUBLIC_ enum MAPISTATUS openchangedb_get_users_from_partial_uri(TALLOC_CTX *parent_ctx,
								 struct ldb_context *ldb_ctx,
								 const char *partialURI,
								 uint32_t *count,
								 char ***MAPIStoreURI,
								 char ***users)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_result	*mres = NULL;
	const char		*mailboxDN;
	struct ldb_dn		*dn;
	const char * const	attrs[] = { "*", NULL };
	const char		*tmp;
	int			i;
	int			ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!partialURI, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MAPIStoreURI, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!users, MAPI_E_INVALID_PARAMETER, NULL);
	
	mem_ctx = talloc_named(NULL, 0, "get_users_from_partial_uri");

	/* Search mapistoreURI given partial URI */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=%s)", partialURI);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*count = res->count;
	*MAPIStoreURI = talloc_array(parent_ctx, char *, *count);
	*users = talloc_array(parent_ctx, char *, *count);

	for (i = 0; i != *count; i++) {
		tmp = ldb_msg_find_attr_as_string(res->msgs[i], "MAPIStoreURI", NULL);
		*MAPIStoreURI[i] = talloc_strdup((TALLOC_CTX *)*MAPIStoreURI, tmp);

		/* Retrieve the system user name */
		mailboxDN = ldb_msg_find_attr_as_string(res->msgs[i], "mailboxDN", NULL);
		dn = ldb_dn_new(mem_ctx, ldb_ctx, mailboxDN);
		ret = ldb_search(ldb_ctx, mem_ctx, &mres, dn, LDB_SCOPE_SUBTREE, attrs, "(distinguishedName=%s)", mailboxDN);
		/* This should NEVER happen */
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
		tmp = ldb_msg_find_attr_as_string(mres->msgs[0], "cn", NULL);
		*users[i] = talloc_strdup((TALLOC_CTX *)*users, tmp);
		talloc_free(mres);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Create a folder in openchangedb

   \param ldb_ctx pointer to the openchangedb LDB context
   \param username the owner of the mailbox
   \param systemIdx the id of the mailbox
   \param fidp a pointer to the fid of the mailbox

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_create_mailbox(struct ldb_context *ldb_ctx, const char *username, int systemIdx, uint64_t *fidp)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct ldb_dn		*mailboxdn;
	struct ldb_message	*msg;
	NTTIME			now;
	uint64_t		fid, changeNum;
	struct GUID		guid;

	/* Sanity Checks */
	MAPI_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!username, MAPI_E_NOT_INITIALIZED, NULL);

	unix_to_nt_time(&now, time(NULL));

	mem_ctx = talloc_named(NULL, 0, "openchangedb_create_mailbox");

	openchangedb_get_new_folderID(ldb_ctx, &fid);
	openchangedb_get_new_changeNumber(ldb_ctx, &changeNum);

	/* Retrieve distinguesName for parent folder */

	mailboxdn = ldb_dn_copy(mem_ctx, ldb_get_default_basedn(ldb_ctx));
	MAPI_RETVAL_IF(!mailboxdn, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	ldb_dn_add_child_fmt(mailboxdn, "CN=%s", username);
	MAPI_RETVAL_IF(!ldb_dn_validate(mailboxdn), MAPI_E_BAD_VALUE, mem_ctx);
	
	msg = ldb_msg_new(mem_ctx);
	MAPI_RETVAL_IF(!msg, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	msg->dn = mailboxdn;
	ldb_msg_add_string(msg, "objectClass", "systemfolder");
	ldb_msg_add_string(msg, "objectClass", "container");
	ldb_msg_add_string(msg, "ReplicaID", "1");
	guid = GUID_random();
	ldb_msg_add_fmt(msg, "ReplicaGUID", "%s", GUID_string(mem_ctx, &guid));
	guid = GUID_random();
	ldb_msg_add_fmt(msg, "MailboxGUID", "%s", GUID_string(mem_ctx, &guid));
	ldb_msg_add_string(msg, "cn", username);
	/* FIXME: PidTagAccess and PidTagRights are user-specific */
	ldb_msg_add_string(msg, "PidTagAccess", "63");
	ldb_msg_add_string(msg, "PidTagRights", "2043");
	ldb_msg_add_fmt(msg, "PidTagDisplayName", "OpenChange Mailbox: %s", username);
	ldb_msg_add_fmt(msg, "PidTagCreationTime", "%"PRId64, now);
	ldb_msg_add_fmt(msg, "PidTagLastModificationTime", "%"PRId64, now);
	ldb_msg_add_string(msg, "PidTagSubFolders", "TRUE");
	ldb_msg_add_fmt(msg, "PidTagFolderId", "%"PRIu64, fid);
	ldb_msg_add_fmt(msg, "PidTagChangeNumber", "%"PRIu64, changeNum);
	ldb_msg_add_fmt(msg, "PidTagFolderType", "1");
	if (systemIdx > -1) {
		ldb_msg_add_fmt(msg, "SystemIdx", "%d", systemIdx);
	}
	ldb_msg_add_fmt(msg, "distinguishedName", "%s", ldb_dn_get_linearized(msg->dn));

	msg->elements[0].flags = LDB_FLAG_MOD_ADD;

	if (ldb_add(ldb_ctx, msg) != LDB_SUCCESS) {
		retval = MAPI_E_CALL_FAILED;
	}
	else {
		if (fidp) {
			*fidp = fid;
		}

		retval = MAPI_E_SUCCESS;
	}

	talloc_free(mem_ctx);

	return retval;
}

/**
   \details Create a folder in openchangedb

   \param ldb_ctx pointer to the openchangedb LDB context
   \param parentFolderID the FID of the parent folder
   \param fid the FID of the folder to create
   \param MAPIStoreURI the mapistore URI to associate to this folder
   \param nt_time the creation time of the folder
   \param changeNumber the change number

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_create_folder(struct ldb_context *ldb_ctx, uint64_t parentFolderID, uint64_t fid, uint64_t changeNumber, const char *MAPIStoreURI, int systemIdx)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	char			*dn;
	char			*mailboxDN;
	char			*parentDN;
	struct ldb_dn		*basedn;
	struct ldb_message	*msg;
	int			error;
	NTTIME			now;

	/* Sanity Checks */
	MAPI_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!parentFolderID, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!fid, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!changeNumber, MAPI_E_NOT_INITIALIZED, NULL);

	unix_to_nt_time(&now, time(NULL));

	mem_ctx = talloc_named(NULL, 0, "openchangedb_create_folder");

	/* Retrieve distinguesName for parent folder */
	retval = openchangedb_get_distinguishedName(mem_ctx, ldb_ctx, parentFolderID, &parentDN);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = openchangedb_get_mailboxDN(mem_ctx, ldb_ctx, parentFolderID, &mailboxDN);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	dn = talloc_asprintf(mem_ctx, "CN=%"PRIu64",%s", fid, parentDN);
	MAPI_RETVAL_IF(!dn, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	basedn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	talloc_free(dn);

	MAPI_RETVAL_IF(!ldb_dn_validate(basedn), MAPI_E_BAD_VALUE, mem_ctx);
	
	msg = ldb_msg_new(mem_ctx);
	MAPI_RETVAL_IF(!msg, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	msg->dn = ldb_dn_copy(mem_ctx, basedn);
	ldb_msg_add_string(msg, "objectClass", "systemfolder");
	ldb_msg_add_fmt(msg, "cn", "%"PRIu64, fid);
	ldb_msg_add_string(msg, "FolderType", "1");
	ldb_msg_add_string(msg, "PidTagContentUnreadCount", "0");
	ldb_msg_add_string(msg, "PidTagContentCount", "0");
	ldb_msg_add_string(msg, "PidTagAttributeHidden", "0");
	ldb_msg_add_string(msg, "PidTagAttributeSystem", "0");
	ldb_msg_add_string(msg, "PidTagAttributeReadOnly", "0");
	/* FIXME: PidTagAccess and PidTagRights are user-specific */
	ldb_msg_add_string(msg, "PidTagAccess", "63");
	ldb_msg_add_string(msg, "PidTagRights", "2043");
	ldb_msg_add_fmt(msg, "PidTagFolderType", "1");
	ldb_msg_add_fmt(msg, "PidTagCreationTime", "%"PRIu64, now);
	if (mailboxDN) {
		ldb_msg_add_string(msg, "mailboxDN", mailboxDN);
	}
	if (parentFolderID) {
		ldb_msg_add_fmt(msg, "PidTagParentFolderId", "%"PRIu64, parentFolderID);
	}
	ldb_msg_add_fmt(msg, "PidTagFolderId", "%"PRIu64, fid);
	ldb_msg_add_fmt(msg, "PidTagChangeNumber", "%"PRIu64, changeNumber);
	if (MAPIStoreURI) {
		ldb_msg_add_string(msg, "MAPIStoreURI", MAPIStoreURI);
	}
	if (systemIdx > -1) {
		ldb_msg_add_fmt(msg, "SystemIdx", "%d", systemIdx);
	}
	ldb_msg_add_fmt(msg, "distinguishedName", "%s", ldb_dn_get_linearized(msg->dn));

	msg->elements[0].flags = LDB_FLAG_MOD_ADD;

	error = ldb_add(ldb_ctx, msg);
	switch (error) {
	case 0:
		retval = MAPI_E_SUCCESS;
		break;
	case 68:
		retval = MAPI_E_COLLISION;
		break;
	default:
		retval = MAPI_E_CALL_FAILED;
	}

	talloc_free(mem_ctx);

	return retval;
}

/**
   \details Retrieve the number of messages within the specified folder

   \param ldb_ctx pointer to the openchange LDB context
   \param fid the folder identifier to use for the search
   \param RowCount pointer to the returned number of results

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_message_count(struct ldb_context *ldb_ctx, uint64_t fid, uint32_t *RowCount, bool fai)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	const char		*objectClass;
	int			ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!RowCount, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_message_count");
	*RowCount = 0;

	objectClass = (fai ? "faiMessage" : "systemMessage");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, 
			 "(&(objectClass=%s)(PidTagParentFolderId=%"PRIu64"))", objectClass, fid);
	printf("ldb error: %s\n", ldb_errstring(ldb_ctx));
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);
	
	*RowCount = res->count;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Retrieve the system idx associated with a folder record

   \param ldb_ctx pointer to the openchange LDB context
   \param fid the folder identifier to use for the search
   \param system_idx_p pointer to the returned value

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_system_idx(struct ldb_context *ldb_ctx, uint64_t fid, int *system_idx_p)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "SystemIdx", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	*system_idx_p = ldb_msg_find_attr_as_int(res->msgs[0], "SystemIdx", -1);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
