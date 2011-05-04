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

#include <string.h>

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"
#include "mapistore_common.h"

#include <param.h>

/**
   \file mapistoredb.c

   \brief MAPIStore database provisioning interface
 */

/**
   \details Create a new mapistore database context

   \param mem_ctx pointer to the memory context

   \return Allocated mapistore database context on success, otherwise
   NULL
 */
struct mapistoredb_context *mapistoredb_new(TALLOC_CTX *mem_ctx)
{
	struct mapistoredb_context	*mdb_ctx;
	char				**domaindn;
	int				i;

	/* Step 1. Initialize mapistoredb context */
	mdb_ctx = talloc_zero(mem_ctx, struct mapistoredb_context);
	if (!mdb_ctx) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Failed to allocated memory for %s\n", "mdb_ctx");
		return NULL;
	}

	mdb_ctx->param = talloc_zero(mem_ctx, struct mapistoredb_conf);
	if (!mdb_ctx->param) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Failed to allocate memory for %s\n", "mdb_ctx->param");
		talloc_free(mdb_ctx);
		return NULL;
	}

	/* Step 2. Initialize Samba loadparm context and load default values */
	mdb_ctx->lp_ctx = loadparm_init(mdb_ctx);
	lpcfg_load_default(mdb_ctx->lp_ctx);

	/* Step 3. Retrieve default values from smb.conf */
	mdb_ctx->param->netbiosname = strlower_talloc(mdb_ctx->param, lpcfg_netbios_name(mdb_ctx->lp_ctx));
	mdb_ctx->param->dnsdomain = strlower_talloc(mdb_ctx->param, lpcfg_realm(mdb_ctx->lp_ctx));
	mdb_ctx->param->domain = strlower_talloc(mdb_ctx->param, lpcfg_sam_name(mdb_ctx->lp_ctx));

	switch (lpcfg_server_role(mdb_ctx->lp_ctx)) {
	case ROLE_DOMAIN_CONTROLLER:
	{
		char *el_lower;
		domaindn = str_list_make(mdb_ctx->param, mdb_ctx->param->dnsdomain, ".");
		el_lower = strlower_talloc(mem_ctx, domaindn[0]);
		mdb_ctx->param->domaindn = talloc_asprintf(mdb_ctx->param, "DC=%s", el_lower);
		talloc_free(el_lower);
		for (i = 1; domaindn[i]; i++) {
			el_lower = strlower_talloc(mem_ctx, domaindn[i]);
			mdb_ctx->param->domaindn = talloc_asprintf_append_buffer(mdb_ctx->param->domaindn, ",DC=%s", el_lower);
			talloc_free(el_lower);
		}
		talloc_free(domaindn);
		break;
	}
	default:
		mdb_ctx->param->domaindn = talloc_asprintf(mdb_ctx->param, "CN=%s", mdb_ctx->param->domain);
		break;
	}

	mdb_ctx->param->serverdn = talloc_asprintf(mdb_ctx->param, TMPL_MDB_SERVERDN,
						   mdb_ctx->param->netbiosname,
						   mdb_ctx->param->domaindn);
	mdb_ctx->param->firstorg = talloc_strdup(mdb_ctx->param, DFLT_MDB_FIRSTORG);
	mdb_ctx->param->firstou = talloc_strdup(mdb_ctx->param, DFLT_MDB_FIRSTOU);
	mdb_ctx->param->firstorgdn = talloc_asprintf(mdb_ctx->param, TMPL_MDB_FIRSTORGDN,
						     mdb_ctx->param->firstou,
						     mdb_ctx->param->firstorg,
						     mdb_ctx->param->serverdn);
	mdb_ctx->param->db_path = NULL;
	mdb_ctx->param->mstore_path = NULL;
	mdb_ctx->param->db_named_path = NULL;

	mdb_ctx->mstore_ctx = NULL;

	return mdb_ctx;
}

/**
   \details Initialize the mapistore database context

   \param mdb_ctx pointer to the mapistoredb context
   \param path string pointer to the mapistore database location

   If path is NULL, then the default mapistore database path will be used instead.
   
   mdb_ctx is expected to have been created using mapistoredb_new

   \return Allocated mapistore database context on success, otherwise NULL
 */
enum MAPISTORE_ERROR mapistoredb_init(struct mapistoredb_context *mdb_ctx,
					     const char *path)
{
	struct stat			sb;
	int				ret;

	/* Sanity checks */
	if (path == NULL) {
		path = MAPISTORE_DBPATH;
	}

	if (!mdb_ctx->param->db_path)
	{
		mdb_ctx->param->db_path = talloc_asprintf(mdb_ctx->param, "%s/mapistore.ldb", path);
		/* Ensure the path is valid */
		ret = stat(path, &sb);
		MAPISTORE_RETVAL_IF(ret == -1, MAPISTORE_ERR_NO_DIRECTORY, NULL);
	}
	if (!mdb_ctx->param->mstore_path)
		mdb_ctx->param->mstore_path = talloc_asprintf(mdb_ctx->param, "%s/mapistore", path);
	if (!mdb_ctx->param->db_named_path)
		mdb_ctx->param->db_named_path = talloc_asprintf(mdb_ctx->param, "%s/%s", 
								mdb_ctx->param->mstore_path,
								MAPISTORE_DB_NAMED_V2);

	/* Step 4. Initialize mapistore */
	if (stat(mdb_ctx->param->mstore_path, &sb) == -1) {
		ret = mkdir(mdb_ctx->param->mstore_path, 0700);
		if (ret == -1) {
			perror(mdb_ctx->param->mstore_path);
			return MAPISTORE_ERR_NO_DIRECTORY;
		}
	}

	mapistore_set_database_path(mdb_ctx->param->db_path);
	mapistore_set_mapping_path(mdb_ctx->param->mstore_path);
	mapistore_set_named_properties_database_path(mdb_ctx->param->db_named_path);
	mdb_ctx->mstore_ctx = mapistore_init(mdb_ctx, NULL);
	if (!mdb_ctx->mstore_ctx) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Failed to initialize mapistore %s\n", "context");
		talloc_free(mdb_ctx);
		return MAPISTORE_ERR_NOT_INITIALIZED;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Free a mapistore database context

   \param mdb_ctx the context to free (from mapistoredb_new())
 */
void mapistoredb_release(struct mapistoredb_context *mdb_ctx)
{
	if (!mdb_ctx) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Invalid mapistore database %s\n", "context");
		return;
	}

	talloc_free(mdb_ctx->lp_ctx);
	talloc_free(mdb_ctx->param);
	talloc_free(mdb_ctx);
}


/**
   \details Get a mapistore URI for a system/special folder from
   backend

   \param mdb_ctx pointer to the mapistore database context
   \param index the special folder index
   \param username the username for which we want to retrieve the uri
   \param uri the uri namespace for the backend we want to use
   \param namespace_uri pointer to the resulting namespace uri (return value)

   The special folder is specified by the \p index parameter. For example
   to create the "inbox" folder, pass MDB_INBOX.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistoredb_get_mapistore_uri(struct mapistoredb_context *mdb_ctx,
						   enum MAPISTORE_DFLT_FOLDERS index,
						   const char *namespace_uri,
						   const char *username,
						   char **uri)
{
	enum MAPISTORE_ERROR	retval;
	char			*_uri;

	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->mstore_ctx) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Invalid mapistore database %s\n", "context");
		return MAPISTORE_ERR_INVALID_CONTEXT;
	}
	if (!namespace_uri || !username) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Invalid  %s\n", "parameter");
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	retval = mapistore_create_uri(mdb_ctx->mstore_ctx, index, namespace_uri, username, &_uri);
	if (retval == MAPISTORE_SUCCESS) {
		*uri = _uri;
	}

	return retval;
}


/**
   \details Retrieve the next available folder or message identifier

   This function is a wrapper over mapistore_get_new_fmid from
   mapistore_processing.c

   \param mdb_ctx pointer to the mapistore database context
   \param username the user to get the folder or message identifier for (i.e. identifier scope)
   \param _fmid pointer on the next fmid available to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
   
   \sa mapistoredb_get_new_allocation_range for an alternative function returning multiple identifiers
 */
enum MAPISTORE_ERROR mapistoredb_get_new_fmid(struct mapistoredb_context *mdb_ctx,
					      const char *username,
					      uint64_t *_fmid)
{
	enum MAPISTORE_ERROR	retval;
	uint64_t		fmid = 0;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx->processing_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!_fmid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	retval = mapistore_get_new_fmid(mdb_ctx->mstore_ctx->processing_ctx, username, &fmid);
	if (retval == MAPISTORE_SUCCESS) {
		*_fmid = fmid;
		return MAPISTORE_SUCCESS;
	}
	
	return retval;
}


/**
   \details Retrieve a new allocation range

   This function obtains a range of folder / message identifiers. Conceptually
   you specify how many identifiers you want, and are provided a contiguous block
   of identiers (in terms of a start and end, which are inclusive).

   This function is a wrapper over mapistore_get_new_allocation_range
   from mapistore_processing.c

   \param mdb_ctx pointer to the mapistore database context
   \param username the user for which we want to retrieve an
   allocation range
   \param range the number of IDs to allocate
   \param range_start pointer to the first ID of the range to return
   \param range_end pointer to the last ID of the range to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistoredb_get_new_allocation_range(struct mapistoredb_context *mdb_ctx,
							  const char *username,
							  uint64_t range,
							  uint64_t *range_start,
							  uint64_t *range_end)
{
	enum MAPISTORE_ERROR	retval;
	uint64_t		_range_start = 0;
	uint64_t		_range_end = 0;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx->processing_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!range_start || !range_end, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	retval = mapistore_get_new_allocation_range(mdb_ctx->mstore_ctx->processing_ctx, username, range, &_range_start, &_range_end);
	if (retval == MAPISTORE_SUCCESS) {
		*range_start = _range_start;
		*range_end = _range_end;

		return MAPISTORE_SUCCESS;
	}

	return retval;
}

/** TODO: this is a copy of code in mapistore_mstoredb.c */
static bool write_ldif_string_to_store(struct mapistoredb_context *mdb_ctx, const char *ldif_string)
{
	enum MAPISTORE_ERROR	retval;

	retval = mapistore_write_ldif_string_to_store(mdb_ctx->mstore_ctx->processing_ctx, ldif_string);
	return (retval == MAPISTORE_SUCCESS) ? true : false;
}


/**
   \details Register a new folder in the mapistore database

   This function is mainly used to encapsulate the creation of the
   User store container, root Mailbox folder. subfolders getting
   created through mstoredb backend.

   This function is a wrapper over the
   mapistore_indexing_add_fmid_record function from
   indexing/mapistore_indexing.c file.

   \param mdb_ctx pointer to the mapistore database context
   \param username the username for which we want to create the
   mailbox container
   \param mapistore_uri the mapistore URI to register

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
*/
enum MAPISTORE_ERROR mapistoredb_register_new_mailbox(struct mapistoredb_context *mdb_ctx,
						      const char *username,
						      const char *mapistore_uri)
{
	TALLOC_CTX				*mem_ctx;
	enum MAPISTORE_ERROR			retval;
	struct mapistore_indexing_context_list	*indexing_ctx;
	const char				*firstorgdn;
	char					*user_store_ldif;
	char					*mailbox_ldif;
	uint64_t				fid;
	char					*dn;
	struct GUID				guid;
	char					*mailboxGUID;
	char					*replicaGUID;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. We want to ensure the mapistore_uri is a mstoredb:// one */
	if (strncmp("mstoredb://", mapistore_uri, strlen("mstoredb://"))) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Invalid mapistore URI. MUST be %s\n", "mstoredb");
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	/* Retrieve configuration parameters */
	firstorgdn = mapistore_get_firstorgdn();
	if (!firstorgdn) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Invalid  %s\n", "firstorgdn");
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	/* Step 2. Create the user store entry within the mapistore database */
	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
	user_store_ldif = talloc_asprintf(mem_ctx, MDB_USER_STORE_LDIF_TMPL,
					  username, firstorgdn, username);
	if (write_ldif_string_to_store(mdb_ctx, user_store_ldif) == false) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Failed to add user store %s\n", "container");
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	talloc_free(user_store_ldif);

	/* Step 3. Generate a fid for this mailbox root container */
	retval = mapistore_get_new_fmid(mdb_ctx->mstore_ctx->processing_ctx, username, &fid);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 4. Add an indexing context for user */
	retval = mapistore_indexing_context_add(mdb_ctx->mstore_ctx, username, &indexing_ctx);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 5. Register the mailbox root container */
	retval = mapistore_indexing_add_fmid_record(indexing_ctx, fid, mapistore_uri, 0, MAPISTORE_INDEXING_FOLDER);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 6. Delete the indexing context */
	retval = mapistore_indexing_context_del(mdb_ctx->mstore_ctx, username);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Step 7. Add the mailbox root container to mapistore.ldb */	

	dn = (char *) &mapistore_uri[strlen("mstoredb://")];

	guid = GUID_random();
	mailboxGUID = GUID_string(mem_ctx, &guid);

	guid = GUID_random();
	replicaGUID = GUID_string(mem_ctx, &guid);

	mailbox_ldif = talloc_asprintf(mem_ctx, MDB_MAILBOX_LDIF_TMPL,
				       username, firstorgdn, dn, "Mailbox Root",
				       mailboxGUID, replicaGUID,
				       MDB_ROOT_FOLDER, mapistore_uri, dn);
	if (write_ldif_string_to_store(mdb_ctx, mailbox_ldif) == false) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Failed to add mailbox root %s\n", "container");
		talloc_free(mem_ctx);

		return MAPISTORE_ERR_DATABASE_OPS;
	}

	talloc_free(mailboxGUID);
	talloc_free(replicaGUID);
	talloc_free(mailbox_ldif);

	talloc_free(mem_ctx);

	return retval;
}


/**
   \details Add an allocation range for messages to the mailbox root
   container within the mapistore database.

   \param mdb_ctx pointer to the mapistore database context
   \param username the username for which we want to add a new
   allocation range to the mailbox container
   \param rstart the beginning of the allocation ID range
   \param rend the end of the allocation ID range

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistoredb_register_new_mailbox_allocation_range(struct mapistoredb_context *mdb_ctx,
								       const char *username,
								       uint64_t rstart,
								       uint64_t rend)
{
	enum MAPISTORE_ERROR			retval;
	struct mapistore_indexing_context_list	*indexing_ctx;
	uint64_t				fid;
	char					*mailbox_root;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!rstart || !rend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(rstart > rend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. Add an indexing context for user */
	retval = mapistore_indexing_context_add(mdb_ctx->mstore_ctx, username, &indexing_ctx);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 2. Retrieve the FID for the user mailbox root folder */
	retval = mapistore_get_mailbox_uri(mdb_ctx->mstore_ctx->processing_ctx, username, &mailbox_root);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	retval = mapistore_indexing_get_record_fmid_by_uri(indexing_ctx, mailbox_root, &fid);
	talloc_free(mailbox_root);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Update the allocation range for root container */
	retval = mapistore_indexing_add_folder_record_allocation_range(indexing_ctx, fid, rstart, rend);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 4. Delete the indexing context */
	retval = mapistore_indexing_context_del(mdb_ctx->mstore_ctx, username);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	return retval;
}


/**
   \details Default provisioning for mapistore.ldb database

   \param mdb_ctx pointer to the mapistore database context
   
   \return MAPISTORE_SUCCESS on success, otherwise a non-zero MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistoredb_provision(struct mapistoredb_context *mdb_ctx)
{
	char	*ldif_str;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Step 1. Add database schema */
	if (write_ldif_string_to_store(mdb_ctx, MDB_INIT_LDIF_TMPL) == false) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Failed to add database %s\n", "schema");
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	/* Step 2. Add RootDSE schema */
	ldif_str = talloc_asprintf(mdb_ctx, MDB_ROOTDSE_LDIF_TMPL,
				   mdb_ctx->param->firstou,
				   mdb_ctx->param->firstorg,
				   mdb_ctx->param->serverdn,
				   mdb_ctx->param->serverdn);
	if (write_ldif_string_to_store(mdb_ctx, ldif_str) == false) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Failed to add RootDSE %s\n", "schema");
		talloc_free(ldif_str);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	talloc_free(ldif_str);

	/* Step 3. Provision Server object responsible for maintaining
	 * the Replica and GlobalCount identifier */
	ldif_str = talloc_asprintf(mdb_ctx, MDB_SERVER_LDIF_TMPL,
				   mdb_ctx->param->serverdn,
				   mdb_ctx->param->netbiosname,
				   mdb_ctx->param->firstorg,
				   mdb_ctx->param->serverdn,
				   mdb_ctx->param->firstorg,
				   mdb_ctx->param->firstou,
				   mdb_ctx->param->firstorg,
				   mdb_ctx->param->serverdn,
				   mdb_ctx->param->firstou);
	if (write_ldif_string_to_store(mdb_ctx, ldif_str) == false) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Failed to provision server %s\n", "object");
		talloc_free(ldif_str);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	talloc_free(ldif_str);

	return MAPISTORE_SUCCESS;
}
