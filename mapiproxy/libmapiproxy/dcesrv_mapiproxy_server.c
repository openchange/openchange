/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2011

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
#include "libmapiproxy.h"
#include "utils/dlinklist.h"
#include <util/debug.h>

/**
   \file dcesrv_mapiproxy_server.c

   \brief mapiproxy server modules management
 */

static struct server_module {
	struct mapiproxy_module	*server_module;
} *server_modules = NULL;

int					num_server_modules;
static struct mapiproxy_module_list	*server_list = NULL;

static TDB_CONTEXT			*emsabp_tdb_ctx = NULL;
static void				*openchange_ldb_ctx = NULL;

NTSTATUS mapiproxy_server_dispatch(struct dcesrv_call_state *dce_call,
				   TALLOC_CTX *mem_ctx, void *r,
				   struct mapiproxy *mapiproxy)
{
	struct mapiproxy_module_list		*server;
	const struct ndr_interface_table	*table;
	NTSTATUS				status;

	table = (const struct ndr_interface_table *)dce_call->context->iface->private_data;

	for (server = server_list; server; server = server->next) {
		if (server->module->endpoint && table->name &&
		    !strcmp(table->name, server->module->endpoint)) {
			if (server->module->dispatch) {
				mapiproxy->norelay = true;
				status = server->module->dispatch(dce_call, mem_ctx, r, mapiproxy);
				NT_STATUS_NOT_OK_RETURN(status);
			}
		}
	}

	return NT_STATUS_OK;
}


NTSTATUS mapiproxy_server_unbind(struct server_id server_id, uint32_t context_id)
{
	struct mapiproxy_module_list		*server;
	NTSTATUS				status;

	for (server = server_list; server; server = server->next) {
		if (server->module->unbind) {
			status = server->module->unbind(server_id, context_id);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	}

	return NT_STATUS_OK;
}


extern NTSTATUS mapiproxy_server_register(const void *_server_module)
{
	const struct mapiproxy_module	*server_module = (const struct mapiproxy_module *) _server_module;

	server_modules = realloc_p(server_modules, struct server_module, num_server_modules + 1);
	if (!server_modules) {
		smb_panic("out of memory in mapiproxy_server_register");
	}

	server_modules[num_server_modules].server_module = (struct mapiproxy_module *) smb_xmemdup(server_module, sizeof (*server_module));
	server_modules[num_server_modules].server_module->name = smb_xstrdup(server_module->name);

	num_server_modules++;

	DEBUG(3, ("MAPIPROXY server '%s' registered\n", server_module->name));

	return NT_STATUS_OK;
}


_PUBLIC_ bool mapiproxy_server_loaded(const char *endpoint)
{
	struct mapiproxy_module_list	*server;
	
	if (!endpoint) return false;

	for (server = server_list; server; server = server->next) {
		if (server->module->endpoint && !strcmp(endpoint, server->module->endpoint)) {
			return true;
		}
	}

	return false;
}


static NTSTATUS mapiproxy_server_overwrite(TALLOC_CTX *mem_ctx, const char *name, const char *endpoint)
{
	struct mapiproxy_module_list	*server;
	struct mapiproxy_module_list	*server_load;

	if (!name) return NT_STATUS_NOT_FOUND;
	if (!endpoint) return NT_STATUS_NOT_FOUND;

	/* Step 0. Ensure given module matches with endpoint */
	server_load = talloc_zero(mem_ctx, struct mapiproxy_module_list);
	server_load->module = mapiproxy_server_byname(name);
	if (!server_load->module) {
		DEBUG(0, ("MAPIPROXY ERROR: couldn't load server '%s'\n", name));
		talloc_free(server_load);
		return NT_STATUS_NOT_FOUND;
	} else {
		if (strcmp(server_load->module->endpoint, endpoint)) {
			DEBUG(0, ("MAPIPROXY ERROR: %s endpoint expected for %s but %s found!\n",
				  endpoint, server_load->module->name, server_load->module->endpoint));
			talloc_free(server_load);
			return NT_STATUS_NOT_FOUND;
		}
	}

	/* Step 1. Seek if this module has already been loaded */
	for (server = server_list; server; server = server->next) {
		if (!strcmp(server->module->name, name) &&
		    !strcmp(server->module->endpoint, endpoint)) {
			DEBUG(0, ("MAPIPROXY: server '%s' already loaded - skipped\n", name));
			talloc_free(server_load);
			return NT_STATUS_OK;
		}
	}

	/* Step 2. Delete any loaded server matching given endpoint */
	for (server = server_list; server; server = server->next) {
		if (!strcmp(server->module->endpoint, endpoint)) {
			DLIST_REMOVE(server_list, server);
			talloc_free(server);
		}
	}

	/* Step 3. Load custom server */
	DLIST_ADD_END(server_list, server_load, struct mapiproxy_module_list *);

	return NT_STATUS_OK;
}


static NTSTATUS mapiproxy_server_load(struct dcesrv_context *dce_ctx)
{
	NTSTATUS				status;
	struct mapiproxy_module_list		*server;
	bool					server_mode;
	int					i;
	const char				*nspi;
	const char				*emsmdb;
	const char				*rfr;
	const char				*server_name[] = { NDR_EXCHANGE_NSP_NAME, 
								   NDR_EXCHANGE_EMSMDB_NAME,
								   NDR_EXCHANGE_DS_RFR_NAME, NULL };

	/* Check server mode */
	server_mode = lpcfg_parm_bool(dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "server", true);
	DEBUG(0, ("MAPIPROXY server mode %s\n", (server_mode == false) ? "disabled" : "enabled"));

	if (server_mode == true) {
		DEBUG(0, ("MAPIPROXY proxy mode disabled\n"));

		for (i = 0; server_name[i]; i++) {
			server = talloc_zero(dce_ctx, struct mapiproxy_module_list);
			server->module = mapiproxy_server_bystatus(server_name[i], MAPIPROXY_DEFAULT);
			if (server->module) {
				DLIST_ADD_END(server_list, server, struct mapiproxy_module_list *);
			} else {
				DEBUG(0, ("MAPIPROXY ERROR: couldn't load server '%s'\n", server_name[i]));
			}
		}
	}

	/* Check for override/custom NSPI server */
	nspi = lpcfg_parm_string(dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "nspi_server");
	mapiproxy_server_overwrite(dce_ctx, nspi, NDR_EXCHANGE_NSP_NAME);

	/* Check for override/custom EMSMDB server */
	emsmdb = lpcfg_parm_string(dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "emsmdb_server");
	mapiproxy_server_overwrite(dce_ctx, emsmdb, NDR_EXCHANGE_EMSMDB_NAME);

	/* Check for override/custom RFR server */
	rfr = lpcfg_parm_string(dce_ctx->lp_ctx, NULL, "dcerpc_mapiproxy", "rfr_server");
	mapiproxy_server_overwrite(dce_ctx, rfr, NDR_EXCHANGE_DS_RFR_NAME);

	for (server = server_list; server; server = server->next) {
		DEBUG(3, ("mapiproxy_server_load '%s' (%s)\n", 
			  server->module->name, server->module->description));
		if (server->module->init) {
			status = server->module->init(dce_ctx);
			NT_STATUS_NOT_OK_RETURN(status);
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize mapiproxy servers modules

   \param dce_ctx pointer to the connection context

   \return NT_STATUS_OK on success otherwise NT error
 */
_PUBLIC_ NTSTATUS mapiproxy_server_init(struct dcesrv_context *dce_ctx)
{
	openchange_plugin_init_fn *servers;
	NTSTATUS		ret;

	servers = load_openchange_plugins(NULL, "dcerpc_mapiproxy_server");

	if (servers != NULL) {
		int i;
		for (i = 0; servers[i]; i++) { servers[i](); }
	}

	talloc_free(servers);

	ret = mapiproxy_server_load(dce_ctx);

	return ret;
}


const struct mapiproxy_module *mapiproxy_server_bystatus(const char *name, enum mapiproxy_status status)
{
	int	i;

	if (!name) return NULL;

	for (i = 0; i < num_server_modules; i++) {
		if ((strcmp(server_modules[i].server_module->name, name) == 0) && 
		    (server_modules[i].server_module->status == status)) {
			return server_modules[i].server_module;
		}
	}

	return NULL;
}


const struct mapiproxy_module *mapiproxy_server_byname(const char *name)
{
	int	i;

	if (!name) return NULL;

	for (i = 0; i < num_server_modules; i++) {
		if ((strcmp(server_modules[i].server_module->name, name) == 0)) {
			return server_modules[i].server_module;
		}
	}

	return NULL;
}


/**
   \details Initialize an EMSABP TDB context available to all
   instances when Samba is not run in single mode.

   \param lp_ctx pointer to the loadparm context

   \note TDB database can't be opened twice with O_RDWR flags. We
   ensure here we have a general context initialized, which we'll
   reopen within forked instances

   return Allocated TDB context on success, otherwise NULL
 */
_PUBLIC_ TDB_CONTEXT *mapiproxy_server_emsabp_tdb_init(struct loadparm_context *lp_ctx)
{
	char			*tdb_path;
	TALLOC_CTX		*mem_ctx;

	if (emsabp_tdb_ctx) return emsabp_tdb_ctx;

	mem_ctx = talloc_named(NULL, 0, "mapiproxy_server_emsabp_tdb_init");
	if (!mem_ctx) return NULL;

	/* Step 0. Retrieve a TDB context pointer on the emsabp_tdb database */
	tdb_path = talloc_asprintf(mem_ctx, "%s/%s", lpcfg_private_dir(lp_ctx), EMSABP_TDB_NAME);
	emsabp_tdb_ctx = tdb_open(tdb_path, 0, 0, O_RDWR|O_CREAT, 0600);
	talloc_free(tdb_path);
	if (!emsabp_tdb_ctx) {
		DEBUG(3, ("[%s:%d]: %s\n", __FUNCTION__, __LINE__, strerror(errno)));
		talloc_free(mem_ctx);
		return NULL;
	}

	talloc_free(mem_ctx);

	return emsabp_tdb_ctx;
}


/**
   \details Initialize an openchangedb context available to all
   mapiproxy instances. This context points on the OpenChange
   dispatcher database used within emsmdb default provider.

   \param lp_ctx pointer to the loadparm context

   \note The memory context is not free'd leading and causes a loss
   record.

   \return Allocated openchangedb context on success, otherwise NULL
 */
_PUBLIC_ void *mapiproxy_server_openchangedb_init(struct loadparm_context *lp_ctx)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	ret;

	/* Sanity checks */
	if (openchange_ldb_ctx) return openchange_ldb_ctx;

	mem_ctx = talloc_named(NULL, 0, "mapiproxy_server_openchangedb_init");
	if (!mem_ctx) return NULL;

	ret = openchangedb_initialize(mem_ctx, lp_ctx,
		(struct openchangedb_context **)&openchange_ldb_ctx);

	if (ret != MAPI_E_SUCCESS) return NULL;

	return openchange_ldb_ctx;
}
