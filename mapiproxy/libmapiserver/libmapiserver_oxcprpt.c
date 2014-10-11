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

   \brief OXCPRPT ROP Response size calculations
 */

#include "libmapiserver.h"
#include "libmapi/libmapi.h"
#include "libmapi/mapidefs.h"
#include "gen_ndr/ndr_exchange.h"
#include <util/debug.h>

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
   \details Calculate GetPropertiesAll Rop size

   \param response pointer to the GetPropertiesAll EcDoRpc_MAPI_REPL structure

   \return Size of GetPropsAll response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetPropertiesAll_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;
	
	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPROPERTIESALL;
	size += libmapiserver_mapi_SPropValue_size(response->u.mapi_GetPropsAll.properties.cValues,
						   response->u.mapi_GetPropsAll.properties.lpProps);
	return size;
}


/**
   \details Calculate GetPropertiesList Rop size

   \param response pointer to the GetPropertiesList EcDoRpc_MAPI_REPL structure
   
   \return Size of GetPropertiesList response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetPropertiesList_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPROPERTIESLIST;
	size += sizeof (uint32_t) * response->u.mapi_GetPropList.count;

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
   \details Calculate SetProperties Rop size

   \param response pointer to the SetProperties EcDoRpc_MAPI_REPL
   structure

   \return Size of SetProperties response
 */
_PUBLIC_ uint16_t libmapiserver_RopDeleteProperties_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPDELETEPROPERTIES;

	if (response->u.mapi_DeleteProps.PropertyProblemCount) {
		size += response->u.mapi_DeleteProps.PropertyProblemCount * sizeof(struct PropertyProblem);
	}

	return size;
}


/**
   \details Calculate CopyTo Rop size

   \param response pointer to the CopyTo EcDoRpc_MAPI_REPL
   structure

   \return Size of SetProperties response
 */
_PUBLIC_ uint16_t libmapiserver_RopCopyTo_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPCOPYTO;
        size += response->u.mapi_CopyTo.PropertyProblemCount * sizeof(struct PropertyProblem);

	return size;
}

/**
   \details Calculate OpenStream Rop size

   \param response pointer to the OpenStream EcDoRpc_MAPI_REPL
   structure

   \return Size of OpenStream response   
 */
_PUBLIC_ uint16_t libmapiserver_RopOpenStream_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPOPENSTREAM;

	return size;
}


/**
   \details Calculate ReadStream Rop size

   \param response pointer to the ReadStream EcDoRpc_MAPI_REPL
   structure

   \return Size of ReadStream response   
 */
_PUBLIC_ uint16_t libmapiserver_RopReadStream_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPREADSTREAM;

	if (response->u.mapi_ReadStream.data.length) {
		size += response->u.mapi_ReadStream.data.length;
	}

	return size;
}


/**
   \details Calculate WriteStream Rop size

   \param response pointer to the WriteStream EcDoRpc_MAPI_REPL
   structure

   \return Size of WriteStream response   
 */
_PUBLIC_ uint16_t libmapiserver_RopWriteStream_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPWRITESTREAM;

	return size;
}

/**
   \details Calculate CommitStream Rop size

   \param response pointer to the CommitStream
   EcDoRpc_MAPI_REPL structure

   \return Size of CommitStream response
 */
_PUBLIC_ uint16_t libmapiserver_RopCommitStream_size(struct EcDoRpc_MAPI_REPL *response)
{
	return SIZE_DFLT_MAPI_RESPONSE;
}


/**
   \details Calculate GetStreamSize Rop size

   \param response pointer to the GetStreamSize
   EcDoRpc_MAPI_REPL structure

   \return Size of GetStreamSize response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetStreamSize_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETSTREAMSIZE;

	return size;
}

/**
   \details Calculate SeekStream Rop size

   \param response pointer to the SeekStream EcDoRpc_MAPI_REPL structure

   \return Size of SeekStream response
 */
_PUBLIC_ uint16_t libmapiserver_RopSeekStream_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPSEEKSTREAM;

	return size;
}

/**
   \details Calculate SetStreamSize Rop size

   \param response pointer to the SetStreamSize EcDoRpc_MAPI_REPL structure

   \return Size of SeekStream response
 */
_PUBLIC_ uint16_t libmapiserver_RopSetStreamSize_size(struct EcDoRpc_MAPI_REPL *response)
{
        return SIZE_DFLT_MAPI_RESPONSE;
}

/**
   \details Calculate GetNamesFromIDs Rop size

   \param response pointer to the GetNamesFromIDs
   EcDoRpc_MAPI_REPL structure

   \return Size of GetNamesFromIDs response
 */
_PUBLIC_ uint16_t libmapiserver_RopGetNamesFromIDs_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;
	uint16_t	i;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPGETNAMESFROMIDS;
	for (i = 0; i < response->u.mapi_GetNamesFromIDs.count; i++) {
		size += libmapiserver_PropertyName_size(response->u.mapi_GetNamesFromIDs.nameid + i);
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

	if (!response || (response->error_code && response->error_code != MAPI_W_ERRORS_RETURNED)) {
		return size;
	}

	size += SIZE_DFLT_ROPGETPROPERTYIDSFROMNAMES;

	if (response->u.mapi_GetIDsFromNames.count) {
		size += response->u.mapi_GetIDsFromNames.count * sizeof (uint16_t);
	}

	return size;
}

/**
   \details Calculate DeletePropertiesNoReplicate Rop size

   \param response pointer to the DeletePropertiesNoReplicate
   EcDoRpc_MAPI_REPL structure

   \return Size of DeletePropertiesNoReplicate response
 */
_PUBLIC_ uint16_t libmapiserver_RopDeletePropertiesNoReplicate_size(struct EcDoRpc_MAPI_REPL *response)
{
	uint16_t	size = SIZE_DFLT_MAPI_RESPONSE;

	if (!response || response->error_code) {
		return size;
	}

	size += SIZE_DFLT_ROPDELETEPROPERTIESNOREPLICATE;
	size += (response->u.mapi_DeletePropertiesNoReplicate.PropertyProblemCount
		 * (sizeof(uint16_t) /* PropertyProblem.Index */
		    + sizeof(uint32_t) /* PropertyProblem.PropertyTag */
		    + sizeof(uint32_t) /* PropertyProblem.ErrorCode */));

	return size;
}


/**
   \details Add a property value to a DATA blob. This convenient
   function should be used when creating a GetPropertiesSpecific reply
   response blob.

   \param mem_ctx pointer to the memory context
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
					 uint32_t property, 
					 const void *value, 
					 DATA_BLOB *blob,
					 uint8_t layout, 
					 uint8_t flagged,
					 uint8_t untyped)
{
	struct ndr_push		*ndr;
        struct SBinary_short    bin;
        struct BinaryArray_r    *bin_array;
	uint32_t		i;
	
	ndr = ndr_push_init_ctx(mem_ctx);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;
	if (blob->length) {
		talloc_free(ndr->data);
		ndr->data = blob->data;
		ndr->offset = blob->length;
	}

	/* Step 1. Is the property typed */
	if (untyped) {
		ndr_push_uint16(ndr, NDR_SCALARS, property & 0xFFFF);
	}



	/* Step 2. Is the property flagged */
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
		/* Step 3. Set the layout */
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

	/* Step 3. Push property data if supported */
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
		ndr_push_double(ndr, NDR_SCALARS, *(double *) value);
		break;
	case PT_I8:
		ndr_push_dlong(ndr, NDR_SCALARS, *(int64_t *) value);
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
	case PT_SVREID:
                /* PropertyRow expect a 16 bit header for BLOB in RopQueryRows and RopGetPropertiesSpecific */
		bin.cb = ((struct Binary_r *) value)->cb;
		bin.lpb = ((struct Binary_r *) value)->lpb;
		ndr_push_SBinary_short(ndr, NDR_SCALARS, &bin);
		break;
	case PT_CLSID:
		ndr_push_GUID(ndr, NDR_SCALARS, (struct GUID *) value);
		break;
	case PT_SYSTIME:
		ndr_push_FILETIME(ndr, NDR_SCALARS, (struct FILETIME *) value);
		break;

	case PT_MV_LONG:
		ndr_push_mapi_MV_LONG_STRUCT(ndr, NDR_SCALARS, (struct mapi_MV_LONG_STRUCT *) value);
		break;

	case PT_MV_UNICODE:
                ndr_push_mapi_SLPSTRArrayW(ndr, NDR_SCALARS, (struct mapi_SLPSTRArrayW *) value);
		break;

	case PT_MV_BINARY:
		bin_array = (struct BinaryArray_r *) value;
		ndr_push_uint32(ndr, NDR_SCALARS, bin_array->cValues);
		for (i = 0; i < bin_array->cValues; i++) {
			bin.cb = bin_array->lpbin[i].cb;
			bin.lpb = bin_array->lpbin[i].lpb;
			ndr_push_SBinary_short(ndr, NDR_SCALARS, &bin);
		}
		break;
	default:
		if (property != 0) {
			DEBUG(5, ("unsupported type: %.4x\n", (property & 0xffff)));
			abort();
		}
		break;
	}
end:
	/* Step 4. Steal ndr context */
	blob->data = ndr->data;
	talloc_steal(mem_ctx, blob->data);
	blob->length = ndr->offset;

	talloc_free(ndr);
	return 0;
}


/**
   \details Turn request parameters to SPropValue array. This
   convenient function should be used among MAPI ROPs that have
   parameters which can be turned to MAPI properties and are stored
   within backends.

   \param mem_ctx pointer to the memory context
   \param request generic pointer to the ROP request
   \param opnum MAPI opnum identifying ROP contents

   \note Developers must talloc_free returned SRow after they finish
   using it.

   \return Allocated SRow on success, otherwise NULL
 */
_PUBLIC_ struct SRow *libmapiserver_ROP_request_to_properties(TALLOC_CTX *mem_ctx, 
							      void *request, 
							      uint8_t opnum)
{
	struct SRow			*aRow;
	struct CreateFolder_req		*CreateFolder_req;

	aRow = talloc_zero(mem_ctx, struct SRow);
	aRow->lpProps = talloc_array(aRow, struct SPropValue, 2);
	aRow->cValues = 0;

	switch (opnum) {
	case op_MAPI_CreateFolder:
		CreateFolder_req = (struct CreateFolder_req *) request;
		aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
					       PR_FOLDER_TYPE, (void *)&(CreateFolder_req->ulFolderType));
		switch (CreateFolder_req->ulType) {
		case MAPI_FOLDER_ANSI:
			aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
						       PR_DISPLAY_NAME, (void *)(CreateFolder_req->FolderName.lpszA));
			aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
						       PR_COMMENT, (void *)(CreateFolder_req->FolderComment.lpszA));
			break;
		case MAPI_FOLDER_UNICODE:
			aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
						       PR_DISPLAY_NAME_UNICODE, (void *)(CreateFolder_req->FolderName.lpszW));
			aRow->lpProps = add_SPropValue(mem_ctx, aRow->lpProps, &(aRow->cValues),
						       PR_COMMENT_UNICODE, (void *)(CreateFolder_req->FolderComment.lpszW));
			break;
		}
		
		break;
	default:
		DEBUG(0, ("[%s:%d]: opnum %d not implemented yet\n", __FUNCTION__, __LINE__, opnum));
		talloc_free(aRow);
		return NULL;
	}
	
	return aRow;
}
