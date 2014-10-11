/*
   libmapiserver - MAPI library for Server side

   OpenChange Project

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
   \file libmapiserver_oxcfold.c

   \brief OXCFOLD ROP Response size calculations
 */

#include "libmapiserver.h"

/**
   \details Calculate OpenFolder Rop size

   \param response pointer to the OpenFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of OpenFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopOpenFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPOPENFOLDER;
	
	/* No ghosted folder for the moment */
	return size;
}


/**
   \details Calculate GetHierarchyTable Rop size

   \param response pointer to the GetHierarchyTable EcDoRpc_MAPI_REPL
   structure

   \return Size of GetHierarchyTable response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetHierarchyTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETHIERARCHYTABLE;

	return size;
}


/**
   \details Calculate GetContentsTable Rop size

   \param response pointer to the GetContentsTable EcDoRpc_MAPI_REPL
   structure

   \return Size of GetContentsTable response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetContentsTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETCONTENTSTABLE;
	
	return size;
}


/**
   \details Calculate CreateFolder Rop size

   \param response pointer to the CreateFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of CreateFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopCreateFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPCREATEFOLDER;

	if (response->u.mapi_CreateFolder.IsExistingFolder != 0) {
		size += sizeof(response->u.mapi_CreateFolder.GhostUnion.GhostInfo.HasRules);
		size += sizeof(response->u.mapi_CreateFolder.GhostUnion.GhostInfo.IsGhosted);
		if (response->u.mapi_CreateFolder.GhostUnion.GhostInfo.IsGhosted != 0) {
			size += sizeof(response->u.mapi_CreateFolder.GhostUnion.GhostInfo.Ghost.Replicas.ServerCount);
			size += sizeof(response->u.mapi_CreateFolder.GhostUnion.GhostInfo.Ghost.Replicas.CheapServerCount);
			/* TODO: size += sizeof( servers )*/
		}
	}
	return size;
}


/**
   \details Calculate DeleteFolder Rop size

   \param response pointer to the DeleteFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of DeleteFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopDeleteFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPDELETEFOLDER;

	return size;
}

/**
   \details Calculate DeleteMessage (0x1e) Rop size

   \param response pointer to the DeleteMessage EcDoRpc_MAPI_REPL
   structure

   \return Size of DeleteMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopDeleteMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPDELETEMESSAGE;

	return size;
}


/**
   \details Calculate SetSearchCriteria (0x30) Rop size

   \param response pointer to the SetSearchCriteria EcDoRpc_MAPI_REPL
   structure

   \return Size of SetSearchCriteria response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetSearchCriteria_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate GetSearchCriteria (0x31) Rop size

   \param response pointer to the GetSearchCriteria EcDoRpc_MAPI_REPL
   structure

   \return Size of GetSearchCriteria response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetSearchCriteria_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETSEARCHCRITERIA;
	size += response->u.mapi_GetSearchCriteria.RestrictionDataSize;
	size += response->u.mapi_GetSearchCriteria.FolderIdCount * sizeof (int64_t);

	return size;
}


 /**
   \details Calculate EmptyFolder Rop size

   \param response pointer to the EmptyFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of EmptyFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopEmptyFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t        size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPEMPTYFOLDER;
	return size;
}

/**
   \details Calculate MoveCopyMessages rop size

   \param response pointer to the MoveCopyMessags EcDoRpc_MAPI_REPL
   structure

   \return Size of MoveCopyMessages response
 */
_PUBLIC_ uint16_t libmapiserver_RopMoveCopyMessages_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t        size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPMOVECOPYMESSAGES;
	return size;
}

/**
   \details Calculate MoveFolder rop size

   \param response pointer to the MoveFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of MoveFolder response
 */
uint16_t libmapiserver_RopMoveFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t        size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || (response->error_code && response->error_code != ecDstNullObject)) {
		return size;
	}

	size += SIZE_DFLT_ROPMOVEFOLDER;
	if (response->error_code == ecDstNullObject) {
		size += sizeof(uint32_t); /* DestHandleIndex */
	}

	return size;
}

/**
   \details Calculate CopyFolder rop size

   \param response pointer to the CopyFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of CopyFolder response
 */
uint16_t libmapiserver_RopCopyFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t        size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || (response->error_code && response->error_code != ecDstNullObject)) {
		return size;
	}

	size += SIZE_DFLT_ROPCOPYFOLDER;
	if (response->error_code == ecDstNullObject) {
		size += sizeof(uint32_t); /* DestHandleIndex */
	}

	return size;
}
