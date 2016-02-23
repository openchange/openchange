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
   \file oxorule.c

   \brief E-mail rules object routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details EcDoRpc GetRulesTable (0x3f) Rop. This operation gets the
   rules table of a folder.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the GetRulesTable EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the GetRulesTable EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetRulesTable(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*parent;
	struct mapi_handles	*rec;
	struct emsmdbp_object	*object;
	void			*data = NULL;
	uint32_t		handle;

	OC_DEBUG(4, "exchange_emsmdb: [OXORULE] GetRulesTable (0x3f) -- stub\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Initialize default GetRulesTable reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_GetRulesTable.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	/* Ensure parent handle references a folder object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Check we have a logon user */
	if (!emsmdbp_ctx->logon_user) {
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		goto end;
	}

	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	object = (struct emsmdbp_object *) data;
	if (object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = MAPI_E_INVALID_OBJECT;
		OC_DEBUG(5, "  unhandled object type: %d\n", object->type);
		goto end;
	}

	/* Initialize Table object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
	handles[mapi_repl->handle_idx] = rec->handle;

	object = emsmdbp_object_table_init((TALLOC_CTX *)rec, emsmdbp_ctx, object);
	if (object) {
		retval = mapi_handles_set_private_data(rec, object);
		/* rules tables are stub objects for now */
		object->object.table->denominator = 0;
		object->object.table->ulType = MAPISTORE_RULE_TABLE;
	}
end:
	*size += libmapiserver_RopGetRulesTable_size();

	return MAPI_E_SUCCESS;
}


/**
   \details EcDoRpc ModifyRules (0x41) Rop. This operation modifies
   the rules associated with a folder


   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the ModifyRules EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the ModifyRules EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopModifyRules(TALLOC_CTX *mem_ctx,
						struct emsmdbp_context *emsmdbp_ctx,
						struct EcDoRpc_MAPI_REQ *mapi_req,
						struct EcDoRpc_MAPI_REPL *mapi_repl,
						uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	struct mapi_handles	*parent;
	struct emsmdbp_object	*object;
	void			*data = NULL;
	uint32_t		handle;

	OC_DEBUG(4, "exchange_emsmdb: [OXORULE] ModifyRules (0x41)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	/* Ensure parent handle references a folder object */
	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent);
	OPENCHANGE_RETVAL_IF(retval, retval, NULL);

	/* Initialize default ModifyRules reply */
	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	mapi_handles_get_private_data(parent, &data);
	object = (struct emsmdbp_object *) data;
	if (!object || object->type != EMSMDBP_OBJECT_FOLDER) {
		mapi_repl->error_code = MAPI_E_NOT_FOUND;
		goto end;
	}

	handles[mapi_repl->handle_idx] = handles[mapi_req->handle_idx];

end:
	*size += libmapiserver_RopModifyRules_size();

	return MAPI_E_SUCCESS;
}
