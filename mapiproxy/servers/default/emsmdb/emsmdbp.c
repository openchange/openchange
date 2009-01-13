/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009

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
   \file emsmdbp.c

   \brief EMSMDB Provider implementation
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details Initialize the EMSMDBP context and open connections to
   Samba databases.

   \param lp_ctx pointer to the loadparm_context
   
   \return Allocated emsmdbp_context pointer on success, otherwise
   NULL
 */
_PUBLIC_ struct emsmdbp_context *emsmdbp_init(struct loadparm_context *lp_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct emsmdbp_context	*emsmdbp_ctx;
	struct event_context	*ev;
	char			*configuration = NULL;
	char			*users = NULL;
	int			ret;

	/* Sanity Checks */
	if (!lp_ctx) return NULL;

	mem_ctx = talloc_init("emsmdbp_init");

	emsmdbp_ctx = talloc_zero(mem_ctx, struct emsmdbp_context);
	if (!emsmdbp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsmdbp_ctx->mem_ctx = mem_ctx;

	ev = event_context_init(mem_ctx);
	if (!ev) {
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Save a pointer to the loadparm context */
	emsmdbp_ctx->lp_ctx = lp_ctx;

	/* Return an opaque context pointer on the configuration database */
	configuration = private_path(mem_ctx, lp_ctx, "configuration.ldb");
	emsmdbp_ctx->conf_ctx = ldb_init(mem_ctx, ev);
	if (!emsmdbp_ctx->conf_ctx) {
		talloc_free(configuration);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_connect(emsmdbp_ctx->conf_ctx, configuration, LDB_FLG_RDONLY, NULL);
	talloc_free(configuration);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("[%s:%d]: Connection to \"configuration.ldb\" failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Return an opaque pointer on the users database */
	users = private_path(mem_ctx, lp_ctx, "users.ldb");
	emsmdbp_ctx->users_ctx = ldb_init(mem_ctx, ev);
	if (!emsmdbp_ctx->users_ctx) {
		talloc_free(users);
		talloc_free(mem_ctx);
		return NULL;
	}

	ret = ldb_connect(emsmdbp_ctx->users_ctx, users, LDB_FLG_RDONLY, NULL);
	talloc_free(users);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, ("[%s:%d]: Connection to \"users.ldb\" failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}

	return emsmdbp_ctx;
}


_PUBLIC_ bool emsmdbp_destructor(void *data)
{
	struct emsmdbp_context	*emsmdbp_ctx = (struct emsmdbp_context *)data;

	if (emsmdbp_ctx) {
		talloc_free(emsmdbp_ctx->mem_ctx);
		DEBUG(0, ("[%s:%d]: emsmdbp_ctx found and released\n", __FUNCTION__, __LINE__));
		return true;
	}

	return false;
}


/**
   \details Check if the authenticated user belongs to the Exchange
   organization and is enabled

   \param dce_call pointer to the session context
   \param emsmdbp_ctx pointer to the EMSMDBP context

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsmdbp_verify_user(struct dcesrv_call_state *dce_call,
				  struct emsmdbp_context *emsmdbp_ctx)
{
	int			ret;
	const char		*username = NULL;
	int			msExchUserAccountControl;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	recipient_attrs[] = { "msExchUserAccountControl", NULL };

	username = dce_call->context->conn->auth_state.session_info->server_info->account_name;

	ldb_filter = talloc_asprintf(emsmdbp_ctx->mem_ctx, "CN=%s", username);
	ret = ldb_search(emsmdbp_ctx->users_ctx, emsmdbp_ctx->mem_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->users_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, ldb_filter);
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
   \details Check if the user record which legacyExchangeDN points to
   belongs to the Exchange organization and is enabled

   \param dce_call pointer to the session context
   \param emsmdbp_ctx pointer to the EMSMDBP context
   \param legacyExchangeDN pointer to the userDN to lookup
   \param msg pointer on pointer to the LDB message matching the record

   \note Users can set msg to NULL if they do not intend to retrieve
   the message

   \return true on success, otherwise false
 */
_PUBLIC_ bool emsmdbp_verify_userdn(struct dcesrv_call_state *dce_call,
				    struct emsmdbp_context *emsmdbp_ctx,
				    const char *legacyExchangeDN,
				    struct ldb_message **msg)
{
	int			ret;
	int			msExchUserAccountControl;
	struct ldb_result	*res = NULL;
	char			*ldb_filter;
	const char * const	recipient_attrs[] = { "*", NULL };

	/* Sanity Checks */
	if (!legacyExchangeDN) return false;

	ldb_filter = talloc_asprintf(emsmdbp_ctx->mem_ctx, "(legacyExchangeDN=%s)", legacyExchangeDN);
	ret = ldb_search(emsmdbp_ctx->users_ctx, emsmdbp_ctx->mem_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->users_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, ldb_filter);
	talloc_free(ldb_filter);

	/* If the search failed */
	if (ret != LDB_SUCCESS || !res->count) {
		return false;
	}

	/* Checks msExchUserAccountControl value */
	msExchUserAccountControl = ldb_msg_find_attr_as_int(res->msgs[0], "msExchUserAccountControl", 2);
	if (msExchUserAccountControl == 2) {
		return false;
	}

	if (msg) {
		*msg = res->msgs[0];
	}

	return true;
}
