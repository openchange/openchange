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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "gen_ndr/asyncemsmdb.h"
#include "gen_ndr/ndr_asyncemsmdb.h"
#include "libmapi/oc_log.h"

/**
   \file dcesrv_exchange_emsmdb.c

   \brief OpenChange EMSMDB Server implementation
 */

struct EcDoAsyncWaitEx_private {
	struct dcesrv_call_state	*dce_call;
	struct EcDoAsyncWaitEx		*r;
};

static void EcDoAsyncWaitEx_handler(struct tevent_context *ev, struct tevent_timer *te,
				    struct timeval t, void *private_data)
{
	struct EcDoAsyncWaitEx_private	*p = talloc_get_type(private_data,
							     struct EcDoAsyncWaitEx_private);
	struct EcDoAsyncWaitEx	*r = p->r;
	NTSTATUS status;

	OC_DEBUG(0, "EcDoAsyncWaitEx_handler");
	*r->out.pulFlagsOut = 0x1;

	status = dcesrv_reply(p->dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(0, "EcDoAsyncWaitEx_handler: dcesrv_reply() failed - %s", nt_errstr(status));
	}
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
	struct EcDoAsyncWaitEx_private	*p;

	OC_DEBUG(0, "exchange_asyncemsmdb: EcDoAsyncWaitEx (0x0)");

	*r->out.pulFlagsOut = 0x1;

	/* we're allowed to reply async */
	p = talloc(mem_ctx, struct EcDoAsyncWaitEx_private);
	if (!p) {
		return NT_STATUS_OK;
	}

	p->dce_call = dce_call;
	p->r = r;

	tevent_add_timer(dce_call->event_ctx, p,
			 timeval_add(&dce_call->time, 10, 0),
			 EcDoAsyncWaitEx_handler, p);

	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;
	return NT_STATUS_OK;
}

NTSTATUS dcerpc_server_asyncemsmdb_init(void);
NTSTATUS ndr_table_register(const struct ndr_interface_table *);


NTSTATUS samba_init_module(void)
{
	NTSTATUS status;

	status = dcerpc_server_asyncemsmdb_init();
	NT_STATUS_NOT_OK_RETURN(status);

	status = ndr_table_register(&ndr_table_asyncemsmdb);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

#include "gen_ndr/ndr_asyncemsmdb_s.c"
