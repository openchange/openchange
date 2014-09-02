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
   \file entryid.c

   \brief EntryID convenient routines
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

/**
   \details Build an Address Book EntryID from a legacyExchangeDN

   \param mem_ctx pointer to the memory context
   \param legacyExchangeDN the string to copy into the binary blob
   \param bin the binary blob where the function stores results
 */
_PUBLIC_ enum MAPISTATUS entryid_set_AB_EntryID(TALLOC_CTX *mem_ctx, 
						const char *legacyExchangeDN,
						struct SBinary_short *bin)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!legacyExchangeDN, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!bin, MAPI_E_INVALID_PARAMETER, NULL);

	bin->cb = 28 + strlen(legacyExchangeDN) + 1;
	bin->lpb = talloc_array(mem_ctx, uint8_t, bin->cb);
	
	/* Fill EntryID: FIXME we should use PermanentEntryID here */
	memset(bin->lpb, 0, bin->cb);
	memcpy(&bin->lpb[4], GUID_NSPI, 16);
	bin->lpb[20] = 0x1;
	memcpy(bin->lpb + 28, legacyExchangeDN, strlen(legacyExchangeDN));

	return MAPI_E_SUCCESS;
}


/**
   \details Build a folder EntryID

   \param mem_ctx pointer to the memory context
   \param MailboxGuid pointer to the Mailbox Guid
   \param ReplGuid pointer to the Replica Guid
   \param FolderType the type of folder
   \param fid the folder identifier
   \param rbin the Binary_r structure where the function stores results

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS entryid_set_folder_EntryID(TALLOC_CTX *mem_ctx,
						    struct GUID *MailboxGuid,
						    struct GUID *ReplGuid,
						    uint16_t FolderType,
						    int64_t fid,
						    struct Binary_r **rbin)
{
	struct Binary_r	*bin;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!MailboxGuid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ReplGuid, MAPI_E_INVALID_PARAMETER, NULL);

	bin = talloc_zero(mem_ctx, struct Binary_r);

	bin->cb = 46;
	bin->lpb = talloc_array(mem_ctx, uint8_t, bin->cb);

	/* 4 bytes:  EntryID flags set to 0 */
	memset(bin->lpb, 0, bin->cb);

	/* 16 bytes: MailboxGuid */
	bin->lpb[4] = (MailboxGuid->time_low & 0xFF);
	bin->lpb[5] = ((MailboxGuid->time_low >> 8)  & 0xFF);
	bin->lpb[6] = ((MailboxGuid->time_low >> 16) & 0xFF);
	bin->lpb[7] = ((MailboxGuid->time_low >> 24) & 0xFF);
	bin->lpb[8] = (MailboxGuid->time_mid & 0xFF);
	bin->lpb[9] = ((MailboxGuid->time_mid >> 8)  & 0xFF);
	bin->lpb[10] = (MailboxGuid->time_hi_and_version & 0xFF);
	bin->lpb[11] = ((MailboxGuid->time_hi_and_version >> 8) & 0xFF);
	memcpy(&bin->lpb[12],  MailboxGuid->clock_seq, sizeof (uint8_t) * 2);
	memcpy(&bin->lpb[14], MailboxGuid->node, sizeof (uint8_t) * 6);

	/* 2 bytes: FolderType */
	bin->lpb[20] = (FolderType & 0xFF);
	bin->lpb[21] = ((FolderType >> 8) & 0xFF);

	/* 16 bytes: ReplGuid */
	bin->lpb[22] = (ReplGuid->time_low & 0xFF);
	bin->lpb[23] = ((ReplGuid->time_low >> 8)  & 0xFF);
	bin->lpb[24] = ((ReplGuid->time_low >> 16) & 0xFF);
	bin->lpb[25] = ((ReplGuid->time_low >> 24) & 0xFF);
	bin->lpb[26] = (ReplGuid->time_mid & 0xFF);
	bin->lpb[27] = ((ReplGuid->time_mid >> 8)  & 0xFF);
	bin->lpb[28] = (ReplGuid->time_hi_and_version & 0xFF);
	bin->lpb[29] = ((ReplGuid->time_hi_and_version >> 8) & 0xFF);
	memcpy(&bin->lpb[30],  ReplGuid->clock_seq, sizeof (uint8_t) * 2);
	memcpy(&bin->lpb[32], ReplGuid->node, sizeof (uint8_t) * 6);

	/* 6 bytes: FolderID, first two byte unset */
	bin->lpb[38] = ((fid >> 16) & 0xFF);
	bin->lpb[39] = ((fid >> 24) & 0xFF);
	bin->lpb[40] = ((fid >> 32) & 0xFF);
	bin->lpb[41] = ((fid >> 40) & 0xFF);
	bin->lpb[42] = ((fid >> 48) & 0xFF);
	bin->lpb[43] = ((fid >> 56) & 0xFF);
	/* 44 and 45 are zero padding */

	*rbin = bin;

	return MAPI_E_SUCCESS;
}
