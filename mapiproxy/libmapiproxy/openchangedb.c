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
	char				*ldb_filter;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	const char			*dn;
	struct ldb_dn			*ldb_dn = NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
	
	mem_ctx = talloc_named(NULL, 0, "get_SystemFolderID");

	/* Step 1. Search Mailbox DN */
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 2. Search FolderID */
	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn, MAPI_E_CORRUPT_STORE, mem_ctx);
	talloc_free(res);

	ldb_filter = talloc_asprintf(mem_ctx, "(&(objectClass=systemfolder)(SystemIdx=%d))", SystemIdx);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "fid", 0);
	OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, NULL);

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
	char				*ldb_filter;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MailboxGUID, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_MailboxGuid");

	/* Step 1. Search Mailbox DN */
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

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
	char				*ldb_filter;
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
	ldb_filter = talloc_asprintf(mem_ctx, "CN=%s", recipient);
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

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


_PUBLIC_ enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *parent_ctx,
						       void *ldb_ctx,
						       uint64_t fid,
						       char **mapistoreURL)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	ldb_filter = talloc_asprintf(mem_ctx, "CN=0x%.16llx", fid);
	DEBUG(0, ("ldb_filter = '%s'\n", ldb_filter));
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter);
	talloc_free(ldb_filter);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mapistoreURL = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "mapistore_uri", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
