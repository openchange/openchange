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

/**
   \file mapistore_mgmt.c

   \brief Provides mapistore management routine for
   administrative/services tools. Using this interface virtually
   restrict mapistore features to the specific management functions
   subset.
 */

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/mapistore_mgmt.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.h"

static void mgmt_user_process_notif(struct mapistore_mgmt_context *mgmt_ctx,
				    DATA_BLOB data)
{
	struct mapistore_mgmt_user_cmd	user_cmd;
	struct ndr_pull			*ndr_pull = NULL;
	struct ndr_print		*ndr_print;

	ndr_pull = ndr_pull_init_blob(&data, (TALLOC_CTX *)mgmt_ctx);
	ndr_pull_mapistore_mgmt_user_cmd(ndr_pull, NDR_SCALARS|NDR_BUFFERS, &user_cmd);

	ndr_print = talloc_zero((TALLOC_CTX *)mgmt_ctx, struct ndr_print);
	ndr_print->print = ndr_print_printf_helper;
	ndr_print->depth = 1;
	ndr_print_mapistore_mgmt_user_cmd(ndr_print, "user command", &user_cmd);

	talloc_free(ndr_print);
	talloc_free(ndr_pull);
}

static void mgmt_user_notif_handler(int signo, siginfo_t *info, void *unused)
{
	struct mapistore_mgmt_context	*mgmt_ctx;
	struct mq_attr			attr;
	struct sigevent			se;
	DATA_BLOB			data;
	unsigned int			prio;

	mgmt_ctx = (struct mapistore_mgmt_context *)info->si_value.sival_ptr;

	if (mq_getattr(mgmt_ctx->mq_users, &attr) != 0) {
		perror("mq_getattr");
		return;
	}

	data.data = talloc_size(mgmt_ctx, attr.mq_msgsize);
	data.length = mq_receive(mgmt_ctx->mq_users, (char *)data.data, attr.mq_msgsize, &prio);
	if (data.length == -1) {
		talloc_free(data.data);
		perror("mq_receive");
		return;
	}
	mgmt_user_process_notif(mgmt_ctx, data);
	talloc_free(data.data);

	/* Reset notification as it resets each time */
	se.sigev_signo = info->si_signo;
	se.sigev_value = info->si_value;
	se.sigev_notify = SIGEV_SIGNAL;
	if (mq_notify(mgmt_ctx->mq_users, &se) == -1) {
		perror("mq_notify");
		return;
	}

	data.data = talloc_size(mgmt_ctx, attr.mq_msgsize);
	while ((data.length = mq_receive(mgmt_ctx->mq_users, (char *)data.data, attr.mq_msgsize, NULL)) != -1) {
		if (data.length == -1) {
			talloc_free(data.data);
			return;
		} else {
			mgmt_user_process_notif(mgmt_ctx, data);
			talloc_free(data.data);
			data.data = talloc_size(mgmt_ctx, attr.mq_msgsize);
		}
	}

	talloc_free(data.data);
}

/**
   \details Initialize a mapistore manager context.

   \param mstore_ctx Pointer to an existing mapistore_context

   \return allocated mapistore_mgmt context on success, otherwise NULL
 */
_PUBLIC_ struct mapistore_mgmt_context *mapistore_mgmt_init(struct mapistore_context *mstore_ctx)
{
	struct mapistore_mgmt_context	*mgmt_ctx;
	int				ret;
	unsigned int			prio;
	DATA_BLOB			data;
	struct mq_attr			attr;
	struct sigaction		sa;
	struct sigevent			se;

	if (!mstore_ctx) return NULL;

	mgmt_ctx = talloc_zero(mstore_ctx, struct mapistore_mgmt_context);
	if (!mgmt_ctx) {
		return NULL;
	}

	mgmt_ctx->mstore_ctx = mstore_ctx;
	mgmt_ctx->mq_users = mq_open(MAPISTORE_MQUEUE_USER, O_RDONLY|O_NONBLOCK|O_CREAT, 0755, NULL);
	if (mgmt_ctx->mq_users == -1) {
		perror("mq_open");
		talloc_free(mgmt_ctx);
		return NULL;
	}

	/* Setup asynchronous notification request on this message queue */
	sa.sa_sigaction = mgmt_user_notif_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGIO, &sa, NULL) == -1) {
		perror("sigaction");
		talloc_free(mgmt_ctx);
		return NULL;
	}
	se.sigev_notify = SIGEV_SIGNAL;
	se.sigev_signo = SIGIO;
	se.sigev_value.sival_ptr = (void *) mgmt_ctx;
	if (mq_notify(mgmt_ctx->mq_users, &se) == -1) {
		perror("mq_notify");
		talloc_free(mgmt_ctx);
		return NULL;
	}

	/* Empty queue since new notification only occurs flushed/empty queue */
	ret = mq_getattr(mgmt_ctx->mq_users, &attr);
	if (ret == -1) {
		perror("mq_getattr");
		return mgmt_ctx;
	}
	data.data = talloc_size(mgmt_ctx, attr.mq_msgsize);

	while ((data.length = mq_receive(mgmt_ctx->mq_users, (char *)data.data, attr.mq_msgsize, &prio)) != -1) {
		mgmt_user_process_notif(mgmt_ctx, data);
		talloc_free(data.data);
		data.data = talloc_size(mgmt_ctx, attr.mq_msgsize);
	}
	talloc_free(data.data);

	return mgmt_ctx;
}


/**
   \details Release  the mapistore management context  and destory any
   data associated.

   \param mgmt_ctx pointer to the mapistore management context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_mgmt_release(struct mapistore_mgmt_context *mgmt_ctx)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mgmt_ctx->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	if (mq_close(mgmt_ctx->mq_users) == -1) {
		perror("mq_close");
		talloc_free(mgmt_ctx);
		return MAPISTORE_SUCCESS;
	}

	if (mq_unlink(MAPISTORE_MQUEUE_USER) == -1) {
		perror("mq_unlink");
	}

	talloc_free(mgmt_ctx);

	return MAPISTORE_SUCCESS;
}

/**
   \details Check if the specified backend is registered in mapistore

   \param mgmt_ctx pointer to the mapistore management context
   \param backend pointer to the backend name

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_mgmt_registered_backend(struct mapistore_mgmt_context *mgmt_ctx, const char *backend)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(backend == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(mgmt_ctx == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(mgmt_ctx->mstore_ctx == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	return mapistore_backend_registered(backend);
}

static int mgmt_user_registration_cmd(enum mapistore_mgmt_user_status status,
				      unsigned msg_prio,
				      struct mapistore_connection_info *conn_info,
				      const char *backend, const char *vuser)
{
	int				ret;
	TALLOC_CTX			*mem_ctx;
	DATA_BLOB			data;
	struct mapistore_mgmt_user_cmd	cmd;
	enum ndr_err_code		ndr_err;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!conn_info, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!conn_info->mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!conn_info->username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!backend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!vuser, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	cmd.status = status;
	cmd.backend = backend;
	cmd.username = conn_info->username;
	cmd.vuser = vuser;
	
	mem_ctx = talloc_new(NULL);
	ndr_err = ndr_push_struct_blob(&data, mem_ctx, &cmd, (ndr_push_flags_fn_t)ndr_push_mapistore_mgmt_user_cmd);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("! [%s:%d][%s]: Failed to push mapistore_mgmt_user_cmd into NDR blob\n", 
			  __FILE__, __LINE__, __FUNCTION__));
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_INVALID_DATA;
	}

	ret = mq_send(conn_info->mstore_ctx->mq_users, (const char *)data.data, data.length, msg_prio);
	if (ret == -1) {
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_MSG_SEND;
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;	
}


/**
   \details Register the mapping between a system user and a backend
   user for a specific backend.

   \param conn_info pointer to the connection information
   \param backend the name of the backend
   \param vuser the name of the matching user in the backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPIStore error
 */
_PUBLIC_ int mapistore_mgmt_backend_register_user(struct mapistore_connection_info *conn_info,
						  const char *backend,
						  const char *vuser)
{
	return mgmt_user_registration_cmd(MAPISTORE_MGMT_USER_REGISTER, 
					  0, conn_info, backend, vuser);
}


/**
   \details Unregister the mapping between a system user and a backend
   user for a specific backend.

   \param conn_info pointer to the connection information
   \param backend the name of the backend
   \param vuser the name of the matching user in the backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_mgmt_backend_unregister_user(struct mapistore_connection_info *conn_info,
						    const char *backend,
						    const char *vuser)
{
	return mgmt_user_registration_cmd(MAPISTORE_MGMT_USER_UNREGISTER,
					  31, conn_info, backend, vuser);
}
