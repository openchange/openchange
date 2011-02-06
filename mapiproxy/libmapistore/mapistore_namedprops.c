/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2011

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

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_common.h"
#include "mapistore_private.h"
#include "libmapi/libmapi_private.h"
#include <ldb.h>

#include <sys/stat.h>

/**
   \details Return pointer to the existing mapistore name properties
   database.

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer on pointer to the ldb context the function
   returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_namedprops_init(TALLOC_CTX *mem_ctx, struct ldb_context **_ldb_ctx)
{
	struct ldb_context	*ldb_ctx = NULL;
	struct tevent_context	*ev;
	char			*database;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!_ldb_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ev = tevent_context_init(mem_ctx);
	MAPISTORE_RETVAL_IF(!ev, MAPISTORE_ERR_NO_MEMORY, NULL);

	database = talloc_strdup(mem_ctx, mapistore_get_named_properties_database_path());
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Full path to mapistore named properties database is: %s\n",
			  database);

	ldb_ctx = mapistore_ldb_wrap_connect(ldb_ctx, ev, database, 0);
	talloc_free(database);
	MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERR_DATABASE_INIT, NULL);

	*_ldb_ctx = ldb_ctx;

	return MAPISTORE_SUCCESS;
}

/**
   \details Retrieve the default and first available ID from the
   mapistore named properties database for internal or external
   purposes

   \param mstore_ctx pointer to the mapistore context
   \param ntype the type of mapping index to return (internal or external)
   \param dflt_id pointer to the default ID to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_namedprops_get_default_id(struct mapistore_context *mstore_ctx, 
							 enum MAPISTORE_NAMEDPROPS_TYPE ntype,
							 uint32_t *dflt_id)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	const char		*stype = NULL;
	int			ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!dflt_id, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ldb_ctx = mstore_ctx->mapistore_nprops_ctx;
	mem_ctx = talloc_named(NULL, 0, "mapistore_namedprops_get_default_id");
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Step 1. Search for CN=External,CN=Server record and retrieve its attributes */
	switch (ntype) {
	case MAPISTORE_NAMEDPROPS_INTERNAL:
		stype = "Internal";
		break;
	case MAPISTORE_NAMEDPROPS_EXTERNAL:
		stype = "External";
		break;
	}
	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(objectClass=container)(cn=%s))", stype);
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	/* Step 2. Retrieve and return the mapping_index attribute */
	*dflt_id = ldb_msg_find_attr_as_int(res->msgs[0], "mapping_index", 0);
	MAPISTORE_RETVAL_IF(!*dflt_id, MAPISTORE_ERR_NOT_FOUND, mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details return the mapped property ID matching the nameid
   structure passed in parameter.

   TODO: This function should take a username parameter so we can
   fetch custom added properties within user namespace

   \param _ldb_ctx pointer to the namedprops ldb context
   \param nameid the MAPINAMEID structure to lookup
   \param propID pointer to the property ID the function returns

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ int mapistore_namedprops_get_mapped_id(void *_ldb_ctx, 
						struct MAPINAMEID nameid, 
						uint16_t *propID)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx = (struct ldb_context *)_ldb_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;
	char			*filter = NULL;
	char			*guid;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!propID, MAPISTORE_ERROR, NULL);

	*propID = 0;
	mem_ctx = talloc_named(NULL, 0, "mapistore_namedprops_get_mapped_propID");
	guid = GUID_string(mem_ctx, (const struct GUID *)&nameid.lpguid);

	switch (nameid.ulKind) {
	case MNID_ID:
		filter = talloc_asprintf(mem_ctx, "(&(objectClass=MNID_ID)(oleguid=%s)(cn=0x%.4x))",
					 guid, nameid.kind.lid);
		break;
	case MNID_STRING:
		filter = talloc_asprintf(mem_ctx, "(&(objectClass=MNID_STRING)(oleguid=%s)(cn=%s))",
					 guid, nameid.kind.lpwstr.Name);
		break;
	}
	talloc_free(guid);

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "%s", filter);
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS || !res->count, MAPISTORE_ERROR, mem_ctx);

	*propID = ldb_msg_find_attr_as_uint(res->msgs[0], "mapped_id", 0);
	MAPISTORE_RETVAL_IF(!*propID, MAPISTORE_ERROR, mem_ctx);

	talloc_free(filter);
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Check if a user exists in the named properties database

   \param mstore_ctx pointer to the mapistore context
   \param username the username to lookup
   
   \return MAPISTORE_ERR_EXIST if the user exists and
   MAPISTORE_ERR_NOT_FOUND if it doesn't, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_namedprops_user_exist(struct mapistore_context *mstore_ctx,
						     const char *username)
{
	TALLOC_CTX		*mem_ctx;
	struct ldb_context	*ldb_ctx;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "*", NULL };
	int			ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_namedprops_user_exist");
	ldb_ctx = mstore_ctx->mapistore_nprops_ctx;

	ret = ldb_search(ldb_ctx, mem_ctx, &res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, "(&(objectClass=user)(cn=%s))", username);
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);
	MAPISTORE_RETVAL_IF(!res->count, MAPISTORE_ERR_NOT_FOUND, mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_ERR_EXIST;
}
