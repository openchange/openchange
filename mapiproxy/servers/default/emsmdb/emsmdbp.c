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
#include "dcesrv_exchange_emsmdb.h"

/**
   \details Release the MAPISTORE context used by EMSMDB provider
   context

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int emsmdbp_mapi_store_destructor(void *data)
{
	struct mapistore_context *mstore_ctx = (struct mapistore_context *) data;

	mapistore_release(mstore_ctx);
	DEBUG(6, ("[%s:%d]: MAPISTORE context released\n", __FUNCTION__, __LINE__));
	return true;
}

/**
   \details Release the MAPI handles context used by EMSMDB provider
   context

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int emsmdbp_mapi_handles_destructor(void *data)
{
	enum MAPISTATUS			retval;
	struct mapi_handles_context	*handles_ctx = (struct mapi_handles_context *) data;

	retval = mapi_handles_release(handles_ctx);
	DEBUG(6, ("[%s:%d]: MAPI handles context released (%s)\n", __FUNCTION__, __LINE__,
		  mapi_get_errstr(retval)));

	return (retval == MAPI_E_SUCCESS) ? 0 : -1;
}

/**
   \details Initialize the EMSMDBP context and open connections to
   Samba databases.

   \param lp_ctx pointer to the loadparm_context
   \param ldb_ctx pointer to the openchange dispatcher ldb database
   
   \return Allocated emsmdbp_context pointer on success, otherwise
   NULL
 */
_PUBLIC_ struct emsmdbp_context *emsmdbp_init(struct loadparm_context *lp_ctx,
					      void *ldb_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct emsmdbp_context	*emsmdbp_ctx;
	struct tevent_context	*ev;

	/* Sanity Checks */
	if (!lp_ctx) return NULL;

	mem_ctx  = talloc_named(NULL, 0, "emsmdbp_init");

	emsmdbp_ctx = talloc_zero(mem_ctx, struct emsmdbp_context);
	if (!emsmdbp_ctx) {
		talloc_free(mem_ctx);
		return NULL;
	}

	emsmdbp_ctx->mem_ctx = mem_ctx;

	ev = tevent_context_init(mem_ctx);
	if (!ev) {
		talloc_free(mem_ctx);
		return NULL;
	}

	/* Save a pointer to the loadparm context */
	emsmdbp_ctx->lp_ctx = lp_ctx;

	/* return an opaque context pointer on samDB database */
	emsmdbp_ctx->samdb_ctx = samdb_connect(mem_ctx, ev, lp_ctx, system_session(lp_ctx));
	if (!emsmdbp_ctx->samdb_ctx) {
		talloc_free(mem_ctx);
		DEBUG(0, ("[%s:%d]: Connection to \"sam.ldb\" failed\n", __FUNCTION__, __LINE__));
		return NULL;
	}

	/* Reference global OpenChange dispatcher database pointer within current context */
	emsmdbp_ctx->oc_ctx = ldb_ctx;

	/* Initialize the mapistore context */		
	emsmdbp_ctx->mstore_ctx = mapistore_init(mem_ctx, NULL);
	if (!emsmdbp_ctx->mstore_ctx) {
		DEBUG(0, ("[%s:%d]: MAPISTORE initialization failed\n", __FUNCTION__, __LINE__));

		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->mstore_ctx, (int (*)(void *))emsmdbp_mapi_store_destructor);

	/* Initialize MAPI handles context */
	emsmdbp_ctx->handles_ctx = mapi_handles_init(mem_ctx);
	if (!emsmdbp_ctx->handles_ctx) {
		DEBUG(0, ("[%s:%d]: MAPI handles context initialization failed\n", __FUNCTION__, __LINE__));
		talloc_free(mem_ctx);
		return NULL;
	}
	talloc_set_destructor((void *)emsmdbp_ctx->handles_ctx, (int (*)(void *))emsmdbp_mapi_handles_destructor);

	return emsmdbp_ctx;
}


/**
   \details Open openchange.ldb database

   \param lp_ctx pointer on the loadparm_context

   \note This function is just a wrapper over
   mapiproxy_server_openchange_ldb_init

   \return Allocated LDB context on success, otherwise NULL
 */
_PUBLIC_ void *emsmdbp_openchange_ldb_init(struct loadparm_context *lp_ctx)
{
	/* Sanity checks */
	if (!lp_ctx) return NULL;

	return mapiproxy_server_openchange_ldb_init(lp_ctx);
}


_PUBLIC_ bool emsmdbp_destructor(void *data)
{
	struct emsmdbp_context	*emsmdbp_ctx = (struct emsmdbp_context *)data;

	if (!emsmdbp_ctx) return false;

	talloc_free(emsmdbp_ctx);
	DEBUG(0, ("[%s:%d]: emsmdbp_ctx found and released\n", __FUNCTION__, __LINE__));

	return true;
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
	const char * const	recipient_attrs[] = { "msExchUserAccountControl", NULL };

	username = dce_call->context->conn->auth_state.session_info->server_info->account_name;

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "CN=%s", username);

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
	const char * const	recipient_attrs[] = { "*", NULL };

	/* Sanity Checks */
	if (!legacyExchangeDN) return false;

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "(legacyExchangeDN=%s)",
			 legacyExchangeDN);

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
