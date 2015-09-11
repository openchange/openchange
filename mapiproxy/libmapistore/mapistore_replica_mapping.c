/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Wolfgang Sourdeau 2011

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

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include "utils/dlinklist.h"
#include "libmapi/libmapi_private.h"

#include <tdb.h>

static void mapistore_replica_mapping_set_next_replid(struct tdb_context *, uint16_t);
static uint16_t mapistore_replica_mapping_get_next_replid(struct tdb_context *);

/* Special repl ids:
   - 0x01 is for server replica
   - 0x02 is for GetLocalReplicaIDs */

/**
   \details Search the replica_mapping record matching the username

   \param mstore_ctx pointer to the mapistore context
   \param username the username to lookup

   \return pointer to the tdb_wrap structure on success, otherwise NULL
 */
static struct replica_mapping_context_list *mapistore_replica_mapping_search(struct mapistore_context *mstore_ctx, const char *username)
{
	struct replica_mapping_context_list	*el;

	/* Sanity checks */
	if (!mstore_ctx) return NULL;
	if (!mstore_ctx->replica_mapping_list) return NULL;
	if (!username) return NULL;

	for (el = mstore_ctx->replica_mapping_list; el; el = el->next) {
		if (el && el->username && !strcmp(el->username, username)) {
			return el;
		}
	}

	return NULL;
}

/**
   \details Open connection to replica_mapping database for a given user

   \param mstore_ctx pointer to the mapistore context
   \param username name for which the replica_mapping database has to be
   created

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */

static int context_list_destructor(struct replica_mapping_context_list *rmctx)
{
	tdb_close(rmctx->tdb);

	return 1;
}

_PUBLIC_ enum mapistore_error mapistore_replica_mapping_add(struct mapistore_context *mstore_ctx, const char *username, struct replica_mapping_context_list **rmctxp)
{
	TALLOC_CTX				*mem_ctx;
	struct replica_mapping_context_list	*rmctx;
	char					*dbpath = NULL;
	char					*mapistore_dir = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL);

	/* Step 1. Search if the context already exists */
	rmctx = mapistore_replica_mapping_search(mstore_ctx, username);
	*rmctxp = rmctx;
	MAPISTORE_RETVAL_IF(rmctx, MAPISTORE_SUCCESS, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapistore_replica_mapping_init");

	/* ensure the user mapistore directory exists before any mapistore operation occurs */
	mapistore_dir = talloc_asprintf(mem_ctx, "%s/%s",
					mapistore_get_mapping_path(), username);
	MAPISTORE_RETVAL_IF(!mapistore_dir, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	mkdir(mapistore_dir, 0700);

	/* Step 1. Open/Create the replica_mapping database */
	dbpath = talloc_asprintf(mem_ctx, "%s/%s/" MAPISTORE_DB_REPLICA_MAPPING, 
				 mapistore_get_mapping_path(), username);
	MAPISTORE_RETVAL_IF(!dbpath, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	rmctx = talloc_zero(mstore_ctx->replica_mapping_list, struct replica_mapping_context_list);
	MAPISTORE_RETVAL_IF(!rmctx, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	rmctx->tdb = tdb_open(dbpath, 0, 0, O_RDWR|O_CREAT, 0600);
	if (!rmctx->tdb) {
		OC_DEBUG(3, "%s (%s)", strerror(errno), dbpath);
		talloc_free(rmctx);
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_DATABASE_INIT;
	}
	talloc_set_destructor(rmctx, context_list_destructor);
	rmctx->username = talloc_strdup(rmctx, username);
	rmctx->ref_count = 0;
	DLIST_ADD_END(mstore_ctx->replica_mapping_list, rmctx, struct replica_mapping_context_list *);

	*rmctxp = rmctx;

	/* Step 2. Initialize database if it freshly created */
	if (mapistore_replica_mapping_get_next_replid(rmctx->tdb) == 0xffff) {
		mapistore_replica_mapping_set_next_replid(rmctx->tdb, 0x3);
	}

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

/* _PUBLIC_ enum mapistore_error mapistore_replica_mapping_add(struct mapistore_context *mstore_ctx, const char *username) */
/* { */
/* 	TALLOC_CTX			*mem_ctx; */
/* 	char				*dbpath = NULL; */

/* 	/\* Sanity checks *\/ */
/* 	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL); */
/* 	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERROR, NULL); */

/* 	mem_ctx = talloc_named(NULL, 0, "mapistore_replica_mapping_add"); */

/* 	/\* Step 1. Open/Create the replica_mapping database *\/ */
/* 	dbpath = talloc_asprintf(mem_ctx, "%s/%s/" MAPISTORE_DB_REPLICA_MAPPING, */
/* 				 mapistore_get_mapping_path(), username); */
/* 	mstore_ctx->replica_mapping_ctx = tdb_wrap_open(mstore_ctx, dbpath, 0, 0, O_RDWR|O_CREAT, 0600); */
/* 	talloc_free(dbpath); */
/* 	if (!mstore_ctx->replica_mapping_ctx) { */
/* 		OC_DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, strerror(errno))); */
/* 		talloc_free(mem_ctx); */
/* 		return MAPISTORE_ERR_DATABASE_INIT; */
/* 	} */
/* 	if (mapistore_replica_mapping_get_next_replid(mstore_ctx->replica_mapping_ctx->tdb) == 0xffff) { */
/* 		mapistore_replica_mapping_set_next_replid(mstore_ctx->replica_mapping_ctx->tdb, 0x3); */
/* 	} */

/* 	talloc_free(mem_ctx); */

/* 	return MAPI_E_SUCCESS; */
/* } */

static void mapistore_replica_mapping_set_next_replid(struct tdb_context *tdb, uint16_t replid) {
	TDB_DATA	key;
	TDB_DATA	replid_data;
	void		*mem_ctx;
	int		ret, op;

	mem_ctx = talloc_zero(NULL, void);

	/* GUID to ReplID */
	key.dptr = (unsigned char *) "next_replid";
	key.dsize = strlen((const char *) key.dptr);

	replid_data.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%.4x", replid);
	replid_data.dsize = strlen((const char *) replid_data.dptr);

	ret = tdb_exists(tdb, key);
	op = (ret) ? TDB_MODIFY : TDB_INSERT;	
	tdb_store(tdb, key, replid_data, op);

	talloc_free(mem_ctx);
}

static uint16_t mapistore_replica_mapping_get_next_replid(struct tdb_context *tdb) {
	TDB_DATA	key;
	TDB_DATA	replid_data;
	int		ret;
	char		*tmp_data;
	uint16_t	replid;

	/* GUID to ReplID */
	key.dptr = (unsigned char *) "next_replid";
	key.dsize = strlen((const char *) key.dptr);

	ret = tdb_exists(tdb, key);
	if (!ret) {
		return 0xffff;
	}

	replid_data = tdb_fetch(tdb, key);

	tmp_data = talloc_strndup(NULL, (char *) replid_data.dptr, replid_data.dsize);
	replid = strtoul(tmp_data, NULL, 16);
	talloc_free(tmp_data);

	return replid;
}

static void mapistore_replica_mapping_add_pair(struct tdb_context *tdb, const struct GUID *guidP, uint16_t replid)
{
	TDB_DATA	guid_key;
	TDB_DATA	replid_key;
	void		*mem_ctx;

	mem_ctx = talloc_zero(NULL, void);

	/* GUID to ReplID */
	guid_key.dptr = (unsigned char *) GUID_string(mem_ctx, guidP);
	guid_key.dsize = strlen((const char *) guid_key.dptr);

	replid_key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%.4x", replid);
	replid_key.dsize = strlen((const char *) replid_key.dptr);

	tdb_store(tdb, guid_key, replid_key, TDB_INSERT);
	tdb_store(tdb, replid_key, guid_key, TDB_INSERT);

	talloc_free(mem_ctx);
}

static enum mapistore_error mapistore_replica_mapping_search_guid(struct tdb_context *tdb, const struct GUID *guidP, uint16_t *replidP)
{
	TDB_DATA	guid_key;
	TDB_DATA	replid_key;
	void		*mem_ctx;
	int		ret;

	mem_ctx = talloc_zero(NULL, void);

	guid_key.dptr = (unsigned char *) GUID_string(mem_ctx, guidP);
	guid_key.dsize = strlen((const char *) guid_key.dptr);

	ret = tdb_exists(tdb, guid_key);
	if (!ret) {
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}

	replid_key = tdb_fetch(tdb, guid_key);
	*replidP = strtoul((char *) replid_key.dptr + 2, NULL, 16);

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

/**
   \details Search a replica guid in the database, creates it if it does not exist

   \param mstore_ctx pointer to the mapistore context
   \param guidP the replica guid
   \param replidP pointer to the returned replica id

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_replica_mapping_guid_to_replid(struct mapistore_context *mstore_ctx, const char *username, const struct GUID *guidP, uint16_t *replidP)
{
	int		ret;
	uint16_t	new_replid;
	struct replica_mapping_context_list *list;

	MAPISTORE_RETVAL_IF(!guidP, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!replidP, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ret = mapistore_replica_mapping_add(mstore_ctx, username, &list);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!list, MAPISTORE_ERROR, NULL);

	ret = mapistore_replica_mapping_search_guid(list->tdb, guidP, replidP);
	if (ret == MAPISTORE_SUCCESS) {
		return ret;
	}

	new_replid = mapistore_replica_mapping_get_next_replid(list->tdb);
	if (new_replid == 0xffff) { /* should never occur */
		oc_log(OC_LOG_FATAL, "next replica id is not configured for this database");
		return MAPISTORE_ERROR;
	}

	mapistore_replica_mapping_add_pair(list->tdb, guidP, new_replid);
	mapistore_replica_mapping_set_next_replid(list->tdb, new_replid + 1);

	*replidP = new_replid;

	return MAPISTORE_SUCCESS;
}

/**
   \details Search a replica id in the database

   \param mstore_ctx pointer to the mapistore context
   \param replid the replica id
   \param guidP pointer to the returned replica guid

   \return MAPISTORE_SUCCESS on success, otherwise a MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_replica_mapping_replid_to_guid(struct mapistore_context *mstore_ctx, const char *username, uint16_t replid, struct GUID *guidP)
{
	void					*mem_ctx;
	TDB_DATA				guid_key, replid_key;
	int					ret;
	struct replica_mapping_context_list	*list;

	MAPISTORE_RETVAL_IF(!guidP, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	ret = mapistore_replica_mapping_add(mstore_ctx, username, &list);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);
	MAPISTORE_RETVAL_IF(!list, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_zero(NULL, void);

	replid_key.dptr = (unsigned char *) talloc_asprintf(mem_ctx, "0x%.4x", replid);
	replid_key.dsize = strlen((const char *) replid_key.dptr);

	ret = tdb_exists(list->tdb, replid_key);
	if (!ret) {
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	guid_key = tdb_fetch(list->tdb, replid_key);
	GUID_from_string((char *) guid_key.dptr, guidP);

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}
