/*
   MAPI Proxy

   This proxy is based on dcesrv_remote.c code from Stefan Metzemacher

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/dcesrv_mapiproxy_proto.h"
#include <libmapi/dlinklist.h>

struct dcesrv_mapiproxy_private {
	struct dcerpc_pipe	*c_pipe;
};

static NTSTATUS mapiproxy_op_reply(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	DEBUG(5, ("mapiproxy::mapiproxy_op_reply\n"));
	return NT_STATUS_OK;
}


/**
   \details This function is called when the client binds to one of
   the interfaces mapiproxy handles.
 */
static NTSTATUS mapiproxy_op_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
	NTSTATUS				status;
	const struct ndr_interface_table	*table;
	struct dcesrv_mapiproxy_private		*private;
	const char				*binding;
	const char				*user;
	const char				*pass;
	const char				*domain;
	struct cli_credentials			*credentials;
	bool					machine_account;

	DEBUG(5, ("mapiproxy::mapiproxy_op_bind: [session = 0x%x] [session server id = 0x%llx 0x%x 0x%x]\n", dce_call->context->context_id,
		  dce_call->conn->server_id.id, dce_call->conn->server_id.id2, dce_call->conn->server_id.node));

	/* Retrieve parametric options */
	binding = lp_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "binding");
	machine_account = lp_parm_bool(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "use_machine_account", false);

	private = talloc(dce_call->conn, struct dcesrv_mapiproxy_private);
	if (!private) {
		return NT_STATUS_NO_MEMORY;
	}

	private->c_pipe = NULL;
	dce_call->context->private = private;

	if (!binding) {
		DEBUG(0, ("You must specify a DCE/RPC binding string\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	user = lp_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "username");
	pass = lp_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "password");
	domain = lp_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "domain");

	table = ndr_table_by_uuid(&iface->syntax_id.uuid);
	if (!table) {
		dce_call->fault_code = DCERPC_FAULT_UNK_IF;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	if (user && pass) {
		DEBUG(5, ("dcerpc_mapiproxy: RPC proxy: Using specified account\n"));
		credentials = cli_credentials_init(private);
		if (!credentials) {
			return NT_STATUS_NO_MEMORY;
		}

		cli_credentials_set_conf(credentials, dce_call->conn->dce_ctx->lp_ctx);
		cli_credentials_set_username(credentials, user, CRED_SPECIFIED);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		cli_credentials_set_password(credentials, pass, CRED_SPECIFIED);
	} else if (machine_account) {
		DEBUG(5, ("dcerpc_mapiproxy: RPC proxy: Using machine account\n"));
		credentials = cli_credentials_init(private);
		if (!credentials) {
			return NT_STATUS_NO_MEMORY;
		}
		cli_credentials_set_conf(credentials, dce_call->conn->dce_ctx->lp_ctx);
		if (domain) {
			cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
		}
		status = cli_credentials_set_machine_account(credentials, dce_call->conn->dce_ctx->lp_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	} else if (dce_call->conn->auth_state.session_info->credentials) {
		DEBUG(5, ("dcerpc_mapiproxy: RPC proxy: Using delegated credentials\n"));
		credentials = dce_call->conn->auth_state.session_info->credentials;
	} else {
		DEBUG(1, ("dcerpc_mapiproxy: RPC proxy: You must supply binding, user and password or have delegated credentials\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = dcerpc_pipe_connect(private,
				     &(private->c_pipe), binding, table,
				     credentials, dce_call->event_ctx,
				     dce_call->conn->dce_ctx->lp_ctx);
	talloc_free(credentials);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	DEBUG(5, ("dcerpc_mapiproxy: RPC proxy: CONNECTED\n"));

	return NT_STATUS_OK;
}


/**
   \details Called when the client disconnects from one of the
   endpoints managed by mapiproxy.
 */
static void mapiproxy_op_unbind(struct dcesrv_connection_context *context, const struct dcesrv_interface *iface)
{
	struct dcesrv_mapiproxy_private	*private = (struct dcesrv_mapiproxy_private *) context->private;

	DEBUG(5, ("mapiproxy::mapiproxy_op_unbind\n"));

	mapiproxy_module_unbind(context->conn->server_id, context->context_id);

	if (private) {
		talloc_free(private->c_pipe);
	}

	return;
}


/**
   Retrieve either the user request or the remote server reply
 */
static NTSTATUS mapiproxy_op_ndr_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_pull *pull, void **r)
{
	enum ndr_err_code			ndr_err;
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	DEBUG(5, ("mapiproxy::mapiproxy_op_ndr_pull\n"));

	table = (const struct ndr_interface_table *)dce_call->context->iface->private;
	opnum = dce_call->pkt.u.request.opnum;

	dce_call->fault_code = 0;

	if (opnum >= table->num_calls) {
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	*r = talloc_size(mem_ctx, table->calls[opnum].struct_size);
	if (!*r) {
		return NT_STATUS_NO_MEMORY;
	}

	/* directly alter the pull struct before it got pulled from ndr */
	mapiproxy_module_ndr_pull(dce_call, mem_ctx, pull);

	ndr_err = table->calls[opnum].ndr_pull(pull, NDR_IN, *r);

	mapiproxy_module_pull(dce_call, mem_ctx, *r);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("mapiproxy: mapiproxy_ndr_pull: ERROR\n"));
		dcerpc_log_packet(table, opnum, NDR_IN, 
				  &dce_call->pkt.u.request.stub_and_verifier);
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}


static NTSTATUS mapiproxy_op_ndr_push(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_push *push, const void *r)
{
	enum ndr_err_code			ndr_err;
	const struct ndr_interface_table	*table;
	const struct ndr_interface_call		*call;
	uint16_t				opnum;
	const char				*name;

	DEBUG(5, ("mapiproxy::mapiproxy_op_ndr_push\n"));

	table = (const struct ndr_interface_table *)dce_call->context->iface->private;
	opnum = dce_call->pkt.u.request.opnum;

	name = table->calls[opnum].name;
	call = &table->calls[opnum];

	dce_call->fault_code = 0;

	/* NspiGetProps binding strings replacement */
	if (table->name && !strcmp(table->name, "exchange_nsp")) {
		if (name && !strcmp(name, "NspiGetProps")) {
			mapiproxy_NspiGetProps(dce_call, (struct NspiGetProps *)r);
		}
	}	

	/* RfrGetNewDSA FQDN replacement */
	if (table->name && !strcmp(table->name, "exchange_ds_rfr")) {
		if (name && !strcmp(name, "RfrGetNewDSA")) {
			mapiproxy_RfrGetNewDSA(dce_call, (struct RfrGetNewDSA *)r);
		}
	}

	mapiproxy_module_push(dce_call, mem_ctx, (void *)r);

	ndr_err = table->calls[opnum].ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("mapiproxy: mapiproxy_ndr_push: ERROR\n"));
		dce_call->fault_code = DCERPC_FAULT_NDR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

static NTSTATUS mapiproxy_op_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	struct dcesrv_mapiproxy_private		*private;
	struct ndr_push				*push;
	enum ndr_err_code			ndr_err;
	struct mapiproxy			mapiproxy;
	const struct ndr_interface_table	*table;
	const struct ndr_interface_call		*call;
	uint16_t				opnum;
	const char				*name;
	NTSTATUS				status;

	private = dce_call->context->private;
	table = dce_call->context->iface->private;
	opnum = dce_call->pkt.u.request.opnum;

	name = table->calls[opnum].name;
	call = &table->calls[opnum];

	mapiproxy.norelay = false;
	mapiproxy.ahead = false;

	if (!private) {
		dce_call->fault_code = DCERPC_FAULT_ACCESS_DENIED;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	DEBUG(5, ("mapiproxy::mapiproxy_op_dispatch: %s(0x%x): %d bytes\n", 
		  table->calls[opnum].name, opnum, table->calls[opnum].struct_size));

	if (private->c_pipe->conn->flags & DCERPC_DEBUG_PRINT_IN) {
		ndr_print_function_debug(call->ndr_print, name, NDR_IN | NDR_SET_VALUES, r);
	}

	private->c_pipe->conn->flags |= DCERPC_NDR_REF_ALLOC;

 ahead:
	if (mapiproxy.ahead == true) {
		push = ndr_push_init_ctx(dce_call, 
					 lp_iconv_convenience(dce_call->conn->dce_ctx->lp_ctx));
		NT_STATUS_HAVE_NO_MEMORY(push);
		ndr_err = call->ndr_push(push, NDR_OUT, r);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(0, ("mapiproxy: mapiproxy_op_dispatch:push: ERROR\n"));
			dce_call->fault_code = DCERPC_FAULT_NDR;
			return NT_STATUS_NET_WRITE_FAULT;
		}
	}

	status = mapiproxy_module_dispatch(dce_call, mem_ctx, r, &mapiproxy);
	if (!NT_STATUS_IS_OK(status)) {
		private->c_pipe->last_fault_code = dce_call->fault_code;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	private->c_pipe->last_fault_code = 0;
	if (mapiproxy.norelay == false) {
	  status = dcerpc_ndr_request(private->c_pipe, NULL, table, opnum, mem_ctx, r);
	}

	dce_call->fault_code = private->c_pipe->last_fault_code;
	if (dce_call->fault_code != 0 || !NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("mapiproxy: call[%s] failed with %s! (status = %s)\n", name, 
			  dcerpc_errstr(mem_ctx, dce_call->fault_code), nt_errstr(status)));
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	if ((dce_call->fault_code == 0) && 
	    (private->c_pipe->conn->flags & DCERPC_DEBUG_PRINT_OUT) && mapiproxy.norelay == false) {
		ndr_print_function_debug(call->ndr_print, name, NDR_OUT | NDR_SET_VALUES, r);
	}

	if (mapiproxy.ahead == true) goto ahead;

	return NT_STATUS_OK;
}


/**
   \details Register an interface
 */
static NTSTATUS mapiproxy_register_one_iface(struct dcesrv_context *dce_ctx, const struct dcesrv_interface *iface)
{
	const struct ndr_interface_table	*table = iface->private;
	int					i;

	for (i = 0; i < table->endpoints->count; i++) {
		NTSTATUS	ret;
		const char	*name = table->endpoints->names[i];

		ret = dcesrv_interface_register(dce_ctx, name, iface, NULL);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(1,("mapiproxy_op_init_server: failed to register endpoint '%s'\n", name));
			return ret;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Initializes the server and register emsmdb and nspi interfaces
 */
static NTSTATUS mapiproxy_op_init_server(struct dcesrv_context *dce_ctx, const struct dcesrv_endpoint_server *ep_server)
{
	NTSTATUS		ret;
	struct dcesrv_interface	iface;
	const char     		**ifaces;
	uint32_t		i;

	/* Register mapiproxy modules */
	ret = mapiproxy_module_init(dce_ctx);
	NT_STATUS_NOT_OK_RETURN(ret);

	ifaces = str_list_make(dce_ctx, lp_parm_string(dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "interfaces"), NULL);

	for (i = 0; ifaces[i]; i++) {
		/* Register the interface */
		if (!ep_server->interface_by_name(&iface, ifaces[i])) {
			DEBUG(0, ("mapiproxy_op_init_server: failed to find interface '%s'\n", ifaces[i]));
			return NT_STATUS_UNSUCCESSFUL;
		}

		ret = mapiproxy_register_one_iface(dce_ctx, &iface);
		if (!NT_STATUS_IS_OK(ret)) {
			DEBUG(0, ("mapiproxy_op_init_server: failed to register interface '%s'\n", ifaces[i]));
			return ret;
		}
	}

	return NT_STATUS_OK;
}


static bool mapiproxy_fill_interface(struct dcesrv_interface *iface, const struct ndr_interface_table *tbl)
{
	iface->name = tbl->name;
	iface->syntax_id = tbl->syntax_id;
	
	iface->bind = mapiproxy_op_bind;
	iface->unbind = mapiproxy_op_unbind;
	
	iface->ndr_pull = mapiproxy_op_ndr_pull;
	iface->dispatch = mapiproxy_op_dispatch;
	iface->reply = mapiproxy_op_reply;
	iface->ndr_push = mapiproxy_op_ndr_push;

	iface->private = tbl;

	return true;
}


static bool mapiproxy_op_interface_by_uuid(struct dcesrv_interface *iface, const struct GUID *uuid, uint32_t if_version)
{
	const struct ndr_interface_list	*l;

	for (l = ndr_table_list(); l; l = l->next) {
		if (l->table->syntax_id.if_version == if_version &&
		    GUID_equal(&l->table->syntax_id.uuid, uuid) == 0) {
			return mapiproxy_fill_interface(iface, l->table);
		}
	}

	return false;
}


static bool mapiproxy_op_interface_by_name(struct dcesrv_interface *iface, const char *name)
{
	const struct ndr_interface_table	*tbl;

	tbl = ndr_table_by_name(name);

	if (tbl) {
		return mapiproxy_fill_interface(iface, tbl);
	}

	return false;
}


/**
   \details register the mapiproxy endpoint server.
 */
NTSTATUS dcerpc_server_mapiproxy_init(void)
{
	NTSTATUS			ret;
	struct dcesrv_endpoint_server	ep_server;

	ZERO_STRUCT(ep_server);

	/* Fill in our name */
	ep_server.name = "mapiproxy";

	/* Fill in all the operations */
	ep_server.init_server = mapiproxy_op_init_server;

	ep_server.interface_by_uuid = mapiproxy_op_interface_by_uuid;
	ep_server.interface_by_name = mapiproxy_op_interface_by_name;

	/* Register ourselves with the DCE/RPC subsystem */
	ret = dcerpc_register_ep_server(&ep_server);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Failed to register 'mapiproxy' endpoint server!"));
		return ret;
	}

	/* Full DCE/RPC interface table needed */
	ndr_table_init();
	
	return ret;
}

/**
   \details Register mapiproxy dynamic shared object modules

   This function registers mapiproxy modules located
 */

/**
   \details Entry point of mapiproxy dynamic shared object.

   This function first registers exchange endpoints and ndr tables,
   then attempts to register the mapiproxy interface.

   \return NT_STATUS_OK on success, otherwise NT_STATUS_UNSUCCESSFUL;
 */
NTSTATUS samba_init_module(void)
{
	NTSTATUS status;

	/* Step1. Register Exchange endpoints */
	status = dcerpc_server_exchange_emsmdb_init();
	NT_STATUS_NOT_OK_RETURN(status);

	status = dcerpc_server_exchange_nsp_init();
	NT_STATUS_NOT_OK_RETURN(status);

	status = dcerpc_server_exchange_ds_rfr_init();
	NT_STATUS_NOT_OK_RETURN(status);

	/* Step2. Register Exchange ndr tables */
	status = ndr_table_register(&ndr_table_exchange_emsmdb);
	NT_STATUS_NOT_OK_RETURN(status);

	status = ndr_table_register(&ndr_table_exchange_nsp);
	NT_STATUS_NOT_OK_RETURN(status);

	status = ndr_table_register(&ndr_table_exchange_ds_rfr);
	NT_STATUS_NOT_OK_RETURN(status);

	/* Step3. Finally register mapiproxy endpoint */
	status = dcerpc_server_mapiproxy_init();
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

/* include server boiler template */
#include <gen_ndr/ndr_exchange_s.c>
