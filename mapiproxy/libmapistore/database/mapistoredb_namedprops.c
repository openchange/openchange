/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011

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

#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_common.h"

/**
   \file mapistoredb_namedprops.c

   \brief MAPIStore named properties database provisioning interface
 */

static const char *mapistoredb_namedprops_get_ldif_path(void)
{
	return MAPISTORE_LDIF;
}


/**
   \details Provision the default named properties database for mapistore

   \param mdb_ctx pointer to the mapistore database context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistoredb_namedprops_provision(struct mapistoredb_context *mdb_ctx)
{
	enum MAPISTORE_ERROR	retval;
	TALLOC_CTX		*mem_ctx;
	int			ret;
	struct stat		sb;
	char			*ldif;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	ret = stat(mapistore_get_named_properties_database_path(), &sb);
	MAPISTORE_RETVAL_IF(ret == -1, MAPISTORE_ERR_DATABASE_INIT, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistoredb_namedprops_provision");
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Step 1. Retrieve the path to the LDIF file */
	ldif = talloc_asprintf(mem_ctx, "%s/%s", mapistoredb_namedprops_get_ldif_path(), MAPISTORE_DB_NAMED_V2_LDIF);
	ret = stat(ldif, &sb);
	MAPISTORE_RETVAL_IF(ret == -1, MAPISTORE_ERR_DATABASE_INIT, mem_ctx);

	/* Step 2. Add database schema */
	retval = mapistore_ldb_write_ldif_string_to_store(mdb_ctx->mstore_ctx->mapistore_nprops_ctx, 
							  MDB_NPROPS_INIT_LDIF_TMPL);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 3. Add RootDSE schema */
	retval = mapistore_ldb_write_ldif_string_to_store(mdb_ctx->mstore_ctx->mapistore_nprops_ctx,
							  MDB_NPROPS_ROOTDSE_LDIF_TMPL);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 2. Commit the database structure and default set of named properties */
	retval = mapistore_ldb_write_ldif_file_to_store(mdb_ctx->mstore_ctx->mapistore_nprops_ctx, ldif);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(ldif);
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

/**
   \details Add an entry for the user within the named properties
   database and retrieve the default mapped index that can be used for
   custom named properties

   \param mdb_ctx pointer to the mapistore database context
   \param username pointer to the username entry to create

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistoredb_namedprops_provision_user(struct mapistoredb_context *mdb_ctx,
							   const char *username)
{
	enum MAPISTORE_ERROR	retval;
	TALLOC_CTX		*mem_ctx;
	char			*ldif;
	uint32_t		mapped_index;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Check if the username already exists */
	retval = mapistore_namedprops_user_exist(mdb_ctx->mstore_ctx, username);
	MAPISTORE_RETVAL_IF(retval == MAPISTORE_ERR_EXIST, retval, NULL);

	/* Step 2. Retrieve the first default available id */
	retval = mapistore_namedprops_get_default_id(mdb_ctx->mstore_ctx, MAPISTORE_NAMEDPROPS_EXTERNAL, &mapped_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Create the LDIF */
	mem_ctx = talloc_named(NULL, 0, "mapistore_namedprops_provision_user");
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	ldif = talloc_asprintf(mem_ctx, MDB_NPROPS_USER_LDIF, username, mapped_index, username);
	MAPISTORE_RETVAL_IF(!ldif, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	/* Step 4. Commit the LDIF */
	retval = mapistore_ldb_write_ldif_string_to_store(mdb_ctx->mstore_ctx->mapistore_nprops_ctx, ldif);
	talloc_free(ldif);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

enum MAPISTORE_ERROR mapistoredb_namedprops_register_application(struct mapistoredb_context *mdb_ctx,
								 const char *username,
								 const char *app_name,
								 const char *app_GUID,
								 const char *ldif)
{
	return MAPISTORE_SUCCESS;
}

enum MAPISTORE_ERROR mapistoredb_namedprops_unregister_application(struct mapistoredb_context *mdb_ctx,
								   const char *username,
								   const char *app_name,
								   const char *app_GUID,
								   const char *ldif)
{
	return MAPISTORE_SUCCESS;
}
