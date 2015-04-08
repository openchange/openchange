/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2015

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


#include "mapiproxy/libmapistore/mapistore_notification.h"

/**
   \details Map memcached to mapistore error mapping

   \param rc memcached_return error

   \return matching MAPISTORE error
 */
static enum mapistore_error ret_to_mapistore(memcached_return rc)
{
	switch (rc) {
	case MEMCACHED_SUCCESS:
	case MEMCACHED_STORED:
		return MAPISTORE_SUCCESS;
	case MEMCACHED_FAILURE:
	case MEMCACHED_NOTSTORED:
		return MAPISTORE_ERROR;
	case MEMCACHED_HOST_LOOKUP_FAILURE:
	case MEMCACHED_CONNECTION_FAILURE:
		return MAPISTORE_ERR_CONN_REFUSED;
	case MEMCACHED_WRITE_FAILURE:
	case MEMCACHED_READ_FAILURE:
	case MEMCACHED_UNKNOWN_READ_FAILURE:
		return MAPISTORE_ERR_INVALID_DATA;
	case MEMCACHED_NOTFOUND:
		return MAPISTORE_ERR_NOT_FOUND;
	case MEMCACHED_ERROR:
		return MAPISTORE_ERROR;
	case MEMCACHED_DATA_EXISTS:
		return MAPISTORE_ERR_EXIST;
	default:
		oc_log(OC_LOG_WARNING, "memcached return valud %d (%s) is not mapped", rc, memcached_strerror(NULL, rc));
		return MAPISTORE_ERROR;
	};
}

/**
   \details Release the notification context used by mapistore
   notification subsystem

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int mapistore_notification_destructor(void *data)
{
	struct mapistore_notification_context	*notification_ctx = (struct mapistore_notification_context *) data;

	if (notification_ctx->memc_ctx) {
		memcached_free(notification_ctx->memc_ctx);
	}
	return true;
}

/**
   \details Initialize notification framework

   \param mem_ctx pointer to the memory context
   \param lp_ctx loadparm_context to get smb.conf options
   \param _notification_ctx pointer to the notification context to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */

enum mapistore_error mapistore_notification_init(TALLOC_CTX *mem_ctx,
						 struct loadparm_context *lp_ctx,
						 struct mapistore_notification_context **_notification_ctx)
{
	struct mapistore_notification_context	*notification_ctx = NULL;
	const char				*url = NULL;
	memcached_server_st			*servers = NULL;
	memcached_return			rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!lp_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!_notification_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	notification_ctx = talloc_zero(mem_ctx, struct mapistore_notification_context);
	MAPISTORE_RETVAL_IF(!notification_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);
	notification_ctx->memc_ctx = NULL;

	url = lpcfg_parm_string(lp_ctx, NULL, "mapistore", "notification_cache");
	if (url) {
		notification_ctx->memc_ctx = memcached(url, strlen(url));
		MAPISTORE_RETVAL_IF(!notification_ctx->memc_ctx, MAPISTORE_ERR_CONTEXT_FAILED, notification_ctx);
	} else {
		notification_ctx->memc_ctx = (memcached_st *) memcached_create(NULL);
		MAPISTORE_RETVAL_IF(!notification_ctx->memc_ctx, MAPISTORE_ERR_CONTEXT_FAILED, notification_ctx);

		servers = memcached_server_list_append(servers, MSTORE_MEMC_DFLT_HOST, 0, &rc);
		MAPISTORE_RETVAL_IF(!servers, MAPISTORE_ERR_CONTEXT_FAILED, notification_ctx);

		rc = memcached_server_push(notification_ctx->memc_ctx, servers);
		memcached_server_list_free(servers);
		MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, MAPISTORE_ERR_CONTEXT_FAILED, notification_ctx);
	}

	talloc_set_destructor((void *)notification_ctx, (int (*)(void *))mapistore_notification_destructor);

	*_notification_ctx = notification_ctx;
	return MAPISTORE_SUCCESS;
}


/**
   \details Generate the session key

   \param mem_ctx pointer to the memory context
   \param async_uuid asynchronous emsmdb session uuid to compute
   \param _key pointer on pointer to the key to return

   \note the caller is responsible for freeing _key

   \return MAPISTORE_SUCCES on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_notification_session_set_key(TALLOC_CTX *mem_ctx,
								   struct GUID async_uuid,
								   char **_key)
{
	char	*guid = NULL;
	char	*key = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!_key, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	guid = GUID_string(mem_ctx, (const struct GUID *)&async_uuid);
	MAPISTORE_RETVAL_IF(!guid, MAPISTORE_ERR_NO_MEMORY, NULL);

	key = talloc_asprintf(mem_ctx, MSTORE_MEMC_FMT_SESSION, guid);
	talloc_free(guid);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, NULL);

	*_key = key;
	return MAPISTORE_SUCCESS;
}


/**
   \details Register and bind asynchronous emsmdb session handle to
   emsmdb one

   \param mstore_ctx pointer to the mapistore context
   \param uuid the emsmdb session uuid
   \param async_uuid the asynchronous emsmdb session uuid
   \param cn the common name (username or email) to associate

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_session_add(struct mapistore_context *mstore_ctx,
								 struct GUID uuid,
								 struct GUID async_uuid,
								 const char *cn)
{
	TALLOC_CTX				*mem_ctx;
	enum mapistore_error			retval;
	struct mapistore_notification_session	r;
	struct ndr_push				*ndr;
	enum ndr_err_code			ndr_err_code;
	char					*key;
	memcached_return			rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Create session v1 blob */
	ndr = ndr_push_init_ctx(mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	ndr->offset = 0;

	r.vnum = 1;
	r.v.v1.uuid = uuid;
	r.v.v1.cn = cn;

	ndr_err_code = ndr_push_mapistore_notification_session(ndr, NDR_SCALARS, &r);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

	/* Prepare session key */
	retval = mapistore_notification_session_set_key(mem_ctx, async_uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Register the key */
	rc = memcached_add(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
			   (char *)ndr->data, ndr->offset, 0, 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Unregister and unbind specified asynchronous emsmdb
   session

   \param mstore_ctx pointer to the mstore context
   \param async_uuid the asynchronous emsmdb session uuid used for
   computing the session key to be deleted

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_session_delete(struct mapistore_context *mstore_ctx,
								    struct GUID async_uuid)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval;
	char			*key;
	memcached_return	rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare session key */
	retval = mapistore_notification_session_set_key(mem_ctx, async_uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Delete the key */
	rc = memcached_delete(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Check if an asynchronous session context exists

   \param mstore_ctx pointer to the mapistore context
   \param async_uuid the asynchronous emsmdb session uuid to lookup

   \return MAPISTORE_SUCCESS on success, otherwise
   MAPISTORE_ERR_NOT_FOUND or MAPISTORE error upon failure.
 */
_PUBLIC_ enum mapistore_error mapistore_notification_session_exist(struct mapistore_context *mstore_ctx,
								    struct GUID async_uuid)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval;
	char			*key = NULL;
	memcached_return	rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare session key */
	retval = mapistore_notification_session_set_key(mem_ctx, async_uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	rc = memcached_exist(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key));
	talloc_free(key);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve EMSMDB session UUID and user name associated to
   specified asynchronous emsmdb session UUID

   \param mem_ctx pointer to the memory context to use to allocate data to return
   \param mstore_ctx pointer to the mapistore context
   \param async_uuid the GUID of the asynchronous emsmdb session
   \param uuidp pointer to the GUID of the emsmdb session
   \param cnp pointer on pointer to the common name to return

   \note calling function is responsible for freeing cnp

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_session_get(TALLOC_CTX *mem_ctx,
								 struct mapistore_context *mstore_ctx,
								 struct GUID async_uuid,
								 struct GUID *uuid,
								 char **cnp)
{
	TALLOC_CTX				*local_mem_ctx;
	enum mapistore_error			retval;
	enum ndr_err_code			ndr_err_code;
	struct ndr_pull				*ndr;
	struct mapistore_notification_session	r;
	DATA_BLOB				blob;
	char					*key = NULL;
	char					*value = NULL;
	size_t					value_len = 0;
	memcached_return_t			rc;
	uint32_t				flags;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!uuid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!cnp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	local_mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!local_mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Retrieve key/value */
	retval = mapistore_notification_session_set_key(local_mem_ctx, async_uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, local_mem_ctx);

	value = memcached_get(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), &value_len,
			      &flags, &rc);
	talloc_free(key);
	MAPISTORE_RETVAL_IF(!value, ret_to_mapistore(rc), local_mem_ctx);

	/* Unpack session structure */
	blob.data = (uint8_t *) value;
	blob.length = value_len;

	ndr = ndr_pull_init_blob(&blob, local_mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, local_mem_ctx);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_err_code = ndr_pull_mapistore_notification_session(ndr, NDR_SCALARS, &r);
	talloc_free(ndr);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERROR, local_mem_ctx);

	*uuid = r.v.v1.uuid;
	*cnp = talloc_strdup(mem_ctx, r.v.v1.cn);
	MAPISTORE_RETVAL_IF(!*cnp, MAPISTORE_ERR_NO_MEMORY, local_mem_ctx);

	talloc_free(local_mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Generate the resolver key

   \param mem_ctx pointer to the memory context
   \param cn the common name to compute
   \param _key pointer on pointer to the key to return

   \note the caller is responsible for freeing _key

   \return MAPISTORE_SUCCES on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_notification_resolver_set_key(TALLOC_CTX *mem_ctx,
								    const char *cn,
								    char **_key)
{
	char	*key = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!strlen(cn), MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!_key, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	key = talloc_asprintf(mem_ctx, MSTORE_MEMC_FMT_RESOLVER, cn);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, NULL);

	*_key = key;
	return MAPISTORE_SUCCESS;
}


/**
   \details Add a record to the resolver

   \param mstore_ctx pointer to the mapistore context
   \param cn the common name (key) to use for host registration
   \param host the host to register

   \note This function acts as a wrapper and manages both the creation
   of a new key/value pair and the update of an existing record. The
   following logic is implemented:

   [START] Does the key exist?
	If yes: Update record
	If no: Insert key

   \todo Note that a race condition is possible between the exist
   check and the addition of the record, where the same key could have
   been created by another instance, leading to the failure of the add
   operation. While this scenario is very unlikely to happen in real
   world, a retry with a counter should be implemented in further
   iterations to reasonably circumvent this use case.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_resolver_add(struct mapistore_context *mstore_ctx,
								  const char *cn,
								  const char *host)
{
	TALLOC_CTX				*mem_ctx;
	enum mapistore_error			retval;
	struct mapistore_notification_resolver	r;
	struct ndr_push				*ndr;
	enum ndr_err_code			ndr_err_code;
	memcached_return			rc = MEMCACHED_ERROR;
	char					*key = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!host, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	/* memcached does not allow key with space */
	if (strchr(cn, ' ')) {
		OC_DEBUG(0, "space not allowed in cn field: '%s'", cn);
		return MAPISTORE_ERR_INVALID_DATA;
	}

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare key */
	retval = mapistore_notification_resolver_set_key(mem_ctx, cn, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Create resolver v1 value */
	ndr = ndr_push_init_ctx(mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	ndr->offset = 0;

	/* If the key already exist: update if no duplicate */
	retval = mapistore_notification_resolver_exist(mstore_ctx, cn);
	if (retval == MAPISTORE_SUCCESS) {
		uint32_t	count = 0;
		uint32_t	i = 0;
		const char	**hosts = NULL;

		retval = mapistore_notification_resolver_get(mem_ctx, mstore_ctx, cn, &count, &hosts);
		MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

		for (i = 0; i < count; i++) {
			if (hosts[i] && !strncmp(hosts[i], host, strlen(host))) {
				OC_DEBUG(0, "host '%s' is already registered for cn '%s'", host, cn);
				talloc_free(mem_ctx);
				return MAPISTORE_ERR_EXIST;
			}
		}

		r.vnum = 1;
		r.v.v1.count = count + 1;
		r.v.v1.hosts = talloc_array(mem_ctx, const char *, r.v.v1.count);
		for (i = 0; i < count; i++) {
			r.v.v1.hosts[i] = talloc_strdup(r.v.v1.hosts, hosts[i]);
			MAPISTORE_RETVAL_IF(!r.v.v1.hosts[i], MAPISTORE_ERR_NO_MEMORY, mem_ctx);
		}
		r.v.v1.hosts[count] = talloc_strdup(r.v.v1.hosts, host);
		MAPISTORE_RETVAL_IF(!r.v.v1.hosts[count], MAPISTORE_ERR_NO_MEMORY, mem_ctx);

		ndr_err_code = ndr_push_mapistore_notification_resolver(ndr, NDR_SCALARS, &r);
		MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

		/* Add the key/value record */
		rc = memcached_set(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
				   (char *)ndr->data, ndr->offset, 0, 0);
	} else if (retval == MAPISTORE_ERR_NOT_FOUND) {
		r.vnum = 1;
		r.v.v1.count = 1;
		r.v.v1.hosts = talloc_array(mem_ctx, const char *, 1);
		MAPISTORE_RETVAL_IF(!r.v.v1.hosts, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
		r.v.v1.hosts[0] = talloc_strdup(r.v.v1.hosts, host);
		MAPISTORE_RETVAL_IF(!r.v.v1.hosts[0], MAPISTORE_ERR_NO_MEMORY, mem_ctx);

		ndr_err_code = ndr_push_mapistore_notification_resolver(ndr, NDR_SCALARS, &r);
		MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

		/* Add the key/value record */
		rc = memcached_add(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
				   (char *)ndr->data, ndr->offset, 0, 0);
	}

	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Get resolver data for a given resolver entry

   \param mem_ctx pointer to the memory context
   \param mstore_ctx pointer to the mapistore context
   \param cn the resolver key to lookup
   \param countp pointer on the number of hosts returned
   \param hostsp pointer on the list of host to return

   \note calling function is responsible for freeing hostsp

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_resolver_get(TALLOC_CTX *mem_ctx,
								  struct mapistore_context *mstore_ctx,
								  const char *cn, uint32_t *countp,
								  const char ***hostsp)
{
	TALLOC_CTX				*local_mem_ctx;
	enum mapistore_error			retval;
	enum ndr_err_code			ndr_err_code;
	struct ndr_pull				*ndr;
	struct mapistore_notification_resolver	r;
	DATA_BLOB				blob;
	char					*key = NULL;
	char					*value = NULL;
	size_t					value_len = 0;
	memcached_return_t			rc;
	uint32_t				flags;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!countp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!hostsp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	local_mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!local_mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	retval = mapistore_notification_resolver_set_key(local_mem_ctx, cn, &key);
	MAPISTORE_RETVAL_IF(retval, retval, local_mem_ctx);

	value = memcached_get(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), &value_len,
			      &flags, &rc);
	talloc_free(key);
	MAPISTORE_RETVAL_IF(!value, ret_to_mapistore(rc), local_mem_ctx);

	/* Unpack resolver structure */
	blob.data = (uint8_t *) value;
	blob.length = value_len;

	ndr = ndr_pull_init_blob(&blob, local_mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, local_mem_ctx);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_err_code = ndr_pull_mapistore_notification_resolver(ndr, NDR_SCALARS, &r);
	talloc_free(ndr);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, local_mem_ctx);

	*countp = r.v.v1.count;
	*hostsp = talloc_steal(mem_ctx, r.v.v1.hosts);
	MAPISTORE_RETVAL_IF(!hostsp, MAPISTORE_ERR_NO_MEMORY, local_mem_ctx);

	talloc_free(local_mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Unregister a host from a resolver entry

   \param mstore_ctx pointer to the mapistore context
   \param cn the resolver key to lookup
   \param host the host entry to delete within record

   \note If the record has no longer host entry, the function deletes
   the record

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_resolver_delete(struct mapistore_context *mstore_ctx,
								     const char *cn, const char *host)
{
	TALLOC_CTX				*mem_ctx;
	memcached_return			rc;
	struct mapistore_notification_resolver	r;
	enum mapistore_error			retval;
	enum ndr_err_code			ndr_err_code;
	struct ndr_push				*ndr;
	char					*key;
	uint32_t				count;
	const char				**hosts = NULL;
	uint32_t				i, j;
	int					index = -1;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!host, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Get the record and fetch host entry index */
	retval = mapistore_notification_resolver_get(mem_ctx, mstore_ctx, cn, &count, &hosts);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	for (i = 0; i < count; i++) {
		if (hosts[i] && !strncmp(hosts[i], host, strlen(host))) {
			index = i;
			break;
		}
	}
	MAPISTORE_RETVAL_IF(index == -1, MAPISTORE_ERR_NOT_FOUND, mem_ctx);

	/* Prepare the resolver key */
	retval = mapistore_notification_resolver_set_key(mem_ctx, cn, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* If host is the only entry, delete the record */
	if (count == 1) {
		rc = memcached_delete(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), 0);
		MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);
		goto end;
	}

	/* Otherwise remove the entry from the array and update record */
	ndr = ndr_push_init_ctx(mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	ndr->offset = 0;

	r.vnum = 1;
	r.v.v1.count = count - 1;
	r.v.v1.hosts = talloc_array(mem_ctx, const char *, r.v.v1.count);

	MAPISTORE_RETVAL_IF(!r.v.v1.hosts, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	for (i = 0, j = 0; i < count; i++) {
		if (i != index) {
			r.v.v1.hosts[j] = talloc_strdup(r.v.v1.hosts, hosts[i]);
			MAPISTORE_RETVAL_IF(!r.v.v1.hosts[j], MAPISTORE_ERR_NO_MEMORY, mem_ctx);
			j++;
		}
	}

	ndr_err_code = ndr_push_mapistore_notification_resolver(ndr, NDR_SCALARS, &r);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

	rc = memcached_set(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
			   (char *)ndr->data, ndr->offset, 0, 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

end:
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Check if the resolver entry pointer by cn exist

   \param mstore_ctx pointer to the mapistore context
   \param cn the common name to lookup

   \return MAPISTORE_SUCCESS on success, otherwise
   MAPISTORE_ERR_NOT_FOUND or MAPISTORE generic error upon other
   failures.
 */
_PUBLIC_ enum mapistore_error mapistore_notification_resolver_exist(struct mapistore_context *mstore_ctx,
								    const char *cn)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval;
	char			*key = NULL;
	memcached_return	rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!cn, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare the resolver key */
	retval = mapistore_notification_resolver_set_key(mem_ctx, cn, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	rc = memcached_exist(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key));
	talloc_free(key);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Generate the subscription key

   \param mem_ctx pointer to the memory context
   \param uuid the uuid of the session to compute
   \param pointer on pointer to the key to return

   \note the caller is responsible for freeing _key

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error mapistore_notification_subscription_set_key(TALLOC_CTX *mem_ctx,
									struct GUID uuid,
									char **_key)
{
	char	*guid = NULL;
	char	*key = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!_key, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	guid = GUID_string(mem_ctx, (const struct GUID *)&uuid);
	MAPISTORE_RETVAL_IF(!guid, MAPISTORE_ERR_NO_MEMORY, NULL);

	key = talloc_asprintf(mem_ctx, MSTORE_MEMC_FMT_SUBSCRIPTION, guid);
	talloc_free(guid);
	MAPISTORE_RETVAL_IF(!key, MAPISTORE_ERR_NO_MEMORY, NULL);

	*_key = key;
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve all the subscriptions associated to a uuid

   \param mem_ctx pointer to the memory context
   \param mstore_ctx pointer to the mapistore context
   \param uuid the UUID session to lookup
   \param r pointer to the mapistore_notification_subscription to return

   \note the caller is responsible for freeing memory associated to
   mapistore_notification_subscription

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_notification_subscription_get(TALLOC_CTX *mem_ctx,
							     struct mapistore_context *mstore_ctx,
							     struct GUID uuid,
							     struct mapistore_notification_subscription *_r)
{
	TALLOC_CTX					*local_mem_ctx;
	enum mapistore_error				retval;
	enum ndr_err_code				ndr_err_code;
	struct ndr_pull					*ndr;
	DATA_BLOB					blob;
	struct mapistore_notification_subscription	r;
	char						*key;
	char						*value = NULL;
	size_t						value_len = 0;
	memcached_return_t				rc;
	uint32_t					flags;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!_r, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	local_mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!local_mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Retrieve key/value */
	retval = mapistore_notification_subscription_set_key(local_mem_ctx, uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, local_mem_ctx);

	value = memcached_get(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), &value_len,
			      &flags, &rc);
	talloc_free(key);
	MAPISTORE_RETVAL_IF(!value, ret_to_mapistore(rc), local_mem_ctx);

	/* Unpack subscription structure */
	blob.data = (uint8_t *) value;
	blob.length = value_len;

	ndr = ndr_pull_init_blob(&blob, mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, local_mem_ctx);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_err_code = ndr_pull_mapistore_notification_subscription(ndr, NDR_SCALARS, &r);
	talloc_free(ndr);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERROR, local_mem_ctx);

	*_r = r;

	talloc_free(local_mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Add a subscription entry for a MAPI object

   \param mstore_ctx pointer to the mapistore context
   \param uuid the session UUID
   \param handle the handle to associate this subscription with
   \param flags the notification bitmask to associate to this subscription
   \param fid the FolderID where relevant
   \param mid the MessageId where relevant
   \param count the number of properties when subscribing for a table events
   \param properties pointer on an array of MAPI properties to add for table events

   \note subscription are uniquely identified by the com

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_subscription_add(struct mapistore_context *mstore_ctx,
								      struct GUID uuid,
								      uint32_t handle,
								      uint16_t flags,
								      uint64_t fid,
								      uint64_t mid,
								      uint32_t count,
								      enum MAPITAGS *properties)
{
	TALLOC_CTX					*mem_ctx;
	enum mapistore_error				retval;
	struct mapistore_notification_subscription	r;
	struct ndr_push					*ndr;
	enum ndr_err_code				ndr_err_code;
	memcached_return				rc = MEMCACHED_ERROR;
	char						*key = NULL;


	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(count && !properties, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare key */
	retval = mapistore_notification_subscription_set_key(mem_ctx, uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Create subscription v1 blob */
	ndr = ndr_push_init_ctx(mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	ndr->offset = 0;

	retval = mapistore_notification_subscription_exist(mstore_ctx, uuid);
	if (retval == MAPISTORE_SUCCESS) {
		uint32_t	i = 0;
		uint32_t	last = 0;

		retval = mapistore_notification_subscription_get(mem_ctx, mstore_ctx, uuid, &r);
		MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

		/* Check if the subscription does not already exist */
		for (i = 0; i < r.v.v1.count; i++) {
			if (r.v.v1.subscription[i].handle == handle) {
				OC_DEBUG(0, "subscription with handle=0x%x already exist", handle);
				talloc_free(mem_ctx);
				return MAPISTORE_ERR_EXIST;
			}
		}

		r.vnum = 1;
		last = r.v.v1.count;
		r.v.v1.count = r.v.v1.count + 1;
		r.v.v1.subscription = talloc_realloc(mem_ctx, r.v.v1.subscription, struct subscription_object_v1, r.v.v1.count + 1);
		r.v.v1.subscription[last].handle = handle;
		r.v.v1.subscription[last].flags = flags;
		r.v.v1.subscription[last].fid = fid;
		r.v.v1.subscription[last].mid = mid;
		r.v.v1.subscription[last].count = count;
		r.v.v1.subscription[last].properties = (uint32_t *)properties;

		ndr_err_code = ndr_push_mapistore_notification_subscription(ndr, NDR_SCALARS, &r);
		MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

		/* Add the key/value record */
		rc = memcached_set(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
				   (char *) ndr->data, ndr->offset, 0, 0);

	} else {
		r.vnum = 1;
		r.v.v1.count = 1;
		r.v.v1.subscription = talloc_array(mem_ctx, struct subscription_object_v1, 1);
		MAPISTORE_RETVAL_IF(!r.v.v1.subscription, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
		r.v.v1.subscription[0].handle = handle;
		r.v.v1.subscription[0].flags = flags;
		r.v.v1.subscription[0].fid = fid;
		r.v.v1.subscription[0].mid = mid;
		r.v.v1.subscription[0].count = count;
		r.v.v1.subscription[0].properties = (uint32_t *)properties;

		ndr_err_code = ndr_push_mapistore_notification_subscription(ndr, NDR_SCALARS, &r);
		MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

		/* Add the key/value record */
		rc = memcached_add(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
				   (char *)ndr->data, ndr->offset, 0, 0);
	}

	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \detaild Check if a subscription record exist for the given session UUID

   \param mstore_ctx pointer to the mapistore context
   \param uuid the session UUID to lookup

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERR_NOT_FOUND
 */
_PUBLIC_ enum mapistore_error mapistore_notification_subscription_exist(struct mapistore_context *mstore_ctx,
									struct GUID uuid)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval;
	char			*key = NULL;
	memcached_return	rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare subscription key */
	retval = mapistore_notification_subscription_set_key(mem_ctx, uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	rc = memcached_exist(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key));
	talloc_free(key);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Delete the subscription record associated to specified session UUID

   \param mstore_ctx pointer to the mapistore context
   \param uuid the session UUID

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_subscription_delete(struct mapistore_context *mstore_ctx,
									 struct GUID uuid)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval;
	char			*key;
	memcached_return	rc;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Prepare the subscription key */
	retval = mapistore_notification_subscription_set_key(mem_ctx, uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* Delete the key */
	rc = memcached_delete(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Delete the subscription record associated to specified
   session UUID and referenced by specified handle

   \param mstore_ctx pointer to the mapistore context
   \param uuid the session uuid
   \param handle the handle of the entry to delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_notification_subscription_delete_by_handle(struct mapistore_context *mstore_ctx,
										   struct GUID uuid,
										   uint32_t handle)
{
	TALLOC_CTX					*mem_ctx;
	memcached_return				rc;
	struct mapistore_notification_subscription	r, _r;
	enum mapistore_error				retval;
	enum ndr_err_code				ndr_err_code;
	struct ndr_push					*ndr;
	char						*key;
	uint32_t					i, j;
	int						index = -1;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);
	MAPISTORE_RETVAL_IF(!mstore_ctx->notification_ctx->memc_ctx, MAPISTORE_ERR_NOT_AVAILABLE, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Get the record */
	retval = mapistore_notification_subscription_get(mem_ctx, mstore_ctx, uuid, &r);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	for (i = 0; i < r.v.v1.count; i++) {
		if (r.v.v1.subscription[i].handle == handle) {
			index = i;
			break;
		}
	}
	MAPISTORE_RETVAL_IF(index == -1, MAPISTORE_ERR_NOT_FOUND, mem_ctx);

	/* Prepare the subscription key */
	retval = mapistore_notification_subscription_set_key(mem_ctx, uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, mem_ctx);

	/* If we only have one entry left, delete the record */
	if (r.v.v1.count == 1) {
		rc = memcached_delete(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), 0);
		MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);
		goto end;
	}

	/* Otherwise remove the entry from the array and update record */
	ndr = ndr_push_init_ctx(mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	ndr->offset = 0;

	_r.vnum = 1;
	_r.v.v1.count = r.v.v1.count - 1;
	_r.v.v1.subscription = talloc_array(mem_ctx, struct subscription_object_v1, _r.v.v1.count + 1);
	MAPISTORE_RETVAL_IF(!_r.v.v1.subscription, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	for (i = 0, j = 0; i < r.v.v1.count; i++) {
		if (i != index) {
			_r.v.v1.subscription[j] = r.v.v1.subscription[i];
			j++;
		}
	}

	ndr_err_code = ndr_push_mapistore_notification_subscription(ndr, NDR_SCALARS, &_r);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERR_INVALID_DATA, mem_ctx);

	rc = memcached_set(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key),
			   (char *)ndr->data, ndr->offset, 0, 0);
	MAPISTORE_RETVAL_IF(rc != MEMCACHED_SUCCESS, ret_to_mapistore(rc), mem_ctx);

end:
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

