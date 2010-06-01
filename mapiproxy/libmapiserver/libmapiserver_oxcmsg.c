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
   \file libmapiserver_oxcmsg.c

   \brief OXCMSG ROP Response size calculations
 */

#include "libmapiserver.h"

/**
   \details Calculate CreateMessage (0x6) Rop size

   \param response pointer to the CreateMessage EcDoRpc_MAPI_REPL
   structure

   \return Size of CreateMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopCreateMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPCREATEMESSAGE;

	if (response->u.mapi_CreateMessage.HasMessageId == 1) {
		size += sizeof (uint64_t);
	}

	return size;
}


/**
   \details Calculate SaveChangesMessage (0xc) Rop size

   \param response pointer to the SaveChangesMessage EcDoRpc_MAPI_REPL
   structure

   \return Size of SaveChangesMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopSaveChangesMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSAVECHANGESMESSAGE;

	return size;
}


/**
   \details Calculate ModifyRecipients (0xe) Rop size

   \param response pointer to the ModifyRecipients EcDoRpc_MAPI_REPL
   structure

   \return Size of ModifyRecipients response
 */
_PUBLIC_ uint16_t libmapiserver_RopModifyRecipients_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate GetAttachmentTable (0x21) Rop size

   \param response pointer to the GetAttachmentTable EcDoRpc_MAPI_REPL

   \return Size of GetAttachmentTable response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetAttachmentTable_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}
