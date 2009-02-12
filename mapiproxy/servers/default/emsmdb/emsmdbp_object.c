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
   \file emsmdbp_object.c

   \brief Server-side specific objects init/release routines
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details Initialize a mailbox object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the Logon MAPI request

   \return Allocated emsmdbp mailbox object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object_mailbox *emsmdbp_object_mailbox_init(TALLOC_CTX *mem_ctx,
								    struct emsmdbp_context *emsmdbp_ctx,
								    struct EcDoRpc_MAPI_REQ *request)
{
	struct emsmdbp_object_mailbox	*object;
	const char			*displayName;
	char				*ldb_filter;
	const char * const		recipient_attrs[] = { "*", NULL };
	int				ret;
	struct ldb_result		*res = NULL;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!request) return NULL;

	object = talloc_zero(mem_ctx, struct emsmdbp_object_mailbox);
	if (!object) return NULL;

	object->owner_EssDN = talloc_strdup(object, request->u.mapi_Logon.EssDN);
	object->szUserDN = talloc_strdup(object, emsmdbp_ctx->szUserDN);

	ldb_filter = talloc_asprintf(mem_ctx, "(legacyExchangeDN=%s)", object->owner_EssDN);
	ret = ldb_search(emsmdbp_ctx->users_ctx, mem_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->users_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);

	if (res->count == 1) {
		displayName = ldb_msg_find_attr_as_string(res->msgs[0], "displayName", NULL);
		if (displayName) {
			object->owner_Name = talloc_strdup(object, displayName);
		} else {
			object->owner_Name = NULL;
		}
	}

	talloc_free(res);

	return object;
}


/**
   \details Initialize a folder object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the OpenFolder MAPI request
   \param parent pointer to the parent MAPI handle

   \return Allocated emsmdbp folder object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object_folder *emsmdbp_object_folder_init(TALLOC_CTX *mem_ctx,
								  struct emsmdbp_context *emsmdbp_ctx,
								  struct EcDoRpc_MAPI_REQ *request,
								  struct mapi_handles *parent)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object_folder	*object;
	int				mailboxfolder;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!request) return NULL;

	object = talloc_zero(mem_ctx, struct emsmdbp_object_folder);
	if (!object) return NULL;

	object->folderID = request->u.mapi_OpenFolder.folder_id;
	
	retval = mapi_handles_get_systemfolder(parent, &mailboxfolder);
	object->IsSystemFolder = (!mailboxfolder) ? true : false;

	if (object->IsSystemFolder == false) {
		/* Retrieve the systemfolder value */
		object->systemfolder = 1;
		/* mapistore backend initialization goes here */
	} else {
		/* assign mapistore context from parent */
		object->systemfolder = -1;
	}

	return object;
}
