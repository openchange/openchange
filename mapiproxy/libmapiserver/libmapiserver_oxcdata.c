/*
   libmapiserver - MAPI library for Server side

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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
   \file libmapiserver_oxcdata.c

   \brief OXCDATA Data Structures
 */

#include "libmapiserver.h"

/**
   \details Calculate the size of a TypedString structure

   \param typedstring TypedString structure

   \return Size of typedstring structure
 */
_PUBLIC_ uint16_t libmapiserver_TypedString_size(struct TypedString typedstring)
{
	uint16_t	size = 0;

	size += sizeof (uint8_t);

	switch (typedstring.StringType) {
	case StringType_NONE:
	case StringType_EMPTY:
		break;
	case StringType_STRING8:
		if (typedstring.String.lpszA) {
			size += strlen(typedstring.String.lpszA) + 1;
		}
		break;
	case StringType_UNICODE_REDUCED:
		if (typedstring.String.lpszW_reduced) {
			size += strlen(typedstring.String.lpszW_reduced) + 1;
		}
		break;
	case StringType_UNICODE:
		if (typedstring.String.lpszW) {
                        size += strlen_m_ext(typedstring.String.lpszW, CH_UTF8, CH_UTF16BE) * 2 + 2;
		}
		break;
	}

	return size;
}


/**
   \details Calculate the size of a RecipientRow structure

   \param recipientrow RecipientRow structure

   \return Size of RecipientRow structure
 */
_PUBLIC_ uint16_t libmapiserver_RecipientRow_size(struct RecipientRow recipientrow)
{
	uint16_t	size = 0;

	/* RecipientFlags */
	size += sizeof (uint16_t);

	/* /\* recipient_type *\/ */
	/* if (recipientrow.RecipientFlags & 0x1) { */
	/* 	size += sizeof (uint8_t) * 2; /\* AddressPrefixUsed + DisplayType *\/ */
	/* 	size += strlen(recipientrow.X500DN.recipient_x500name) + 1; */
	/* } */

	/* recipient_EmailAddress */
	switch (recipientrow.RecipientFlags & 0x208) {
	case 0x8:
		size += strlen(recipientrow.EmailAddress.lpszA);
		break;
	case 0x208:
		size += strlen(recipientrow.EmailAddress.lpszW) * 2 + 2;
		break;
	default:
		break;
	}

	/* recipient_DisplayName */
	switch (recipientrow.RecipientFlags & 0x210) {
	case 0x10:
		size += strlen(recipientrow.DisplayName.lpszA);
		break;
	case 0x210:
		size += strlen(recipientrow.DisplayName.lpszW) * 2 + 2;
		break;
	default:
		break;
	}

	/* recipient_SimpleDisplayName */
	switch (recipientrow.RecipientFlags & 0x600) {
	case 0x400:
		size += strlen(recipientrow.SimpleDisplayName.lpszA);
		break;
	case 0x600:
		size += strlen(recipientrow.SimpleDisplayName.lpszW) * 2 + 2;
		break;
	default:
		break;
	}

	/* recipient_TransmittableDisplayName */
	switch (recipientrow.RecipientFlags & 0x260) {
	case 0x20:
		size += strlen(recipientrow.TransmittableDisplayName.lpszA);
		break;
	case 0x220:
		size += strlen(recipientrow.TransmittableDisplayName.lpszW) * 2 + 2;
		break;
	default:
		break;
	}

	/* prop_count */
	size += sizeof (uint16_t);

	/* layout */
	size += sizeof (uint8_t);

	/* prop_values */
	size += sizeof (uint16_t);
	size += recipientrow.prop_values.length;

	return size;
}


/**
   \details Calculate the size of a LongTermId structure

   \return Size of LongTermId structure
 */
_PUBLIC_ uint16_t libmapiserver_LongTermId_size(void)
{
	return SIZE_DFLT_LONGTERMID;
}
