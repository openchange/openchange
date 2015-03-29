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

	/* Retrieve key/value */
	retval = mapistore_notification_session_set_key(mem_ctx, async_uuid, &key);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	value = memcached_get(mstore_ctx->notification_ctx->memc_ctx, key, strlen(key), &value_len,
			      &flags, &rc);
	talloc_free(key);
	MAPISTORE_RETVAL_IF(!value, ret_to_mapistore(rc), NULL);

	/* Unpack session structure */
	blob.data = (uint8_t *) value;
	blob.length = value_len;

	ndr = ndr_pull_init_blob(&blob, mem_ctx);
	MAPISTORE_RETVAL_IF(!ndr, MAPISTORE_ERR_NO_MEMORY, NULL);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_err_code = ndr_pull_mapistore_notification_session(ndr, NDR_SCALARS, &r);
	talloc_free(ndr);
	MAPISTORE_RETVAL_IF(ndr_err_code != NDR_ERR_SUCCESS, MAPISTORE_ERROR, NULL);

	*uuid = r.v.v1.uuid;
	*cnp = talloc_strdup(mem_ctx, r.v.v1.cn);
	MAPISTORE_RETVAL_IF(!*cnp, MAPISTORE_ERR_NO_MEMORY, NULL);

	return MAPISTORE_SUCCESS;
}
