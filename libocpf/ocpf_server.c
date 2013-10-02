/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2010.

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

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"

/**
   \file ocpf_server.c

   \brief ocpf public API for server side. Do not perform any libmapi
   calls and trust incoming data. Most of these functions have
   equivalent in ocpf_public.c or ocpf_api.c
 */

/**
   \details Set the message class (type) associated to an OCPF file.

   \param context_id identifier of the context to set type for
   \param type pointer to the type's string to set

   \return MAPI_E_SUCCESS on success, otherwise MAPI/OCPF error
 */
_PUBLIC_ enum MAPISTATUS ocpf_server_set_type(uint32_t context_id,
					      const char *type)
{
	struct ocpf_context	*ctx;

	/* Sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);

	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	return ocpf_type_add(ctx, type);
}


/**
   \details Set an arbitrary folder identifier for a loaded
   message. This is required for migration purposes.

   \param context_id identifier to the context to update
   \param folderID new folder identifier value

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_INVALID_PARAMETER
 */
_PUBLIC_ enum MAPISTATUS ocpf_server_set_folderID(uint32_t context_id, 
						  mapi_id_t folderID)
{
	struct ocpf_context	*ctx;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return MAPI_E_INVALID_PARAMETER;

	ctx->folder = folderID;
	return MAPI_E_SUCCESS;
}


/**
   \details Build a SPropValue array from ocpf context

   This function builds a SPropValue array from the ocpf context and
   information stored.
   
   \param mem_ctx pointer to the memory context to use for memory
   allocation
   \param context_id identifier of the context to build a SPropValue
   array for

   \note This function is a server-side convenient function only. It
   doesn't handle named properties and its scope is much more limited
   than ocpf_set_SpropValue. Developers working on a client-side
   software/library must use ocpf_set_SPropValue instead.

   \return MAPI_E_SUCCESS on success, otherwise MAPI/OCPF error

   \sa ocpf_get_SPropValue
 */
_PUBLIC_ enum MAPISTATUS ocpf_server_set_SPropValue(TALLOC_CTX *mem_ctx, 
						    uint32_t context_id)
{
	struct ocpf_property	*pel;
	struct ocpf_context	*ctx;

	/* sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);

	/* Step 1. Search for the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	/* Step 2. Allocate SPropValue */
	ctx->cValues = 0;
	ctx->lpProps = talloc_array(ctx, struct SPropValue, 2);

	/* Step 3. Add Known properties */
	if (ctx->props && ctx->props->next) {
		for (pel = ctx->props; pel->next; pel = pel->next) {
			switch (pel->aulPropTag) {
			case PidTagMessageClass:
				ocpf_server_set_type(context_id, (const char *)pel->value);
				ctx->lpProps = add_SPropValue(ctx, ctx->lpProps, &ctx->cValues, 
							      pel->aulPropTag, pel->value);
				break;
			default:
				ctx->lpProps = add_SPropValue(ctx, ctx->lpProps, &ctx->cValues, 
							      pel->aulPropTag, pel->value);
			}
		}
	}
	/* Step 4. Add message class */
	if (ctx->type) {
		ctx->lpProps = add_SPropValue(ctx, ctx->lpProps, &ctx->cValues,
					      PidTagMessageClass, (const void *)ctx->type);
	}
	
	return MAPI_E_SUCCESS;	
}


/**
   \details Add a SPropValue structure to the context

   This functions adds a SPropValue to the ocpf context. This property
   must be part of the known property namespace. If the property
   already exists in the list, it is automatically replaced with the
   new one.

   \param context_id identifier of the ocpf context
   \param lpProps pointer to the SPropValue structure to add to the context

   \return MAPI_E_SUCCESS on success, otheriwse MAPI/OCPF error

   \sa ocpf_server_add_named_SPropValue
 */
_PUBLIC_ enum MAPISTATUS ocpf_server_add_SPropValue(uint32_t context_id, 
						    struct SPropValue *lpProps)
{
	int			ret;
	struct ocpf_context	*ctx;
	struct ocpf_property	*pel;
	struct ocpf_property	*element;
	bool			found = false;

	/* Sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!lpProps, MAPI_E_INVALID_PARAMETER, NULL);

	/* Step 1. Search the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);

	if (ctx->props && ctx->props->next) {
		for (pel = ctx->props; pel->next; pel = pel->next) {
			if (pel->aulPropTag == lpProps->ulPropTag) {
				talloc_free((void *)pel->value);
				ret = ocpf_set_propvalue((TALLOC_CTX *)pel, ctx, &pel->value,
							 (uint16_t)(pel->aulPropTag & 0xFFFF),
							 (uint16_t)(pel->aulPropTag & 0xFFFF),
							 lpProps->value, true);
				if (ret == -1) {
					return OCPF_ERROR;
				}
				found = true;
				break;
			}
		}
	} 

	/* Add the element if not found */
	if (found == false) {
		element = NULL;
		element = talloc_zero(ctx->props, struct ocpf_property);
		element->aulPropTag = lpProps->ulPropTag;
		ret = ocpf_set_propvalue((TALLOC_CTX *)element, ctx, &element->value,
					 (uint16_t)(lpProps->ulPropTag & 0xFFFF),
					 (uint16_t)(lpProps->ulPropTag & 0xFFFF),
					 lpProps->value, true);
		if (ret == -1) {
			talloc_free(element);
			return OCPF_ERROR;
		}
		DLIST_ADD(ctx->props, element);
	}
	return MAPI_E_SUCCESS;
}


/**
   \details Synchronize data on filesystem

   \param context_id identifier of the ocpf context

   \return MAPI_E_SUCCESS on success, otherwise otheriwse MAPI/OCPF error
 */
_PUBLIC_ enum MAPISTATUS ocpf_server_sync(uint32_t context_id)
{
	struct ocpf_context	*ctx;

	/* Sanity checks */
	MAPI_RETVAL_IF(!ocpf, MAPI_E_NOT_INITIALIZED, NULL);

	/* Step 1. Search the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);	

	if (ctx->flags == OCPF_FLAGS_CREATE) {
		ctx->flags = OCPF_FLAGS_RDWR;
	}

	if (ctx->fp) {
		fclose(ctx->fp);
	}
	
	switch (ctx->flags) {
	case OCPF_FLAGS_RDWR:
		ctx->fp = fopen(ctx->filename, "r+");
		break;
	case OCPF_FLAGS_READ:
		ctx->fp = fopen(ctx->filename, "r");
		break;
	case OCPF_FLAGS_WRITE:
		ctx->fp = fopen(ctx->filename, "w");
		break;
	}

	return MAPI_E_SUCCESS;
}
