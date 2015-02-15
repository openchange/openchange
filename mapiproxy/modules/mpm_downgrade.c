/*
   MAPI Proxy - Downgrade Module

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

/**
   \file mpm_downgrade.c

   \brief Downgrade EMSMDB protocol version EcDoConnect/EcDoRpc
 */

#include "libmapi/libmapi.h"
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/dcesrv_mapiproxy_proto.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"


/**
   \details This function replaces the store_version short array
   returned by Exchange in EcDoConnect with a version matching
   Exchange 2000. Otherwise Outlook tries to upgrade indefinitely.

   \param dce_call pointer to the session context
   \param r pointer to the EcDoConnect structure

   \return true on success
 */
static bool downgrade_EcDoConnect(struct dcesrv_call_state *dce_call, struct EcDoConnect *r)
{
	r->out.rgwServerVersion[0] = 0x0006;
	r->out.rgwServerVersion[1] = 0x1141;
	r->out.rgwServerVersion[2] = 0x0005;

	return true;
}


static NTSTATUS downgrade_push(struct dcesrv_call_state *dce_call,
			       TALLOC_CTX *mem_ctx, void *r)
{
	const struct ndr_interface_table	*table;
	uint16_t				opnum;
	const char				*name;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;
	opnum = dce_call->pkt.u.request.opnum;
	name = table->calls[opnum].name;

	if (table->name && !strcmp(table->name, "exchange_emsmdb")) {
		if (name && !strcmp(name, "EcDoConnect")) {
			downgrade_EcDoConnect(dce_call, (struct EcDoConnect *)r);
		}
	}

	return NT_STATUS_OK;
}


static NTSTATUS downgrade_ndr_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, struct ndr_pull *pull)
{
	return NT_STATUS_OK;
}


static NTSTATUS downgrade_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	return NT_STATUS_OK;
}


/**
   \details Returns the nca_op_rng_error DCERPC status code when
   Outlook sends an EcDoConnectEx requrest.

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r generic pointer to EcDoConnectEx structure
   \param mapiproxy pointer to the mapiproxy structure

   \return NT_STATUS_NET_WRITE_FAULT when EcDoConnectEx is detected,
   otherwise NT_STATUS_OK

*/
static NTSTATUS downgrade_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r,
				   struct mapiproxy *mapiproxy)
{
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;
	opnum = dce_call->pkt.u.request.opnum;
	
	if ((opnum == 0xA) && (table->name && !strcmp(table->name, "exchange_emsmdb"))) {
		dce_call->fault_code = DCERPC_FAULT_OP_RNG_ERROR;
		return NT_STATUS_NET_WRITE_FAULT;
	}

	return NT_STATUS_OK;
}

/**
   \details Entry point for the downgrade mapiproxy module
   
   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	module;
	NTSTATUS		ret;

	/* Fill in our name */
	module.name = "downgrade";
	module.description = "Downgrade EMSMDB protocol version EcDoConnect/EcDoRpc";
	module.endpoint = "exchange_emsmdb";

	/* Fill in all the operations */
	module.init = NULL;
	module.unbind = NULL;
	module.push = downgrade_push;
	module.ndr_pull = downgrade_ndr_pull;
	module.pull = downgrade_pull;
	module.dispatch = downgrade_dispatch;

	/* Register ourselves with the MAPIPROXY subsystem */
	ret = mapiproxy_module_register(&module);
	if (!NT_STATUS_IS_OK(ret)) {
		OC_DEBUG(0, "Failed to register the 'downgrade' mapiproxy module!");
		return ret;
	}
	
	return ret;
}
