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

#include <string.h>

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"
#include "mapistore_common.h"

/**
   \file mapistoredb_namedprops.c

   \brief MAPIStore named properties database provisioning interface
 */

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

	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Step 1. Retrieve the path to the LDIF file */
	ldif = talloc_asprintf(mem_ctx, "%s/%s", mapistore_get_named_properties_ldif_path(), MAPISTORE_DB_NAMED_V2_LDIF);
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

static enum MAPISTORE_ERROR mapistoredb_namedprops_provision_backend_set(TALLOC_CTX *mem_ctx,
									 struct mapistore_context *mstore_ctx,
									 struct ldb_ldif *ldif,
									 char **ldif_record,
									 uint32_t *ext_index, uint32_t *int_index)
{
	enum MAPISTORE_ERROR		retval;
	struct ldb_context		*ldb_ctx;
	struct ldb_message		*normalized_msg;
	struct ldb_message_element	*ldb_element;
	const char			*objectClass;
	bool				oclass_external = false;
	bool				oclass_internal = false;
	enum MAPISTORE_NAMEDPROPS_TYPE	ntype;
	bool				mnidstring = false;
	uint32_t			index;
	const char			*dn;
	int				ret;
	int				i;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!ldif, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ldb_ctx = mstore_ctx->mapistore_nprops_ctx;

	/* Step 1. Normalize the message */
	ret = ldb_msg_normalize(ldb_ctx, mem_ctx, ldif->msg, &normalized_msg);
	MAPISTORE_RETVAL_IF(ret != LDB_SUCCESS, MAPISTORE_ERR_DATABASE_OPS, NULL);

	/* Step 2. Ensure the record has a DN */
	dn = ldb_dn_get_linearized(normalized_msg->dn);
	if (!dn) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Missing %s!\n", "dn");
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	/* Step 3. Search for the objectClass attribute */
	ldb_element = ldb_msg_find_element(normalized_msg, "objectClass");
	MAPISTORE_RETVAL_IF(!ldb_element, MAPISTORE_ERR_DATABASE_OPS, NULL);

	oclass_external = false;
	oclass_internal = false;
	mnidstring = false;
	for (i = 0; i < ldb_element->num_values; i++) {
		if (ldb_element->values[i].length) {
			objectClass = (const char *)ldb_element->values[i].data;
			if (!strncmp(objectClass, "External", strlen(objectClass))) {
				oclass_external = true;
			} else if (!strncmp(objectClass, "Internal", strlen(objectClass))) {
				oclass_internal = true;
			} else if (!strncmp(objectClass, "MNID_ID", strlen(objectClass)) ||
				   !strncmp(objectClass, "MNID_STRING", strlen(objectClass))) {
				mnidstring = true;
			}
		} else {
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Invalid objectClass for dn: %s\n", dn);
			return MAPISTORE_ERR_DATABASE_OPS;
		}
	}

	/* Step 4. Perform sanity checks on objectClass */

	/* Step 4.1. Record can't have both External and Internal set for objectClass attribute */
	if (oclass_external == true && oclass_internal == true) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "External and Internal objectClass set for dn: %s\n", dn);
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	/* Step 4.2. Neither can they be both unset */
	if (oclass_external == false && oclass_internal == false && mnidstring == true) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "objectClass must be set to External or Internal for dn: %s\n", dn);
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	/* Step 5. Set index to appropriate value */
	if (oclass_external == true) {
		ntype = MAPISTORE_NAMEDPROPS_EXTERNAL;
		index = *ext_index;
	} else {
		ntype = MAPISTORE_NAMEDPROPS_INTERNAL;
		index = *int_index;
	}

	/* Ensure the index doesn't equal a reserved value */
	while ((retval = mapistore_namedprops_check_id(mstore_ctx, ntype, index)) != MAPISTORE_SUCCESS) {
		index++;
	}

	/* Step 6. Add the mapped_id attribute to the record */
	if (mnidstring == true) {
		ret = ldb_msg_add_fmt(normalized_msg, "mapped_id", "0x%.4x", index);
		if (ret != LDB_SUCCESS) {
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Failed to add attribute mapped_id for dn: %s\n", dn);
			return MAPISTORE_ERR_DATABASE_OPS;
		}

		/* Step 7. Update the appropriate index */
		index++;
		if (oclass_external == true) {
			*ext_index = index;
		} else {
			*int_index = index;
		}
	}

	/* Step 8. Add the new msg to our LDIF buffer */
	*ldif_record = ldb_ldif_message_string(ldb_ctx, mem_ctx, LDB_CHANGETYPE_ADD, normalized_msg);
	if (!*ldif_record) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Failed to save intermediary LDIF record: %s\n", dn);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	
	return MAPISTORE_SUCCESS;
}


static enum MAPISTORE_ERROR mapistoredb_namedprops_provision_backend_ldif_file(TALLOC_CTX *mem_ctx,
									       struct mapistore_context *mstore_ctx,
									       char *filename, char **ldif_buffer, 
									       char **mod_ldif)
{
	enum MAPISTORE_ERROR		retval;
	FILE				*f;
	struct ldb_context		*ldb_ctx;
	struct ldb_ldif			*ldif;
	struct stat			sb;
	char				*ldif_record;
	uint32_t			int_index;
	uint32_t			ext_index;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!filename, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!ldif_buffer, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mod_ldif, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ldb_ctx = mstore_ctx->mapistore_nprops_ctx;

	/* Step 1. Retrieve mapped index for CN=External and CN=Internal */
	retval = mapistore_namedprops_get_default_id(mstore_ctx, MAPISTORE_NAMEDPROPS_INTERNAL, &int_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	retval = mapistore_namedprops_get_default_id(mstore_ctx, MAPISTORE_NAMEDPROPS_EXTERNAL, &ext_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 2. Ensure the file exists */
	if (stat(filename, &sb) == -1) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Invalid LDIF file: %s\n", filename);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	f = fopen(filename, "r");
	MAPISTORE_RETVAL_IF(!f, MAPISTORE_ERR_DATABASE_INIT, NULL);

	/* Step 3. Process the LDIF file */
	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		retval = mapistoredb_namedprops_provision_backend_set(mem_ctx, mstore_ctx, ldif, &ldif_record, 
								      &ext_index, &int_index);
		if (retval) {
			if (*ldif_buffer) {
				talloc_free(*ldif_buffer);
				*ldif_buffer = NULL;
			}
			goto exit;
		}

		if (!*ldif_buffer) {
			*ldif_buffer = talloc_strdup(mem_ctx, ldif_record);
		} else {
			*ldif_buffer = talloc_asprintf_append(*ldif_buffer, "%s", ldif_record);
		}
		talloc_free(ldif_record);
		
	}


	/* Step 4. Update mapped_index attributes for CN=External and CN=Internal records */
	*mod_ldif = talloc_asprintf(mem_ctx, MDB_NPROPS_MAPPED_INDEX_CHANGE_LDIF,
				    "External", ext_index);
	*mod_ldif = talloc_asprintf_append(*mod_ldif, MDB_NPROPS_MAPPED_INDEX_CHANGE_LDIF,
					   "Internal", int_index);
	
exit:
	fclose(f);

	return retval;
}


static enum MAPISTORE_ERROR mapistoredb_namedprops_provision_backend_ldif_buffer(TALLOC_CTX *mem_ctx,
										 struct mapistore_context *mstore_ctx,
										 char *buffer, char **ldif_buffer,
										 char **mod_ldif)
{
	enum MAPISTORE_ERROR	retval;
	struct ldb_context	*ldb_ctx;
	struct ldb_ldif		*ldif;
	char			*ldif_record;
	uint32_t		int_index;
	uint32_t		ext_index;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!buffer, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!ldif_buffer, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mod_ldif, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ldb_ctx = mstore_ctx->mapistore_nprops_ctx;

	/* Step 1. Retrieved mapped index for CN=External and CN=Internal */
	retval = mapistore_namedprops_get_default_id(mstore_ctx, MAPISTORE_NAMEDPROPS_INTERNAL, &int_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	retval = mapistore_namedprops_get_default_id(mstore_ctx, MAPISTORE_NAMEDPROPS_EXTERNAL, &ext_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 2. Process the LDIF buffer */
	while ((ldif = ldb_ldif_read_string(ldb_ctx, (const char **)&buffer))) {
		retval = mapistoredb_namedprops_provision_backend_set(mem_ctx, mstore_ctx, ldif, &ldif_record, 
								      &ext_index, &int_index);
		if (retval) {
			if (*ldif_buffer) {
				talloc_free(*ldif_buffer);
				*ldif_buffer = NULL;
			}
			goto exit;
		}
		
		if (!*ldif_buffer) {
			*ldif_buffer = talloc_strdup(mem_ctx, ldif_record);
		} else {
			*ldif_buffer = talloc_asprintf_append(*ldif_buffer, "%s", ldif_record);
		}
		talloc_free(ldif_record);
	}

	retval = MAPISTORE_SUCCESS;

	/* Step 4. Update mapped_index attributes for CN=External and CN=Internal records */
	*mod_ldif = talloc_asprintf(mem_ctx, MDB_NPROPS_MAPPED_INDEX_CHANGE_LDIF,
				    "External", ext_index);
	*mod_ldif = talloc_asprintf_append(*mod_ldif, MDB_NPROPS_MAPPED_INDEX_CHANGE_LDIF,
					   "Internal", int_index);
exit:
	return retval;
}


/**
   \details Provision mapistore named properties database with a LDIF
   file from a backend. This internal function also adds a mapped_id
   to Internal/External records and finally update the mapped_index
   value upon success.

   \param mdb_ctx pointer to the mapistore database context
   \param ldif_data pointer to the LDIF data
   \param ntype the type of ldif_data

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR mapistoredb_namedprops_provision_backend(struct mapistoredb_context *mdb_ctx,
								     char *ldif_data,
								     enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE ntype)
{
	enum MAPISTORE_ERROR		retval;
	TALLOC_CTX			*mem_ctx;
	uint32_t			ext_index;
	uint32_t			int_index;
	struct ldb_context		*ldb_ctx;
	struct ldb_ldif			*ldif;
	char				*ldif_buffer = NULL;
	char				*ldif_mod = NULL;
	int				trans;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!ldif_data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(ntype == MAPISTORE_NAMEDPROPS_PROVISION_NONE, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ldb_ctx = mdb_ctx->mstore_ctx->mapistore_nprops_ctx;

	/* Step 1. Retrieve mapped index for CN=External and CN=Internal */
	retval = mapistore_namedprops_get_default_id(mdb_ctx->mstore_ctx, MAPISTORE_NAMEDPROPS_INTERNAL, &int_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	retval = mapistore_namedprops_get_default_id(mdb_ctx->mstore_ctx, MAPISTORE_NAMEDPROPS_EXTERNAL, &ext_index);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Step 2. Process and retrieved updated LDIF data */
	if (ntype == MAPISTORE_NAMEDPROPS_PROVISION_FILE) {
		retval = mapistoredb_namedprops_provision_backend_ldif_file(mem_ctx, mdb_ctx->mstore_ctx, 
									    ldif_data, &ldif_buffer, &ldif_mod);
	} else if (ntype == MAPISTORE_NAMEDPROPS_PROVISION_BUFFER) {
		retval = mapistoredb_namedprops_provision_backend_ldif_buffer(mem_ctx, mdb_ctx->mstore_ctx,
									      ldif_data, &ldif_buffer, &ldif_mod);
	}
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 3. Do the LDB transaction */
	retval = MAPISTORE_SUCCESS;
	trans = ldb_transaction_start(ldb_ctx);
	if (trans != LDB_SUCCESS) {
		retval = MAPISTORE_ERR_DATABASE_OPS;
		goto failed;
	}

	/* Add LDIF data from backend */
	while ((ldif = ldb_ldif_read_string(ldb_ctx, (const char **)&ldif_buffer))) {
		trans = ldb_msg_normalize(ldb_ctx, ldif, ldif->msg, &ldif->msg);
		if (trans != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to normalize %s\n", "ldif");
			retval = MAPISTORE_ERR_DATABASE_OPS;
			goto failed;
		}

		trans = ldb_add(ldb_ctx, ldif->msg);
		if (trans != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to add ldb msg: %s\n", ldb_errstring(ldb_ctx));
			retval = MAPISTORE_ERR_DATABASE_OPS;
			goto failed;
		}
	}

	/* Update record from mapistore named properties database */
	while ((ldif = ldb_ldif_read_string(ldb_ctx, (const char **)&ldif_mod))) {
		trans = ldb_msg_normalize(ldb_ctx, ldif, ldif->msg, &ldif->msg);
		if (trans != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to normalize %s\n", "ldif");
			retval = MAPISTORE_ERR_DATABASE_OPS;
			goto failed;
		}

		trans = ldb_modify(ldb_ctx, ldif->msg);
		if (trans != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to modify ldb %s\n", "msg");
			retval = MAPISTORE_ERR_DATABASE_OPS;
			goto failed;
		}
	}	

	trans = ldb_transaction_commit(ldb_ctx);
	if (trans != LDB_SUCCESS) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Transaction failed: %s\n", ldb_errstring(ldb_ctx));
		retval = MAPISTORE_ERR_DATABASE_OPS;
		goto failed;
	}

failed:
	talloc_free(mem_ctx);

	return retval;
}

/**
   \details Let registered backends provision additional Internal or
   External properties within the named properties database

   \param mdb_ctx pointer to the mapistore database context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistoredb_namedprops_provision_backends(struct mapistoredb_context *mdb_ctx)
{
	enum MAPISTORE_ERROR				retval;
	enum MAPISTORE_ERROR				retval2;
	char						*ldif;
	enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE	ntype;
	uint32_t					bindex = 0;
	const char					*bname = NULL;
	const char					*bns = NULL;
	const char					*bdesc = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx->mapistore_nprops_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Step 1. Loop over all the different registered backends */
	do {
		retval = mapistore_get_next_backend(&bname, &bns, &bdesc, &bindex);
		if (retval == MAPISTORE_SUCCESS) {
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Looking for LDIF data for backend '%s'\n", bname);
			/* Step 2. Try to fetch LDIF information from each backend */
			retval2 = mapistore_get_backend_ldif(mdb_ctx->mstore_ctx, bname, &ldif, &ntype);
			if (retval2 == MAPISTORE_SUCCESS) {
				/* Step 3. Upon success, try to register LDIF data within the database */
				retval2 = mapistoredb_namedprops_provision_backend(mdb_ctx, ldif, ntype);
				if (retval2) {
					MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, 
							   "Unable to provision LDIF data for backend '%s': %s\n",
							   bname, mapistore_errstr(retval2));
				}
				talloc_free(ldif);
				ldif = NULL;
			}
		} 
	} while (retval != MAPISTORE_ERR_NOT_FOUND);
	
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
	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
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
