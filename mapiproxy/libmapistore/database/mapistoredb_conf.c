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

/**
   \details Get accessor for netbiosname

   \param mdb_ctx pointer to the mapistore database context

   \return pointer to netbiosname string on success, otherwise NULL
 */
const char *mapistoredb_get_netbiosname(struct mapistoredb_context *mdb_ctx)
{
	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->param || !mdb_ctx->param->netbiosname) {
		return NULL;
	}

	return (const char *) mdb_ctx->param->netbiosname;
}

/**
   \details Set accessor for netbiosname

   \param mdb_ctx pointer to the mapistore database context
   \param netbiosname the netbios name string to set

   \return MAPISTORE_SUCCESS on success, otherwise a non-zero MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistoredb_set_netbiosname(struct mapistoredb_context *mdb_ctx,
						 const char *netbiosname)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->param, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!netbiosname, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mdb_ctx->param->netbiosname) {
		talloc_free(mdb_ctx->param->netbiosname);
	}

	mdb_ctx->param->netbiosname = talloc_strdup(mdb_ctx->param, netbiosname);

	/* Update serverdn */
	if (mdb_ctx->param->serverdn) {
		talloc_free(mdb_ctx->param->serverdn);
	}
	mdb_ctx->param->serverdn = talloc_asprintf(mdb_ctx->param, TMPL_MDB_SERVERDN,
						   mdb_ctx->param->netbiosname,
						   mdb_ctx->param->domaindn);

	/* Update firstorgdn */
	if (mdb_ctx->param->firstorgdn) {
		talloc_free(mdb_ctx->param->firstorgdn);
	}
	mdb_ctx->param->firstorgdn = talloc_asprintf(mdb_ctx->param, TMPL_MDB_FIRSTORGDN,
						     mdb_ctx->param->firstou, 
						     mdb_ctx->param->firstorg,
						     mdb_ctx->param->serverdn);

	return MAPISTORE_SUCCESS;
}

/**
   \details Get accessor for firstorg

   \param mdb_ctx pointer to the mapistore database context

   \return pointer to firstorg string on success, otherwise NULL
 */
const char *mapistoredb_get_firstorg(struct mapistoredb_context *mdb_ctx)
{
	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->param || !mdb_ctx->param->firstorg) {
		return NULL;
	}

	return (const char *) mdb_ctx->param->firstorg;
}

/**
   \details Accessor for first organization

   \param mdb_ctx pointer to the mapistore database context
   \param firstorg the first organization string to set

   \return MAPISTORE_SUCCESS on success, otherwise a non-zero MAPISTORE_ERROR
*/
enum MAPISTORE_ERROR mapistoredb_set_firstorg(struct mapistoredb_context *mdb_ctx,
					      const char *firstorg)
{
	enum MAPISTORE_ERROR	retval;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->param, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!firstorg, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mdb_ctx->param->firstorg) {
		talloc_free(mdb_ctx->param->firstorg);
	}

	mdb_ctx->param->firstorg = talloc_strdup(mdb_ctx->param, firstorg);

	/* Update firstorgdn */
	if (mdb_ctx->param->firstorgdn) {
		talloc_free(mdb_ctx->param->firstorgdn);
	}
	mdb_ctx->param->firstorgdn = talloc_asprintf(mdb_ctx->param, TMPL_MDB_FIRSTORGDN,
						     mdb_ctx->param->firstou, 
						     mdb_ctx->param->firstorg,
						     mdb_ctx->param->serverdn);

	retval = mapistore_set_firstorgdn(mdb_ctx->param->firstou, 
					  mdb_ctx->param->firstorg, 
					  mdb_ctx->param->serverdn);

	return retval;
}

/**
   \details Get accessor for firstou

   \param mdb_ctx pointer to the mapistore database context

   \return pointer to firstou string on success, otherwise NULL
 */
const char *mapistoredb_get_firstou(struct mapistoredb_context *mdb_ctx)
{
	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->param || !mdb_ctx->param->firstou) {
		return NULL;
	}

	return (const char *) mdb_ctx->param->firstou;
}

/**
   \details Accessor for first organization unit

   \param mdb_ctx pointer to the mapistore database context
   \param firstou the first organization unit string to set

   \return MAPISTORE_SUCCESS on success, otherwise a non-zero MAPISTORE_ERROR
 */
enum MAPISTORE_ERROR mapistoredb_set_firstou(struct mapistoredb_context *mdb_ctx,
					     const char *firstou)
{
	enum MAPISTORE_ERROR	retval;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mdb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mdb_ctx->param, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!firstou, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mdb_ctx->param->firstou) {
		talloc_free(mdb_ctx->param->firstou);
	}

	mdb_ctx->param->firstou = talloc_strdup(mdb_ctx->param, firstou);

	/* Update firstorgdn */
	if (mdb_ctx->param->firstorgdn) {
		talloc_free(mdb_ctx->param->firstorgdn);
	}
	mdb_ctx->param->firstorgdn = talloc_asprintf(mdb_ctx->param, TMPL_MDB_FIRSTORGDN,
						     mdb_ctx->param->firstou, 
						     mdb_ctx->param->firstorg,
						     mdb_ctx->param->serverdn);

	retval = mapistore_set_firstorgdn(mdb_ctx->param->firstou, 
					  mdb_ctx->param->firstorg, 
					  mdb_ctx->param->serverdn);

	return retval;
}

/**
   \details Helper function, dumps current mapistore_context
   configuration parameters

   \param mdb_ctx the mapistore database context

   \sa mapistoredb_init
 */
void mapistoredb_dump_conf(struct mapistoredb_context *mdb_ctx)
{
	/* Sanity checks */
	if (!mdb_ctx || !mdb_ctx->param) return;

	DEBUG(0, ("Database Path:                %s\n", mdb_ctx->param->db_path));
	DEBUG(0, ("Netbios Name:                 %s\n", mdb_ctx->param->netbiosname));
	DEBUG(0, ("DNS Domain:                   %s\n", mdb_ctx->param->dnsdomain));
	DEBUG(0, ("Domain:                       %s\n", mdb_ctx->param->domain));
	DEBUG(0, ("Domain DN:                    %s\n", mdb_ctx->param->domaindn));
	DEBUG(0, ("Server DN:                    %s\n", mdb_ctx->param->serverdn));
	DEBUG(0, ("First Organization:           %s\n", mdb_ctx->param->firstorg));
	DEBUG(0, ("First Organization Unit:      %s\n", mdb_ctx->param->firstou));
	DEBUG(0, ("First Organization DN:        %s\n", mdb_ctx->param->firstorgdn));
}

