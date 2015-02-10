/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008-2011

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
#include "utils/dlinklist.h"
#include "libmapiproxy.h"
#include <util/debug.h>

/**
   \file dcesrv_mapiproxy_module.c

   \brief mapiproxy modules management
 */

static struct mp_module {
	struct mapiproxy_module	*mp_module;
} *mp_modules = NULL;

int					num_mp_modules;
static struct mapiproxy_module_list	*mpm_list = NULL;


NTSTATUS mapiproxy_module_push(struct dcesrv_call_state *dce_call,
			       TALLOC_CTX *mem_ctx, void *r)
{
	struct mapiproxy_module_list		*mpm;
	const struct ndr_interface_table	*table;
	NTSTATUS				status;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;

	for (mpm = mpm_list; mpm; mpm = mpm->next) {
		if (mpm->module->endpoint && 
		    ((strcmp(mpm->module->endpoint, "any") == 0) ||
		     (table->name && (strcmp(table->name, mpm->module->endpoint) == 0)))) {
			if (mpm->module->push) {
				status = mpm->module->push(dce_call, mem_ctx, r);
				NT_STATUS_NOT_OK_RETURN(status);
			}
		}
	}

	return NT_STATUS_OK;
}


NTSTATUS mapiproxy_module_ndr_pull(struct dcesrv_call_state *dce_call,
				   TALLOC_CTX *mem_ctx, struct ndr_pull *pull)
{
	struct mapiproxy_module_list		*mpm;
	const struct ndr_interface_table	*table;
	NTSTATUS				status;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;

	for (mpm = mpm_list; mpm; mpm = mpm->next) {
		if (mpm->module->endpoint && 
		    ((strcmp(mpm->module->endpoint, "any") == 0) ||
		     (table->name && (strcmp(table->name, mpm->module->endpoint) == 0)))) {
			if (mpm->module->ndr_pull) {
				status = mpm->module->ndr_pull(dce_call, mem_ctx, pull);
				NT_STATUS_NOT_OK_RETURN(status);
			}
		}
	}

	return NT_STATUS_OK;
}


NTSTATUS mapiproxy_module_pull(struct dcesrv_call_state *dce_call,
			       TALLOC_CTX *mem_ctx, void *r)
{
	struct mapiproxy_module_list		*mpm;
	const struct ndr_interface_table	*table;
	NTSTATUS				status;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;

	for (mpm = mpm_list; mpm; mpm = mpm->next) {
		if (mpm->module->endpoint && 
		    ((strcmp(mpm->module->endpoint, "any") == 0) ||
		     (table->name && (strcmp(table->name, mpm->module->endpoint) == 0)))) {
			if (mpm->module->pull) {
				status = mpm->module->pull(dce_call, mem_ctx, r);
				NT_STATUS_NOT_OK_RETURN(status);
			}
		}
	}

	return NT_STATUS_OK;
}


NTSTATUS mapiproxy_module_dispatch(struct dcesrv_call_state *dce_call,
				   TALLOC_CTX *mem_ctx, void *r, 
				   struct mapiproxy *mapiproxy)
{
	struct mapiproxy_module_list		*mpm;
	const struct ndr_interface_table	*table;
	NTSTATUS				status;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;

	for (mpm = mpm_list; mpm; mpm = mpm->next) {
		if (mpm->module->endpoint && 
		    ((strcmp(mpm->module->endpoint, "any") == 0) ||
		     (table->name && (strcmp(table->name, mpm->module->endpoint) == 0)))) {
			if (mpm->module->dispatch) {
				status = mpm->module->dispatch(dce_call, mem_ctx, r, mapiproxy);
				NT_STATUS_NOT_OK_RETURN(status);
			}
		}
	}

	return NT_STATUS_OK;
}


NTSTATUS mapiproxy_module_unbind(struct server_id server_id, uint32_t context_id)
{
	struct mapiproxy_module_list	*mpm;
	NTSTATUS			status;

	for (mpm = mpm_list; mpm; mpm = mpm->next) {
		if (mpm->module->unbind) {
			status = mpm->module->unbind(server_id, context_id);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	}

	return NT_STATUS_OK;
}


extern NTSTATUS mapiproxy_module_register(const void *_mp_module)
{
	const struct mapiproxy_module	*mp_module = (const struct mapiproxy_module *) _mp_module;

	mp_modules = realloc_p(mp_modules, struct mp_module, num_mp_modules + 1);
	if (!mp_modules) {
		smb_panic("out of memory in mapiproxy_register");
	}

	mp_modules[num_mp_modules].mp_module = (struct mapiproxy_module *) smb_xmemdup(mp_module, sizeof (*mp_module));
	mp_modules[num_mp_modules].mp_module->name = smb_xstrdup(mp_module->name);

	num_mp_modules++;

	DEBUG(3, ("MAPIPROXY module '%s' registered\n", mp_module->name));

	return NT_STATUS_OK;
}


static NTSTATUS mapiproxy_module_load(struct dcesrv_context *dce_ctx)
{
	char				**modules;
	struct mapiproxy_module_list	*module;
	int				i;
	NTSTATUS			status;

	/* Fetch the module list from smb.conf */
	modules = str_list_make(dce_ctx, lpcfg_parm_string(dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "modules"), NULL);

	/* Add modules to the list */
	for (i = 0; modules[i]; i++) {
		module = talloc_zero(dce_ctx, struct mapiproxy_module_list);
		module->module = mapiproxy_module_byname(modules[i]);
		if (module->module) {
			DLIST_ADD_END(mpm_list, module, struct mapiproxy_module_list *);
			DEBUG(3, ("MAPIPROXY module '%s' loaded\n", modules[i]));
			if (module->module->init) {
				status = module->module->init(dce_ctx);
				NT_STATUS_NOT_OK_RETURN(status);
			}
		} else {
			DEBUG(0, ("MAPIPROXY module '%s' not found\n", modules[i]));
		}
	}

	for (module = mpm_list; module; module = module->next) {
		DEBUG(3, ("mapiproxy_module_load '%s' (%s)\n", module->module->name, module->module->description));
	}

	return NT_STATUS_OK;
}


_PUBLIC_ NTSTATUS mapiproxy_module_init(struct dcesrv_context *dce_ctx)
{
	openchange_plugin_init_fn *mpm;
	NTSTATUS			ret;

	mpm = load_openchange_plugins(NULL, "dcerpc_mapiproxy");

	if (mpm != NULL) {
		int i;
		for (i = 0; mpm[i]; i++) { mpm[i](); }
	}

	talloc_free(mpm);

	ret = mapiproxy_module_load(dce_ctx);

	return ret;
}

const struct mapiproxy_module *mapiproxy_module_byname(const char *name)
{
	int	i;

	if (!name) return NULL;

	for (i = 0; i < num_mp_modules; i++) {
		if (strcmp(mp_modules[i].mp_module->name, name) == 0) {
			return mp_modules[i].mp_module;
		}
	}

	return NULL;
}
