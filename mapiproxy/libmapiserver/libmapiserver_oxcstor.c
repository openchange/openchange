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

   \brief OXCSTOR Rops
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
		return size;
	}

	switch (request->u.mapi_Logon.LogonFlags) {
	case LogonPrivate:
		size += SIZE_DFLT_ROPLOGON_MAILBOX;
		break;
	default:
		break;
	}

	return size;
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
