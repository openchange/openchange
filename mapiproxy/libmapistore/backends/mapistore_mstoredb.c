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
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_INFO, MSTORE_SINGLE_MSG, "mstoredb backend initialized\n");
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
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_PEDANTIC, MSTORE_SINGLE_MSG, "Invalid parameter\n");
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	firstorgdn = mapistore_get_firstorgdn();
	if (!firstorgdn) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_PEDANTIC, MSTORE_SINGLE_MSG, "Invalid firstorgdn\n");
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	for (i = 0; dflt_folders[i].name; i++) {
		if (dflt_folders[i].index == index) {
			*mapistore_uri = talloc_asprintf(mem_ctx, "mstoredb://%s,CN=%s,%s", dflt_folders[i].name, username, firstorgdn);
			MSTORE_DEBUG_SUCCESS(MSTORE_LEVEL_DEBUG, "URI = %s\n", *mapistore_uri);
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}

/**
   \details Create a connection context to the mstoredb backend 

   \param ctx pointer to the opaque mapistore backend context
   \param login_user the username used to authenticate
   \param username the username we want to impersonate
   \param uri pointer to the mstoredb DN to open
   \param private_data pointer to the private backend context to return
 */
static enum MAPISTORE_ERROR mstoredb_create_context(struct mapistore_backend_context *ctx,
						    const char *login_user,
						    const char *username,
						    const char *uri,
						    void **private_data)
{
	enum MAPISTORE_ERROR	retval;
	TALLOC_CTX		*mem_ctx;
	struct mstoredb_context	*mstoredb_ctx;
	char			*new_uri;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!private_data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	MSTORE_DEBUG_SUCCESS(MSTORE_LEVEL_PEDANTIC, "uri = %s\n", uri);

	mem_ctx = (TALLOC_CTX *) ctx;
	
	/* Step 1. Initialize mstoredb context */
	mstoredb_ctx = talloc_zero(mem_ctx, struct mstoredb_context);
	mstoredb_ctx->context_dn = talloc_strdup(mstoredb_ctx, uri);
	mstoredb_ctx->login_user = talloc_strdup(mstoredb_ctx, username);
	mstoredb_ctx->username = talloc_strdup(mstoredb_ctx, username);
	mstoredb_ctx->mdb_ctx = ctx;

	/* Step 2. Retrieve path to the mapistore database */
	mstoredb_ctx->dbpath = mapistore_get_database_path();
	MSTORE_DEBUG_SUCCESS(MSTORE_LEVEL_PEDANTIC, "database path = %s\n", mstoredb_ctx->dbpath);

	/* Step 3. Open a wrapped connection to mapistore.ldb */
	mstoredb_ctx->ldb_ctx = mapistore_public_ldb_connect(mstoredb_ctx->mdb_ctx, mstoredb_ctx->dbpath);
	if (!mstoredb_ctx->ldb_ctx) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to open mapistore.ldb at %s\n", mstoredb_ctx->dbpath);
		talloc_free(mstoredb_ctx);
		return MAPISTORE_ERR_DATABASE_INIT;
	}

	/* Step 4. Retrieve the FID associated to this URI */
	new_uri = talloc_asprintf(mem_ctx, "mstoredb://%s", uri);
	retval = mapistore_exist(mstoredb_ctx->mdb_ctx, username, new_uri);
	if (retval != MAPISTORE_ERR_EXIST) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_DEBUG, "Indexing database failed to find a record for URI: %s\n", uri);
		talloc_free(new_uri);
		talloc_free(mstoredb_ctx);
		
		return retval;
	}
	talloc_free(new_uri);

	mstoredb_ctx->basedn = ldb_dn_new(mstoredb_ctx, mstoredb_ctx->ldb_ctx, uri);
	if (!mstoredb_ctx->basedn) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "Unable to create DN from URI");
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
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	return MAPISTORE_SUCCESS;
}

/**
   \details Create a root default/system mailbox folder in the
   mstoredb backend and store store common attributes for caching
   purposes.

   \param private_data pointer to the current mstoredb context
   \param mapistore_uri pointer to the mapistore URI for the folder
   \param folder_name the name of the folder to create
   \param folder_desc the description for the folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR mstoredb_op_db_mkdir(void *private_data,
						 enum MAPISTORE_DFLT_FOLDERS system_idx,
						 const char *mapistore_uri,
						 const char *folder_name)
{
	TALLOC_CTX			*mem_ctx;
	struct mstoredb_context		*mstoredb_ctx = (struct mstoredb_context *) private_data;
	enum MAPISTORE_ERROR		retval;
	char				*mapistore_root_folder = NULL;
	int				i;
	const char			*cn = NULL;
	const char			*uri;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstoredb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Ensure the mapistore URI doesn't already exist in the indexing database */
	retval = mapistore_exist(mstoredb_ctx->mdb_ctx, mstoredb_ctx->username, mapistore_uri);
	if (retval == MAPISTORE_ERR_EXIST) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_HIGH, "URI for %s already registered (%s)\n", mstoredb_ctx->username, mapistore_uri);
		return retval;
	}

	/* Step 2. Retrieve the cn attribute's value from our dflt folder array */
	for (i = 0; dflt_folders[i].name; i++) {
		if (dflt_folders[i].index == system_idx) {
			if (!folder_name) {
				folder_name = dflt_folders[i].cn;
			}
			cn = dflt_folders[i].cn;
			break;
		}
	}
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERR_INVALID_URI, NULL);

	/* Step 3. Strip out the namespace from URI if required */
	if (!strncmp("mstoredb://", mapistore_uri, strlen("mstoredb://"))) {
		uri = &mapistore_uri[strlen("mstoredb://")];
	} else {
		uri = mapistore_uri;
	}

	/* Step 3. Create the LDIF formated entry for the folder */
	mem_ctx = talloc_new(NULL);
	mapistore_root_folder = talloc_asprintf(mem_ctx, MDB_ROOTFOLDER_LDIF_TMPL,
						uri, cn, folder_name, system_idx);

	/* Step 3. Create folder entry within mapistore.ldb */
	retval = mapistore_ldb_write_ldif_string_to_store(mstoredb_ctx->ldb_ctx, mapistore_root_folder);
	talloc_free(mapistore_root_folder);
	talloc_free(mem_ctx);

	return retval;
}

/**
   \details Create a folder in the mstoredb backend

   \param private_data pointer to the current mstoredb backend
   \param parent_uri the parent folder mapistore URI
   \param folder_name the folder name to be created
   \param folder_desc the folder description for the folder 
   \param folder_uri the folder URI to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR mstoredb_op_mkdir(void *private_data,
					      const char *parent_uri,
					      const char *folder_name,
					      const char *folder_desc,
					      char **folder_uri)
{
	/* struct mstoredb_context		*mstoredb_ctx = (struct mstoredb_context *) private_data; */

	/* Sanity checks */
	

	/* Ensure the parent uri exists */

	/* Generate the URI for the folder */

	/* Do ACLs check on parent URI */
	
	/* Do folder creation */

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
	backend.op_mkdir = mstoredb_op_mkdir;

	/* Fill in mapistore db operations */
	backend.op_db_mkdir = mstoredb_op_db_mkdir;

	/* Register ourselves with the MAPISTORE subsystem */
	retval = mapistore_backend_register(&backend);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(5, ("Failed to register the '%s' mapistore backend!\n", backend.name));
	}

	return retval;
}
