/*
   MAPI Proxy - OpenchangeDB backend LDB implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2014
   Copyright (C) Jesús García Sáez 2014

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

#include "openchangedb_ldb.h"

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "../libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

#define	OPENCHANGE_LDB_NAME "openchange.ldb"

extern struct ldb_val ldb_binary_decode(TALLOC_CTX *, const char *);


static enum MAPISTATUS get_SpecialFolderID(struct openchangedb_context *self,
					  const char *recipient, uint32_t system_idx,
					  int64_t *folder_id)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	const char			*dn;
	struct ldb_dn			*ldb_dn = NULL;
	struct ldb_context		*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_SpecialFolderID");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(mem_ctx, recipient));

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs,
			 "(&(objectClass=systemfolder)(SystemIdx=12))");

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	OPENCHANGE_RETVAL_IF(!dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	ldb_dn = ldb_dn_new(mem_ctx, ldb_ctx, dn);
	OPENCHANGE_RETVAL_IF(!ldb_dn, MAPI_E_CORRUPT_STORE, mem_ctx);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs,
				 "(&(objectClass=systemfolder)(SystemIdx=%d))", system_idx);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*folder_id = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
	OPENCHANGE_RETVAL_IF(!*folder_id, MAPI_E_CORRUPT_STORE, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_SystemFolderID(struct openchangedb_context *self,
					  const char *recipient, uint32_t SystemIdx,
					  int64_t *FolderId)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	const char			*dn;
	struct ldb_dn			*ldb_dn = NULL;
	struct ldb_context		*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_SystemFolderID");

	/* Step 1. Search Mailbox Root DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(mem_ctx, recipient));

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

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn, LDB_SCOPE_SUBTREE, attrs,
			 "(&(objectClass=systemfolder)(SystemIdx=%d))", SystemIdx);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*FolderId = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
	OPENCHANGE_RETVAL_IF(!*FolderId, MAPI_E_CORRUPT_STORE, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_PublicFolderID(struct openchangedb_context *self,
					  const char *username,
					  uint32_t SystemIdx,
					  int64_t *FolderId)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	struct ldb_context		*ldb_ctx = self->data;

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

static enum MAPISTATUS get_distinguishedName(TALLOC_CTX *parent_ctx,
					     struct openchangedb_context *self,
					     int64_t fid,
					     char **distinguishedName)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_distinguishedName");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*distinguishedName = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_mailboxDN(TALLOC_CTX *parent_ctx,
				     struct openchangedb_context *self,
				     int64_t fid,
				     char **mailboxDN)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_mailboxDN");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mailboxDN = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "mailboxDN", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_MailboxGuid(struct openchangedb_context *self,
				       const char *recipient,
				       struct GUID *MailboxGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	struct ldb_context		*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_MailboxGuid");

	/* Step 1. Search Mailbox DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(mem_ctx, recipient));
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Retrieve MailboxGUID attribute's value */
	guid = ldb_msg_find_attr_as_string(res->msgs[0], "MailboxGUID", NULL);
	OPENCHANGE_RETVAL_IF(!guid, MAPI_E_CORRUPT_STORE, mem_ctx);

	GUID_from_string(guid, MailboxGUID);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_MailboxReplica(struct openchangedb_context *self,
					  const char *recipient, uint16_t *ReplID,
				  	  struct GUID *ReplGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	struct ldb_context		*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_MailboxReplica");

	/* Step 1. Search Mailbox DN */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(mem_ctx, recipient));

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

static enum MAPISTATUS get_PublicFolderReplica(struct openchangedb_context *self,
					       const char *username,
					       uint16_t *ReplID,
					       struct GUID *ReplGUID)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	const char			*guid;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	struct ldb_context		*ldb_ctx = self->data;

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

static enum MAPISTATUS get_mapistoreURI(TALLOC_CTX *parent_ctx,
				        struct openchangedb_context *self,
				        const char *username,
				        int64_t fid, char **mapistoreURL,
				        bool mailboxstore)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	if (mailboxstore == true) {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	} else {
		DEBUG(1, ("Called get_mapistoreURI with mailboxstore=false!\n"));
		talloc_free(mem_ctx);
		return MAPI_E_NOT_IMPLEMENTED;
		//ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
		//		 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	}

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*mapistoreURL = talloc_strdup(parent_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "MAPIStoreURI", NULL));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS set_mapistoreURI(struct openchangedb_context *self,
					const char *username, int64_t fid,
					const char *mapistoreURL)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

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

static enum MAPISTATUS get_parent_fid(struct openchangedb_context *self,
				      const char *username, int64_t fid,
				      int64_t *parent_fidp, bool mailboxstore)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "PidTagParentFolderId", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_parent_fid");

	if (mailboxstore == true) {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	} else {
		ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
				 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	}
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*parent_fidp = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagParentFolderId", 0x0);
	OPENCHANGE_RETVAL_IF(*parent_fidp == 0x0, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_fid(struct openchangedb_context *self,
			       const char *mapistoreURL, int64_t *fidp)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	char			*slashLessURL;
	size_t			len;
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "openchangedb_ldb get_fid");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=%s)",
			 ldb_binary_encode_string(mem_ctx, mapistoreURL));
	if (ret != LDB_SUCCESS || !res->count) {
		len = strlen(mapistoreURL);
		if (mapistoreURL[len-1] == '/') {
			slashLessURL = talloc_strdup(mem_ctx, mapistoreURL);
			slashLessURL[len-1] = 0;
			ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx), LDB_SCOPE_SUBTREE,
					 attrs, "(MAPIStoreURI=%s)", ldb_binary_encode_string(mem_ctx, slashLessURL));
		}
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	}
	*fidp = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0x0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_MAPIStoreURIs(struct openchangedb_context *self,
					 const char *username,
					 TALLOC_CTX *mem_ctx,
					 struct StringArrayW_r **urisP)
{
	TALLOC_CTX		*local_mem_ctx;
	struct ldb_result	*res = NULL;
	struct ldb_dn		*dn;
	const char * const	attrs[] = { "*", NULL };
	char			*dnstr;
	int			i, elements, ret;
	struct StringArrayW_r	*uris;
	struct ldb_context	*ldb_ctx = self->data;

	local_mem_ctx = talloc_named(NULL, 0, "openchangedb_ldb get_MAPIStoreURIs");

	/* fetch mailbox DN */
	ret = ldb_search(ldb_ctx, local_mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(cn=%s)(MailboxGUID=*))",
			 ldb_binary_encode_string(mem_ctx, username));
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

static enum MAPISTATUS get_ReceiveFolder(TALLOC_CTX *parent_ctx,
					 struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass,
					 int64_t *fid,
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
	struct ldb_context		*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_ReceiveFolder");

	DEBUG(5, ("openchangedb_ldb get_ReceiveFolder, recipient: %s\n", recipient));
	DEBUG(5, ("openchangedb_ldb get_ReceiveFolder, MessageClass: %s\n", MessageClass));

	/* Step 1. Find mailbox DN for the recipient */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(mem_ctx, recipient));
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
	*fid = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0x0);
	*ExplicitMessageClass = "";
	DEBUG(5, ("openchangedb_ldb get_ReceiveFolder (All target), class: %s, fid: %.16"PRIx64"\n",
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
		DEBUG(6, ("openchangedb_ldb get_ReceiveFolder, checking fid: %.16"PRIx64"\n",
			  ldb_msg_find_attr_as_int64(res->msgs[j], "PidTagFolderId", 0x0)));
		for (i = 0, length = 0; i < ldb_element->num_values; i++) {
			DEBUG(6, ("openchangedb_ldb get_ReceiveFolder, element %i, data: %s\n", i, (char *)ldb_element->values[i].data));
			if (MessageClass &&
			    !strncasecmp(MessageClass, (char *)ldb_element->values[i].data, strlen((char *)ldb_element->values[i].data)) &&
			    strlen((char *)ldb_element->values[i].data) > length) {
				*fid = ldb_msg_find_attr_as_int64(res->msgs[j], "PidTagFolderId", 0x0);

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
	DEBUG(5, ("openchangedb_ldb get_ReceiveFolder, fid: %.16"PRIx64"\n", *fid));

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_TransportFolder(struct openchangedb_context *self,
					   const char *recipient,
					   int64_t *FolderId)
{
	return get_SystemFolderID(self, recipient, TRANSPORT_FOLDER_SYSTEM_IDX,
				  FolderId);
}

static enum MAPISTATUS get_folder_count(struct openchangedb_context *self,
					const char *username, int64_t fid,
					uint32_t *RowCount)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

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

static char *_unknown_property(TALLOC_CTX *mem_ctx, uint32_t proptag)
{
	return talloc_asprintf(mem_ctx, "Unknown%.8x", proptag);
}

static enum MAPISTATUS lookup_folder_property(struct openchangedb_context *self,
					      uint32_t proptag, int64_t fid)
{
	TALLOC_CTX	       	*mem_ctx;
	struct ldb_result      	*res = NULL;
	const char * const     	attrs[] = { "*", NULL };
	const char	       	*PidTagAttr = NULL;
	int		       	ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "lookup_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	if (!PidTagAttr) {
		PidTagAttr = _unknown_property(mem_ctx, proptag);
	}

	/* Step 3. Search for attribute */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static void *_get_special_property(TALLOC_CTX *mem_ctx, struct ldb_context *ldb_ctx,
				   struct ldb_result *res, uint32_t proptag,
				   const char *PidTagAttr)
{
	uint32_t *l;

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

			if (tmp[0] != 0 && strcmp(tmp, nil_string) != 0) {
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

static void *get_property_data_message(TALLOC_CTX *mem_ctx, struct ldb_message *msg,
				       uint32_t proptag, const char *PidTagAttr)
{
	void			*data;
	const char     		*str;
	int64_t			*ll;
	uint64_t		*ull;
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
		ll = talloc_zero(mem_ctx, int64_t);
		*ll = ldb_msg_find_attr_as_int64(msg, PidTagAttr, 0x0);
		data = (void *)ll;
		break;
	case PT_STRING8:
	case PT_UNICODE:
		str = ldb_msg_find_attr_as_string(msg, PidTagAttr, NULL);
		if (!str) return NULL;

		local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
		val = ldb_binary_decode(local_mem_ctx, str);
		if (val.length == 0) {
			talloc_free(local_mem_ctx);
			return NULL;
		}

		data = (char *) talloc_strndup(mem_ctx, (char *) val.data, val.length);
		talloc_free(local_mem_ctx);
		break;
	case PT_SYSTIME:
		ft = talloc_zero(mem_ctx, struct FILETIME);
		ull = talloc_zero(mem_ctx, uint64_t);
		*ull = ldb_msg_find_attr_as_uint64(msg, PidTagAttr, 0x0);
		ft->dwLowDateTime = (*ull & 0xffffffff);
		ft->dwHighDateTime = *ull >> 32;
		data = (void *)ft;
		talloc_free(ull);
		break;
	case PT_BINARY:
		str = ldb_msg_find_attr_as_string(msg, PidTagAttr, 0x0);
		bin = talloc_zero(mem_ctx, struct Binary_r);
		if (strcmp(str, nil_string) == 0) {
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
   \details Retrieve a MAPI property from a OpenChange LDB result set

   \param mem_ctx pointer to the memory context
   \param res pointer to the LDB results
   \param pos the LDB result index
   \param proptag the MAPI property tag to lookup
   \param PidTagAttr the mapped MAPI property name

   \return valid data pointer on success, otherwise NULL
 */
static void *get_property_data(TALLOC_CTX *mem_ctx, struct ldb_result *res,
			       uint32_t pos, uint32_t proptag,
			       const char *PidTagAttr)
{
	return get_property_data_message(mem_ctx, res->msgs[pos], proptag, PidTagAttr);
}

static enum MAPISTATUS get_ReceiveFolderTable(TALLOC_CTX *mem_ctx,
					      struct openchangedb_context *oc_ctx,
					      const char *recipient,
					      uint32_t *cValues,
					      struct ReceiveFolder **entries)
{
	TALLOC_CTX			*tmp_ctx;
	int				ret;
	struct ldb_context		*ldb_ctx;
	struct ldb_result		*res = NULL;
	struct ldb_message_element	*el;
	struct ldb_dn			*dn;
	const char * const		attrs[] = { "*", NULL };
	char				*dnstr = NULL;
	struct ReceiveFolder		*rcvfolders;
	int				i, j;
	int				idx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!oc_ctx->data, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!cValues, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!entries, MAPI_E_INVALID_PARAMETER, NULL);

	tmp_ctx = talloc_named(NULL, 0, "get_ReceiveFoldertable");
	OPENCHANGE_RETVAL_IF(!tmp_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	ldb_ctx = (struct ldb_context *) oc_ctx->data;

	/* Step 1. Find mailbox DN for the recipient */
	ret = ldb_search(ldb_ctx, tmp_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(tmp_ctx, recipient));
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, tmp_ctx);

	dnstr = talloc_strdup(tmp_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_ENOUGH_MEMORY, tmp_ctx);
	talloc_free(res);
	dn = ldb_dn_new(tmp_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	/* Step 2. Search for all MessageClasses within user's mailbox */
	ret = ldb_search(ldb_ctx, tmp_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs,
			 "(PidTagMessageClass=*)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, tmp_ctx);

	rcvfolders = talloc_array(tmp_ctx, struct ReceiveFolder, res->count + 1);
	OPENCHANGE_RETVAL_IF(!rcvfolders, MAPI_E_NOT_ENOUGH_MEMORY, tmp_ctx);

	/* Step 3. Export all entries */
	*cValues = 0;
	for (i = 0, idx = 0; i < res->count; i++) {
		el = ldb_msg_find_element(res->msgs[i], "PidTagMessageClass");
		rcvfolders = talloc_realloc(tmp_ctx, rcvfolders, struct ReceiveFolder, *cValues + el->num_values);

		for (j = 0; j < el->num_values; j++) {
			struct FILETIME	*ft;

			rcvfolders[idx].flag = 0;
			rcvfolders[idx].fid = ldb_msg_find_attr_as_int64(res->msgs[i], "PidTagFolderId", 0x0);

			ft = (struct FILETIME *) get_property_data(tmp_ctx, res, i,
								   PidTagLastModificationTime,
								   "PidTagLastModificationTime");
			rcvfolders[idx].modiftime.dwLowDateTime = ft->dwLowDateTime;
			rcvfolders[idx].modiftime.dwHighDateTime = ft->dwHighDateTime;
			talloc_free(ft);

			if (!strncmp("All", (char *)el->values[j].data,
				     strlen((char *)el->values[j].data))) {
				rcvfolders[idx].lpszMessageClass = "";
			} else {
				rcvfolders[idx].lpszMessageClass = talloc_strdup(rcvfolders,
										 (char *)el->values[j].data);
			}
			*cValues += 1;
			idx++;
		}
	}

	talloc_steal(mem_ctx, rcvfolders);
	*entries = rcvfolders;

	talloc_free(tmp_ctx);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_new_changeNumber(struct openchangedb_context *self, const char *username, int64_t *cn)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	struct ldb_context	*ldb_ctx = self->data;

	/* Get the current GlobalCount */
	mem_ctx = talloc_named(NULL, 0, "get_next_changeNumber");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*cn = ldb_msg_find_attr_as_int64(res->msgs[0], "ChangeNumber", 1);

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

static enum MAPISTATUS get_new_changeNumbers(struct openchangedb_context *self,
					     TALLOC_CTX *mem_ctx,
					     const char *username,
					     uint32_t max,
					     struct UI8Array_r **cns_p)
{
	TALLOC_CTX		*local_mem_ctx;
	int			ret;
	struct ldb_result	*res;
	struct ldb_message	*msg;
	const char * const	attrs[] = { "*", NULL };
	int64_t			cn;
	uint32_t		count;
	struct UI8Array_r	*cns;
	struct ldb_context	*ldb_ctx = self->data;

	/* Get the current GlobalCount */
	local_mem_ctx = talloc_named(NULL, 0, "get_new_changeNumber");
	ret = ldb_search(ldb_ctx, local_mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, local_mem_ctx);

	cn = ldb_msg_find_attr_as_int64(res->msgs[0], "ChangeNumber", 1);

	cns = talloc_zero(local_mem_ctx, struct UI8Array_r);
	cns->cValues = max;
	cns->lpui8 = talloc_array(cns, int64_t, max);

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

static enum MAPISTATUS get_next_changeNumber(struct openchangedb_context *self,
					     const char *username, int64_t *cn)
{
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct ldb_result	*res;
	const char * const	attrs[] = { "ChangeNumber", NULL };
	struct ldb_context	*ldb_ctx = self->data;

	/* Get the current GlobalCount */
	mem_ctx = talloc_named(NULL, 0, "get_next_changeNumber");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_root_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(objectClass=server)");
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	*cn = ldb_msg_find_attr_as_int64(res->msgs[0], "ChangeNumber", 1);

	talloc_free(mem_ctx);

	*cn = (exchange_globcnt(*cn) << 16) | 0x0001;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_folder_property(TALLOC_CTX *parent_ctx,
					   struct openchangedb_context *self,
					   const char *username,
					   uint32_t proptag, int64_t fid,
					   void **data)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	const char		*PidTagAttr = NULL;
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_folder_property");

	/* Step 1. Find PidTagFolderId record */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 2. Convert proptag into PidTag attribute */
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	if (!PidTagAttr) {
		PidTagAttr = _unknown_property(mem_ctx, proptag);
	}

	/* Step 3. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 4. Check if this is a "special property" */
	*data = _get_special_property(parent_ctx, ldb_ctx, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	/* Step 5. If this is not a "special property" */
	*data = get_property_data(parent_ctx, res, 0, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_NOT_FOUND;
}

static enum MAPISTATUS set_folder_properties(struct openchangedb_context *self,
					     const char *username, int64_t fid,
					     struct SRow *row)
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
	struct ldb_context	*ldb_ctx = self->data;

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
				PidTagAttr = _unknown_property(mem_ctx, value->ulPropTag);
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
	get_new_changeNumber(self, username, (int64_t *) &value->value.d);
	str_value = openchangedb_set_folder_property_data(mem_ctx, value);
	ldb_msg_add_string(msg, "PidTagChangeNumber", str_value);
	msg->elements[msg->num_elements-1].flags = LDB_FLAG_MOD_REPLACE;

	talloc_free(value);

	ret = ldb_modify(ldb_ctx, msg);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NO_SUPPORT, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_table_property(TALLOC_CTX *parent_ctx,
					  struct openchangedb_context *self,
					  const char *ldb_filter,
					  uint32_t proptag, uint32_t pos,
					  void **data)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	const char		*PidTagAttr = NULL;
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

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
		PidTagAttr = _unknown_property(mem_ctx, proptag);
	}

	/* Step 4. Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, mem_ctx);

	/* Step 5. Check if this is a "special property" */
	*data = _get_special_property(parent_ctx, ldb_ctx, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	/* Step 6. Check if this is not a "special property" */
	*data = get_property_data(parent_ctx, res, pos, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_NOT_FOUND;
}

static enum MAPISTATUS get_fid_by_name(struct openchangedb_context *self,
				       const char *username,
				       int64_t parent_fid,
				       const char* foldername, int64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_fid_by_name");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(PidTagParentFolderId=%"PRIu64")(PidTagDisplayName=%s))",
			 parent_fid, ldb_binary_encode_string(mem_ctx, foldername));

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	/* We should only ever get 0 records or 1 record, but there is always a chance
	   that things got confused at some point, so just return one of the records */
	OPENCHANGE_RETVAL_IF(res->count < 1, MAPI_E_NOT_FOUND, mem_ctx);

	*fid = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_mid_by_subject(struct openchangedb_context *self,
					  const char *username,
					  int64_t parent_fid,
					  const char *subject,
					  bool mailboxstore, int64_t *mid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	struct ldb_dn		*base_dn;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_mid_by_subject");

	if (mailboxstore) {
		base_dn = ldb_get_default_basedn(ldb_ctx);
	} else {
		base_dn = ldb_get_root_basedn(ldb_ctx);
	}

	ret = ldb_search(ldb_ctx, mem_ctx, &res, base_dn,
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(PidTagParentFolderId=%"PRIu64")(PidTagNormalizedSubject=%s))",
			 parent_fid, ldb_binary_encode_string(mem_ctx, subject));

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	/* We should only ever get 0 records or 1 record, but there is always a chance
	   that things got confused at some point, so just return one of the records */
	OPENCHANGE_RETVAL_IF(res->count < 1, MAPI_E_NOT_FOUND, mem_ctx);

	*mid = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagMessageId", 0);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS delete_folder(struct openchangedb_context *self,
				     const char *username, int64_t fid)
{
	TALLOC_CTX	*mem_ctx;
	char		*dnstr;
	struct ldb_dn	*dn;
	int		retval;
	enum MAPISTATUS	ret;
	struct ldb_context *ldb_ctx = (struct ldb_context *)self->data;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	ret = get_distinguishedName(mem_ctx, self, fid, &dnstr);
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

static enum MAPISTATUS set_ReceiveFolder(struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass, int64_t fid)
{
	TALLOC_CTX			*mem_ctx;
	struct ldb_result		*res = NULL;
	struct ldb_dn			*dn;
	char				*dnstr;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	struct ldb_context		*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "set_ReceiveFolder");

	DEBUG(5, ("openchangedb_ldb set_ReceiveFolder, recipient: %s\n", recipient));
	DEBUG(5, ("openchangedb_ldb set_ReceiveFolder, MessageClass: %s\n", MessageClass));
	DEBUG(5, ("openchangedb_ldb set_ReceiveFolder, fid: 0x%.16"PRIx64"\n", fid));

	/* Step 1. Find mailbox DN for the recipient */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "CN=%s", ldb_binary_encode_string(mem_ctx, recipient));
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);

	dnstr = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	DEBUG(5, ("openchangedb_ldb set_ReceiveFolder, dnstr: %s\n", dnstr));

	OPENCHANGE_RETVAL_IF(!dnstr, MAPI_E_NOT_FOUND, mem_ctx);

	talloc_free(res);

	dn = ldb_dn_new(mem_ctx, ldb_ctx, dnstr);
	talloc_free(dnstr);

	/* Step 2. Search for the MessageClass within user's mailbox */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_SUBTREE, attrs,
			 "(PidTagMessageClass=%s)", ldb_binary_encode_string(mem_ctx, MessageClass));
	DEBUG(5, ("openchangedb_ldb get_ReceiveFolder, res->count: %i\n", res->count));

	/* We should never have more than one record with a specific MessageClass */
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || (res->count > 1), MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 3. Delete the old entry if applicable */
	if (res->count) {
		/* we already have an entry for this message class, so delete it before creating the new one */
		char			*distinguishedName;
		struct ldb_message	*msg;

		int64_t folderid = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0x0);
		DEBUG(6, ("openchangedb_ldb set_ReceiveFolder, fid to delete from: 0x%.16"PRIx64"\n", folderid));

		get_distinguishedName(mem_ctx, self, folderid, &distinguishedName);
		DEBUG(6, ("openchangedb_ldb set_ReceiveFolder, dn to delete from: %s\n", distinguishedName));
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

		get_distinguishedName(mem_ctx, self, fid, &distinguishedName);
		DEBUG(6, ("openchangedb_ldb set_ReceiveFolder, dn to create in: %s\n", distinguishedName));

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

static enum MAPISTATUS get_fid_from_partial_uri(struct openchangedb_context *self,
						const char *partialURI, int64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_fid_from_partial_uri");

	/* Search mapistoreURI given partial URI */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(MAPIStoreURI=%s)",
			 ldb_binary_encode_string(mem_ctx, partialURI));

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	OPENCHANGE_RETVAL_IF(res->count > 1, MAPI_E_COLLISION, mem_ctx);

	*fid = ldb_msg_find_attr_as_int64(res->msgs[0], "PidTagFolderId", 0);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_users_from_partial_uri(TALLOC_CTX *parent_ctx,
						  struct openchangedb_context *self,
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
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_users_from_partial_uri");

	/* Search mapistoreURI given partial URI */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(MAPIStoreURI=%s)(mailboxDN=*))",
			 ldb_binary_encode_string(mem_ctx, partialURI));
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
		ret = ldb_search(ldb_ctx, mem_ctx, &mres, dn, LDB_SCOPE_SUBTREE, attrs,
				 "(distinguishedName=%s)", ldb_binary_encode_string(mem_ctx, mailboxDN));
		/* This should NEVER happen */
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
		tmp = ldb_msg_find_attr_as_string(mres->msgs[0], "cn", NULL);
		*users[i] = talloc_strdup((TALLOC_CTX *)*users, tmp);
		talloc_free(mres);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS create_mailbox(struct openchangedb_context *self,
				      const char *username,
				      const char *organization_name,
				      const char *groupo_name,
				      int systemIdx, int64_t fid,
				      const char *display_name)
{
	int			ret;
	TALLOC_CTX		*mem_ctx;
	struct ldb_dn		*mailboxdn;
	struct ldb_message	*msg;
	NTTIME			now;
	int64_t			changeNum;
	struct GUID		guid;
	struct ldb_context	*ldb_ctx = self->data;

	unix_to_nt_time(&now, time(NULL));

	mem_ctx = talloc_named(NULL, 0, "openchangedb_ldb create_mailbox");

	get_new_changeNumber(self, username, &changeNum);

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
	ldb_msg_add_string(msg, "PidTagDisplayName", display_name);
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

	ret = ldb_add(ldb_ctx, msg);
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_CALL_FAILED, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS create_folder(struct openchangedb_context *self,
				     const char *username,
				     int64_t parentFolderID, int64_t fid,
				     int64_t changeNumber,
				     const char *MAPIStoreURI, int systemIdx)
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
	struct ldb_context	*ldb_ctx = self->data;

	unix_to_nt_time(&now, time(NULL));

	mem_ctx = talloc_named(NULL, 0, "openchangedb_ldb create_folder");

	/* Retrieve distinguesName for parent folder */
	retval = get_distinguishedName(mem_ctx, self, parentFolderID, &parentDN);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = get_mailboxDN(mem_ctx, self, parentFolderID, &mailboxDN);
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

static enum MAPISTATUS get_message_count(struct openchangedb_context *self,
					 const char *username, int64_t fid,
					 uint32_t *RowCount, bool fai)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res;
	const char * const	attrs[] = { "*", NULL };
	const char		*objectClass;
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!RowCount, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "get_message_count");
	*RowCount = 0;

	objectClass = (fai ? "faiMessage" : "systemMessage");
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs,
			 "(&(objectClass=%s)(PidTagParentFolderId=%"PRIu64"))",
			 ldb_binary_encode_string(mem_ctx, objectClass), fid);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	*RowCount = res->count;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_system_idx(struct openchangedb_context *self,
				      const char *username, int64_t fid,
				      int *system_idx_p)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "SystemIdx", NULL };
	int			ret;
	struct ldb_context	*ldb_ctx = self->data;

	mem_ctx = talloc_named(NULL, 0, "get_mapistoreURI");

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(PidTagFolderId=%"PRIu64")", fid);

	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPI_E_NOT_FOUND, mem_ctx);
	*system_idx_p = ldb_msg_find_attr_as_int(res->msgs[0], "SystemIdx", -1);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS transaction_start(struct openchangedb_context *self)
{
	ldb_transaction_start(self->data);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS transaction_commit(struct openchangedb_context *self)
{
	ldb_transaction_commit(self->data);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS get_new_public_folderID(struct openchangedb_context *self,
					       const char *username,
					       int64_t *fid)
{
	DEBUG(0, ("get_new_public_folderID called on openchangedb ldb backend. "
		  "This should never happen because the folder ids are global"));
	return MAPI_E_NOT_IMPLEMENTED;
}

static bool is_public_folder_id(struct openchangedb_context *self, int64_t fid)
{
	return false;
}

static enum MAPISTATUS get_indexing_url(struct openchangedb_context *self, const char *username, const char **indexing_uri)
{
	return MAPI_E_NOT_IMPLEMENTED;
}

static bool set_locale(struct openchangedb_context *self, const char *username, uint32_t lcid)
{
	return true;
}

static const char **get_folders_names(TALLOC_CTX *mem_ctx, struct openchangedb_context *self, const char *locale, const char *type)
{
	return NULL;
}
// ^ openchangedb -------------------------------------------------------------

// v openchangedb table -------------------------------------------------------

struct openchangedb_table {
	int64_t				folderID;
	uint8_t				table_type;
	struct SSortOrderSet		*lpSortCriteria;
	struct mapi_SRestriction	*restrictions;
	struct ldb_result		*res;
};


static enum MAPISTATUS table_init(TALLOC_CTX *mem_ctx,
				  struct openchangedb_context *self,
				  const char *username,
				  uint8_t table_type, int64_t folderID,
				  void **table_object)
{
	struct openchangedb_table *table = talloc_zero(mem_ctx, struct openchangedb_table);

	if (!table) {
		return MAPI_E_NOT_ENOUGH_MEMORY;
	}
	/* printf("openchangedb_table_init: folderID=%"PRIu64"\n", folderID); */
	table->folderID = folderID;
	table->table_type = table_type;
	table->lpSortCriteria = NULL;
	table->restrictions = NULL;
	table->res = NULL;

	*table_object = (void *)table;

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS table_set_sort_order(struct openchangedb_context *self,
					    void *table_object,
					    struct SSortOrderSet *lpSortCriteria)
{
	struct openchangedb_table *table = (struct openchangedb_table *)table_object;

	if (table->res) {
		talloc_free(table->res);
		table->res = NULL;
	}

	if (table->lpSortCriteria) {
		talloc_free(table->lpSortCriteria);
	}
	if (lpSortCriteria) {
		table->lpSortCriteria = talloc_memdup((TALLOC_CTX *)table, lpSortCriteria, sizeof(struct SSortOrderSet));
		if (!table->lpSortCriteria) {
			return MAPI_E_NOT_ENOUGH_MEMORY;
		}
		table->lpSortCriteria->aSort = talloc_memdup((TALLOC_CTX *)table->lpSortCriteria, lpSortCriteria->aSort, lpSortCriteria->cSorts * sizeof(struct SSortOrder));
		if (!table->lpSortCriteria->aSort) {
			return MAPI_E_NOT_ENOUGH_MEMORY;
		}
	} else {
		table->lpSortCriteria = NULL;
	}

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS table_set_restrictions(struct openchangedb_context *self,
					      void *table_object,
					      struct mapi_SRestriction *res)
{
	struct openchangedb_table *table = (struct openchangedb_table *)table_object;

	if (table->res) {
		talloc_free(table->res);
		table->res = NULL;
	}

	if (table->restrictions) {
		talloc_free(table->restrictions);
		table->restrictions = NULL;
	}

	table->restrictions = talloc_zero((TALLOC_CTX *)table_object, struct mapi_SRestriction);

	switch (res->rt) {
	case RES_PROPERTY:
		table->restrictions->rt = res->rt;
		table->restrictions->res.resProperty.relop = res->res.resProperty.relop;
		table->restrictions->res.resProperty.ulPropTag = res->res.resProperty.ulPropTag;
		table->restrictions->res.resProperty.lpProp.ulPropTag = res->res.resProperty.lpProp.ulPropTag;

		switch (table->restrictions->res.resProperty.lpProp.ulPropTag & 0xFFFF) {
		case PT_STRING8:
			table->restrictions->res.resProperty.lpProp.value.lpszA = talloc_strdup((TALLOC_CTX *)table->restrictions, res->res.resProperty.lpProp.value.lpszA);
			break;
		case PT_UNICODE:
			table->restrictions->res.resProperty.lpProp.value.lpszW = talloc_strdup((TALLOC_CTX *)table->restrictions, res->res.resProperty.lpProp.value.lpszW);
			break;
		default:
			DEBUG(0, ("Unsupported property type for RES_PROPERTY restriction\n"));
			break;
		}
		break;
	default:
		DEBUG(0, ("Unsupported restriction type: 0x%x\n", res->rt));
	}

	return MAPI_E_SUCCESS;
}

static char *_table_build_filter(TALLOC_CTX *mem_ctx, struct openchangedb_table *table,
				 int64_t row_fmid, struct mapi_SRestriction *restrictions)
{
	char		*filter = NULL;
	const char	*PidTagAttr = NULL;

	switch (table->table_type) {
	case 0x3 /* EMSMDBP_TABLE_FAI_TYPE */:
		filter = talloc_asprintf(mem_ctx, "(&(objectClass=faiMessage)(PidTagParentFolderId=%"PRIu64")(PidTagMessageId=", table->folderID);
		break;
	case 0x2 /* EMSMDBP_TABLE_MESSAGE_TYPE */:
		filter = talloc_asprintf(mem_ctx, "(&(objectClass=systemMessage)(PidTagParentFolderId=%"PRIu64")(PidTagMessageId=", table->folderID);
		break;
	case 0x1 /* EMSMDBP_TABLE_FOLDER_TYPE */:
		filter = talloc_asprintf(mem_ctx, "(&(PidTagParentFolderId=%"PRIu64")(PidTagFolderId=", table->folderID);
		break;
	}

	if (row_fmid == 0) {
		filter = talloc_asprintf_append(filter, "*)");
	}
	else {
		filter = talloc_asprintf_append(filter, "%"PRIu64")", row_fmid);
	}

	if (restrictions) {
		switch (restrictions->rt) {
		case RES_PROPERTY:
			/* Retrieve PidTagName */
			PidTagAttr = openchangedb_property_get_attribute(restrictions->res.resProperty.ulPropTag);
			if (!PidTagAttr) {
				talloc_free(filter);
				return NULL;
			}
			filter = talloc_asprintf_append(filter, "(%s=", PidTagAttr);
			switch (restrictions->res.resProperty.ulPropTag & 0xFFFF) {
			case PT_STRING8:
				filter = talloc_asprintf_append(filter, "%s)", restrictions->res.resProperty.lpProp.value.lpszA);
				break;
			case PT_UNICODE:
				filter = talloc_asprintf_append(filter, "%s)", restrictions->res.resProperty.lpProp.value.lpszW);
				break;
			default:
				DEBUG(0, ("Unsupported RES_PROPERTY property type: 0x%.4x\n", (restrictions->res.resProperty.ulPropTag & 0xFFFF)));
				talloc_free(filter);
				return NULL;
			}
		}
	}

	/* Close filter */
	filter = talloc_asprintf_append(filter, ")");

	return filter;
}

static enum MAPISTATUS table_get_property(TALLOC_CTX *mem_ctx,
					  struct openchangedb_context *self,
					  void *table_object,
					  enum MAPITAGS proptag, uint32_t pos,
					  bool live_filtered, void **data)
{
	struct openchangedb_table	*table = (struct openchangedb_table *)table_object;
	char				*ldb_filter = NULL;
	struct ldb_result		*res = NULL, *live_res = NULL;
	const char * const		attrs[] = { "*", NULL };
	const char			*PidTagAttr = NULL, *childIdAttr;
	int64_t				*row_fmid;
	int				ret;
	struct ldb_context 		*ldb_ctx = self->data;

	/* Fetch results */
	if (!table->res) {
		/* Build ldb filter */
		if (live_filtered) {
			ldb_filter = _table_build_filter(NULL, table, 0, NULL);
			DEBUG(5, ("(live-filtered) ldb_filter = %s\n", ldb_filter));
		}
		else {
			ldb_filter = _table_build_filter(NULL, table, 0, table->restrictions);
			DEBUG(5, ("(pre-filtered) ldb_filter = %s\n", ldb_filter));
		}
		OPENCHANGE_RETVAL_IF(!ldb_filter, MAPI_E_TOO_COMPLEX, NULL);
		ret = ldb_search(ldb_ctx, (TALLOC_CTX *)table_object, &table->res, ldb_get_default_basedn(ldb_ctx), LDB_SCOPE_SUBTREE, attrs, ldb_filter, NULL);
		talloc_free(ldb_filter);
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_INVALID_OBJECT, NULL);
	}
	res = table->res;

	/* Ensure position is within search results range */
	OPENCHANGE_RETVAL_IF(pos >= res->count, MAPI_E_INVALID_OBJECT, NULL);

	/* If live filtering, make sure the specified row match the restrictions */
	if (live_filtered) {
		TALLOC_CTX *local_mem_ctx;

		switch (table->table_type) {
		case 0x3 /* EMSMDBP_TABLE_FAI_TYPE */:
		case 0x2 /* EMSMDBP_TABLE_MESSAGE_TYPE */:
			childIdAttr = "PidTagMessageId";
			break;
		case 0x1 /* EMSMDBP_TABLE_FOLDER_TYPE */:
			childIdAttr = "PidTagFolderId";
			break;
		default:
			DEBUG(5, ("unsupported table type for openchangedb: %d\n", table->table_type));
			abort();
		}
		row_fmid = get_property_data(mem_ctx, res, pos, PR_MID, childIdAttr);
		if (!row_fmid || !*row_fmid) {
			DEBUG(5, ("ldb object must have a '%s' field\n", childIdAttr));
			abort();
		}

		local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);

		ldb_filter = _table_build_filter(local_mem_ctx, table, *row_fmid, table->restrictions);
		OPENCHANGE_RETVAL_IF(!ldb_filter, MAPI_E_TOO_COMPLEX, NULL);
		DEBUG(5, ("  row ldb_filter = %s\n", ldb_filter));
		ret = ldb_search(ldb_ctx, local_mem_ctx, &live_res, ldb_get_default_basedn(ldb_ctx), LDB_SCOPE_SUBTREE, attrs, ldb_filter, NULL);
		OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || live_res->count == 0, MAPI_E_INVALID_OBJECT, local_mem_ctx);
		talloc_free(local_mem_ctx);
	}

	/* hacks for some attributes specific to tables */
	if (proptag == PR_INST_ID) {
		if (table->table_type == 1) {
			proptag = PR_FID;
		}
		else {
			proptag = PR_MID;
		}
	}
	else if (proptag == PR_INSTANCE_NUM) {
		*data = talloc_zero(mem_ctx, uint32_t);
		return MAPI_E_SUCCESS;
	}

	/* Convert proptag into PidTag attribute */
	if ((table->table_type != 0x1) && proptag == PR_FID) {
		proptag = PR_PARENT_FID;
	}
	PidTagAttr = openchangedb_property_get_attribute(proptag);
	OPENCHANGE_RETVAL_IF(!PidTagAttr, MAPI_E_NOT_FOUND, NULL);

	/* Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(res->msgs[pos], PidTagAttr), MAPI_E_NOT_FOUND, NULL);

	/* Check if this is a "special property" */
	*data = _get_special_property(mem_ctx, ldb_ctx, res, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, NULL);

	/* Check if this is NOT a "special property" */
	*data = get_property_data(mem_ctx, res, pos, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, NULL);

	return MAPI_E_NOT_FOUND;
}

// ^ openchangedb table -------------------------------------------------------

// v openchangedb message -----------------------------------------------------

enum openchangedb_message_status {
	OPENCHANGEDB_MESSAGE_CREATE	= 0x1,
	OPENCHANGEDB_MESSAGE_OPEN	= 0x2
};

struct openchangedb_message {
	enum openchangedb_message_status	status;
	int64_t					messageID;
	int64_t					folderID;
	struct ldb_context			*ldb_ctx;
	struct ldb_message			*msg;
	struct ldb_result			*res;
};

static enum MAPISTATUS message_create(TALLOC_CTX *mem_ctx,
				      struct openchangedb_context *self,
				      const char *username,
				      int64_t messageID, int64_t folderID,
				      bool fai, void **message_object)
{
	enum MAPISTATUS			retval;
	struct openchangedb_message	*msg;
	struct ldb_dn			*basedn;
	char				*dn;
	char				*parentDN;
	char				*mailboxDN;
	int				i;
	struct ldb_context		*ldb_ctx = self->data;

	/* Retrieve distinguishedName of parent folder */
	retval = get_distinguishedName(mem_ctx, self, folderID, &parentDN);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Retrieve mailboxDN of parent folder */
	retval = get_mailboxDN(mem_ctx, self, folderID, &mailboxDN);
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

static enum MAPISTATUS message_save(struct openchangedb_context *self,
				    void *_msg, uint8_t SaveFlags)
{
	struct openchangedb_message *msg = (struct openchangedb_message *)_msg;
	struct ldb_message *message_to_save;
	struct ldb_message_element *el;
	int i;
	TALLOC_CTX *mem_ctx;

	switch (msg->status) {
	case OPENCHANGEDB_MESSAGE_CREATE:
		OPENCHANGE_RETVAL_IF(!msg->msg, MAPI_E_NOT_INITIALIZED, NULL);
		if (ldb_add(msg->ldb_ctx, msg->msg) != LDB_SUCCESS) {
			printf("Create: %s\n", ldb_errstring(msg->ldb_ctx));
			return MAPI_E_CALL_FAILED;
		}
		break;
	case OPENCHANGEDB_MESSAGE_OPEN:
		mem_ctx = talloc_named(NULL, 0, "message_save");
		el = msg->res->msgs[0]->elements;
		message_to_save = ldb_msg_new(mem_ctx);
		message_to_save->dn = ldb_dn_copy(mem_ctx, msg->res->msgs[0]->dn);
		for (i = 0; i < msg->res->msgs[0]->num_elements; i++) {
			if (el[i].flags) {
				ldb_msg_add(message_to_save, &el[i], el[i].flags);
			}
		}
		if (ldb_modify(msg->ldb_ctx, message_to_save) != LDB_SUCCESS) {
			printf("Modify: %s\n", ldb_errstring(msg->ldb_ctx));
			talloc_free(mem_ctx);
			return MAPI_E_CALL_FAILED;
		}
		talloc_free(mem_ctx);
		break;
	}

	/* FIXME: Deal with SaveFlags */

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS message_open(TALLOC_CTX *mem_ctx,
				    struct openchangedb_context *self,
				    const char *username,
				    int64_t messageID, int64_t folderID,
				    void **message_object, void **msgp)
{
	struct mapistore_message	*mmsg;
	struct openchangedb_message	*msg;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	char				*ldb_filter;
	struct ldb_context		*ldb_ctx = self->data;

	msg = talloc_zero(mem_ctx, struct openchangedb_message);
	if (!msg) {
		return MAPI_E_NOT_ENOUGH_MEMORY;
	}
	DEBUG(5, ("openchangedb_ldb message_open: folderID=%"PRIu64" messageID=%"PRIu64"\n", folderID, messageID));

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
	DEBUG(5, ("We have found: %d messages for ldb_filter = %s\n", msg->res->count, ldb_filter));
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

static enum MAPISTATUS message_get_property(TALLOC_CTX *mem_ctx,
					    struct openchangedb_context *self,
					    void *message_object,
					    uint32_t proptag, void **data)
{
	struct openchangedb_message	*msg = (struct openchangedb_message *)message_object;
	struct ldb_message		*message;
	char				*PidTagAttr = NULL;
	bool				tofree = false;

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
	*data = get_property_data_message(mem_ctx, message, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, (tofree == true) ? PidTagAttr : NULL);

	if (tofree == true) {
		talloc_free(PidTagAttr);
	}

	return MAPI_E_NOT_FOUND;
}

static enum MAPISTATUS message_set_properties(TALLOC_CTX *mem_ctx,
					      struct openchangedb_context *self,
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

	DEBUG(5, ("openchangedb_ldb message_set_properties\n"));

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

	DEBUG(5, ("openchangedb_ldb message_set_properties end\n"));
	return MAPI_E_SUCCESS;
}

// ^ openchangedb message -----------------------------------------------------

_PUBLIC_ enum MAPISTATUS openchangedb_ldb_initialize(TALLOC_CTX *mem_ctx,
						     const char *private_dir,
						     struct openchangedb_context **ctx)
{
	struct openchangedb_context 	*oc_ctx = talloc_zero(mem_ctx, struct openchangedb_context);
	char				*ldb_path;
	struct tevent_context		*ev;
	int				ret;
	struct ldb_result		*res;
	struct ldb_dn			*tmp_dn = NULL;
	static const char		*attrs[] = {
		"rootDomainNamingContext",
		"defaultNamingContext",
		NULL
	};
	struct ldb_context		*ldb_ctx;

	// Connect to ldb
	ev = tevent_context_init(talloc_autofree_context());
	OPENCHANGE_RETVAL_IF(!ev, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	/* Step 0. Retrieve a LDB context pointer on openchange.ldb database */
	ldb_path = talloc_asprintf(mem_ctx, "%s/%s", private_dir, OPENCHANGE_LDB_NAME);
	ldb_ctx = ldb_init(mem_ctx, ev);
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_ENOUGH_MEMORY, oc_ctx);

	/* Step 1. Connect to the database */
	ret = ldb_connect(ldb_ctx, ldb_path, 0, NULL);
	talloc_free(ldb_path);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_INITIALIZED, ldb_ctx);

	/* Step 2. Search for the rootDSE record */
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_dn_new(mem_ctx, ldb_ctx, "@ROOTDSE"),
			  LDB_SCOPE_BASE, attrs, NULL);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_INITIALIZED, ldb_ctx);
	OPENCHANGE_RETVAL_IF(res->count != 1, MAPI_E_NOT_INITIALIZED, ldb_ctx);

	/* Step 3. Set opaque naming */
	tmp_dn = ldb_msg_find_attr_as_dn(ldb_ctx, ldb_ctx, res->msgs[0],
					 "rootDomainNamingContext");
	ldb_set_opaque(ldb_ctx, "rootDomainNamingContext", tmp_dn);

	tmp_dn = ldb_msg_find_attr_as_dn(ldb_ctx, ldb_ctx, res->msgs[0],
					 "defaultNamingContext");
	ldb_set_opaque(ldb_ctx, "defaultNamingContext", tmp_dn);

	oc_ctx->data = ldb_ctx;

	// Initialize struct with function pointers
	oc_ctx->backend_type = talloc_strdup(mem_ctx, "ldb");

	oc_ctx->get_new_changeNumber = get_new_changeNumber;
	oc_ctx->get_new_changeNumbers = get_new_changeNumbers;
	oc_ctx->get_next_changeNumber = get_next_changeNumber;
	oc_ctx->get_SystemFolderID = get_SystemFolderID;
	oc_ctx->get_SpecialFolderID = get_SpecialFolderID;
	oc_ctx->get_PublicFolderID = get_PublicFolderID;
	oc_ctx->get_distinguishedName = get_distinguishedName;
	oc_ctx->get_MailboxGuid = get_MailboxGuid;
	oc_ctx->get_MailboxReplica = get_MailboxReplica;
	oc_ctx->get_PublicFolderReplica = get_PublicFolderReplica;
	oc_ctx->get_parent_fid = get_parent_fid;
	oc_ctx->get_MAPIStoreURIs = get_MAPIStoreURIs;
	oc_ctx->get_mapistoreURI = get_mapistoreURI;
	oc_ctx->set_mapistoreURI = set_mapistoreURI;
	oc_ctx->get_fid = get_fid;
	oc_ctx->get_ReceiveFolder = get_ReceiveFolder;
	oc_ctx->get_ReceiveFolderTable = get_ReceiveFolderTable;
	oc_ctx->get_TransportFolder = get_TransportFolder;
	oc_ctx->lookup_folder_property = lookup_folder_property;
	oc_ctx->set_folder_properties = set_folder_properties;
	oc_ctx->get_folder_property = get_folder_property;
	oc_ctx->get_folder_count = get_folder_count;
	oc_ctx->get_message_count = get_message_count;
	oc_ctx->get_system_idx = get_system_idx;
	oc_ctx->get_table_property = get_table_property;
	oc_ctx->get_fid_by_name = get_fid_by_name;
	oc_ctx->get_mid_by_subject = get_mid_by_subject;
	oc_ctx->set_ReceiveFolder = set_ReceiveFolder;
	oc_ctx->create_mailbox = create_mailbox;
	oc_ctx->create_folder = create_folder;
	oc_ctx->delete_folder = delete_folder;
	oc_ctx->get_fid_from_partial_uri = get_fid_from_partial_uri;
	oc_ctx->get_users_from_partial_uri = get_users_from_partial_uri;

	oc_ctx->table_init = table_init;
	oc_ctx->table_set_sort_order = table_set_sort_order;
	oc_ctx->table_set_restrictions = table_set_restrictions;
	oc_ctx->table_get_property = table_get_property;

	oc_ctx->message_create = message_create;
	oc_ctx->message_save = message_save;
	oc_ctx->message_open = message_open;
	oc_ctx->message_get_property = message_get_property;
	oc_ctx->message_set_properties = message_set_properties;

	oc_ctx->transaction_start = transaction_start;
	oc_ctx->transaction_commit = transaction_commit;

	oc_ctx->get_new_public_folderID = get_new_public_folderID;
	oc_ctx->is_public_folder_id = is_public_folder_id;

	oc_ctx->get_indexing_url = get_indexing_url;
	oc_ctx->set_locale = set_locale;
	oc_ctx->get_folders_names = get_folders_names;

	*ctx = oc_ctx;

	return MAPI_E_SUCCESS;
}
