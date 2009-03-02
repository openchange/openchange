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
   \file libmapiserver_oxcprpt.c

   \brief OXCPRPT Rops
 */

#include "libmapiserver.h"
#include <libmapi/mapidefs.h>
#include <gen_ndr/ndr_exchange.h>

/**
   \details Calculate GetPropertiesSpecific Rop size

   \param request pointer to the GetPropertiesSpecific
   EcDoRpc_MAPI_REQ structure
   \param response pointer to the GetPropertiesSpecific
   EcDoRpc_MAPI_REPL structure

   \return Size of GetPropsSpecific response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetPropertiesSpecific_size(struct EcDoRpc_MAPI_REQ *request,
							      struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPROPERTIESSPECIFIC;
	size += response->u.mapi_GetProps.prop_data.length;

	return size;
}


/**
   \details Calculate SetProperties Rop size

   \param response pointer to the SetProperties EcDoRpc_MAPI_REPL
   structure

   \return Size of SetProperties response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetProperties_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSETPROPERTIES;

	if (response->u.mapi_SetProps.PropertyProblemCount) {
		size += response->u.mapi_SetProps.PropertyProblemCount * sizeof(struct PropertyProblem);
	}

	return size;
}


/**
   \details Calculate GetPropertyIdsFromNames Rop size

   \param response pointer to the GetPropertyIdsFromNames
   EcDoRpc_MAPI_REPL structure

   \return Size of GetPropertyIdsFromNames response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetPropertyIdsFromNames_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPROPERTYIDSFROMNAMES;

	if (response->u.mapi_GetIDsFromNames.count) {
		size += response->u.mapi_GetIDsFromNames.count * sizeof (uint16_t);
	}

	return size;
}


/**
   \details Add a property value to a DATA blob. This convenient
   function should be used when creating a GetPropertiesSpecific reply
   response blob.

   \param mem_ctx pointer to the memory context
   \param iconv_convenience pointer to the iconv_convenience context
   \param property the property tag which value is meant to be
   appended to the blob
   \param value generic pointer on the property value
   \param blob the data blob the function uses to return the blob
   \param layout whether values should be prefixed by a layout
   \param flagged define if the properties are flagged or not

   \note blob.length must be set to 0 before this function is called
   the first time. Also the function only supports a limited set of
   property types at the moment.

   \return 0 on success;
 */
_PUBLIC_ int libmapiserver_push_property(TALLOC_CTX *mem_ctx,
					 struct smb_iconv_convenience *iconv_convenience,
					 uint32_t property, 
					 const void *value, 
					 DATA_BLOB *blob,
					 uint8_t layout, 
					 uint8_t flagged)
{
	struct ndr_push		*ndr;
	
	ndr = ndr_push_init_ctx(mem_ctx, iconv_convenience);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;
	if (blob->length) {
		talloc_free(ndr->data);
		ndr->data = blob->data;
		ndr->offset = blob->length;
	}

	/* Step 1. Is the property flagged */
	if (flagged) {
		switch (property & 0xFFFF) {
		case PT_ERROR:
			switch (layout) {
			case 0x1:
				ndr_push_uint8(ndr, NDR_SCALARS, layout);
				goto end;
			case PT_ERROR:
				ndr_push_uint8(ndr, NDR_SCALARS, PT_ERROR);
				break;
			}
			break;
		default:
			ndr_push_uint8(ndr, NDR_SCALARS, 0x0);
			break;
		}
	} else {
		/* Step 2. Set the layout */
		if (layout) {
			switch (property & 0xFFFF) {
			case PT_ERROR:
				ndr_push_uint8(ndr, NDR_SCALARS, PT_ERROR);
				break;
			default:
				ndr_push_uint8(ndr, NDR_SCALARS, 0x0);
			}
		}
	}

	/* Step 2. Push property data if supported */
	switch (property & 0xFFFF) {
	case PT_I2:
		ndr_push_uint16(ndr, NDR_SCALARS, *(uint16_t *) value);
		break;
	case PT_LONG:
	case PT_ERROR:
	case PT_OBJECT:
		ndr_push_uint32(ndr, NDR_SCALARS, *(uint32_t *) value);
		break;
	case PT_DOUBLE:
	case PT_I8:
		ndr_push_dlong(ndr, NDR_SCALARS, *(uint64_t *) value);
		break;
	case PT_BOOLEAN:
		ndr_push_uint8(ndr, NDR_SCALARS, *(uint8_t *) value);
		break;
	case PT_STRING8:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM|LIBNDR_FLAG_STR_ASCII);
		ndr_push_string(ndr, NDR_SCALARS, (char *) value);
		break;
	case PT_UNICODE:
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
		ndr_push_string(ndr, NDR_SCALARS, (char *) value);
		break;
	case PT_BINARY:
		ndr_push_SBinary_short(ndr, NDR_SCALARS, (struct SBinary_short *) value);
		break;
	case PT_CLSID:
		ndr_push_GUID(ndr, NDR_SCALARS, (struct GUID *) value);
		break;
	case PT_SYSTIME:
		ndr_push_FILETIME(ndr, NDR_SCALARS, (struct FILETIME *) value);
		break;
	default:
		break;
	}
end:
	/* Step 3. Steal ndr context */
	blob->data = ndr->data;
	talloc_steal(mem_ctx, blob->data);
	blob->length = ndr->offset;

	talloc_free(ndr);
	return 0;
}
