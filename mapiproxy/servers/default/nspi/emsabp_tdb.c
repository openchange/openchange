/*
   OpenChange Server implementation.

   EMSABP: Address Book Provider implementation

   Copyright (C) Julien Kerihuel 2006-2009.

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
   \file emsabp_tdb.c

   \brief EMSABP TDB database API
*/

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "dcesrv_exchange_nsp.h"

/**
   \details Structure to be used for MId traversal within TDB
 */
struct traverse_MId {
	uint32_t	MId;
	bool		found;
};

/**
   \details Open EMSABP TDB database

   \param mem_ctx pointer to the memory context
   \param lp_ctx pointer to the loadparm context

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ TDB_CONTEXT *emsabp_tdb_init(TALLOC_CTX *mem_ctx, 
				      struct loadparm_context *lp_ctx)
{
	enum MAPISTATUS			retval;
	TDB_CONTEXT			*tdb_ctx;
	TDB_DATA			key;
	TDB_DATA			dbuf;
	int				ret;

	/* Sanity checks */
	if (!lp_ctx) return NULL;

	/* Step 0. Retrieve a TDB context pointer on the emsabp_tdb database */
	tdb_ctx = mapiproxy_server_emsabp_tdb_init(lp_ctx);
	if (!tdb_ctx) return NULL;

	/* Step 1. If EMSABP_TDB_DATA_REC doesn't exist, create it */
	retval = emsabp_tdb_fetch(tdb_ctx, EMSABP_TDB_DATA_REC, &dbuf);
	if (retval == MAPI_E_NOT_FOUND) {
		key.dptr = (unsigned char *) EMSABP_TDB_DATA_REC;
		key.dsize = strlen(EMSABP_TDB_DATA_REC);

		dbuf.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", EMSABP_TDB_MID_START);
		dbuf.dsize = strlen((const char *)dbuf.dptr);

		ret = tdb_store(tdb_ctx, key, dbuf, TDB_INSERT);
		if (ret == -1) {
			OC_DEBUG(3, "Unable to create %s record: %s",
				  EMSABP_TDB_DATA_REC, tdb_errorstr(tdb_ctx));
			tdb_close(tdb_ctx);
			return NULL;
		}
	} else {
		free (dbuf.dptr);
	}

	return tdb_ctx;
}


/**
   \details Initialize a temporary (on-memory) TDB database. This
   database is used to store temporary MId used within a session
   lifetime.

   \param mem_ctx pointer to the memory context

   \return Allocated TDB context on success, otherwise NULL
 */
_PUBLIC_ TDB_CONTEXT *emsabp_tdb_init_tmp(TALLOC_CTX *mem_ctx)
{
	TDB_CONTEXT	*tdb_ctx;
	TDB_DATA       	key;
	TDB_DATA       	dbuf;
	int	       	ret;

	/* Step 0. Initialize the temporary TDB database */
	tdb_ctx = tdb_open(NULL, 0, TDB_INTERNAL, O_RDWR|O_CREAT, 0600);
	
	/* Step 1. Create EMSABP_TMP_TDB_DATA_REC record */
	key.dptr = (unsigned char *) EMSABP_TDB_DATA_REC;
	key.dsize = strlen(EMSABP_TDB_DATA_REC);

	dbuf.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%x", EMSABP_TDB_TMP_MID_START);
	dbuf.dsize = strlen((const char *)dbuf.dptr);

	ret = tdb_store(tdb_ctx, key, dbuf, TDB_INSERT);
	if (ret == -1) {
		OC_DEBUG(3, "Unable to create %s record: %s",
			  EMSABP_TDB_DATA_REC, tdb_errorstr(tdb_ctx));
		tdb_close(tdb_ctx);
		return NULL;
	}

	return tdb_ctx;
}


/**
   \details Close EMSABP TDB database

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS emsabp_tdb_close(TDB_CONTEXT *tdb_ctx)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!tdb_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	tdb_close(tdb_ctx);
	OC_DEBUG(0, "TDB database closed");

	return MAPI_E_SUCCESS;
}


/**
   \details Fetch an element within a TDB database given its key

   \param tdb_ctx pointer to the EMSABP TDB context
   \param keyname pointer to the TDB key to fetch
   \param result pointer on TDB results

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS emsabp_tdb_fetch(TDB_CONTEXT *tdb_ctx,
					  const char *keyname,
					  TDB_DATA *result)
{
	TDB_DATA	key;
	TDB_DATA	dbuf;
	size_t		keylen;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!keyname, MAPI_E_INVALID_PARAMETER, NULL);

	keylen = strlen(keyname);
	OPENCHANGE_RETVAL_IF(!keylen, MAPI_E_INVALID_PARAMETER, NULL);

	key.dptr = (unsigned char *)keyname;
	key.dsize = keylen;

	dbuf = tdb_fetch(tdb_ctx, key);
	OPENCHANGE_RETVAL_IF(!dbuf.dptr, MAPI_E_NOT_FOUND, NULL);
	OPENCHANGE_RETVAL_IF(!dbuf.dsize, MAPI_E_NOT_FOUND, NULL);

	if (!result) {
		free (dbuf.dptr);
	} else {
		*result = dbuf;
	}

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the Minimal EntryID associated to a given DN

   \param tdb_ctx pointer to the EMSABP TDB context
   \param keyname pointer to the TDB key to search for
   \param MId pointer on the integer the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_tdb_fetch_MId(TDB_CONTEXT *tdb_ctx,
					      const char *keyname,
					      uint32_t *MId)
{
	TALLOC_CTX	*mem_ctx;
	TDB_DATA	key;
	TDB_DATA	dbuf;
	char		*str;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!keyname, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MId, MAPI_E_INVALID_PARAMETER, NULL);

	key.dptr = (unsigned char *) keyname;
	key.dsize = strlen(keyname);

	dbuf = tdb_fetch(tdb_ctx, key);
	OPENCHANGE_RETVAL_IF(!dbuf.dptr, MAPI_E_NOT_FOUND, NULL);
	OPENCHANGE_RETVAL_IF(!dbuf.dsize, MAPI_E_NOT_FOUND, NULL);

	mem_ctx = talloc_named(NULL, 0, "emsabp_tdb_fetch_MId");
	str = talloc_strndup(mem_ctx, (char *)dbuf.dptr, dbuf.dsize);
	*MId = strtol((const char *)str, NULL, 16);
	talloc_free(mem_ctx);
	free(dbuf.dptr);

	return MAPI_E_SUCCESS;
}


static int emsabp_tdb_traverse_MId(TDB_CONTEXT *tdb_ctx, 
				   TDB_DATA key, TDB_DATA dbuf, 
				   void *state)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		value;
	char			*value_str = NULL;
	struct traverse_MId	*mid_trav = (struct traverse_MId *) state;

	mem_ctx = talloc_named(NULL, 0, "emsabp_tdb_traverse_MId");
	value_str = talloc_strndup(mem_ctx, (char *)dbuf.dptr, dbuf.dsize);
	value = strtol((const char *)value_str, NULL, 16);
	talloc_free(mem_ctx);

	if (value == mid_trav->MId) {
		mid_trav->found = true;
		return 1;
	}

	return 0;
}


/**
   \details Traverse the EMSABP TDB database and look for the input
   MId

   \param tdb_ctx pointer to the EMSABP TDB context
   \param MId MID to lookup

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsabp_tdb_lookup_MId(TDB_CONTEXT *tdb_ctx,
				    uint32_t MId)
{
	int			ret;
	struct traverse_MId	mid_trav = { MId, false };

	ret = tdb_traverse(tdb_ctx, emsabp_tdb_traverse_MId, (void *)&mid_trav);

	return (ret > 0) && mid_trav.found;
}


static int emsabp_tdb_traverse_MId_DN(TDB_CONTEXT *tdb_ctx,
				      TDB_DATA key, TDB_DATA dbuf,
				      void *state)
{
	char			*MId;
	uint32_t		value;
	struct emsabp_MId	*emsabp_MId = (struct emsabp_MId *) state;

	if (key.dptr) {
		if (!strncmp((const char *)key.dptr, "CN=", 3)) {
			MId = talloc_strndup(emsabp_MId, (char *)dbuf.dptr, dbuf.dsize);
			value = strtol((const char *)MId, NULL, 16);
			talloc_free(MId);
			if (value == emsabp_MId->MId) {
				emsabp_MId->dn = talloc_strndup(emsabp_MId, (char *)key.dptr, key.dsize);
				return 1;
			}
		}
	}

	return 0;
}


/**
   \details Traverse the EMSABP TDB and fetch the DN associated with
   the MId

   \param mem_ctx pointer to the memory context
   \param tdb_ctx pointer to the EMSABP TDB context
   \param MId MID to search
   \param dn pointer on pointer to the dn to return

   \return MAPI_E_SUCCESS on success, otherwise false
 */
_PUBLIC_ enum MAPISTATUS emsabp_tdb_fetch_dn_from_MId(TALLOC_CTX *mem_ctx,
						      TDB_CONTEXT *tdb_ctx,
						      uint32_t MId,
						      char **dn)
{
	int			ret;
	struct emsabp_MId	*emsabp_MId;
	
	emsabp_MId = talloc_zero(mem_ctx, struct emsabp_MId);
	emsabp_MId->dn = NULL;
	emsabp_MId->MId = MId;
	
	ret = tdb_traverse(tdb_ctx, emsabp_tdb_traverse_MId_DN, (void *)emsabp_MId);
	if (ret > -1 && emsabp_MId->dn) {
		*dn = talloc_strdup(mem_ctx, emsabp_MId->dn);
		talloc_free(emsabp_MId);
		return MAPI_E_SUCCESS;
	}

	*dn = NULL;
	talloc_free(emsabp_MId);

	return MAPI_E_NOT_FOUND;
}


/**
   \details Insert an element into TDB database

   \param tdb_ctx pointer to the EMSABP TDB context
   \param keyname pointer to the TDB key name string

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsabp_tdb_insert(TDB_CONTEXT *tdb_ctx,
					   const char *keyname)
{
	enum MAPISTATUS	retval;
	TALLOC_CTX	*mem_ctx;
	TDB_DATA	key;
	TDB_DATA	dbuf;
	char		*str;
	int		index;
	int		ret;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!tdb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!keyname, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_named(NULL, 0, "emsabp_tdb_insert");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);

	/* Step 1. Check if the record already exists */
	retval = emsabp_tdb_fetch(tdb_ctx, keyname, &dbuf);
	OPENCHANGE_RETVAL_IF(!retval, ecExiting, mem_ctx);

	/* Step 2. Retrieve the latest TDB data value inserted */
	retval = emsabp_tdb_fetch(tdb_ctx, EMSABP_TDB_DATA_REC, &dbuf);
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	str = talloc_strndup(mem_ctx, (char *)dbuf.dptr, dbuf.dsize);
	index = strtol(str, NULL, 16);
	index += 1;
	talloc_free(str);
	free(dbuf.dptr);

	dbuf.dptr = (unsigned char *)talloc_asprintf(mem_ctx, "0x%x", index);
	dbuf.dsize = strlen((const char *)dbuf.dptr);

	/* Step 3. Insert the new record */
	key.dptr = (unsigned char *)keyname;
	key.dsize = strlen(keyname);
	
	ret = tdb_store(tdb_ctx, key, dbuf, TDB_INSERT);
	OPENCHANGE_RETVAL_IF(ret == -1, MAPI_E_CORRUPT_STORE, mem_ctx);

	/* Step 4. Update Data record */
	key.dptr = (unsigned char *) EMSABP_TDB_DATA_REC;
	key.dsize = strlen((const char *)key.dptr);

	ret = tdb_store(tdb_ctx, key, dbuf, TDB_MODIFY);
	OPENCHANGE_RETVAL_IF(ret == -1, MAPI_E_CORRUPT_STORE, mem_ctx);

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
