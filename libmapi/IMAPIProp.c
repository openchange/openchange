/*
 *  OpenChange MAPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2007.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "openchange.h"
#include "exchange.h"
#include "ndr_exchange.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/mapicode.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"

/**
 * retrieves the property value of one or more properties of an
 * object.
 */

MAPISTATUS     GetProps(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_message, struct SPropTagArray* properties)
{
       struct mapi_request	*mapi_request;
       struct mapi_response	*mapi_response;
       struct EcDoRpc_MAPI_REQ	*mapi_req;
       struct GetProps_req	request;
       NTSTATUS			status;
       uint32_t			size = 0;
       TALLOC_CTX		*mem_ctx;
       struct SRow		*aRow;
 
       mem_ctx = talloc_init("GetProps");

       /* Fill the GetProps operation */
       properties->cValues -= 1;
       request.unknown = 0x0;
       request.unknown2 = 0x0;
       request.prop_count = (uint16_t)properties->cValues;
       request.properties = properties->aulPropTag;
       size = sizeof (uint8_t) + sizeof(uint32_t) + sizeof(uint16_t) + properties->cValues * sizeof(uint32_t);

       /* Fill the MAPI_REQ request */
       mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
       mapi_req->opnum = op_MAPI_GetProps;
       mapi_req->mapi_flags = ulFlags;
       mapi_req->u.mapi_GetProps = request;
       size += 4;

       /* Fill the mapi_request structure */
       mapi_request = talloc_zero(mem_ctx, struct mapi_request);
       mapi_request->mapi_len = size + sizeof (uint32_t);
       mapi_request->length = size;
       mapi_request->mapi_req = mapi_req;
       mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
       mapi_request->handles[0] = hdl_message;

       status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

       if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
               return mapi_response->mapi_repl->error_code;
       }

       emsmdb->prop_count = properties->cValues;
       emsmdb->properties = properties->aulPropTag;
       aRow = emsmdb_get_SRow(emsmdb, 1, mapi_response->mapi_repl->u.mapi_GetProps.prop_data, 0);

       talloc_free(mem_ctx);

       return MAPI_E_SUCCESS;
}


/**
 * updates one or more properties.
 */

MAPISTATUS	SetProps(struct emsmdb_context *emsmdb, uint32_t ulFlags, struct SPropValue *sprops, unsigned long cn_props, uint32_t hdl_object)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetProps_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	unsigned long		i_prop;
	struct mapi_SPropValue	*mapi_props;
 
	mem_ctx = talloc_init("SetProps");

	/* Fill the SetProps operation */
	request.unknown = 0x00;
	size += sizeof(uint8_t);

	/* build the array */
	request.values.sprop_array = talloc_array(mem_ctx, struct mapi_SPropValue, cn_props);
	mapi_props = request.values.sprop_array;
	for (i_prop = 0; i_prop < cn_props; ++i_prop) {
		size += cast_mapi_SPropValue(&mapi_props[i_prop], &sprops[i_prop]);
		size += sizeof(uint32_t);
	  }

	request.values.sprop_count = cn_props;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetProps;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SetProps = request;
	size += 6;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t);
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 1);
	mapi_request->handles[0] = hdl_object;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/* FIXME: My name is SetProps2, I'm a bad function and I should soon disappear. */

MAPISTATUS	SetProps2(struct emsmdb_context *emsmdb, uint32_t ulFlags, struct SPropValue *sprops, unsigned long cn_props, uint32_t hdl_related, uint32_t hdl_object)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SetProps_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	unsigned long		i_prop;
	struct mapi_SPropValue	*mapi_props;
 
	mem_ctx = talloc_init("SetProps2");

	/* Fill the SetProps operation */
	request.unknown = 0x00;
	size += sizeof(uint8_t);

	/* build the array */
	request.values.sprop_array = talloc_array(mem_ctx, struct mapi_SPropValue, cn_props);
	mapi_props = request.values.sprop_array;
	for (i_prop = 0; i_prop < cn_props; ++i_prop) {
		size += cast_mapi_SPropValue(&mapi_props[i_prop], &sprops[i_prop]);
		size += sizeof(uint32_t);
	  }

	request.values.sprop_count = cn_props;
	size += sizeof(uint16_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SetProps;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SetProps = request;
	size += 6;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_object;
	mapi_request->handles[1] = hdl_related;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
 * makes permanent any changes made to an object since the last save
 * operation.
 */

MAPISTATUS	SaveChanges(struct emsmdb_context *emsmdb, uint32_t ulFlags, uint32_t hdl_related, uint32_t hdl_object)
{
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct SaveChanges_req	request;
	NTSTATUS		status;
	uint32_t		size = 0;
	TALLOC_CTX		*mem_ctx;
 
	mem_ctx = talloc_init("SaveChanges");

	size = 0;

	/* Fill the SaveChanges operation */
	request.flags = 0x001;
	request.unknown = 0x0;
	size += sizeof(uint16_t) + sizeof(uint8_t);

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_SaveChanges;
	mapi_req->mapi_flags = ulFlags;
	mapi_req->u.mapi_SaveChanges = request;
	size += 4;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = hdl_object;
	mapi_request->handles[1] = hdl_related;

	status = emsmdb_transaction(emsmdb, mapi_request, &mapi_response);

	if (mapi_response->mapi_repl->error_code != MAPI_E_SUCCESS) {
		return mapi_response->mapi_repl->error_code;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
