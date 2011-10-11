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
#include "mapiproxy/libmapistore/mgmt/mapistore_mgmt.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.h"

static void mgmt_user_process_notif(struct mapistore_mgmt_context *mgmt_ctx,
				    DATA_BLOB data)
{
	struct mapistore_mgmt_user_cmd	user_cmd;
	struct mapistore_mgmt_users	*el;
	struct ndr_pull			*ndr_pull = NULL;
	bool				found = false;

	ndr_pull = ndr_pull_init_blob(&data, (TALLOC_CTX *)mgmt_ctx);
	ndr_pull_mapistore_mgmt_user_cmd(ndr_pull, NDR_SCALARS|NDR_BUFFERS, &user_cmd);

	if (mgmt_ctx->verbose == true) {
		struct ndr_print	*ndr_print;

		ndr_print = talloc_zero((TALLOC_CTX *)mgmt_ctx, struct ndr_print);
		ndr_print->print = ndr_print_printf_helper;
		ndr_print->depth = 1;
		ndr_print_mapistore_mgmt_user_cmd(ndr_print, "user command", &user_cmd);
		talloc_free(ndr_print);
	}

	if (user_cmd.backend == NULL ||
	    user_cmd.username == NULL ||
	    user_cmd.vuser == NULL) {
		talloc_free(ndr_pull);
		return;
	}

	if (mgmt_ctx->users == NULL) {
		if (user_cmd.status == MAPISTORE_MGMT_USER_REGISTER) {
			el = talloc_zero((TALLOC_CTX *)mgmt_ctx, struct mapistore_mgmt_users);
			if (!el) {
				talloc_free(ndr_pull);
				DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
				return;
			}
			el->info = talloc_zero((TALLOC_CTX *)el, struct mapistore_mgmt_user_cmd);
			if (!el->info) {
				talloc_free(el);
				talloc_free(ndr_pull);
				DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
				return;
			}
			el->info->backend = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.backend);
			el->info->username = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.username);
			el->info->vuser = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.vuser);
			el->ref_count = 1;
			DLIST_ADD_END(mgmt_ctx->users, el, struct mapistore_mgmt_users);
		} else {
			DEBUG(0, ("[%s:%d] Trying to unregister user in empty list\n", __FUNCTION__, __LINE__));
		}
	} else {
		/* Search the users list and perform action */
		for (el = mgmt_ctx->users; el; el = el->next) {
			/* Case where the record exists */
			if (!strcmp(el->info->backend, user_cmd.backend) && 
			    (!strcmp(el->info->username, user_cmd.username)) &&
			    (!strcmp(el->info->vuser, user_cmd.vuser))) {
				found = true;
				switch (user_cmd.status) {
				case MAPISTORE_MGMT_USER_REGISTER:
					el->ref_count += 1;
					break;
				case MAPISTORE_MGMT_USER_UNREGISTER:
					el->ref_count -= 1;
					/* Delete record if ref_count is 0 */
					if (el->ref_count == 0) {
						DLIST_REMOVE(mgmt_ctx->users, el);
						talloc_free(el);
						break;
					} 
					break;
				default:
					DEBUG(0, ("[%s:%d]: Invalid user command status\n", __FUNCTION__, __LINE__));
					break;
				}
			} 
		}
		/* Case where no matching record was found: insert */
		if (found == false) {
			switch (user_cmd.status) {
			case MAPISTORE_MGMT_USER_REGISTER:
				el = talloc_zero((TALLOC_CTX *)mgmt_ctx, struct mapistore_mgmt_users);
				if (!el) {
					talloc_free(ndr_pull);
					DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
					return;
				}
				el->info = talloc_zero((TALLOC_CTX *)el, struct mapistore_mgmt_user_cmd);
				if (!el->info) {
					talloc_free(el);
					talloc_free(ndr_pull);
					DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
					return;
				}
				el->info->backend = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.backend);
				el->info->username = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.username);
				el->info->vuser = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.vuser);
				el->ref_count = 1;
				DLIST_ADD_END(mgmt_ctx->users, el, struct mapistore_mgmt_users);
				break;
			case MAPISTORE_MGMT_USER_UNREGISTER:
				DEBUG(0, ("[%s:%d]: Trying to unregister non-existing user\n", __FUNCTION__, __LINE__));
				break;
			default:
				DEBUG(0, ("[%s:%d]: Invalid user command status\n", __FUNCTION__, __LINE__));
				break;
			}
		}
	}

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
	mgmt_ctx->verbose = false;
	mgmt_ctx->users = NULL;
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
   \details Set mapistore management verbosity

   \param mgmt_ctx pointer to the mapistore management context
   \param verbose boolean value that sets or unset verbosity

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_mgmt_set_verbosity(struct mapistore_mgmt_context *mgmt_ctx, bool verbose)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	mgmt_ctx->verbose = verbose;
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
   \details Retrieve the list of registered usernames

   \param mgmt_ctx pointer to the mapistore management context
   \param backend the name of the backend to lookup
   \param vuser the name of the virtual user to lookup

   \return Allocated mapistore management user list on success, otherwise NULL
 */
_PUBLIC_ struct mapistore_mgmt_users_list *mapistore_mgmt_registered_users(struct mapistore_mgmt_context *mgmt_ctx,
									   const char *backend,
									   const char *vuser)
{
	struct mapistore_mgmt_users_list	*ulist = NULL;
	struct mapistore_mgmt_users		*el;
	bool					found = false;
	int					i;

	/* Sanity checks */
	if (!mgmt_ctx) return NULL;
	if (!mgmt_ctx->users) return NULL;
	if (!backend) return NULL;
	if (!vuser) return NULL;
	
	ulist = talloc_zero((TALLOC_CTX *)mgmt_ctx, struct mapistore_mgmt_users_list);
	ulist->count = 0;
	ulist->user = (const char **) talloc_array((TALLOC_CTX *)ulist, char *, ulist->count + 1);
	for (el = mgmt_ctx->users; el; el = el->next) {
		if (!strcmp(el->info->backend, backend) && !strcmp(el->info->vuser, vuser)) {
			/* Check if the user hasn't already been inserted */
			for (i = 0; i != ulist->count; i++) {
				if (ulist->user && !strcmp(ulist->user[i], el->info->username)) {
					found = true;
					break;
				}
			}
			if (found == false) {
				ulist->user = (const char **) talloc_realloc(ulist->user, ulist->user, 
									     char *, ulist->count + 1);
				ulist->user[ulist->count] = talloc_strdup((TALLOC_CTX *) ulist->user, 
									  el->info->username);
				ulist->count += 1;
			}
		}
	}

	return (ulist);
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


/**
   \details Retrieves a partial or complete URI from the backend
   depending on the specified parameters. This function is used by the
   mapistore management interface to get from backend either the
   expected URI for further registration or a partial (backend
   compliant) URI for partial search.

   \param mgmt_ctx Pointer to the mapistore management context
   \param backend the name of the backend
   \param username the name of the user in the backend
   \param folder the name of the folder in the backend
   \param message the name of the message in the backend
   \param uri pointer on pointer to the URI to return

   \note The returned uri is allocated and needs to be free'd using
   talloc_free() upon end of use.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_mgmt_generate_uri(struct mapistore_mgmt_context *mgmt_ctx,
					 const char *backend, const char *username,
					 const char *folder, const char *message, 
					 char **uri)
{
	struct backend_context *backend_ctx;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!backend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	backend_ctx = mapistore_backend_lookup_by_name((TALLOC_CTX *)mgmt_ctx, backend);
	MAPISTORE_RETVAL_IF(!backend_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mapistore_backend_manager_generate_uri(backend_ctx, (TALLOC_CTX *)mgmt_ctx, username, 
					       folder, message, uri);

	/* We are not really deleting a context, but freeing the
	 * allocated memory to backend_ctx */
	mapistore_backend_delete_context(backend_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Check if a message is already registered within indexing
   database for the user.

   \param mgmt_ctx Pointer to the mapistore management context
   \param backend the name of the backend
   \param sysuser the name of the mapistore user (openchange)
   \param username the name of the user on the remote system the
   backend manages
   \param folder the name of the folder on the remote system the
   backend manages
   \param message the name of the message on the remote system the
   backend manages

   \return true if the message is registered, otherwise false
 */
_PUBLIC_ int mapistore_mgmt_registered_message(struct mapistore_mgmt_context *mgmt_ctx,
					       const char *backend, const char *sysuser,
					       const char *username,
					       const char *folder, const char *message)
{
	struct indexing_context_list	*ictxp;
	char				*uri;
	int				ret;
	uint64_t			mid;
	bool				retval;
	bool				softdeleted;

	ret = mapistore_mgmt_generate_uri(mgmt_ctx, backend, username, folder, message, &uri);
	if (ret != MAPISTORE_SUCCESS) return false;
	
	ret = mapistore_indexing_add(mgmt_ctx->mstore_ctx, sysuser, &ictxp);
	if (ret != MAPISTORE_SUCCESS) {
		talloc_free(uri);
		return false;
	}

	ret = mapistore_indexing_record_get_fmid(mgmt_ctx->mstore_ctx, sysuser, uri, true, &mid, &softdeleted);
	if (ret == MAPISTORE_SUCCESS) {
		retval = true;
	} else {
		retval = false;
	}

	talloc_free(uri);
	talloc_free(ictxp);
	return retval;
}
