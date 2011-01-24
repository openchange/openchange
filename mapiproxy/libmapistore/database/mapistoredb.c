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

#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_common.h"

/**
   \file mapistoredb.c

   \brief MAPIStore database provisioning interface
 */

int lpcfg_server_role(struct loadparm_context *lp_ctx);

/**
   \details Initialize the mapistore database context

   \param mem_ctx pointer to the memory context
   \param path string pointer to the mapistore database location

   If path if NULL use the default mapistore database path instead.

   \return Allocated mapistore database context on success, otherwise NULL
 */
struct mapistoredb_context *mapistoredb_init(TALLOC_CTX *mem_ctx,
					     const char *path)
{
	struct mapistoredb_context	*mdb_ctx;
	char				**domaindn;
	struct stat			sb;
	int				i;
	int				ret;

	/* Sanity checks */
	if (path == NULL) {
		path = MAPISTORE_DBPATH;
	}

	/* Ensure the path is valid */
	if (stat(path, &sb) == -1) {
		perror(path);
		return NULL;
	}

	/* Step 1. Initialize mapistoredb context */
	mdb_ctx = talloc_zero(mem_ctx, struct mapistoredb_context);
	mdb_ctx->param = talloc_zero(mem_ctx, struct mapistoredb_conf);
	if (!mdb_ctx->param) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to allocate memory for mdb_ctx->param", __FILE__, __LINE__, __FUNCTION__));
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
		domaindn = str_list_make(mdb_ctx->param, mdb_ctx->param->dnsdomain, ".");
		strlower_m(domaindn[0]);
		mdb_ctx->param->domaindn = talloc_asprintf(mdb_ctx->param, "DC_%s", domaindn[0]);
		for (i = 1; domaindn[i]; i++) {
			strlower_m(domaindn[i]);
			mdb_ctx->param->domaindn = talloc_asprintf_append_buffer(mdb_ctx->param->domaindn, ",DC=%s", domaindn[i]);
		}
		talloc_free(domaindn);
		break;
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
	mdb_ctx->param->db_path = talloc_asprintf(mdb_ctx->param, "%s/mapistore.ldb", path);
	mdb_ctx->param->mstore_path = talloc_asprintf(mdb_ctx->param, "%s/mapistore", path);

	/* Step 4. Initialize mapistore */
	if (stat(mdb_ctx->param->mstore_path, &sb) == -1) {
		ret = mkdir(mdb_ctx->param->mstore_path, 0700);
		if (ret == -1) {
			perror(mdb_ctx->param->mstore_path);
			talloc_free(mdb_ctx);
			return NULL;
		}
	}

	mapistore_set_database_path(mdb_ctx->param->db_path);
	mapistore_set_mapping_path(mdb_ctx->param->mstore_path);
	mdb_ctx->mstore_ctx = mapistore_init(mdb_ctx, NULL);
	if (!mdb_ctx->mstore_ctx) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to initialize mapistore context\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(mdb_ctx);
		return NULL;
	}

	return mdb_ctx;
}


/**
   \details Free a mapistore database context

   \param mdb_ctx the context to free (from mapistoredb_init())
 */
void mapistoredb_release(struct mapistoredb_context *mdb_ctx)
{
	if (!mdb_ctx) {
		DEBUG(5, ("! [%s:%d][%s]: Invalid mapistore database context\n", __FILE__, __LINE__, __FUNCTION__));
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
		DEBUG(5, ("! [%s:%d][%s]: Invalid mapistore database context\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_INVALID_CONTEXT;
	}
	if (!namespace_uri || !username) {
		DEBUG(5, ("! [%s:%d][%s]: Invalid parameter\n", __FILE__, __LINE__, __FUNCTION__));
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
   \param _fmid pointer on the next fmid available to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
   
   \sa mapistoredb_get_new_allocation_range for an alternative function returning multiple identifiers
 */
enum MAPISTORE_ERROR mapistoredb_get_new_fmid(struct mapistoredb_context *mdb_ctx,
					      uint64_t *_fmid)
{
	enum MAPISTORE_ERROR	retval;
	uint64_t		fmid = 0;

	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->mstore_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;
	if (!mdb_ctx->mstore_ctx->processing_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;
	if (!_fmid) return MAPISTORE_ERR_INVALID_PARAMETER;

	retval = mapistore_get_new_fmid(mdb_ctx->mstore_ctx->processing_ctx, &fmid);
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
   \param range the number of IDs to allocate
   \param range_start pointer to the first ID of the range to return
   \param range_end pointer to the last ID of the range to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistoredb_get_new_allocation_range(struct mapistoredb_context *mdb_ctx,
							  uint64_t range,
							  uint64_t *range_start,
							  uint64_t *range_end)
{
	enum MAPISTORE_ERROR	retval;
	uint64_t		_range_start = 0;
	uint64_t		_range_end = 0;

	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->mstore_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;
	if (!mdb_ctx->mstore_ctx->processing_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;
	if (!range_start || !range_end) return MAPISTORE_ERR_INVALID_PARAMETER;

	retval = mapistore_get_new_allocation_range(mdb_ctx->mstore_ctx->processing_ctx, range, &_range_start, &_range_end);
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
   Root Mailbox folder. subfolders getting created through mstoredb
   backend.

   This function is a wrapper over the
   mapistore_indexing_add_fmid_record function from
   indexing/mapistore_indexing.c file.

   \param mdb_ctx pointer to the mapistore database context
   \param fid the folder identifier to register
   \param mapistore_uri the mapistore URI to register

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
*/
enum MAPISTORE_ERROR mapistoredb_register_new_mailbox(struct mapistoredb_context *mdb_ctx,
						      const char *username,
						      uint64_t fid,
						      const char *mapistore_uri)
{
	TALLOC_CTX				*mem_ctx;
	enum MAPISTORE_ERROR			retval;
	struct mapistore_indexing_context_list	*indexing_ctx;
	const char				*firstorgdn;
	char					*mailbox_ldif;
	char					*dn;
	struct GUID				guid;
	char					*mailboxGUID;
	char					*replicaGUID;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!fid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mapistore_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Step 1. We want to ensure the mapistore_uri is a mstoredb:// one */
	if (strncmp("mstoredb://", mapistore_uri, strlen("mstoredb://"))) {
		DEBUG(5, ("! [%s:%d][%s]: Invalid mapistore URI. MUST be mstoredb\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	/* Retrieve configuration parameters */
	firstorgdn = mapistore_get_firstorgdn();
	if (!firstorgdn) {
		DEBUG(5, ("! [%s:%d][%s]: Invalid firstorgdn\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}



	/* Step 2. Add an indexing context for user */
	retval = mapistore_indexing_context_add(mdb_ctx->mstore_ctx, username, &indexing_ctx);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 3. Register the mailbox root container */
	retval = mapistore_indexing_add_fmid_record(indexing_ctx, fid, mapistore_uri, 0, MAPISTORE_INDEXING_FOLDER);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 4. Delete the indexing context */
	retval = mapistore_indexing_context_del(mdb_ctx->mstore_ctx, username);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Step 5. Add the mailbox root container to mapistore.ldb */	
	mem_ctx = talloc_named(NULL, 0, "mapistoredb_register_new_mailbox");

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
		DEBUG(0, ("! [%s:%d][%s]: Failed to add mailbox root container\n", __FILE__, __LINE__, __FUNCTION__));
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
   \details Default provisioning for mapistore.ldb database

   \param mdb_ctx pointer to the mapistore database context
   
   \return MAPISTORE_SUCCESS on success, otherwise a non-zero MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistoredb_provision(struct mapistoredb_context *mdb_ctx)
{
	char	*ldif_str;

	/* Sanity checks */
	if (!mdb_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;

	/* Step 1. Add database schema */
	if (write_ldif_string_to_store(mdb_ctx, MDB_INIT_LDIF_TMPL) == false) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to add database schema\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	/* Step 2. Add RootDSE schema */
	ldif_str = talloc_asprintf(mdb_ctx, MDB_ROOTDSE_LDIF_TMPL,
				   mdb_ctx->param->firstou,
				   mdb_ctx->param->firstorg,
				   mdb_ctx->param->serverdn,
				   mdb_ctx->param->serverdn);
	if (write_ldif_string_to_store(mdb_ctx, ldif_str) == false) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to add RootDSE schema\n", __FILE__, __LINE__, __FUNCTION__));
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
		DEBUG(5, ("! [%s:%d][%s]: Failed to provision server object\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(ldif_str);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	talloc_free(ldif_str);

	return MAPISTORE_SUCCESS;
}
