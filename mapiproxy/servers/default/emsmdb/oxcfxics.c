/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Wolfgang Sourdeau 2010

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
   \file oxcfxics.c

   \brief FastTransfer and ICS object routines and Rops
 */

#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "dcesrv_exchange_emsmdb.h"

/**
   \details EcDoRpc EcDoRpc_RopFastTransferSourceCopyTo (0x4d) Rop. This operation initializes a FastTransfer operation to download content from a given messaging object and its descendant subobjects.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFastTransferSourceCopyTo(TALLOC_CTX *mem_ctx,
							     struct emsmdbp_context *emsmdbp_ctx,
							     struct EcDoRpc_MAPI_REQ *mapi_req,
							     struct EcDoRpc_MAPI_REPL *mapi_repl,
							     uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] FastTransferSourceCopyTo (0x4d) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopFastTransferSourceCopyTo_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopFastTransferSourceGetBuffer (0x4e) Rop. This operation downloads the next portion of a FastTransfer stream that is produced by a previously configured download operation.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopFastTransferSourceGetBuffer(TALLOC_CTX *mem_ctx,
								struct emsmdbp_context *emsmdbp_ctx,
								struct EcDoRpc_MAPI_REQ *mapi_req,
								struct EcDoRpc_MAPI_REPL *mapi_repl,
								uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] FastTransferSourceGetBuffer (0x4e) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopFastTransferSourceGetBuffer_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncConfigure (0x70) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncConfigure(TALLOC_CTX *mem_ctx,
						  struct emsmdbp_context *emsmdbp_ctx,
						  struct EcDoRpc_MAPI_REQ *mapi_req,
						  struct EcDoRpc_MAPI_REPL *mapi_repl,
						  uint32_t *handles, uint16_t *size)
{
	uint32_t		folder_handle;
	struct mapi_handles	*folder_rec;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*folder_object;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncConfigure (0x70) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncConfigure_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncImportHierarchyChange (0x73) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncImportHierarchyChange(TALLOC_CTX *mem_ctx,
							      struct emsmdbp_context *emsmdbp_ctx,
							      struct EcDoRpc_MAPI_REQ *mapi_req,
							      struct EcDoRpc_MAPI_REPL *mapi_repl,
							      uint32_t *handles, uint16_t *size)
{
	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncImportHierarchyChange (0x73) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncImportHierarchyChange_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamBegin (0x75) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamBegin(TALLOC_CTX *mem_ctx,
							       struct emsmdbp_context *emsmdbp_ctx,
							       struct EcDoRpc_MAPI_REQ *mapi_req,
							       struct EcDoRpc_MAPI_REPL *mapi_repl,
							       uint32_t *handles, uint16_t *size)
{
 	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamBegin (0x75) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncUploadStateStreamBegin_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncUploadStateStreamEnd (0x77) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamEnd(TALLOC_CTX *mem_ctx,
							     struct emsmdbp_context *emsmdbp_ctx,
							     struct EcDoRpc_MAPI_REQ *mapi_req,
							     struct EcDoRpc_MAPI_REPL *mapi_repl,
							     uint32_t *handles, uint16_t *size)
{
 	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncUploadStateStreamEnd (0x77) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncUploadStateStreamEnd_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncOpenCollector (0x7e) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncOpenCollector(TALLOC_CTX *mem_ctx,
						      struct emsmdbp_context *emsmdbp_ctx,
						      struct EcDoRpc_MAPI_REQ *mapi_req,
						      struct EcDoRpc_MAPI_REPL *mapi_repl,
						      uint32_t *handles, uint16_t *size)
{
	uint32_t		folder_handle;
	struct mapi_handles	*folder_rec;
	struct mapi_handles	*synccontext_rec;
	struct emsmdbp_object	*folder_object;
	struct emsmdbp_object	*synccontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncOpenCollector (0x7e)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncOpenCollector_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopGetLocalReplicaIds (0x7f) Rop. This operation reserves a range of IDs to be used by a local replica.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopGetLocalReplicaIds(TALLOC_CTX *mem_ctx,
                                                       struct emsmdbp_context *emsmdbp_ctx,
                                                       struct EcDoRpc_MAPI_REQ *mapi_req,
                                                       struct EcDoRpc_MAPI_REPL *mapi_repl,
                                                       uint32_t *handles, uint16_t *size)
{
	char			*recipient;
        
	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopGetLocalReplicaIds (0x7f) - stub\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopGetLocalReplicaIds_size(mapi_repl);

	return MAPI_E_SUCCESS;
}

/**
   \details EcDoRpc EcDoRpc_RopSyncGetTransferState (0x82) Rop.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the OpenMessage EcDoRpc_MAPI_REQ
   structure
   \param mapi_repl pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopSyncGetTransferState(TALLOC_CTX *mem_ctx,
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct EcDoRpc_MAPI_REQ *mapi_req,
							 struct EcDoRpc_MAPI_REPL *mapi_repl,
							 uint32_t *handles, uint16_t *size)
{
	uint32_t		synccontext_handle;
	struct mapi_handles	*synccontext_rec;
	struct mapi_handles	*ftcontext_rec;
	struct emsmdbp_object	*synccontext_object;
	struct emsmdbp_object	*ftcontext_object;
	enum MAPISTATUS		retval;
	void			*data = NULL;

	DEBUG(4, ("exchange_emsmdb: [OXCFXICS] RopSyncGetTransferState (0x82)\n"));

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->error_code = MAPI_E_SUCCESS;
	mapi_repl->handle_idx = mapi_req->handle_idx;

	/* TODO: actually implement this */
	mapi_repl->error_code = MAPI_E_NO_SUPPORT;

	*size += libmapiserver_RopSyncGetTransferState_size(mapi_repl);

	return MAPI_E_SUCCESS;
}
