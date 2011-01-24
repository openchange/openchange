/*
   OpenChange Storage Abstraction Layer library
   MAPIStore database backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2011
   Copyright (C) Brad Hards 2010-2011

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

#include "mapistore_mstoredb.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"

/**
   \details Initialize mstoredb mapistore backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR mstoredb_init(void)
{
	DEBUG(0, ("* [%s:%d][%s]: mstoredb backend initialized\n", __FILE__, __LINE__, __FUNCTION__));
	return MAPISTORE_SUCCESS;
}

/**
   \details Generate a mapistore URI for root (system/special) folders

   \param mem_ctx pointer to the memory context
   \param index the folder index for which to create the mapistore URI
   \param username the username for which to create the mapistore URI
   \param mapistore_uri pointer on pointer to the string to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR mstoredb_create_mapistore_uri(TALLOC_CTX *mem_ctx,
							  enum MAPISTORE_DFLT_FOLDERS index,
							  const char *username,
							  char **mapistore_uri)
{
	const char	*firstorgdn;
	int		i;

	/* Sanity checks */
	if (!username || !mapistore_uri) {
		DEBUG(5, ("! [%s:%d][%s]: Invalid parameter\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	firstorgdn = mapistore_get_firstorgdn();
	if (!firstorgdn) {
		DEBUG(5, ("! [%s:%d][%s]: Invalid firstorgdn\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	for (i = 0; dflt_folders[i].name; i++) {
		if (dflt_folders[i].index == index) {
			*mapistore_uri = talloc_asprintf(mem_ctx, "mstoredb://%s,CN=%s,%s", dflt_folders[i].name, username, firstorgdn);
			DEBUG(5, ("* [%s:%d][%s]: URI = %s\n", __FILE__, __LINE__, __FUNCTION__, *mapistore_uri));
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}

/**
   \details Create a conneciton context to the mstoredb backend 

   \param ctx pointer to the opaque mapistore backend context
   \param uri pointer to the mstoredb DN to open
   \param private_data pointer to the private backend context to return
 */
static enum MAPISTORE_ERROR mstoredb_create_context(struct mapistore_backend_context *ctx,
						    const char *uri,
						    void **private_data)
{
	TALLOC_CTX		*mem_ctx;
	struct mstoredb_context	*mstoredb_ctx;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };
	int			ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!private_data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	DEBUG(0, ("* [%s:%d][%s]: uri = %s\n", __FILE__, __LINE__, __FUNCTION__, uri));

	mem_ctx = (TALLOC_CTX *) ctx;
	
	/* Step 1. Initialize mstoredb context */
	mstoredb_ctx = talloc_zero(mem_ctx, struct mstoredb_context);
	mstoredb_ctx->context_dn = talloc_strdup(mstoredb_ctx, uri);
	mstoredb_ctx->mdb_ctx = ctx;

	/* Step 2. Retrieve path to the mapistore database */
	mstoredb_ctx->dbpath = mapistore_get_database_path();
	DEBUG(5, ("* [%s:%d][%s]: database path = %s\n", __FILE__, __LINE__, __FUNCTION__, mstoredb_ctx->dbpath));

	/* Step 3. Open a wrapped connection to mapistore.ldb */
	mstoredb_ctx->ldb_ctx = mapistore_public_ldb_connect(mstoredb_ctx->mdb_ctx, mstoredb_ctx->dbpath);
	if (!mstoredb_ctx->ldb_ctx) {
		DEBUG(5, ("! [%s:%d][%s]: Unable to open mapistore.ldb\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(mstoredb_ctx);
		return MAPISTORE_ERR_DATABASE_INIT;
	}

	/* Step 4. Check if uri (DN) is correct */
	ret = ldb_search((struct ldb_context *)mstoredb_ctx->ldb_ctx, mstoredb_ctx, &res,
			 ldb_get_default_basedn((struct ldb_context *)mstoredb_ctx->ldb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "(dn=%s)", uri);
	if (ret != LDB_SUCCESS || !res || res->count != 1) {
		talloc_free(mstoredb_ctx);
		return MAPISTORE_ERROR;
	}

	/* Step 5. Use the FID as the folder identifier for this context */
	mstoredb_ctx->context_fid = ldb_msg_find_attr_as_uint64(res->msgs[0], "PidTagFolderId", 0);
	DEBUG(5, ("* [%s:%d][%s]: Root folder identifier for this context is 0x%.16"PRIx64"\n",
		  __FILE__, __LINE__, __FUNCTION__, mstoredb_ctx->context_fid));

	mstoredb_ctx->basedn = ldb_dn_new(mstoredb_ctx, mstoredb_ctx->ldb_ctx, uri);
	if (!mstoredb_ctx->basedn) {
		DEBUG(5, ("! [%s:%d][%s]: Unable to create DN from URI\n",
			  __FILE__, __LINE__, __FUNCTION__));
		talloc_free(mstoredb_ctx);
		return MAPISTORE_ERROR;
	}

	*private_data = (void *)mstoredb_ctx;

	return MAPISTORE_SUCCESS;
}

/**
   \details Delete a connection context from the mstoredb backend

   \param private_data pointer to the current mstoredb context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR mstoredb_delete_context(void *private_data)
{
	DEBUG(5, ("* [%s:%d][%s]\n", __FILE__, __LINE__, __FUNCTION__));
	return MAPISTORE_SUCCESS;
}

/**
   \details Entry point for mapistore MSTOREDB backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_init_backend(void)
{
	struct mapistore_backend	backend;
	enum MAPISTORE_ERROR		retval;

	/* Fill in our name */
	backend.name = "mstoredb";
	backend.description = "mapistore database backend";
	backend.uri_namespace = "mstoredb://";
	
	/* Fill in all the operations */
	backend.init = mstoredb_init;
	backend.create_context = mstoredb_create_context;
	backend.delete_context = mstoredb_delete_context;
	backend.create_uri = mstoredb_create_mapistore_uri;

	/* Register ourselves with the MAPISTORE subsystem */
	retval = mapistore_backend_register(&backend);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(5, ("Failed to register the '%s' mapistore backend!\n", backend.name));
	}

	return retval;
}
