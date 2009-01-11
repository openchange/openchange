/*
   OpenChange Server implementation.

   EMSABP: Address Book Provider implementation

   Copyright (C) Julien Kerihuel 2009.

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
   \file emsabp_property.c

   \brief Property Tag to AD attributes mapping
 */

#include "dcesrv_exchange_nsp.h"

struct emsabp_property {
	uint32_t	ulPropTag;
	const char	*attribute;
};

static const struct emsabp_property emsabp_property[] = {
	{ PR_ACCOUNT,				"sAMAccountName"	},
	{ PR_COMPANY_NAME,			"company"		},
	{ PR_DISPLAY_NAME,			"displayName"		},
	{ PR_EMAIL_ADDRESS,			"legacyExchangeDN"	},
	{ PR_TITLE,				"personalTitle"		},
	{ 0,					NULL			}
};


/**
   \details Return the AD attribute name associated to a property tag

   \param ulPropTag the property tag to lookup

   \return valid pointer to the attribute name on success, otherwise
   NULL
 */
_PUBLIC_ const char *emsabp_property_get_attribute(uint32_t ulPropTag)
{
	int		i;

	/* if ulPropTag type is PT_UNICODE, turn it to PT_STRING8 */
	if ((ulPropTag & 0xFFFF) == PT_UNICODE) {
		ulPropTag &= 0xFFFF0000;
		ulPropTag += PT_STRING8;
	}

	for (i = 0; emsabp_property[i].attribute; i++) {
		if (ulPropTag == emsabp_property[i].ulPropTag) {
			return emsabp_property[i].attribute;
		}
	}

	return NULL;
}


/**
   \details Return the property tag associated to AD attribute name

   \param attribute the AD attribute name to lookup

   \return property tag value on success, otherwise PT_ERROR
 */
_PUBLIC_ uint32_t emsabp_property_get_ulPropTag(const char *attribute)
{
	int		i;

	if (!attribute) return PT_ERROR;

	for (i = 0; emsabp_property[i].attribute; i++) {
		if (!strcmp(attribute, emsabp_property[i].attribute)) {
			return emsabp_property[i].ulPropTag;
		}
	}

	return PT_ERROR;
}
