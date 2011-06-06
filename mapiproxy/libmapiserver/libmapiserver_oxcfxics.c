/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Inverse inc. 2010

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
   \file libmapiserver_oxcfxics.c

   \brief OXCFXICS ROP Response size calculations
 */

#include "libmapiserver.h"

/**
   \details Calculate FastTransferSourceCopyTo (0x4d) Rop size

   \param response pointer to the FastTransferSourceCopyTo EcDoRpc_MAPI_REPL
   structure

   \return Size of FastTransferSourceCopyTo response
 */
_PUBLIC_ uint16_t libmapiserver_RopFastTransferSourceCopyTo_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate FastTransferSourceGetBuffer (0x4d) Rop size

   \param response pointer to the FastTransferSourceGetBuffer EcDoRpc_MAPI_REPL
   structure

   \return Size of FastTransferSourceGetBuffer response
 */
_PUBLIC_ uint16_t libmapiserver_RopFastTransferSourceGetBuffer_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		if (response->error_code == ecServerBusy) {
			size += sizeof (uint32_t); /* size of BackoffTime */
		}
		return size;
	}

	size += SIZE_DFLT_ROPFASTTRANSFERSOURCEGETBUFFER + response->u.mapi_FastTransferSourceGetBuffer.TransferBuffer.length;

	return size;
}

/**
   \details Calculate SyncConfigure (0x70) Rop size

   \param response pointer to the SyncConfigure EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncConfigure response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncConfigure_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SyncImportMessageChange (0x73) Rop size

   \param response pointer to the SyncImportMessageChange EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncImportMessageChange response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncImportMessageChange_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSYNCIMPORTMESSAGECHANGE;

	return size;
}

/**
   \details Calculate SyncImportHierarchyChange (0x73) Rop size

   \param response pointer to the SyncImportHierarchyChange EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncImportHierarchyChange response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncImportHierarchyChange_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSYNCIMPORTHIERARCHYCHANGE;

	return size;
}

/**
   \details Calculate SyncImportDeletes (0x74) Rop size

   \param response pointer to the SyncImportDeletes EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncImportDeletes response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncImportDeletes_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SyncUploadStateStreamBegin (0x75) Rop size

   \param response pointer to the SyncUploadStateStreamBegin EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncUploadStateStreamBegin response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncUploadStateStreamBegin_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SyncUploadStateStreamContinue (0x76) Rop size

   \param response pointer to the SyncUploadStateStreamContinue EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncUploadStateStreamContinue response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncUploadStateStreamContinue_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SyncUploadStateStreamEnd (0x77) Rop size

   \param response pointer to the SyncUploadStateStreamEnd EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncUploadStateStreamEnd response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncUploadStateStreamEnd_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SyncImportMessageMove (0x78) Rop size

   \param response pointer to the SyncImportMessageMove EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncImportMessageMove response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncImportMessageMove_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSYNCIMPORTMESSAGEMOVE;

	return size;
}

/**
   \details Calculate SyncOpenCollector (0x7e) Rop size

   \param response pointer to the SyncOpenCollector EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncOpenCollector response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncOpenCollector_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate GetLocalReplicaIds (0x7f) Rop size

   \param response pointer to the GetLocalReplicaIds EcDoRpc_MAPI_REPL
   structure

   \return Size of GetLocalReplicaIds response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetLocalReplicaIds_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETLOCALREPLICAIDS;

	return size;
}

/**
   \details Calculate SyncImportReadStateChanges (0x80) Rop size

   \param response pointer to the SyncImportReadStateChanges EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncImportReadStateChanges response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncImportReadStateChanges_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SyncGetTransferState (0x82) Rop size

   \param response pointer to the SyncGetTransferState EcDoRpc_MAPI_REPL
   structure

   \return Size of SyncGetTransferState response
 */
_PUBLIC_ uint16_t libmapiserver_RopSyncGetTransferState_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate SetLocalReplicaMidsetDeleted (0x93) Rop size

   \param response pointer to the SetLocalReplicaMidsetDeleted EcDoRpc_MAPI_REPL
   structure

   \return Size of SetLocalReplicaMidsetDeleted response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetLocalReplicaMidsetDeleted_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}
