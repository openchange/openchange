/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Brad Hards <bradh@openchange.org> 2010

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
   \file oxomsg.c

   \brief Server-side message routines and Rops
 */

#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details EcDoRpc SubmitMessage (0x32) Rop. This operation marks a message
   as being ready to send (subject to some flags).

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the SubmitMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the SubmitMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSubmitMessage(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCMSG] SubmitMessage (0x32)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* TODO: actually implement this */

	*size += libmapiserver_RopSubmitMessage_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/* Get the organisation name (like "First Organization") as a DN. */
static bool mapiserver_get_org_dn(struct emsmdbp_context *emsmdbp_ctx,
					    struct ldb_dn **basedn)
{
	int			ret;
	struct ldb_result	*res = NULL;

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 ldb_get_config_basedn(emsmdbp_ctx->samdb_ctx),
                         LDB_SCOPE_SUBTREE, NULL,
			 "(|(objectClass=msExchOrganizationContainer))");

	/* If the search failed */
        if (ret != LDB_SUCCESS) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] mapiserver_get_org_dn ldb_search failure.\n"));
		return false;
        }
        /* If we didn't get the expected entry */
	if (res->count != 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] mapiserver_get_org_dn unexpected entry count: %i (expected 1).\n", res->count));
		return false;
	}
	
	*basedn = ldb_dn_new(emsmdbp_ctx, emsmdbp_ctx->samdb_ctx,
			     ldb_msg_find_attr_as_string(res->msgs[0], "distinguishedName", NULL));
	return true;
}

/**
   \details EcDoRpc GetAddressTypes (0x49) Rop. This operation gets
   the valid address types (e.g. "SMTP", "X400", "EX")

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the AddressTypes EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the AddressTypes EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetAddressTypes(TALLOC_CTX *mem_ctx,
						    struct emsmdbp_context *emsmdbp_ctx,
						    struct EcDoRpc_MAPI_REQ *mapi_req,
						    struct EcDoRpc_MAPI_REPL *mapi_repl,
						    uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;
	int			ret;
	struct ldb_result	*res = NULL;
	const char * const	attrs[] = { "msExchTemplateRDNs", NULL };
        uint32_t                j;
	struct ldb_dn 		*basedn = 0;
	
	DEBUG(4, ("exchange_emsmdb: [OXOMSG] AddressTypes (0x49)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapiserver_get_org_dn(emsmdbp_ctx, &basedn);
	ldb_dn_add_child_fmt(basedn, "CN=ADDRESSING");
	ldb_dn_add_child_fmt(basedn, "CN=ADDRESS-TEMPLATES");

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res, basedn,
                         LDB_SCOPE_SUBTREE, attrs, "CN=%x", emsmdbp_ctx->userLanguage);
        /* If the search failed */
        if (ret != LDB_SUCCESS) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes ldb_search failure.\n"));
		return MAPI_E_CORRUPT_STORE;
        }
        /* If we didn't get the expected entry */
	if (res->count != 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes unexpected entry count: %i (expected 1).\n", res->count));
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get the expected number of elements in our record */
	if (res->msgs[0]->num_elements != 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes unexpected element count: %i (expected 1).\n", res->msgs[0]->num_elements));
		return MAPI_E_CORRUPT_STORE;
	}
	/* If we didn't get at least one address type, things are probably bad. It _could_ be allowable though. */
	if (res->msgs[0]->elements[0].num_values < 1) {
	  	DEBUG(1, ("exchange_emsmdb: [OXOMSG] AddressTypes unexpected values count: %i (expected 1).\n", res->msgs[0]->num_elements));
	}

	/* If we got to here, it looks sane. Build the reply message. */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;
	mapi_repl->u.mapi_AddressTypes.cValues = res->msgs[0]->elements[0].num_values;
	mapi_repl->u.mapi_AddressTypes.size = 0;
	mapi_repl->u.mapi_AddressTypes.transport = talloc_array(mem_ctx, struct mapi_LPSTR, mapi_repl->u.mapi_AddressTypes.cValues);
	for (j = 0; j < mapi_repl->u.mapi_AddressTypes.cValues; ++j) {
		const char *addr_type;
		addr_type = (const char *)res->msgs[0]->elements[0].values[j].data;
		mapi_repl->u.mapi_AddressTypes.transport[j].lppszA = talloc_asprintf(mem_ctx, "%s", addr_type);
		mapi_repl->u.mapi_AddressTypes.size += (strlen(mapi_repl->u.mapi_AddressTypes.transport[j].lppszA) + 1);
	}
	*size = libmapiserver_RopGetAddressTypes_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}


/**
   \details EcDoRpc OptionsData (0x6f) Rop. This doesn't really do anything,
   but could be used to provide HelpData if we wanted to do something like that
   later.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OptionsData EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the OptionsData EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopOptionsData(TALLOC_CTX *mem_ctx,
						struct emsmdbp_context *emsmdbp_ctx,
						struct EcDoRpc_MAPI_REQ *mapi_req,
						struct EcDoRpc_MAPI_REPL *mapi_repl,
						uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;

	DEBUG(4, ("exchange_emsmdb: [OXOMSG] OptionsData (0x6f)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);
	
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;
	mapi_repl->u.mapi_OptionsData.Reserved = 0x00;
	mapi_repl->u.mapi_OptionsData.OptionsInfo.cb = 0x0000;
	mapi_repl->u.mapi_OptionsData.OptionsInfo.lpb = talloc_array(mem_ctx, uint8_t, mapi_repl->u.mapi_OptionsData.OptionsInfo.cb);
	mapi_repl->u.mapi_OptionsData.HelpFileSize = 0x0000;
	mapi_repl->u.mapi_OptionsData.HelpFile = talloc_array(mem_ctx, uint8_t, mapi_repl->u.mapi_OptionsData.HelpFileSize);

	*size = libmapiserver_RopOptionsData_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}

/**
   \details EcDoRpc GetTransportFolder (0x6d) ROP.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetTransportFolder EcDoRpc_MAPI_REQ
   \param mapi_repl pointer to the GetTransportFolder EcDoRpc_MAPI_REPL
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetTransportFolder(TALLOC_CTX *mem_ctx,
						       struct emsmdbp_context *emsmdbp_ctx,
						       struct EcDoRpc_MAPI_REQ *mapi_req,
						       struct EcDoRpc_MAPI_REPL *mapi_repl,
						       uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;

	DEBUG(4, ("exchange_emsmdb: [OXOMSG] GetTransportFolder (0x6d)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* TODO: check if the login is a valid (private?) login */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = retval;
	/* TODO: find the real FID */
	mapi_repl->u.mapi_GetTransportFolder.FolderId = 0x12345678;

	*size = libmapiserver_RopGetTransportFolder_size(mapi_repl);

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

	return retval;
}

