/*
   libmapiserver - MAPI library for Server side

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2010

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
   \details Calculate OpenMessage (0x3) Rop size

   \param response pointer to the OpenMessage EcDoRpc_MAPI_REPL
   structure

   \return Size of OpenMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopOpenMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;
	uint8_t		i;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPOPENMESSAGE;

	/* SubjectPrefix */
	size += libmapiserver_TypedString_size(response->u.mapi_OpenMessage.SubjectPrefix);

	/* NormalizedSubject */
	size += libmapiserver_TypedString_size(response->u.mapi_OpenMessage.NormalizedSubject);

	/* RecipientColumns */
	size += sizeof (uint16_t);
	size += response->u.mapi_OpenMessage.RecipientColumns.cValues * sizeof (uint32_t);

	for (i = 0; i < response->u.mapi_OpenMessage.RowCount; i++) {
		size += sizeof (uint8_t);	/* ulRecipClass */
		size += sizeof (uint16_t);	/* CODEPAGEID */
		size += sizeof (uint16_t);	/* uint16 */
		size += libmapiserver_RecipientRow_size(response->u.mapi_OpenMessage.RecipientRows[i].RecipientRow);
	}

	return size;
}

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
   \details Calculate RemoveAllRecipients (0xd) Rop size

   \param response pointer to the RemoveAllRecipients EcDoRpc_MAPI_REPL
   structure

   \return Size of RemoveAllRecipients response
 */
_PUBLIC_ uint16_t libmapiserver_RopRemoveAllRecipients_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
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
   \details Calculate ReloadCachedInformation (0x10) Rop size

   \param response pointer to the ReloadCachedInformation
   EcDoRpc_MAPI_REPL structure

   \return Size of ReloadCachedInformation response
 */
_PUBLIC_ uint16_t libmapiserver_RopReloadCachedInformation_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;
	uint8_t		i;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPRELOADCACHEDINFORMATION;

	/* SubjectPrefix */
	size += libmapiserver_TypedString_size(response->u.mapi_ReloadCachedInformation.SubjectPrefix);

	/* NormalizedSubject */
	size += libmapiserver_TypedString_size(response->u.mapi_ReloadCachedInformation.NormalizedSubject);

	/* RecipientColumns */
	size += sizeof (uint16_t);
	size += response->u.mapi_ReloadCachedInformation.RecipientColumns.cValues * sizeof (uint32_t);

	for (i = 0; i < response->u.mapi_ReloadCachedInformation.RowCount; i++) {
		size += sizeof (uint8_t);
		size += sizeof (uint16_t);
		size += sizeof (uint16_t);
		size += libmapiserver_RecipientRow_size(response->u.mapi_ReloadCachedInformation.RecipientRows[i].RecipientRow);
	}

	return size;
}


/**
   \details Calculate SetMessageReadFlag (0x11) Rop size

   \param response pointer to the SetMessageReadFlag EcDoRpc_MAPI_REPL
   structure

   \return Size of SetMessageReadFlag response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetMessageReadFlag_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSETMESSAGEREADFLAG;
	
	if (response->u.mapi_SetMessageReadFlag.ReadStatusChanged == 0x1) {
		size += sizeof (uint8_t);
		size += sizeof (uint8_t) * 24;
	}

	return size;
}


/**
   \details Calculate GetMessageStatus (0x1f) Rop size

   \param response pointer to the GetMessageStatus EcDoRpc_MAPI_REPL

   \return Size of the GetMessageStatus response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetMessageStatus_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETMESSAGESTATUS;

	return size;
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


/**
   \details Calculate OpenAttach (0x22) Rop size

   \param response pointer to the OpenAttach EcDoRpc_MAPI_REPL

   \return Size of OpenAttach response
 */
_PUBLIC_ uint16_t libmapiserver_RopOpenAttach_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate CreateAttach (0x23) Rop size

   \param response pointer to the CreateAttach EcDoRpc_MAPI_REPL

   \return Size of CreateAttach response
 */
_PUBLIC_ uint16_t libmapiserver_RopCreateAttach_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPCREATEATTACH;

        return size;
}

/**
   \details Calculate SaveChangesAttachment (0x25) Rop size

   \param response pointer to the SaveChangesAttachment EcDoRpc_MAPI_REPL

   \return Size of SaveChangesAttachment response
 */
_PUBLIC_ uint16_t libmapiserver_RopSaveChangesAttachment_size(struct EcDoRpc_MAPI_REPL *response)
{
        return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate OpenEmbeddedMessage (0x46) Rop size

   \param response pointer to the OpenEmbeddedMessage EcDoRpc_MAPI_REPL

   \return Size of OpenEmbeddedMessage response
 */
_PUBLIC_ uint16_t libmapiserver_RopOpenEmbeddedMessage_size(struct EcDoRpc_MAPI_REPL *response)
{
        uint16_t        size = SIZE_DFLT_MAPI_RESPONSE;
        uint8_t         i;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPOPENEMBEDDEDMESSAGE;

	/* SubjectPrefix */
	size += libmapiserver_TypedString_size(response->u.mapi_OpenMessage.SubjectPrefix);

	/* NormalizedSubject */
	size += libmapiserver_TypedString_size(response->u.mapi_OpenMessage.NormalizedSubject);

	/* RecipientColumns */
	size += response->u.mapi_OpenEmbeddedMessage.RecipientColumns.cValues * sizeof (uint32_t);

	for (i = 0; i < response->u.mapi_OpenEmbeddedMessage.RowCount; i++) {
		size += sizeof (uint8_t);
		size += sizeof (uint16_t);
		size += sizeof (uint16_t);
		size += libmapiserver_RecipientRow_size(response->u.mapi_OpenEmbeddedMessage.RecipientRows[i].RecipientRow);
	}
        
        return size;
}
