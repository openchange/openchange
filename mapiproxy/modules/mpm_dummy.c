/*
   MAPI Proxy - Dummy Module

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

#include "libmapi/libmapi.h"
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/dcesrv_mapiproxy_proto.h"

/**
   \details Dummy init function which reads a parametric option from
   smb.conf and display it on the log channel.
 */
static NTSTATUS dummy_init(struct dcesrv_context *dce_ctx)
{
	const char	*test;

	test = lpcfg_parm_string(dce_ctx->lp_ctx, NULL, "mpm_dummy", "test");
	if (test) {
		OC_DEBUG(0, "Sample dummy string: %s", test);
	}

	return NT_STATUS_OK;
}


static NTSTATUS dummy_unbind(struct server_id server_id, uint32_t context_id)
{
	return NT_STATUS_OK;
}


static NTSTATUS dummy_push(struct dcesrv_call_state *dce_call, 
			   TALLOC_CTX *mem_ctx,  void *r)
{
	return NT_STATUS_OK;
}

static NTSTATUS dummy_ndr_pull(struct dcesrv_call_state *dce_call,
			       TALLOC_CTX *mem_ctx, struct ndr_pull *ndr)
{
	return NT_STATUS_OK;
}

static NTSTATUS dummy_pull(struct dcesrv_call_state *dce_call,
			   TALLOC_CTX *mem_ctx, void *r)
{
	return NT_STATUS_OK;
}

static NTSTATUS dummy_dispatch(struct dcesrv_call_state *dce_call,
			       TALLOC_CTX *mem_ctx, void *r,
			       struct mapiproxy *mapiproxy)
{
	return NT_STATUS_OK;
}


NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	module;
	NTSTATUS		ret;

	/* Fill in our name */
	module.name = "dummy";
	module.description = "dummy MAPIPROXY module";
	module.endpoint = "exchange_emsmdb";

	/* Fill in all the operations */
	module.init = dummy_init;
	module.unbind = dummy_unbind;
	module.push = dummy_push;
	module.ndr_pull = dummy_ndr_pull;
	module.pull = dummy_pull;
	module.dispatch = dummy_dispatch;

	/* Register ourselves with the MAPIPROXY subsytem */
	ret = mapiproxy_module_register(&module);
	if (!NT_STATUS_IS_OK(ret)) {
		OC_DEBUG(0, "Failed to register 'dummy' mapiproxy module!");
		return ret;
	}

	return ret;
}
