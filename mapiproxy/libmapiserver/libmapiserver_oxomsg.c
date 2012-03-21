/*
   libmapiserver - MAPI library for Server side

   OpenChange Project

   Copyright (C) Brad Hards 2010

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
   \file libmapiserver_oxomsg.c

   \brief OXOMSG ROP Response size calculations
 */

#include "libmapiserver.h"
#include <string.h>

/**
   \details Calculate SubmitMessage (0x32) Rop size

   \param response pointer to the SubmitMessage EcDoRpc_MAPI_REPL
   structure

   \return Size of SubmitMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopSubmitMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate SetSpooler (0x47) Rop size

   \param response pointer to the SetSpooler EcDoRpc_MAPI_REPL
   structure

   \return Size of SetSpooler response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetSpooler_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate GetAddressTypes (0x49) Rop size

   \param response pointer to the GetAddressTypes EcDoRpc_MAPI_REPL structure

   \return Size of GetAddressTypes response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetAddressTypes_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETADDRESSTYPES;
	/* The length of the strings is variable, but given by the size parameter */
	size += response->u.mapi_AddressTypes.size;

	return size;
}

/**
   \details Calculate TransportSend (0x4a) Rop size

   \param response pointer to the TransportSend EcDoRpc_MAPI_REPL
   structure

   \return Size of TransportSend response
 */
_PUBLIC_ uint16_t libmapiserver_RopTransportSend_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPTRANSPORTSEND;
	if (!response->u.mapi_TransportSend.NoPropertiesReturned) {
		abort();
	}

	/* The length of the strings is variable, but given by the size parameter */
	/* size += response->u.mapi_AddressTypes.size; */

	return size;
}

/**
   \details Calculate GetTransportFolder (0x6d) ROP size

   \param response pointer to the GetTransportFolder EcDoRpc_MAPI_REPL structure

   \return Size of GetTransportFolder response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetTransportFolder_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETTRANSPORTFOLDER;

	return size;
}


/**
   \details Calculate OptionsData (0x6f) Rop size

   \param response pointer to the OptionsData EcDoRpc_MAPI_REPL structure

   \return Size of OptionsData response
 */
_PUBLIC_ uint16_t libmapiserver_RopOptionsData_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPOPTIONSDATA;
	size += response->u.mapi_OptionsData.OptionsInfo.cb;
	size += response->u.mapi_OptionsData.HelpFileSize;
	if (response->u.mapi_OptionsData.HelpFileSize != 0) {
		size += strlen(response->u.mapi_OptionsData.HelpFileName.HelpFileName) + 1;
	}

	return size;
}
