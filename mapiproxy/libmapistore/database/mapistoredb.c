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

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"

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
	char				*full_path;
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

	/* Step 3. Initialize tevent structure */
	mdb_ctx->ev = tevent_context_init(mdb_ctx);

	/* Step 4. Open a wrapped connection on the mapistore database */
	full_path = talloc_asprintf(mem_ctx, "%s/mapistore.ldb", path);
	mdb_ctx->ldb_ctx = mapistore_ldb_wrap_connect(mdb_ctx, mdb_ctx->ev, full_path, 0);
	talloc_free(full_path);
	if (!mdb_ctx->ldb_ctx) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to open wrapped connection over mapistore.ldb\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(mdb_ctx);
		return NULL;
	}

	/* Step 5. Retrieve default values from smb.conf */
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

	/* Step 6. Initialize mapistore */
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


/** TODO: this is a copy of code in mapistore_mstoredb.c */
static bool write_ldif_string_to_store(struct ldb_context *ldb_ctx, const char *ldif_string)
{
	struct ldb_ldif *ldif;
	int		ret;
	
	while ((ldif = ldb_ldif_read_string(ldb_ctx, (const char **)&ldif_string))) {
		ret = ldb_msg_normalize(ldb_ctx, ldif, ldif->msg, &ldif->msg);
		if (ret != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			return false;
		}
		ret = ldb_add(ldb_ctx, ldif->msg);
		if (ret != LDB_SUCCESS) {
			ldb_ldif_read_free(ldb_ctx, ldif);
			return false;
		}
		ldb_ldif_read_free(ldb_ctx, ldif);
	}
	
	return true;
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
	if (!mdb_ctx || !mdb_ctx->ldb_ctx) return MAPISTORE_ERR_NOT_INITIALIZED;

	/* Step 1. Add database schema */
	if (write_ldif_string_to_store(mdb_ctx->ldb_ctx, MDB_INIT_LDIF_TMPL) == false) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to add database schema\n", __FILE__, __LINE__, __FUNCTION__));
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	/* Step 2. Add RootDSE schema */
	ldif_str = talloc_asprintf(mdb_ctx, MDB_ROOTDSE_LDIF_TMPL,
				   mdb_ctx->param->firstou,
				   mdb_ctx->param->firstorg,
				   mdb_ctx->param->serverdn,
				   mdb_ctx->param->serverdn);
	if (write_ldif_string_to_store(mdb_ctx->ldb_ctx, ldif_str) == false) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to add RootDSE schema\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(ldif_str);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	talloc_free(ldif_str);

	/* Step 3. Provision Server object responsible for maintaining
	 * the Replica identifier */
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
	if (write_ldif_string_to_store(mdb_ctx->ldb_ctx, ldif_str) == false) {
		DEBUG(5, ("! [%s:%d][%s]: Failed to provision server object\n", __FILE__, __LINE__, __FUNCTION__));
		talloc_free(ldif_str);
		return MAPISTORE_ERR_DATABASE_OPS;
	}
	talloc_free(ldif_str);

	return MAPISTORE_SUCCESS;
}
