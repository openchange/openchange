/*
   Asynchronous EMSMDB server

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

#include "dcesrv_asyncemsmdb.h"
#include "utils/dlinklist.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/gen_ndr/ndr_mapistore_notification.h"

/**
   \file dcesrv_asyncemsmdb.c

   \brief Asynchronous EMSMDB server implementation

 */

struct exchange_asyncemsmdb_session	*asyncemsmdb_session = NULL;

static struct exchange_asyncemsmdb_session *dcesrv_find_asyncemsmdb_session(struct GUID *uuid)
{
	struct exchange_asyncemsmdb_session	*session, *found_session = NULL;

	for (session = asyncemsmdb_session; !found_session && session; session = session->next) {
		if (GUID_equal(uuid, &session->uuid)) {
			found_session = session;
		}
	}

	return found_session;
}

/**
   \details Return an available random port

   \return port > 0 on success, otherwise -1
 */
static int _get_random_port(void)
{
	int			sockfd;
	struct sockaddr_in	addr;
	socklen_t		addr_len;
	uint16_t		port = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) return -1;

	bzero((char *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0);
	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(sockfd);
		return -1;
	}

	bzero((char *) &addr, sizeof(addr));
	addr_len = sizeof(addr);
	if (getsockname(sockfd, &addr, &addr_len) == -1) {
		close(sockfd);
		return -1;
	}

	port = addr.sin_port;
	close(sockfd);

	return port;
}

static void EcDoAsyncWaitEx_handler(struct tevent_context *ev,
				    struct tevent_fd *fde,
				    uint16_t flags,
				    void *private_data)
{
	struct asyncemsmdb_private_data			*p = talloc_get_type(private_data,
									     struct asyncemsmdb_private_data);
	TALLOC_CTX					*mem_ctx;
	enum mapistore_error				retval;
	char						*str = NULL;
	int						bytes = 0;
	NTSTATUS					status;
	struct mapistore_notification_subscription	r;
	struct ndr_print				*ndr;

	if (!p) {
		OC_DEBUG(0, "[asyncemsmdb]: private_data is NULL");
		return;
	}

	bytes = nn_recv(p->sock, &str, NN_MSG, NN_DONTWAIT);
	if (bytes == 0) {
		OC_DEBUG(0, "[asyncemsmdb]: EcDoAsyncWaitEx_handler: str is NULL!");
		return;
	}

	str[bytes] = '\0';
	OC_DEBUG(0, "[asyncemsmdb] received message: %s", str);
	nn_freemsg(str);

	OC_DEBUG(0, "Notification received for session: %s", p->emsmdb_session_str);

	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) {
		OC_DEBUG(0, "[asyncemsmdb]: No more memory");
		return;
	}

	retval = mapistore_notification_subscription_get(mem_ctx, p->mstore_ctx, p->emsmdb_uuid, &r);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "no subscription to process");
		return;
	}

	OC_DEBUG(0, "%d subscriptions available:", r.v.v1.count);
	ndr = talloc_zero(mem_ctx, struct ndr_print);
	if (!ndr) {
		OC_DEBUG(0, "[asyncemsmdb]: No more memory");
		return;
	}
	ndr->depth = 1;
	ndr->print = ndr_print_debug_helper;
	ndr->no_newline = false;
	ndr_print_mapistore_notification_subscription(ndr, "subscriptions", &r);
	talloc_free(mem_ctx);

	p->r->out.pulFlagsOut = talloc_zero(p->dce_call, uint32_t);
	*p->r->out.pulFlagsOut = 0x1;

	status = dcesrv_reply(p->dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(0, "EcDoAsyncWaitEx_handler: dcesrv_reply() failed - %s", nt_errstr(status));
	}

	talloc_free(p->fd_event);
	p->fd_event = NULL;
}


 /**
    \details exchange_async_emsmdb EcDoAsyncWaitEx (0x0) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoAsyncWaitEx request data

   \return NT_STATUS_OK on success
 */
static NTSTATUS dcesrv_EcDoAsyncWaitEx(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct EcDoAsyncWaitEx *r)
{
	enum mapistore_error			retval;
	struct exchange_asyncemsmdb_session	*session = NULL;
	struct asyncemsmdb_private_data		*p = NULL;
	struct mapistore_context		*mstore_ctx = NULL;
	struct GUID				uuid;
	char					*cn = NULL;
	const char				*ip_addr = NULL;
	int					nn_retval;
	size_t					sz = 0;
	char					*bind_addr = NULL;
	int					port = 0;

	OC_DEBUG(0, "exchange_asyncemsmdb: EcDoAsyncWaitEx (0x0)");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate");
		*r->out.pulFlagsOut = 0x1;
		return NT_STATUS_OK;
	}

	/* Set the asynchronous dcerpc flag on the connection */
	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;

	/* Step 1. Search for an existing session */
	session = dcesrv_find_asyncemsmdb_session(&r->in.async_handle->uuid);
	if (session) {
		p = (struct asyncemsmdb_private_data *) session->data;
		/* Ensure the session is registered */
		retval = mapistore_notification_session_exist(p->mstore_ctx, r->in.async_handle->uuid);
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb]: no matching emsmdb session found");
			*r->out.pulFlagsOut = 0x1;
			return NT_STATUS_OK;
		}

		p->dce_call = dce_call;
		p->r = r;
	} else {
		/* Step 1. Ensure the session is registered */
		mstore_ctx = mapistore_init(mem_ctx, dce_call->conn->dce_ctx->lp_ctx, NULL);
		if (!mstore_ctx) {
			OC_DEBUG(0, "[asyncemsmdb]: MAPIStore initialized failed");
			*r->out.pulFlagsOut = 0x1;
			return NT_STATUS_OK;
		}

		retval = mapistore_notification_session_get(dce_call, mstore_ctx, r->in.async_handle->uuid, &uuid, &cn);
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb]: unable to fetch emsmdb session data");
			*r->out.pulFlagsOut = 0x1;
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		/* we're allowed to reply async */
		p = talloc(dce_call->event_ctx, struct asyncemsmdb_private_data);
		if (!p) {
			OC_DEBUG(0, "[asyncemsmdb]: no more memory");
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		p->dce_call = dce_call;
		p->mstore_ctx = talloc_steal(p, mstore_ctx);
		p->emsmdb_uuid = uuid;
		p->emsmdb_session_str = GUID_string(p, (const struct GUID *)&uuid);
		if (!p->emsmdb_session_str) {
			OC_DEBUG(0, "[asyncemsmdb]: no more memory");
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}
		p->r = r;
		p->fd_event = NULL;

		p->sock = nn_socket(AF_SP, NN_PULL);
		if (p->sock == -1) {
			OC_DEBUG(0, "[asyncemsmdb]: failed to create socket: %s", nn_strerror(errno));
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		/* Get a random port */
		port = _get_random_port();
		if (port == -1) {
			OC_DEBUG(0, "[asyncemsmdb]: no port available!");
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		/* Get parametric options */
		ip_addr = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "asyncemsmdb", "listen");
		if (ip_addr == NULL) {
			OC_DEBUG(0, "[asyncemsmdb]: no asyncemsmdb:listen option specified, "
				 "using %s as a fallback", ASYNCEMSMDB_FALLBACK_ADDR);
			ip_addr = ASYNCEMSMDB_FALLBACK_ADDR;
		}

		/* TODO: Add transport as a parametric option */
		bind_addr = talloc_asprintf(mem_ctx, "tcp://%s:%d", ip_addr, port);
		if (!bind_addr) {
			OC_DEBUG(0, "[asyncemsmdb][ERR]: no more memory");
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		nn_retval = nn_bind(p->sock, bind_addr);
		if (nn_retval == -1) {
			OC_DEBUG(0, "[asyncemsmdb] nn_bind failed on %s failed: %s", bind_addr, nn_strerror(errno));
			talloc_free(p);
			talloc_free(mstore_ctx);
			talloc_free(bind_addr);
			return NT_STATUS_OK;
		}

		/* Register the address to the resolver */
		retval = mapistore_notification_resolver_add(p->mstore_ctx, cn, bind_addr);
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb] unable to add record to the resolver: %s",
				 mapistore_errstr(retval));
			talloc_free(p);
			talloc_free(mstore_ctx);
			talloc_free(bind_addr);
			return NT_STATUS_OK;
		}

		sz = sizeof(p->fd);
		nn_retval = nn_getsockopt(p->sock, NN_SOL_SOCKET, NN_RCVFD, &p->fd, &sz);
		if (nn_retval == -1) {
			OC_DEBUG(0, "[asyncemsmdb] nn_getsockopt failed: %s", nn_strerror(errno));
			talloc_free(p);
			talloc_free(mstore_ctx);
			talloc_free(bind_addr);
			return NT_STATUS_OK;
		}
	}

	if (!p->fd_event) {
		p->fd_event = tevent_add_fd(dce_call->event_ctx,
					    dce_call->event_ctx,
					    p->fd,
					    TEVENT_FD_READ,
					    EcDoAsyncWaitEx_handler,
					    p);
		if (p->fd_event == NULL) {
			OC_DEBUG(0, "[asyncemsmdb] unable to subscribe for fd event in event loop");
			return NT_STATUS_OK;
		}
	}

	/* Register session  */
	if (!session) {
		session = talloc(asyncemsmdb_session, struct exchange_asyncemsmdb_session);
		if (!session) {
		failure:
			OC_DEBUG(0, "[asyncemsmdb][ERR]: No more memory");
			*r->out.pulFlagsOut = 0x1;
			return NT_STATUS_OK;
		}
		session->uuid = r->in.async_handle->uuid;
		session->data = talloc_steal(session, p);
		session->cn = talloc_strdup(session, cn);
		talloc_free(cn);
		if (!session->cn) {
			talloc_free(session);
			goto failure;
		}
		session->bind_addr = talloc_strdup(session, bind_addr);
		talloc_free(bind_addr);
		if (!session->bind_addr) {
			talloc_free(session);
			goto failure;
		}
		session->mstore_ctx = mstore_ctx;

		/* Add the session to the dcesrv_connection_context */
		dce_call->context->private_data = session;

		OC_DEBUG(5, "[asyncemsmdb]: New session added: %s", session->data->emsmdb_session_str);
		DLIST_ADD_END(asyncemsmdb_session, session, struct exchange_asyncemsmdb_session *);
	}

	return NT_STATUS_OK;
}

static NTSTATUS dcerpc_server_asyncemsmdb_unbind(struct dcesrv_connection_context *context, const struct dcesrv_interface *iface)
{
	enum mapistore_error			retval;
	struct exchange_asyncemsmdb_session	*session = (struct exchange_asyncemsmdb_session *) context->private_data;

	OC_DEBUG(0, "dcerpc_server_asyncemsmdb_unbind");

	if (!session) {
		return NT_STATUS_OK;
	}

	OC_DEBUG(0, "[asyncemsmdb] unbind %s", session->bind_addr);
	retval = mapistore_notification_resolver_delete(session->mstore_ctx, session->cn, session->bind_addr);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "[asyncemsmdb] unable to delete resolver entry %s from record %s", session->bind_addr, session->cn);
	}

	mapistore_release(session->mstore_ctx);
	DLIST_REMOVE(asyncemsmdb_session, session);

	/* flush pending call on connection */
	context->conn->pending_call_list = NULL;

	return NT_STATUS_OK;
}

static NTSTATUS dcerpc_server_asyncemsmdb_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
	return NT_STATUS_OK;
}

NTSTATUS samba_init_module(void)
{
	NTSTATUS status;

	status = dcerpc_server_asyncemsmdb_init();
	NT_STATUS_NOT_OK_RETURN(status);

	/* Initialize exchange_async_emsmdb session */
	asyncemsmdb_session = talloc_zero(talloc_autofree_context(), struct exchange_asyncemsmdb_session);
	if (!asyncemsmdb_session) return NT_STATUS_NO_MEMORY;
	asyncemsmdb_session->data = NULL;


	status = ndr_table_register(&ndr_table_asyncemsmdb);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

#include "gen_ndr/ndr_asyncemsmdb_s.c"
