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
   \file libmapiserver_oxcstor.c

   \brief OXCSTOR ROP Response size calculations
 */

#include "libmapiserver.h"
#include <string.h>

/**
   \details Calculate Logon Rop size

   \param request pointer to the Logon EcDoRpc_MAPI_REQ structure
   \param response pointer to the Logon EcDoRpc_MAPI_REPL structure

   \return Size of Logon response
 */
_PUBLIC_ uint16_t libmapiserver_RopLogon_size(struct EcDoRpc_MAPI_REQ *request,
					      struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || (response->error_code && response->error_code != ecWrongServer)) {
		return size;
	}

	if (response->error_code == ecWrongServer) {
		size += SIZE_DFLT_ROPLOGON_REDIRECT;
		size += strlen (response->us.mapi_Logon.ServerName) + 1;
	} else if (request->u.mapi_Logon.LogonFlags & LogonPrivate) {
		size += SIZE_DFLT_ROPLOGON_MAILBOX;
	} else {
		size += SIZE_DFLT_ROPLOGON_PUBLICFOLDER;
	}
	return size;
}


/**
   \details Calculate SetReceiveFolder (0x26) Rop size

   \param response pointer to the SetReceiveFolder EcDoRpc_MAPI_REPL
   structure

   \return Size of SetReceiveFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetReceiveFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate GetReceiveFolder Rop size

   \param response pointer to the GetReceiveFolder EcDoRpc_MAPI_REPL structure

   \return Size of GetReceiveFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetReceiveFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETRECEIVEFOLDER;
	size += strlen(response->u.mapi_GetReceiveFolder.MessageClass) + 1;

	return size;
}

/**
   \details Calculate LongTermIdFromId Rop size

   \param response pointer to the LongTermIdFromId EcDoRpc_MAPI_REPL structure

   \return Size of LongTermIdFromId response
 */
_PUBLIC_ uint16_t libmapiserver_RopLongTermIdFromId_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPLONGTERMIDFROMID;

	return size;
}

/**
   \details Calculate IdFromLongTermId Rop size

   \param response pointer to the IdFromLongTermId EcDoRpc_MAPI_REPL structure

   \return Size of IdFromLongTermId response
 */
_PUBLIC_ uint16_t libmapiserver_RopIdFromLongTermId_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPIDFROMLONGTERMID;

	return size;	
}

/**
   \details Calculate GetPerUserLongTermIds Rop size

   \param response pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REPL structure

   \return Size of GetPerUserLongTermIds response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetPerUserLongTermIds_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;
	uint16_t	i;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPERUSERLONGTERMIDS;

	for (i = 0; i < response->u.mapi_GetPerUserLongTermIds.LongTermIdCount; i++) {
		size += libmapiserver_LongTermId_size();
	}
	

	return size;
}

/**
   \details Calculate ReadPerUserInformation Rop size

   \param response pointer to the ReadPerUserInformation EcDoRpc_MAPI_REPL structure

   \return Size of ReadPerUserInformation response
 */
_PUBLIC_ uint16_t libmapiserver_RopReadPerUserInformation_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPREADPERUSERINFORMATION;

	if (response->u.mapi_ReadPerUserInformation.DataSize) {
		size += response->u.mapi_ReadPerUserInformation.DataSize;
	}

	return size;
}

/**
   \details Calculate GetPerUserLongTermIds Rop size

   \param response pointer to the GetPerUserLongTermIds EcDoRpc_MAPI_REPL structure

   \return Size of GetPerUserLongTermIds response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetPerUserGuid_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPERUSERGUID;

	return size;
}

/**
   \details Calculate GetStoreState Rop size

   \param response pointer to the GetStoreState EcDoRpc_MAPI_REPL structure

   \return Size of GetStoreState response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetStoreState_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETSTORESTATE;

	return size;
}

/**
   \details Calculate GetReceiveFolderTable ROP size

   \param response pointer to the GetReceiveFolderTable EcDoRpc_MAPI_REPL structure

   \return Size of GetPerUserLongTermIds response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetReceiveFolderTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	int 		i = 0;
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += sizeof(uint32_t); /* cValues */
	for (i = 0; i < response->u.mapi_GetReceiveFolderTable.cValues; ++i) {
		size += sizeof(uint8_t); /* flag */
		size += sizeof(int64_t); /* fid */
		size += strlen(response->u.mapi_GetReceiveFolderTable.entries[i].lpszMessageClass) + 1;
		size += sizeof(struct FILETIME); /* modiftime */
	}

	return size;
}
