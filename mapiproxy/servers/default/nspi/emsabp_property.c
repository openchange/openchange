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
	bool		ref;
	const char	*ref_attr;
};

/* MAPI Property tags to AD attributes mapping */
static const struct emsabp_property emsabp_property[] = {
	{ PidTagAnr,				"anr",			false,	NULL			},
	{ PidTagAccount,			"sAMAccountName",	false,	NULL			},
	{ PidTagGivenName,			"givenName",		false,	NULL			},
	{ PidTagSurname,			"sn",			false,	NULL			},
	{ PidTagOriginalDisplayName,		"displayName",		false,	NULL			},
	{ PidTagTransmittableDisplayName,	"displayName",		false,	NULL			},
	{ PidTagPrimarySmtpAddress,		"mail",			false,	NULL			},
	{ PidTag7BitDisplayName,		"displayName",		false,	NULL			},
	{ PR_EMS_AB_HOME_MTA,			"homeMTA",		true,	"legacyExchangeDN"	},
	{ PR_EMS_AB_ASSOC_NT_ACCOUNT,		"assocNTAccount",	false,	NULL			},
	{ PidTagDepartmentName,			"department",		false,	NULL			},
	{ PidTagCompanyName,			"company",		false,	NULL			},
	{ PidTagDisplayName,			"displayName",		false,	NULL			},
	{ PidTagEmailAddress,			"legacyExchangeDN",	false,	NULL			},
	{ PidTagAddressBookHomeMessageDatabase,	"homeMDB",		true,	"legacyExchangeDN"	},
	{ PidTagAddressBookProxyAddresses,	"proxyAddresses",	false,	NULL			},
	{ PidTagAddressBookNetworkAddress,	"networkAddress",	false,	NULL			},
	{ PidTagTitle,				"personalTitle",	false,	NULL			},
	{ PidTagAddressBookObjectGuid,		"objectGUID",		false,	NULL			},
	{ 0,					NULL,			false,	NULL			}
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
	uint32_t	uniPropTag;

	if ((ulPropTag & 0x0fff) == PT_STRING8) {
		uniPropTag = (ulPropTag & 0xfffff000) | PT_UNICODE;
	}
	else {
		uniPropTag = ulPropTag;
	}

	for (i = 0; emsabp_property[i].attribute; i++) {
		if (uniPropTag == emsabp_property[i].ulPropTag) {
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


/**
   \details Returns whether the given attribute's value references
   another AD record

   \param ulPropTag the property tag to lookup

   \return 1 if the attribute is a reference, 0 if not and -1 if an
   error occurred.
 */
_PUBLIC_ int emsabp_property_is_ref(uint32_t ulPropTag)
{
	int		i;
	uint32_t	uniPropTag;

	if (!ulPropTag) return -1;

	if ((ulPropTag & 0x0fff) == PT_STRING8) {
		uniPropTag = (ulPropTag & 0xfffff000) | PT_UNICODE;
	}
	else {
		uniPropTag = ulPropTag;
	}

	for (i = 0; emsabp_property[i].attribute; i++) {
		if (uniPropTag == emsabp_property[i].ulPropTag) {
			return (emsabp_property[i].ref == true) ? 1 : 0;
		}
	}

	return -1;
}


/**
   \details Returns the reference attr for a given attribute

   \param ulPropTag property tag to lookup

   \return pointer to a valid reference attribute on success,
   otherwise NULL
 */
_PUBLIC_ const char *emsabp_property_get_ref_attr(uint32_t ulPropTag)
{
	int		i;
	uint32_t	uniPropTag;

	if (!ulPropTag) return NULL;

	if ((ulPropTag & 0x0fff) == PT_STRING8) {
		uniPropTag = (ulPropTag & 0xfffff000) | PT_UNICODE;
	}
	else {
		uniPropTag = ulPropTag;
	}

	for (i = 0; emsabp_property[i].attribute; i++) {
		if (uniPropTag == emsabp_property[i].ulPropTag) {
			return emsabp_property[i].ref_attr;
		}
	}

	return NULL;
}
