/*
   OpenChange Server implementation.

   EMSABP: Address Book Provider implementation

   Copyright (C) Julien Kerihuel 2006-2009.
   Copyright (C) Pauline Khun 2006.

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
   \file emsabp.c

   \brief Address Book Provider implementation
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_nsp.h"
#include <ldb.h>
#include <ldb_errors.h>
#include <util/debug.h>

/**
   \details Initialize the EMSABP context and open connections to
   Samba databases.

   \param lp_ctx pointer to the loadparm context

   \return Allocated emsabp_context on success, otherwise NULL
 */
_PUBLIC_ struct emsabp_context *emsabp_init(struct loadparm_context *lp_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct emsabp_context	*emsabp_ctx;
	struct event_context	*ev;
	char			*configuration = NULL;
	char			*users = NULL;
	int			ret;

	/* Sanity checks */
	if (!lp_ctx) return NULL;

	mem_ctx = talloc_init("emsabp_init");
	
	emsabp_ctx = talloc_zero(mem_ctx, struct emsabp_context);
	if (!emsabp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsabp_ctx->mem_ctx = mem_ctx;

	ev = event_context_init(mem_ctx);
	if (!ev) {
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return an opaque context pointer on the configuration database */
	configuration = private_path(mem_ctx, lp_ctx, "configuration.ldb");
	emsabp_ctx->conf_ctx = ldb_init(mem_ctx, ev);
	if (!emsabp_ctx->conf_ctx) {
		talloc_free(configuration);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_connect(emsabp_ctx->conf_ctx, configuration, LDB_FLG_RDONLY, NULL);
	talloc_free(configuration);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("[%s:%d]: Connection to \"configuration.ldb\" failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return an opaque context pointer to the users database */
	users = private_path(mem_ctx, lp_ctx, "users.ldb");
	emsabp_ctx->users_ctx = ldb_init(mem_ctx, ev);
	if (!emsabp_ctx->users_ctx) {
		talloc_free(users);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_connect(emsabp_ctx->users_ctx, users, LDB_FLG_RDONLY, NULL);
	talloc_free(users);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("[%s:%d]: Connection to \"users.ldb\" failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}

	return emsabp_ctx;
}


_PUBLIC_ bool emsabp_destructor(void *data)
{
	struct emsabp_context	*emsabp_ctx = (struct emsabp_context *)data;

	if (emsabp_ctx) {
		DEBUG(6, ("emsabp_ctx is at addr %p\n", emsabp_ctx));
		talloc_free(emsabp_ctx->mem_ctx);
		return true;
	}

	return false;
}


/**
   \details Check if the authenticated user belongs to the Exchange
   organization

   \param dce_call pointer to the session context
   \param emsabp_ctx pointer to the EMSABP context

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsabp_verify_user(struct dcesrv_call_state *dce_call,
				 struct emsabp_context *emsabp_ctx)
{
	int			ret;
	const char		*username = NULL;
	int			msExchUserAccountControl;
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	recipient_attrs[] = { "msExchUserAccountControl", NULL };

	username = dce_call->context->conn->auth_state.session_info->server_info->account_name;

	ldb_filter = talloc_asprintf(emsabp_ctx->mem_ctx, "CN=%s", username);
	ret = ldb_search(emsabp_ctx->users_ctx, emsabp_ctx->mem_ctx, &res, 
			 ldb_get_default_basedn(emsabp_ctx->users_ctx),
			 scope, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);

	/* If the search failed */
	if (ret != LDB_SUCCESS || !res->count) {
		return false;
	}

	/* If msExchUserAccountControl attribute is not found */
	if (!res->msgs[0]->num_elements) {
		return false;
	}

	/* If the attribute exists check its value */
	msExchUserAccountControl = ldb_msg_find_attr_as_int(res->msgs[0], "msExchUserAccountControl", 2);
	if (msExchUserAccountControl == 2) {
		return false;
	}

	return true;
}


/**
   \details Check if the provided codepage is correct

   \param lp_ctx pointer to the loadparm context
   \param emsabp_ctx pointer to the EMSABP context
   \param CodePage the codepage identifier

   \note The prototype is currently incorrect, but we are looking for
   a better way to check codepage, maybe within AD. At the moment this
   function is just a wrapper over libmapi valid_codepage function.

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsabp_verify_codepage(struct loadparm_context *lp_ctx,
				     struct emsabp_context *emsabp_ctx,
				     uint32_t CodePage)
{
	return valid_codepage(CodePage);
}


/**
   \details Retrieve the NSPI server GUID from the server object in
   the configuration LDB database

   \param lp_ctx pointer to the loadparm context
   \param emsabp_ctx pointer to the EMSABP context

   \return An allocated GUID structure on success, otherwise NULL
 */
_PUBLIC_ struct GUID *emsabp_get_server_GUID(struct loadparm_context *lp_ctx,
					     struct emsabp_context *emsabp_ctx)
{
	int			ret;
	struct GUID		*guid = (struct GUID *) NULL;
	const char		*netbiosname = NULL;
	const char		*guid_str = NULL;
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	char			*dn = NULL;
	struct ldb_dn		*ldb_dn = NULL;
	const char * const	recipient_attrs[] = { "*", NULL };
	const char		*firstorgdn = NULL;

	netbiosname = lp_netbios_name(lp_ctx);
	if (!netbiosname) return NULL;

	/* Step 1. Find the Exchange Organization */
	ldb_filter = talloc_strdup(emsabp_ctx->mem_ctx, "(objectClass=msExchOrganizationContainer)");
	ret = ldb_search(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, &res,
			 ldb_get_default_basedn(emsabp_ctx->conf_ctx),
			 scope, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);

	if (ret != LDB_SUCCESS || !res->count) {
		return NULL;
	}

	firstorgdn = ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL);
	if (!firstorgdn) {
		return NULL;
	}

	/* Step 2. Find the OpenChange Server object */
	dn = talloc_asprintf(emsabp_ctx->mem_ctx, "CN=Servers,CN=First Administrative Group,CN=Administrative Groups,%s",
			     firstorgdn);
	ldb_dn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->conf_ctx, dn);
	talloc_free(dn);
	if (!ldb_dn_validate(ldb_dn)) {
		return NULL;
	}

	ret = ldb_search(emsabp_ctx->conf_ctx, emsabp_ctx->mem_ctx, &res, ldb_dn, 
			 scope, recipient_attrs, "(cn=%s)", netbiosname);
	if (ret != LDB_SUCCESS || !res->count) {
		return NULL;
	}

	/* Step 3. Retrieve the objectGUID GUID */
	guid_str = ldb_msg_find_attr_as_string(res->msgs[0], "objectGUID", NULL);
	if (!guid_str) return NULL;

	guid = talloc_zero(emsabp_ctx->mem_ctx, struct GUID);
	GUID_from_string(guid_str, guid);
	
	return guid;
}
