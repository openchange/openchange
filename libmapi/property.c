/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2011.
   Copyright (C) Gregory Schiro 2006
   Copyright (C) Enrique J. Hernández 2015

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <gen_ndr/ndr_exchange.h>
#include <gen_ndr/ndr_property.h>
#include <param.h>

/**
   \file property.c

   \brief Functions for manipulating MAPI properties
 */


/**
  \details Create a property tag array
  
  \param mem_ctx talloc memory context to use for allocation
  \param PropCount the number of properties in the array
  
  The varargs (the third and subsequent arguments) are the property
  tags to make up the array. So the normal way to use this to create
  an array of two tags is like:
  \code
  struct SPropTagArray *array
  array = set_SPropTagArray(mem_ctx, 2, PR_ENTRYID, PR_DISPLAY_NAME);
  \endcode
*/
_PUBLIC_ struct SPropTagArray *set_SPropTagArray(TALLOC_CTX *mem_ctx, 
						 uint32_t PropCount, ...)
{
	struct SPropTagArray	*SPropTagArray;
	va_list			ap;
	uint32_t		i;
	uint32_t		*aulPropTag;

	aulPropTag = talloc_array(mem_ctx, uint32_t, PropCount);

	va_start(ap, PropCount);
	for (i = 0; i < PropCount; i++) {
		aulPropTag[i] = va_arg(ap, int);
	}
	va_end(ap);

	SPropTagArray = talloc(mem_ctx, struct SPropTagArray);
	SPropTagArray->aulPropTag = (enum MAPITAGS *) aulPropTag;
	SPropTagArray->cValues = PropCount;
	return SPropTagArray;
}

/**
   \details Add a property tag to an existing properties array

   \param mem_ctx talloc memory context to use for allocation
   \param SPropTagArray existing properties array to add to
   \param aulPropTag the property tag to add

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: SPropTagArray parameter is not correctly set
*/
_PUBLIC_ enum MAPISTATUS SPropTagArray_add(TALLOC_CTX *mem_ctx, 
					   struct SPropTagArray *SPropTagArray, 
					   enum MAPITAGS aulPropTag)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray->aulPropTag, MAPI_E_INVALID_PARAMETER, NULL);

	SPropTagArray->cValues += 1;
	SPropTagArray->aulPropTag = (enum MAPITAGS *) talloc_realloc(mem_ctx, SPropTagArray->aulPropTag,
								     uint32_t, SPropTagArray->cValues + 1);
	SPropTagArray->aulPropTag[SPropTagArray->cValues - 1] = aulPropTag;
	SPropTagArray->aulPropTag[SPropTagArray->cValues] = (enum MAPITAGS) 0;

	return MAPI_E_SUCCESS;
}

/**
   \details Delete a property tag from an existing properties array

   \param mem_ctx talloc memory context to use for allocation
   \param SPropTagArray existing properties array to remove from
   \param aulPropTag the property tag to remove

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: SPropTagArray parameter is not correctly set
*/
_PUBLIC_ enum MAPISTATUS SPropTagArray_delete(TALLOC_CTX *mem_ctx,
					      struct SPropTagArray *SPropTagArray,
					      uint32_t aulPropTag)
{
	uint32_t i, removed = 0;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray->cValues, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < SPropTagArray->cValues; i++) {
		if (SPropTagArray->aulPropTag[i] == aulPropTag) {
			removed++;
		}
		else if (removed > 0) {
			SPropTagArray->aulPropTag[i-removed] = SPropTagArray->aulPropTag[i];
		}
	}

	SPropTagArray->cValues -= removed;
	SPropTagArray->aulPropTag = (enum MAPITAGS *) talloc_realloc(mem_ctx, SPropTagArray->aulPropTag,
								     uint32_t, SPropTagArray->cValues + 1);
	SPropTagArray->aulPropTag[SPropTagArray->cValues] = (enum MAPITAGS) 0;

	return MAPI_E_SUCCESS;
}

/**
   \details Return the index of a property tag in an existing properties array

   \param SPropTagArray existing properties array to remove from
   \param aulPropTag the property tag to find
   \param propIdx the index of the found property (undefined when MAPI_E_NOT_FOUND is returned)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.

   \note Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_INVALID_PARAMETER: SPropTagArray parameter is not correctly set
*/
_PUBLIC_ enum MAPISTATUS SPropTagArray_find(struct SPropTagArray SPropTagArray,
					    enum MAPITAGS aulPropTag, 
					    uint32_t *propIdx)
{
	uint32_t i;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!propIdx, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < SPropTagArray.cValues; i++) {
		if (SPropTagArray.aulPropTag[i] == aulPropTag) {
			*propIdx = i;
			return MAPI_E_SUCCESS;
		}
	}

	return MAPI_E_NOT_FOUND;
}

/**
   \details Return the index of the email address in a properties array.

   It looks for PR_EMAIL_ADDRESS_UNICODE or PR_SMTP_ADDRESS_UNICODE in this order.

   \param SPropTagArray existing properties array to look for

   \return the index in case of success, -1 otherwise
*/
_PUBLIC_ int get_email_address_index_SPropTagArray(struct SPropTagArray *properties)
{
	uint32_t idx;

	/* Sanity checks */
	if (!properties) return -1;

	if (SPropTagArray_find(*properties, PR_EMAIL_ADDRESS_UNICODE, &idx) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_SMTP_ADDRESS_UNICODE, &idx) == MAPI_E_NOT_FOUND) {
		return -1;
	}

	return (int) idx;
}

/**
   \details Return the index of the display in the properties array.

   It looks for PR_DISPLAY_NAME_UNICODE, then for
   PR_7BIT_DISPLAY_NAME_UNICODE or finally for
   PR_RECIPIENT_DISPLAY_NAME_UNICODE in this order.

   \param SPropTagArray existing properties array to look for

   \return the index in case of success, -1 otherwise
*/
_PUBLIC_ int get_display_name_index_SPropTagArray(struct SPropTagArray *properties)
{
	uint32_t idx;

	/* Sanity checks */
	if (!properties) return -1;

	if (SPropTagArray_find(*properties, PR_DISPLAY_NAME_UNICODE, &idx) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_7BIT_DISPLAY_NAME_UNICODE, &idx) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_RECIPIENT_DISPLAY_NAME_UNICODE, &idx) == MAPI_E_NOT_FOUND) {
		return -1;
	}

	return (int) idx;
}

_PUBLIC_ const void *get_SPropValue(struct SPropValue *lpProps, 
				    enum MAPITAGS ulPropTag)
{
	uint32_t	i;

	/* Sanity checks */
	if (!lpProps) return NULL;

	for (i = 0; lpProps[i].ulPropTag; i++) {
		if (ulPropTag == lpProps[i].ulPropTag) {
			return get_SPropValue_data(&lpProps[i]);
		}
	}
	return NULL;
}

_PUBLIC_ struct SPropValue *get_SPropValue_SRowSet(struct SRowSet *RowSet, 
						   uint32_t ulPropTag)
{
	uint32_t	i;
	uint32_t	j;

	/* Sanity Checks */
	if (!RowSet) return NULL;

	for (i = 0; i != RowSet->cRows; i++) {
		for (j = 0; j < RowSet->aRow[i].cValues; j++) {
			if (ulPropTag == RowSet->aRow[i].lpProps[j].ulPropTag) {
				return (&RowSet->aRow[i].lpProps[j]);
			}
		}
	}

	return NULL;
}

_PUBLIC_ const void *get_SPropValue_SRowSet_data(struct SRowSet *RowSet,
						 uint32_t ulPropTag)
{
	struct SPropValue *lpProp;

	lpProp = get_SPropValue_SRowSet(RowSet, ulPropTag);
	return get_SPropValue(lpProp, ulPropTag);
}

_PUBLIC_ enum MAPISTATUS set_default_error_SPropValue_SRow(struct SRow *aRow, enum MAPITAGS ulPropTag, void *data)
{
	uint32_t	i;

	for (i = 0; i < aRow->cValues; i++) {
		if ((ulPropTag & 0xFFFF0000) == (aRow->lpProps[i].ulPropTag & 0xFFFF0000) &&
		    (aRow->lpProps[i].ulPropTag & 0xFFFF) == 0xA) {
			set_SPropValue_proptag(&(aRow->lpProps[i]), ulPropTag, data);
			return MAPI_E_SUCCESS;
		}
	}
	return MAPI_E_NOT_FOUND;
}

_PUBLIC_ struct SPropValue *get_SPropValue_SRow(struct SRow *aRow,
						uint32_t ulPropTag)
{
	uint32_t	i;

	if ( ! aRow) {
		return NULL;
	}

	for (i = 0; i < aRow->cValues; i++) {
		if (ulPropTag == aRow->lpProps[i].ulPropTag) {
			return (&aRow->lpProps[i]);
		}
	}

	return NULL;
}

_PUBLIC_ const void *get_SPropValue_SRow_data(struct SRow *aRow,
					      uint32_t ulPropTag)
{
	struct SPropValue *lpProp;

	lpProp = get_SPropValue_SRow(aRow, ulPropTag);
	return get_SPropValue(lpProp, ulPropTag);
}

/*
  Create a MAPITAGS array from a SRow entry
 */

enum MAPITAGS *get_MAPITAGS_SRow(TALLOC_CTX *mem_ctx, 
				 struct SRow *aRow, 
				 uint32_t *actual_count)
{
	enum MAPITAGS	*mapitags;
	uint32_t	count, idx;

	mapitags = talloc_array(mem_ctx, enum MAPITAGS, aRow->cValues + 1);

	for (count = 0, idx=0; count < aRow->cValues; count++) {
		if ((aRow->lpProps[count].ulPropTag & 0xFFFF) != PT_ERROR) {
			mapitags[idx] = aRow->lpProps[count].ulPropTag;
			idx++;
		}
	}
	mapitags[idx] = (enum MAPITAGS) 0;
	*actual_count = idx;

	return mapitags;
}

/*
  Remove MAPITAGS entries from a MAPITAGS array
*/

uint32_t MAPITAGS_delete_entries(enum MAPITAGS *mapitags, uint32_t final_count, uint32_t PropCount, ...)
{
	va_list			ap;
	uint32_t		i,j;
	uint32_t		aulPropTag;
	uint32_t		count = 0;
	
	va_start(ap, PropCount);
	for (i = 0; i != PropCount; i++) {
		aulPropTag = va_arg(ap, uint32_t);
		for (count = 0; mapitags[count]; count++) {
			if (aulPropTag == (uint32_t)mapitags[count]) {
				final_count -= 1;
				for (j = count; mapitags[j]; j++) {
				  mapitags[j] = (mapitags[j+1]) ? mapitags[j+1] : (enum MAPITAGS) 0;
				}
			}
		}
	}
	va_end(ap);

	return final_count;
}

_PUBLIC_ const void *find_SPropValue_data(struct SRow *aRow, uint32_t mapitag)
{
	uint32_t i;

	if (!aRow) {
		return NULL;
	}

	for (i = 0; i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == mapitag) {
			return get_SPropValue_data(&(aRow->lpProps[i]));
		}
	}
	return NULL;
}

_PUBLIC_ const void *find_mapi_SPropValue_data(
					struct mapi_SPropValue_array *properties, uint32_t mapitag)
{
	uint32_t i;

	if ( ! properties) {
		return NULL;
	}

	for (i = 0; i < properties->cValues; i++) {
		if (properties->lpProps[i].ulPropTag == mapitag) {
			return get_mapi_SPropValue_data(&properties->lpProps[i]);
		}
	}
	return NULL;
}

_PUBLIC_ const void *get_mapi_SPropValue_data(struct mapi_SPropValue *lpProp)
{
	if ( ! lpProp) {
		return NULL;
	}
	if (lpProp->ulPropTag == 0) {
		return NULL;
	}
	switch(lpProp->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		return (const void *)(uint8_t *)&lpProp->value.b;
	case PT_I2: /* Equivalent to PT_SHORT */
		return (const void *)(uint16_t *)&lpProp->value.i;
	case PT_LONG: /* Equivalent to PT_I4 */
		return (const void *)&lpProp->value.l;
	case PT_DOUBLE:
		return (const void *)&lpProp->value.dbl;
	case PT_I8:
		return (const void *)&lpProp->value.d;
	case PT_SYSTIME:
		return (const void *)(struct FILETIME *)&lpProp->value.ft;
	case PT_ERROR:
		return (const void *)&lpProp->value.err;
	case PT_STRING8:
		return (const void *)lpProp->value.lpszA;
	case PT_UNICODE:
		return (const void *)lpProp->value.lpszW;
	case PT_BINARY:
		return (const void *)(struct SBinary_short *)&lpProp->value.bin;
	case PT_MV_LONG:
		return (const void *)(struct mapi_MV_LONG_STRUCT *)&lpProp->value.MVl;
	case PT_MV_STRING8:
		return (const void *)(struct mapi_SLPSTRArray *)&lpProp->value.MVszA;
	case PT_MV_UNICODE:
		return (const void *)(struct mapi_SLPSTRArrayW *)&lpProp->value.MVszW;
	case PT_MV_BINARY:
		return (const void *)(struct mapi_SBinaryArray *)&lpProp->value.MVbin;
	default:
		return NULL;
	}
}

_PUBLIC_ const void *get_SPropValue_data(struct SPropValue *lpProps)
{
	if (lpProps->ulPropTag == 0) {
		return NULL;
	}

	switch(lpProps->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		return (const void *)&lpProps->value.i;
	case PT_BOOLEAN:
		return (const void *)&lpProps->value.b;
	case PT_I8:
		return (const void *)&lpProps->value.d;
	case PT_STRING8:
		return (const void *)lpProps->value.lpszA;
	case PT_UNICODE:
		return (const void *)lpProps->value.lpszW;
	case PT_SYSTIME:
		return (const void *)(struct FILETIME *)&lpProps->value.ft;
	case PT_ERROR:
		return (const void *)&lpProps->value.err;
	case PT_LONG:
		return (const void *)&lpProps->value.l;
	case PT_DOUBLE:
		return (const void *)&lpProps->value.dbl;
	case PT_CLSID:
		return (const void *)lpProps->value.lpguid;
	case PT_BINARY:
	case PT_SVREID:
		return (const void *)&lpProps->value.bin;
	case PT_OBJECT:
		return (const void *)&lpProps->value.object;
	case PT_MV_SHORT:
		return (const void *)(struct ShortArray_r *)&lpProps->value.MVi;
	case PT_MV_LONG:
		return (const void *)(struct LongArray_r *)&lpProps->value.MVl;
	case PT_MV_STRING8:
		return (const void *)(struct StringArray_r *)&lpProps->value.MVszA;
	case PT_MV_UNICODE:
		return (const void *)(struct StringArrayW_r *)&lpProps->value.MVszW;
	case PT_MV_BINARY:
		return (const void *)(struct BinaryArray_r *)&lpProps->value.MVbin;
	case PT_MV_SYSTIME:
		return (const void *)(struct DateTimeArray_r *)&lpProps->value.MVft;
	case PT_NULL:
		return (const void *)&lpProps->value.null;
	default:
		return NULL;
	}
}

_PUBLIC_ bool set_SPropValue_proptag(struct SPropValue *lpProps, enum MAPITAGS aulPropTag, const void *data)
{
	lpProps->ulPropTag = aulPropTag;
	lpProps->dwAlignPad = 0x0;

	return (set_SPropValue(lpProps, data));
}

_PUBLIC_ struct SPropValue *add_SPropValue(TALLOC_CTX *mem_ctx, 
					   struct SPropValue *lpProps, 
					   uint32_t *cValues, 
					   enum MAPITAGS aulPropTag, 
					   const void * data)
{
	lpProps = talloc_realloc(mem_ctx, lpProps, struct SPropValue, *cValues + 2);

	set_SPropValue_proptag(&lpProps[*cValues], aulPropTag, data);
	*cValues = *cValues + 1;

	return lpProps;
}

_PUBLIC_ bool set_mapi_SPropValue(TALLOC_CTX *mem_ctx, struct mapi_SPropValue *lpProps, const void *data)
{
	if (data == NULL) {
		lpProps->ulPropTag = (lpProps->ulPropTag & 0xffff0000) | PT_ERROR;
		lpProps->value.err = MAPI_E_NOT_FOUND;
		return false;
	}
	switch (lpProps->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		lpProps->value.i = *((const uint16_t *)data);
		break;
	case PT_LONG:
		lpProps->value.l = *((const uint32_t *)data);
		break;
	case PT_DOUBLE:
		lpProps->value.dbl = *((const double *)data);
		break;
	case PT_I8:
		lpProps->value.d = *((const uint64_t *)data);
		break;
	case PT_BOOLEAN:
		lpProps->value.b = *((const uint8_t *)data);
		break;
	case PT_STRING8:
		lpProps->value.lpszA = (const char *) data;
		break;
	case PT_BINARY:
	{
		struct Binary_r *bin;

		bin = (struct Binary_r *)data;
		lpProps->value.bin.cb = (uint16_t)bin->cb;
		lpProps->value.bin.lpb = (void *)bin->lpb;
	}
		break;
	case PT_UNICODE: 
		lpProps->value.lpszW = (const char *) data;
		break;
	case PT_CLSID:
		lpProps->value.lpguid = *((struct GUID *) data);
		break;
	case PT_SYSTIME:
		lpProps->value.ft = *((const struct FILETIME *) data);
		break;
	case PT_ERROR:
		lpProps->value.err = *((const uint32_t *)data);
		break;
	case PT_MV_LONG:
		lpProps->value.MVl = *((const struct mapi_MV_LONG_STRUCT *)data);
		break;
	case PT_MV_STRING8:
		lpProps->value.MVszA = *((const struct mapi_SLPSTRArray *)data);
		break;
	case PT_MV_BINARY:
		lpProps->value.MVbin = *((const struct mapi_SBinaryArray *)data);
		break;
	case PT_MV_CLSID:
		lpProps->value.MVguid = *((const struct mapi_SGuidArray *)data);
		break;
	case PT_MV_UNICODE:
		lpProps->value.MVszW = *((const struct mapi_SLPSTRArrayW *)data);
		break;
	default:
		lpProps->ulPropTag = (lpProps->ulPropTag & 0xffff0000) | PT_ERROR;
		lpProps->value.err = MAPI_E_NOT_FOUND;

		return false;
	}

	return true;
}

_PUBLIC_ bool set_mapi_SPropValue_proptag(TALLOC_CTX *mem_ctx, struct mapi_SPropValue *lpProps, uint32_t aulPropTag, const void *data)
{
	lpProps->ulPropTag = aulPropTag;

	return (set_mapi_SPropValue(mem_ctx, lpProps, data));
}

_PUBLIC_ struct mapi_SPropValue *add_mapi_SPropValue(TALLOC_CTX *mem_ctx, struct mapi_SPropValue *lpProps, uint16_t *cValues, uint32_t aulPropTag, const void *data)
{
	lpProps = talloc_realloc(mem_ctx, lpProps, struct mapi_SPropValue, *cValues + 2);
	OC_DEBUG(0, "%d: setting value for 0x%.8x", *cValues, aulPropTag);
	set_mapi_SPropValue_proptag(mem_ctx, &lpProps[*cValues], aulPropTag, data);
	*cValues = *cValues + 1;

	return lpProps;
}

_PUBLIC_ bool set_SPropValue(struct SPropValue *lpProps, const void *data)
{
	if (data == NULL) {
		lpProps->ulPropTag = (lpProps->ulPropTag & 0xffff0000) | PT_ERROR;
		lpProps->value.err = MAPI_E_NOT_FOUND;
		return false;
	}
	switch (lpProps->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		lpProps->value.i = *((const uint16_t *)data);
		break;
	case PT_LONG:
		lpProps->value.l = *((const uint32_t *)data);
		break;
	case PT_DOUBLE:
		lpProps->value.dbl = *((const double *)data);
		break;
	case PT_I8:
		lpProps->value.d = *((const uint64_t *)data);
		break;
	case PT_BOOLEAN:
		lpProps->value.b = *((const uint8_t *)data);
		break;
	case PT_STRING8:
		lpProps->value.lpszA = (const char *) data;
		break;
	case PT_BINARY:
	case PT_SVREID:
		lpProps->value.bin = *((const struct Binary_r *)data);
		break;
	case PT_UNICODE:
		lpProps->value.lpszW = (const char *) data;
		break;
	case PT_CLSID:
		lpProps->value.lpguid = (struct FlatUID_r *) data;
		break;
	case PT_SYSTIME:
		lpProps->value.ft = *((const struct FILETIME *) data);
		break;
	case PT_ERROR:
		lpProps->value.err = *((enum MAPISTATUS *)data);
		break;
	case PT_MV_SHORT:
		lpProps->value.MVi = *((const struct ShortArray_r *)data);
		break;
	case PT_MV_LONG:
		lpProps->value.MVl = *((const struct LongArray_r *)data);
		break;
	case PT_MV_STRING8:
		lpProps->value.MVszA = *((const struct StringArray_r *)data);
		break;
	case PT_MV_BINARY:
		lpProps->value.MVbin = *((const struct BinaryArray_r *)data);
		break;
	case PT_MV_CLSID:
		lpProps->value.MVguid = *((const struct FlatUIDArray_r *)data);
		break;
	case PT_MV_UNICODE:
		lpProps->value.MVszW = *((const struct StringArrayW_r *)data);
		break;
	case PT_MV_SYSTIME:
		lpProps->value.MVft = *((const struct DateTimeArray_r *)data);
		break;
	case PT_NULL:
		lpProps->value.null = *((const uint32_t *)data);
		break;
	case PT_OBJECT:
		lpProps->value.object = *((const uint32_t *)data);
		break;
	default:
		lpProps->ulPropTag = (lpProps->ulPropTag & 0xffff0000) | PT_ERROR;
		lpProps->value.err = MAPI_E_NOT_FOUND;

		return false;
	}

	return true;
}

_PUBLIC_ uint32_t get_mapi_property_size(struct mapi_SPropValue *lpProp)
{
	switch(lpProp->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		return sizeof (uint8_t);
	case PT_I2:
		return sizeof (uint16_t);
	case PT_LONG:
	case PT_ERROR:
		return sizeof (uint32_t);
	case PT_DOUBLE:
		return sizeof (double);
	case PT_I8:
		return sizeof (uint64_t);
	case PT_STRING8:
		return strlen(lpProp->value.lpszA) + 1;
	case PT_UNICODE:
		return get_utf8_utf16_conv_length(lpProp->value.lpszW);
	case PT_SYSTIME:
		return sizeof (struct FILETIME);
	case PT_BINARY:
		return (lpProp->value.bin.cb + sizeof(uint16_t));
	}
	return 0;
}


/**
   \details Convenience function to copy an array of struct SPropValue or a
   part thereof into another array, by duplicating and properly parenting
   pointer data. The destination array is considered to be preallocated.
*/
_PUBLIC_ void mapi_copy_spropvalues(TALLOC_CTX *mem_ctx, struct SPropValue *source_values, struct SPropValue *dest_values, uint32_t count)
{
	uint32_t		i, k;
	struct SPropValue	*source_value, *dest_value;
	uint16_t		prop_type;

	for (i = 0; i < count; i++) {
		source_value = source_values + i;
		dest_value = dest_values + i;
		*dest_value = *source_value;

		prop_type = (source_value->ulPropTag & 0xFFFF);
		switch(prop_type) {
		case PT_STRING8:
			dest_value->value.lpszA = talloc_strdup(mem_ctx, source_value->value.lpszA);
			break;
		case PT_UNICODE:
			dest_value->value.lpszW = talloc_strdup(mem_ctx, source_value->value.lpszW);
			break;
		case PT_CLSID:
			dest_value->value.lpguid = talloc_memdup(mem_ctx, source_value->value.lpguid, sizeof(*source_value->value.lpguid));
			break;
		case PT_SVREID:
			dest_value->value.bin.cb = source_value->value.bin.cb;
			dest_value->value.bin.lpb = talloc_memdup(mem_ctx, source_value->value.bin.lpb, source_value->value.bin.cb);
			break;
		case PT_BINARY:
			dest_value->value.bin.cb = source_value->value.bin.cb;
			dest_value->value.bin.lpb = talloc_memdup(mem_ctx, source_value->value.bin.lpb, sizeof(uint8_t) * source_value->value.bin.cb);
			break;
		case PT_MV_LONG:
			dest_value->value.MVl.cValues = source_value->value.MVl.cValues;
			dest_value->value.MVl.lpl = talloc_memdup(mem_ctx, source_value->value.MVl.lpl, sizeof(uint32_t) * source_value->value.MVl.cValues);
			break;
		case PT_MV_STRING8:
			dest_value->value.MVszA.cValues = source_value->value.MVszA.cValues;

			dest_value->value.MVszA.lppszA = talloc_array(mem_ctx, const char *, source_value->value.MVszA.cValues);
			for (k = 0; k < source_value->value.MVszA.cValues; k++) {
				dest_value->value.MVszA.lppszA[k] = talloc_strdup(dest_value->value.MVszA.lppszA, source_value->value.MVszA.lppszA[k]);
			}
			break;
		case PT_MV_UNICODE:
			dest_value->value.MVszW.cValues = source_value->value.MVszW.cValues;

			dest_value->value.MVszW.lppszW = talloc_array(mem_ctx, const char *, source_value->value.MVszW.cValues);
			for (k = 0; k < source_value->value.MVszW.cValues; k++) {
				dest_value->value.MVszW.lppszW[k] = talloc_strdup(dest_value->value.MVszW.lppszW, source_value->value.MVszW.lppszW[k]);
			}
			break;
		case PT_MV_BINARY:
			dest_value->value.MVbin.cValues = source_value->value.MVbin.cValues;

			dest_value->value.MVbin.lpbin = talloc_array(mem_ctx, struct Binary_r, source_value->value.MVbin.cValues);
			for (k = 0; k < source_value->value.MVbin.cValues; k++) {
				dest_value->value.MVbin.lpbin[k] = source_value->value.MVbin.lpbin[k];
				if (source_value->value.MVbin.lpbin[k].cb) {
					dest_value->value.MVbin.lpbin[k].lpb = talloc_memdup(dest_value->value.MVbin.lpbin,
											     source_value->value.MVbin.lpbin[k].lpb,
											     source_value->value.MVbin.lpbin[k].cb);
				}
			}
			break;
		default:
			// check if missed to handle a multi-value property
			if (prop_type & MV_FLAG) {
				// TODO: Replace this with OC_PANIC() macro when it gets visible in libmapi too
				OC_DEBUG(0, "Unexpected multi-value property type: %s.",
						get_proptag_name(source_value->ulPropTag));
				smb_panic("Unexpected multi-value property type while copying 'struct SPropValue'");
			}
		}
	}
}

/**
   \details Convenience function to convert a SPropValue structure
   into a mapi_SPropValue structure and return the associated size.

   \param mem_ctx pointer to the memory context to use for allocation
   \param mapi_sprop pointer to the MAPI SPropValue structure to copy data to
   \param sprop pointer to the SPropValue structure to copy data from

   \return size of the converted data on success, otherwise 0
 */
_PUBLIC_ uint32_t cast_mapi_SPropValue(TALLOC_CTX *mem_ctx,
				       struct mapi_SPropValue *mapi_sprop, 
				       struct SPropValue *sprop)
{
	mapi_sprop->ulPropTag = sprop->ulPropTag;

	switch(sprop->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		mapi_sprop->value.b = sprop->value.b;
		return sizeof(uint8_t);
	case PT_I2:
		mapi_sprop->value.i = sprop->value.i;
		return sizeof(uint16_t);
	case PT_LONG:
		mapi_sprop->value.l = sprop->value.l;
		return sizeof(uint32_t);
	case PT_DOUBLE:
		mapi_sprop->value.dbl = sprop->value.dbl;
		return sizeof(double);
	case PT_I8:
		mapi_sprop->value.d = sprop->value.d;
		return sizeof(uint64_t);
	case PT_STRING8:
		mapi_sprop->value.lpszA = sprop->value.lpszA;
		if (!mapi_sprop->value.lpszA) return 0;
		return (strlen(sprop->value.lpszA) + 1);
	case PT_UNICODE:
		mapi_sprop->value.lpszW = sprop->value.lpszW;
		if (!mapi_sprop->value.lpszW) return 0;
		return (get_utf8_utf16_conv_length(mapi_sprop->value.lpszW));
	case PT_SYSTIME:
		mapi_sprop->value.ft.dwLowDateTime = sprop->value.ft.dwLowDateTime;
		mapi_sprop->value.ft.dwHighDateTime = sprop->value.ft.dwHighDateTime;
		return (sizeof (struct FILETIME));
	case PT_BINARY:
		mapi_sprop->value.bin.cb = sprop->value.bin.cb;
		mapi_sprop->value.bin.lpb = sprop->value.bin.lpb;
		return (mapi_sprop->value.bin.cb + sizeof(uint16_t));
        case PT_ERROR:
                mapi_sprop->value.err = sprop->value.err;
                return sizeof(uint32_t);
	case PT_CLSID:
	{
		DATA_BLOB	b;

		b.data = sprop->value.lpguid->ab;
		b.length = 16;
		GUID_from_ndr_blob(&b, &mapi_sprop->value.lpguid);
		return sizeof(struct GUID);
	}
	case PT_SVREID:
		mapi_sprop->value.bin.cb = sprop->value.bin.cb;
		mapi_sprop->value.bin.lpb = sprop->value.bin.lpb;
		return (mapi_sprop->value.bin.cb + sizeof(uint16_t));
	case PT_MV_STRING8:
	{
		uint32_t	i;
		uint32_t	size = 0;
		
		mapi_sprop->value.MVszA.cValues = sprop->value.MVszA.cValues;
		size += 4;
		
		mapi_sprop->value.MVszA.strings = talloc_array(mem_ctx, struct mapi_LPSTR, 
							       mapi_sprop->value.MVszA.cValues);
		for (i = 0; i < mapi_sprop->value.MVszA.cValues; i++) {
			mapi_sprop->value.MVszA.strings[i].lppszA = sprop->value.MVszA.lppszA[i];
			size += strlen(mapi_sprop->value.MVszA.strings[i].lppszA) + 1;
		}
		return size;
	}
	case PT_MV_UNICODE:
	{
		uint32_t	i;
		uint32_t	size = 0;

		mapi_sprop->value.MVszW.cValues = sprop->value.MVszW.cValues;
		size += 4;
		
		mapi_sprop->value.MVszW.strings = talloc_array(mem_ctx, struct mapi_LPWSTR, 
							       mapi_sprop->value.MVszW.cValues);
		for (i = 0; i < mapi_sprop->value.MVszW.cValues; i++) {
			mapi_sprop->value.MVszW.strings[i].lppszW = sprop->value.MVszW.lppszW[i];
			size += get_utf8_utf16_conv_length(mapi_sprop->value.MVszW.strings[i].lppszW);
		}
		return size;
	}
	case PT_MV_BINARY:
	{
		uint32_t        i;
		uint32_t        size = 0;
		
		mapi_sprop->value.MVbin.cValues = sprop->value.MVbin.cValues;
		size += 4;

		mapi_sprop->value.MVbin.bin = talloc_array(mem_ctx, struct SBinary_short, 
							   mapi_sprop->value.MVbin.cValues);
		for (i = 0; i < mapi_sprop->value.MVbin.cValues; i++) {
			mapi_sprop->value.MVbin.bin[i].cb = sprop->value.MVbin.lpbin[i].cb;
			mapi_sprop->value.MVbin.bin[i].lpb = sprop->value.MVbin.lpbin[i].lpb;
			size += sprop->value.MVbin.lpbin[i].cb + sizeof (uint16_t);
		}
		return size;
	}
	case PT_MV_LONG:
	{
		uint32_t i;
		
		mapi_sprop->value.MVl.cValues = sprop->value.MVl.cValues;
		mapi_sprop->value.MVl.lpl = talloc_array (mem_ctx, uint32_t, mapi_sprop->value.MVl.cValues);
		for (i = 0; i < mapi_sprop->value.MVl.cValues; i++) {
			mapi_sprop->value.MVl.lpl[i] = sprop->value.MVl.lpl[i];
		}
		return sizeof(mapi_sprop->value.MVl.cValues) + (mapi_sprop->value.MVl.cValues * sizeof (uint32_t));
	}
	case PT_MV_CLSID:
	{
		uint32_t i;
		DATA_BLOB b;

		mapi_sprop->value.MVguid.cValues = sprop->value.MVguid.cValues;
		mapi_sprop->value.MVguid.lpguid = talloc_array (mem_ctx, struct GUID, mapi_sprop->value.MVguid.cValues);
		for (i = 0; i < mapi_sprop->value.MVguid.cValues; i++) {
			b.data = sprop->value.MVguid.lpguid[i]->ab;
			b.length = 16;

			GUID_from_ndr_blob(&b, &(mapi_sprop->value.MVguid.lpguid[i]));
		}
		return sizeof(mapi_sprop->value.MVguid.cValues) + (mapi_sprop->value.MVguid.cValues * sizeof (struct GUID));
	}
        default:
                printf("unhandled conversion case in cast_mapi_SPropValue(): 0x%x\n", (sprop->ulPropTag & 0xFFFF));
                OPENCHANGE_ASSERT();
	}
	return 0;

}


/**
   \details Convenience function to convert a mapi_SPropValue
   structure into a SPropValue structure and return the associated
   size

   \param mem_ctx pointer to the memory context to use for allocation
   \param mapi_sprop pointer to the MAPI SPropValue structure to copy data from
   \param sprop pointer to the SPropValue structure to copy data to

   \return size of the converted data on success, otherwise 0
 */
_PUBLIC_ uint32_t cast_SPropValue(TALLOC_CTX *mem_ctx, 
				  struct mapi_SPropValue *mapi_sprop, 
				  struct SPropValue *sprop)
{
	sprop->ulPropTag = mapi_sprop->ulPropTag;

	switch(sprop->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		sprop->value.b = mapi_sprop->value.b;
		return sizeof(uint8_t);
	case PT_I2:
		sprop->value.i = mapi_sprop->value.i;
		return sizeof(uint16_t);
	case PT_LONG:
		sprop->value.l = mapi_sprop->value.l;
		return sizeof(uint32_t);
	case PT_DOUBLE:
		sprop->value.dbl = mapi_sprop->value.dbl;
		return sizeof(double);
	case PT_I8:
		sprop->value.d = mapi_sprop->value.d;
		return sizeof(uint64_t);
	case PT_STRING8:
		sprop->value.lpszA = mapi_sprop->value.lpszA;
		if (!mapi_sprop->value.lpszA) return 0;
		return (strlen(sprop->value.lpszA) + 1);
	case PT_UNICODE:
		sprop->value.lpszW = mapi_sprop->value.lpszW;
		if (!sprop->value.lpszW) return 0;
		return (get_utf8_utf16_conv_length(mapi_sprop->value.lpszW));
	case PT_SYSTIME:
		sprop->value.ft.dwLowDateTime = mapi_sprop->value.ft.dwLowDateTime;
		sprop->value.ft.dwHighDateTime = mapi_sprop->value.ft.dwHighDateTime;
		return (sizeof (struct FILETIME));
	case PT_CLSID:
	{
		DATA_BLOB	b;
		
		GUID_to_ndr_blob(&(mapi_sprop->value.lpguid), mem_ctx, &b);
		sprop->value.lpguid = talloc_zero(mem_ctx, struct FlatUID_r);
		sprop->value.lpguid = (struct FlatUID_r *)memcpy(sprop->value.lpguid->ab, b.data, 16);
		return (sizeof (struct FlatUID_r));
	}
	case PT_SVREID:
		sprop->value.bin.cb = mapi_sprop->value.bin.cb;
		sprop->value.bin.lpb = mapi_sprop->value.bin.lpb;
		return (sprop->value.bin.cb + sizeof (uint16_t));
	case PT_BINARY:
		sprop->value.bin.cb = mapi_sprop->value.bin.cb;
		sprop->value.bin.lpb = mapi_sprop->value.bin.lpb;
		return (sprop->value.bin.cb + sizeof(uint16_t));
        case PT_ERROR:
                sprop->value.err = (enum MAPISTATUS)mapi_sprop->value.err;
                return sizeof(uint32_t);
	case PT_MV_LONG:
	{
		uint32_t	i;
		uint32_t	size = 0;

		sprop->value.MVl.cValues = mapi_sprop->value.MVl.cValues;
		size += 4;

		sprop->value.MVl.lpl = talloc_array(mem_ctx, uint32_t, sprop->value.MVl.cValues);
		for (i = 0; i < sprop->value.MVl.cValues; i++) {
			sprop->value.MVl.lpl[i] = mapi_sprop->value.MVl.lpl[i];
			size += sizeof (uint32_t);
		}
		return size;
	}
	case PT_MV_STRING8:
	{
		uint32_t	i;
		uint32_t	size = 0;

		sprop->value.MVszA.cValues = mapi_sprop->value.MVszA.cValues;
		size += 4;

		sprop->value.MVszA.lppszA = talloc_array(mem_ctx, const char *, sprop->value.MVszA.cValues);
		for (i = 0; i < sprop->value.MVszA.cValues; i++) {
			sprop->value.MVszA.lppszA[i] = mapi_sprop->value.MVszA.strings[i].lppszA;
			size += strlen(sprop->value.MVszA.lppszA[i]) + 1;
		}
		return size;
	}
        case PT_MV_UNICODE:
	{
		uint32_t        i;
		uint32_t        size = 0;

		sprop->value.MVszW.cValues = mapi_sprop->value.MVszW.cValues;
		size += 4;

		sprop->value.MVszW.lppszW = talloc_array(mem_ctx, const char*, sprop->value.MVszW.cValues);
		for (i = 0; i < sprop->value.MVszW.cValues; i++) {
			sprop->value.MVszW.lppszW[i] = mapi_sprop->value.MVszW.strings[i].lppszW;
			size += get_utf8_utf16_conv_length(sprop->value.MVszW.lppszW[i]);
		}
		return size;
	}
	case PT_MV_CLSID:
	{
		uint32_t        i;
		uint32_t        size = 0;
		// conceptually we're copying  mapi_SGuidArray over to FlatUIDArray_r
		//	typedef struct {
		//		uint32		cValues;
		//		GUID		lpguid[cValues];
		//	} mapi_SGuidArray;
		// 	typedef [flag(NDR_NOALIGN)] struct {
		//		[range(0,100000)]uint32		cValues;
		//		[size_is(cValues)] FlatUID_r	**lpguid; 
		//	} FlatUIDArray_r;
		sprop->value.MVguid.cValues = mapi_sprop->value.MVguid.cValues;
		size += sizeof(uint32_t);
		
		sprop->value.MVguid.lpguid = talloc_array(mem_ctx, struct FlatUID_r*, sprop->value.MVguid.cValues);
		for (i = 0; i < sprop->value.MVguid.cValues; ++i) {
			DATA_BLOB	b;
			
			sprop->value.MVguid.lpguid[i] = talloc_zero(mem_ctx, struct FlatUID_r);
			GUID_to_ndr_blob(&(mapi_sprop->value.MVguid.lpguid[i]), mem_ctx, &b);
			sprop->value.MVguid.lpguid[i] = (struct FlatUID_r *)memcpy(sprop->value.MVguid.lpguid[i]->ab, b.data, sizeof(struct FlatUID_r));
			size += (sizeof (struct FlatUID_r));
		}
		return size;
	}
	case PT_MV_BINARY:
	{
		uint32_t	i;
		uint32_t	size = 0;

		sprop->value.MVbin.cValues = mapi_sprop->value.MVbin.cValues;
		size += 4;

		sprop->value.MVbin.lpbin = talloc_array(mem_ctx, struct Binary_r, sprop->value.MVbin.cValues);
		for (i = 0; i < sprop->value.MVbin.cValues; i++) {
			sprop->value.MVbin.lpbin[i].cb = mapi_sprop->value.MVbin.bin[i].cb;
			if (sprop->value.MVbin.lpbin[i].cb) {
			  sprop->value.MVbin.lpbin[i].lpb = (uint8_t *)talloc_memdup(sprop->value.MVbin.lpbin, 
										mapi_sprop->value.MVbin.bin[i].lpb,
										mapi_sprop->value.MVbin.bin[i].cb);
			} else {
				sprop->value.MVbin.lpbin[i].lpb = NULL;
			}
			size += sizeof (uint32_t);
			size += sprop->value.MVbin.lpbin[i].cb;
		}
		return size;
	}
        default:
                printf("unhandled conversion case in cast_SPropValue(): 0x%x\n", (sprop->ulPropTag & 0xFFFF));
                OPENCHANGE_ASSERT();
	}
	return 0;
}


/**
   \details add a SPropValue structure to a SRow array

   \param aRow pointer to the SRow array where spropvalue should be
   appended
   \param spropvalue reference to the SPropValue structure to add to
   aRow

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_INVALID_PARAMETER.
 */
_PUBLIC_ enum MAPISTATUS SRow_addprop(struct SRow *aRow, struct SPropValue spropvalue)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		cValues;
	struct SPropValue	lpProp;
	uint32_t		i;
	
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!aRow, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *) aRow;

	/* If the property tag already exist, overwrite its value */
	for (i = 0; i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == spropvalue.ulPropTag) {
			aRow->lpProps[i] = spropvalue;
			return MAPI_E_SUCCESS;
		}
	}

	cValues = aRow->cValues + 1;
	aRow->lpProps = talloc_realloc(mem_ctx, aRow->lpProps, struct SPropValue, cValues);
	lpProp = aRow->lpProps[cValues-1];
	lpProp.ulPropTag = spropvalue.ulPropTag;
	lpProp.dwAlignPad = 0;
	set_SPropValue(&(lpProp), get_SPropValue_data(&spropvalue));
	aRow->cValues = cValues;
	aRow->lpProps[cValues - 1] = lpProp;

	return MAPI_E_SUCCESS;
}


/**
   \details Append a SPropValue structure to given SRowSet

   \param mem_ctx pointer to the memory context
   \param SRowSet pointer to the SRowSet array to update
   \param spropvalue the SPropValue to append within SRowSet

   \return 0 on success, otherwise 1
 */
_PUBLIC_ uint32_t SRowSet_propcpy(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, struct SPropValue spropvalue)
{
	uint32_t		rows;
	uint32_t		cValues;
	struct SPropValue	lpProp;

	/* Sanity checks */
	if (!SRowSet) return 1;

	for (rows = 0; rows < SRowSet->cRows; rows++) {
		cValues = SRowSet->aRow[rows].cValues + 1;
		SRowSet->aRow[rows].lpProps = talloc_realloc(mem_ctx, SRowSet->aRow[rows].lpProps, struct SPropValue, cValues);
		lpProp = SRowSet->aRow[rows].lpProps[cValues-1];
		lpProp.ulPropTag = spropvalue.ulPropTag;
		lpProp.dwAlignPad = 0;
		set_SPropValue(&(lpProp), (void *)&spropvalue.value);
		SRowSet->aRow[rows].cValues = cValues;
		SRowSet->aRow[rows].lpProps[cValues - 1] = lpProp;
	}
	return 0;
}

_PUBLIC_ void mapi_SPropValue_array_named(mapi_object_t *obj, 
					  struct mapi_SPropValue_array *props)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct MAPINAMEID	*nameid;
	uint32_t		propID;
	uint16_t		count;
	uint32_t		i;

	mem_ctx = talloc_named(mapi_object_get_session(obj), 0, "mapi_SPropValue_array_named");

	for (i = 0; i < props->cValues; i++) {
		if ((props->lpProps[i].ulPropTag & 0xFFFF0000) > 0x80000000) {
			propID = props->lpProps[i].ulPropTag;
			propID = (propID & 0xFFFF0000) | PT_NULL;
			nameid = talloc_zero(mem_ctx, struct MAPINAMEID);
			retval = GetNamesFromIDs(obj, (enum MAPITAGS)propID, &count, &nameid);
			if (retval != MAPI_E_SUCCESS) goto end;

			if (count) {
				/* Display property given its propID */
				switch (nameid->ulKind) {
				case MNID_ID:
				  props->lpProps[i].ulPropTag = (enum MAPITAGS)((nameid->kind.lid << 16) | 
										((int)props->lpProps[i].ulPropTag & 0x0000FFFF));
					break;
				case MNID_STRING:
					/* MNID_STRING named properties don't have propIDs */
					break;
				}
			}
			talloc_free(nameid);
		}
	}
end:
	talloc_free(mem_ctx);
}

_PUBLIC_ enum MAPISTATUS get_mapi_SPropValue_array_date_timeval(struct timeval *t,
								struct mapi_SPropValue_array *properties,
								uint32_t mapitag)
{
	const struct FILETIME	*filetime;
	NTTIME			time;
	
	filetime = (const struct FILETIME *) find_mapi_SPropValue_data(properties, mapitag);
	if (!filetime) {
		t = NULL;
		return MAPI_E_NOT_FOUND;
	}

	time = filetime->dwHighDateTime;
	time = time << 32;
	time |= filetime->dwLowDateTime;
	nttime_to_timeval(t, time);
	
	return MAPI_E_SUCCESS;	
}

_PUBLIC_ enum MAPISTATUS get_mapi_SPropValue_date_timeval(struct timeval *t, 
							  struct SPropValue lpProp)
{
	const struct FILETIME	*filetime;
	NTTIME			time;
	
	filetime = (const struct FILETIME *) get_SPropValue_data(&lpProp);
	if (!filetime) {
		t = NULL;
		return MAPI_E_NOT_FOUND;
	}

	time = filetime->dwHighDateTime;
	time = time << 32;
	time |= filetime->dwLowDateTime;
	nttime_to_timeval(t, time);
	
	return MAPI_E_SUCCESS;
}

_PUBLIC_ bool set_SPropValue_proptag_date_timeval(struct SPropValue *lpProps, enum MAPITAGS aulPropTag, const struct timeval *t) 
{
	struct FILETIME	filetime;
	NTTIME		time;
	
	time = timeval_to_nttime(t);

	filetime.dwLowDateTime = (time << 32) >> 32;
	filetime.dwHighDateTime = time >> 32;

	return set_SPropValue_proptag(lpProps, aulPropTag, &filetime);
}


/**
   \details Retrieve a RecurrencePattern structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with non-mapped
   reccurrence data

   \return Allocated RecurrencePattern structure on success,
   otherwise NULL

   \note Developers must free the allocated RecurrencePattern when
   finished.
 */
_PUBLIC_ struct RecurrencePattern *get_RecurrencePattern(TALLOC_CTX *mem_ctx, 
							 struct Binary_r *bin)
{
        struct RecurrencePattern	*RecurrencePattern = NULL;
        struct ndr_pull			*ndr;
        enum ndr_err_code		ndr_err_code;
	
        /* Sanity checks */
        if (!bin) return NULL;
        if (!bin->cb) return NULL;
        if (!bin->lpb) return NULL;

        ndr = talloc_zero(mem_ctx, struct ndr_pull);
        ndr->offset = 0;
        ndr->data = bin->lpb;
        ndr->data_size = bin->cb;

        ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
        RecurrencePattern = talloc_zero(mem_ctx, struct RecurrencePattern);
        ndr_err_code = ndr_pull_RecurrencePattern(ndr, NDR_SCALARS, RecurrencePattern);

        talloc_free(ndr);

        if (ndr_err_code != NDR_ERR_SUCCESS) {
                talloc_free(RecurrencePattern);
                return NULL;
        }
	
	/*Copy DeletedInstanceDates and ModifiedInstanceDates into memory*/ 
	RecurrencePattern->DeletedInstanceDates = (uint32_t *) talloc_memdup(mem_ctx, RecurrencePattern->DeletedInstanceDates, 
									     sizeof(uint32_t) * RecurrencePattern->DeletedInstanceCount);
							      
	RecurrencePattern->ModifiedInstanceDates = (uint32_t *) talloc_memdup(mem_ctx, RecurrencePattern->ModifiedInstanceDates, 
									      sizeof(uint32_t) * RecurrencePattern->ModifiedInstanceCount);
	
	/*Set reference to parent so arrays get free with RecurrencePattern struct*/
	RecurrencePattern->DeletedInstanceDates=talloc_reference(RecurrencePattern, RecurrencePattern->DeletedInstanceDates);
	RecurrencePattern->ModifiedInstanceDates=talloc_reference(RecurrencePattern, RecurrencePattern->ModifiedInstanceDates);

        return RecurrencePattern;
}

_PUBLIC_ size_t set_RecurrencePattern_size(const struct RecurrencePattern *rp)
{
        size_t size = SIZE_DFLT_RECURRENCEPATTERN;

        switch (rp->PatternType) {
        case PatternType_MonthNth:
        case PatternType_HjMonthNth:
                size += sizeof(uint32_t);
		break;
        case PatternType_Week:
        case PatternType_Month:
        case PatternType_MonthEnd:
        case PatternType_HjMonth:
        case PatternType_HjMonthEnd:
                size += sizeof(uint32_t);
		break;
        case PatternType_Day:
                break;
        default:
                OC_DEBUG(0, "unrecognized pattern type: %d", rp->PatternType);
        }

        size += rp->DeletedInstanceCount * sizeof(uint32_t);
        size += rp->ModifiedInstanceCount * sizeof(uint32_t);

        return size;
}

_PUBLIC_ struct Binary_r *set_RecurrencePattern(TALLOC_CTX *mem_ctx, const struct RecurrencePattern *rp)
{
        struct Binary_r                 *bin = NULL;
        struct ndr_push			*ndr;
        enum ndr_err_code		ndr_err_code;
        size_t                          bin_size;
	
        /* SANITY CHECKS */
        if (!rp) return NULL;

        bin_size = set_RecurrencePattern_size(rp);
        bin = talloc_zero(mem_ctx, struct Binary_r);
        bin->cb = bin_size;
        bin->lpb = talloc_array(bin, uint8_t, bin_size);

        ndr = talloc_zero(mem_ctx, struct ndr_push);
        ndr->offset = 0;
        ndr->data = bin->lpb;

        ndr_err_code = ndr_push_RecurrencePattern(ndr, NDR_SCALARS, rp);

        talloc_free(ndr);

        if (ndr_err_code != NDR_ERR_SUCCESS) {
                talloc_free(bin);
                return NULL;
        }

        return bin;
}

_PUBLIC_ struct AppointmentRecurrencePattern *get_AppointmentRecurrencePattern(TALLOC_CTX *mem_ctx, 
									       struct Binary_r *bin)
{
        struct AppointmentRecurrencePattern		*arp = NULL;
        struct ndr_pull					*ndr;
        enum ndr_err_code				ndr_err_code;

        /* Sanity checks */
        if (!bin) return NULL;
        if (!bin->cb) return NULL;
        if (!bin->lpb) return NULL;

        ndr = talloc_zero(mem_ctx, struct ndr_pull);
        ndr->offset = 0;
        ndr->data = bin->lpb;
        ndr->data_size = bin->cb;

        ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
        arp = talloc_zero(mem_ctx, struct AppointmentRecurrencePattern);
        ndr_err_code = ndr_pull_AppointmentRecurrencePattern(ndr, NDR_SCALARS, arp);

        talloc_free(ndr);

        if (ndr_err_code != NDR_ERR_SUCCESS) {
                talloc_free(arp);
                return NULL;
        }

	/* Copy ExceptionInfo array into memory */ 
	arp->ExceptionInfo = (struct ExceptionInfo *) talloc_memdup(mem_ctx,arp->ExceptionInfo, sizeof(struct ExceptionInfo) * arp->ExceptionCount);
	
	/* Copy DeletedInstanceDates and ModifiedInstanceDates into memory */ 
	arp->RecurrencePattern.DeletedInstanceDates = (uint32_t *) talloc_memdup(mem_ctx, arp->RecurrencePattern.DeletedInstanceDates, 
										 sizeof(uint32_t) * arp->RecurrencePattern.DeletedInstanceCount);
							      
	arp->RecurrencePattern.ModifiedInstanceDates = (uint32_t *) talloc_memdup(mem_ctx, arp->RecurrencePattern.ModifiedInstanceDates, 
										  sizeof(uint32_t) * arp->RecurrencePattern.ModifiedInstanceCount);
	
	/* Set reference to parent so arrays get free with rest */
	arp->ExceptionInfo = talloc_reference(arp, arp->ExceptionInfo);
	arp->RecurrencePattern.DeletedInstanceDates = talloc_reference(arp,arp->RecurrencePattern.DeletedInstanceDates);
	arp->RecurrencePattern.ModifiedInstanceDates = talloc_reference(arp, arp->RecurrencePattern.ModifiedInstanceDates);
	
        return arp;
}

_PUBLIC_ size_t set_AppointmentRecurrencePattern_size(const struct AppointmentRecurrencePattern *arp)
{
        size_t size = SIZE_DFLT_APPOINTMENTRECURRENCEPATTERN;
        uint16_t i;

        size += set_RecurrencePattern_size(&arp->RecurrencePattern);
        for (i = 0; i < arp->ExceptionCount; i++) {
                size += set_ExceptionInfo_size(arp->ExceptionInfo + i);
                size += set_ExtendedException_size(arp->WriterVersion2, (arp->ExceptionInfo + i)->OverrideFlags, arp->ExtendedException + i);
        }
        size += arp->ReservedBlock1Size * sizeof(uint32_t);
	size += arp->ReservedBlock2Size * sizeof(uint32_t);

        return size;
}

_PUBLIC_ struct Binary_r *set_AppointmentRecurrencePattern(TALLOC_CTX *mem_ctx, const struct AppointmentRecurrencePattern *arp)
{
        struct Binary_r                 *bin = NULL;
        struct ndr_push			*ndr;
        enum ndr_err_code		ndr_err_code;
	TALLOC_CTX			*local_mem_ctx;
	
        /* SANITY CHECKS */
        if (!arp) return NULL;

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	ndr = ndr_push_init_ctx(local_mem_ctx);
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	ndr->offset = 0;

        ndr_err_code = ndr_push_AppointmentRecurrencePattern(ndr, NDR_SCALARS, arp);
        if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(local_mem_ctx);
                return NULL;
        }

        bin = talloc_zero(mem_ctx, struct Binary_r);
        bin->cb = ndr->offset;
        bin->lpb = ndr->data;
        (void) talloc_reference(bin, bin->lpb);

        talloc_free(local_mem_ctx);

        return bin;
}

_PUBLIC_ size_t set_ExceptionInfo_size(const struct ExceptionInfo *exc_info)
{
        size_t size = SIZE_DFLT_EXCEPTIONINFO;

        if ((exc_info->OverrideFlags & ARO_SUBJECT)) {
                size += 3 * sizeof(uint16_t); /* SubjectLength + SubjectLength2 + Subject */
		size += exc_info->Subject.subjectMsg.msgLength2;
        }
        if ((exc_info->OverrideFlags & ARO_MEETINGTYPE)) {
                size += sizeof(uint32_t);
        }
        if ((exc_info->OverrideFlags & ARO_REMINDERDELTA)) {
                size += sizeof(uint32_t);
        }
        if ((exc_info->OverrideFlags & ARO_REMINDER)) {
                size += sizeof(uint32_t);
        }
        if ((exc_info->OverrideFlags & ARO_LOCATION)) {
                size += 2 * sizeof(uint16_t); /* LocationLength + LocationLength2 */
		size += exc_info->Location.locationMsg.msgLength2;
        }
        if ((exc_info->OverrideFlags & ARO_BUSYSTATUS)) {
                size += sizeof(uint32_t);
        }
        if ((exc_info->OverrideFlags & ARO_ATTACHMENT)) {
                size += sizeof(uint32_t);
        }
        if ((exc_info->OverrideFlags & ARO_SUBTYPE)) {
                size += sizeof(uint32_t);
        }
	/* size += exc_info->ReservedBlock1Size; */
        /* if ((exc_info->OverrideFlags & ARO_APPTCOLOR)) { */
        /*         size += sizeof(uint32_t); */
        /* } */

        return size;
}

size_t set_ExtendedException_size(uint32_t WriterVersion2, enum OverrideFlags flags, const struct ExtendedException *ext_exc)
{
        size_t size = SIZE_DFLT_EXTENDEDEXCEPTION;
	bool subject_or_location;

	subject_or_location = (flags & (ARO_SUBJECT | ARO_LOCATION));

	if (WriterVersion2 > 0x00003008) {
		size += ext_exc->ChangeHighlight.Size;
	}

        if (subject_or_location) {
		size += 3 * sizeof(uint32_t); // StartDateTime + EndDateTime + OriginalStartDate */
		size += sizeof(uint32_t) + ext_exc->ReservedBlockEE2Size;
        }

        if ((flags & ARO_SUBJECT)) {
                size += sizeof(uint16_t) + strlen_m_ext(ext_exc->Subject, CH_UTF8, CH_UTF16LE) * 2;
        }
        if ((flags & ARO_LOCATION)) {
                size += sizeof(uint16_t) + strlen_m_ext(ext_exc->Location, CH_UTF8, CH_UTF16LE) * 2;
        }

        return size;
}

/* TODO: the get_XXX NDR wrapper should be normalized */

/**
   \details Retrieve a TimeZoneStruct structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw
   TimeZoneStruct data

   \return Allocated TimeZoneStruct structure on success, otherwise
   NULL

   \note Developers must free the allocated TimeZoneStruct when
   finished.
 */
_PUBLIC_ struct TimeZoneStruct *get_TimeZoneStruct(TALLOC_CTX *mem_ctx, 
						   struct Binary_r *bin)
{
	struct TimeZoneStruct	*TimeZoneStruct = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	TimeZoneStruct = talloc_zero(mem_ctx, struct TimeZoneStruct);
	ndr_err_code = ndr_pull_TimeZoneStruct(ndr, NDR_SCALARS, TimeZoneStruct);

	talloc_free(ndr);
	
	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(TimeZoneStruct);
		return NULL;
	}

	return TimeZoneStruct;
}

_PUBLIC_ struct Binary_r *set_TimeZoneStruct(TALLOC_CTX *mem_ctx, const struct TimeZoneStruct *TimeZoneStruct)
{
        struct Binary_r                 *bin = NULL;
        struct ndr_push			*ndr;
        enum ndr_err_code		ndr_err_code;
	
        /* SANITY CHECKS */
        if (!TimeZoneStruct) return NULL;

        ndr = talloc_zero(mem_ctx, struct ndr_push);
        ndr_err_code = ndr_push_TimeZoneStruct(ndr, NDR_SCALARS, TimeZoneStruct);
        if (ndr_err_code != NDR_ERR_SUCCESS) {
		goto end;
        }

        bin = talloc_zero(mem_ctx, struct Binary_r);
        bin->cb = ndr->offset;
        bin->lpb = ndr->data;
	(void) talloc_reference(bin, bin->lpb);

end:
        talloc_free(ndr);

        return bin;
}

_PUBLIC_ struct Binary_r *set_TimeZoneDefinition(TALLOC_CTX *mem_ctx, const struct TimeZoneDefinition *TimeZoneDefinition)
{
        struct Binary_r                 *bin = NULL;
        struct ndr_push			*ndr;
        enum ndr_err_code		ndr_err_code;
	
        /* SANITY CHECKS */
        if (!TimeZoneDefinition) return NULL;

        ndr = talloc_zero(mem_ctx, struct ndr_push);
        ndr_err_code = ndr_push_TimeZoneDefinition(ndr, NDR_SCALARS, TimeZoneDefinition);
        if (ndr_err_code != NDR_ERR_SUCCESS) {
		goto end;
        }

        bin = talloc_zero(mem_ctx, struct Binary_r);
        bin->cb = ndr->offset;
        bin->lpb = ndr->data;
	(void) talloc_reference(bin, bin->lpb);

end:
        talloc_free(ndr);

        return bin;
}


/**
   \details Retrieve a PtypServerId structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw PtypServerId data

   \return Allocated PtypServerId structure on success, otherwise
   NULL

   \note Developers must free the allocated PtypServerId when
   finished.
 */
_PUBLIC_ struct PtypServerId *get_PtypServerId(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct PtypServerId	*PtypServerId = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	PtypServerId = talloc_zero(mem_ctx, struct PtypServerId);
	ndr_err_code = ndr_pull_PtypServerId(ndr, NDR_SCALARS, PtypServerId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(PtypServerId);
		return NULL;
	}

	return PtypServerId;
}

/**
   \details Retrieve a GlobalObjectId structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw
   GlobalObjectId data

   \return Allocated GlobalObjectId structure on success, otherwise
   NULL

   \note Developers must free the allocated GlobalObjectId when
   finished.
 */
_PUBLIC_ struct GlobalObjectId *get_GlobalObjectId(TALLOC_CTX *mem_ctx,
						   struct Binary_r *bin)
{
	struct GlobalObjectId	*GlobalObjectId = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	GlobalObjectId = talloc_zero(mem_ctx, struct GlobalObjectId);
	ndr_err_code = ndr_pull_GlobalObjectId(ndr, NDR_SCALARS, GlobalObjectId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(GlobalObjectId);
		return NULL;
	}

	return GlobalObjectId;
}

/**
   \details Retrieve a MessageEntryId structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw
   MessageEntryId data

   \return Allocated MessageEntryId structure on success, otherwise
   NULL

   \note Developers must free the allocated MessageEntryId when
   finished.
 */
_PUBLIC_ struct MessageEntryId *get_MessageEntryId(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct MessageEntryId	*MessageEntryId = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	MessageEntryId = talloc_zero(mem_ctx, struct MessageEntryId);
	ndr_err_code = ndr_pull_MessageEntryId(ndr, NDR_SCALARS, MessageEntryId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(MessageEntryId);
		return NULL;
	}

	return MessageEntryId;
}

/**
   \details Retrieve a FolderEntryId structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw FolderEntryId data

   \return Allocated FolderEntryId structure on success, otherwise
   NULL

   \note Developers must free the allocated FolderEntryId when
   finished.
 */
_PUBLIC_ struct FolderEntryId *get_FolderEntryId(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct FolderEntryId	*FolderEntryId = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	FolderEntryId = talloc_zero(mem_ctx, struct FolderEntryId);
	ndr_err_code = ndr_pull_FolderEntryId(ndr, NDR_SCALARS, FolderEntryId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(FolderEntryId);
		return NULL;
	}

	return FolderEntryId;
}

/**
   \details Retrieve a AddressBookEntryId structure from a binary blob

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw AddressBookEntryId data

   \return Allocated AddressBookEntryId structure on success, otherwise NULL

   \note Developers must free the allocated AddressBookEntryId when finished.
 */
_PUBLIC_ struct AddressBookEntryId *get_AddressBookEntryId(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct AddressBookEntryId	*AddressBookEntryId = NULL;
	struct ndr_pull			*ndr;
	enum ndr_err_code		ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	AddressBookEntryId = talloc_zero(mem_ctx, struct AddressBookEntryId);
	if (!AddressBookEntryId) return NULL;
	ndr_err_code = ndr_pull_AddressBookEntryId(ndr, NDR_SCALARS, AddressBookEntryId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(AddressBookEntryId);
		return NULL;
	}

	return AddressBookEntryId;
}

/**
   \details Retrieve a OneOffEntryId structure from a binary blob. This structure is meant
   to represent recipients that do not exist in the directory.

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw OneOffEntryId data

   \return Allocated OneOffEntryId structure on success, otherwise NULL

   \note Developers must free the allocated OneOffEntryId when finished.
 */
_PUBLIC_ struct OneOffEntryId *get_OneOffEntryId(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct OneOffEntryId	*OneOffEntryId = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	OneOffEntryId = talloc_zero(mem_ctx, struct OneOffEntryId);
	if (!OneOffEntryId) return NULL;
	ndr_err_code = ndr_pull_OneOffEntryId(ndr, NDR_SCALARS, OneOffEntryId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(OneOffEntryId);
		return NULL;
	}

	return OneOffEntryId;
}

/**
   \details Retrieve a PersistData structure from a binary blob. This structure is meant
   to contain the entryID of a special folder and other data related to a special folder.

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw PersistData data

   \return Allocated PersistData structure on success, otherwise NULL

   \note Developers must free the allocated PersistData when finished.
 */
_PUBLIC_ struct PersistData *get_PersistData(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct PersistData	*PersistData = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	PersistData = talloc_zero(mem_ctx, struct PersistData);
	if (!PersistData) return NULL;
	ndr_err_code = ndr_pull_PersistData(ndr, NDR_SCALARS, PersistData);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(PersistData);
		return NULL;
	}

	return PersistData;
}

/**
   \details Retrieve a PersistData structure array from a binary blob. This structure is meant
   to contain entryIDs from special folders and other data related to special folders.

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with a raw PersistData array

   \return Allocated PersistDataArray on success, otherwise NULL

   \note Developers must free the allocated PersistDataArray when finished.
 */
_PUBLIC_ struct PersistDataArray *get_PersistDataArray(TALLOC_CTX *mem_ctx, struct Binary_r *bin)
{
	struct PersistDataArray	*PersistDataArray = NULL;
	struct ndr_pull		*ndr;
	enum ndr_err_code	ndr_err_code;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	PersistDataArray = talloc_zero(mem_ctx, struct PersistDataArray);
	if (!PersistDataArray) return NULL;
	ndr_err_code = ndr_pull_PersistDataArray(ndr, NDR_SCALARS|NDR_BUFFERS, PersistDataArray);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(PersistDataArray);
		return NULL;
	}

	return PersistDataArray;
}

/**
   \details Retrieve a SizedXid structure array from a binary blob. This structure is meant
   to store the PredecessorChangeList property.

   \param mem_ctx pointer to the memory context
   \param bin pointer to the Binary_r structure with raw PredecessorChangeList property value
   \param count_p pointer to the counter where the number of SizedXID structures are stored

   \return Allocated SizedXID array structure on success, otherwise NULL

   \note Developers must free the allocated SizedXID array when finished.
 */
_PUBLIC_ struct SizedXid *get_SizedXidArray(TALLOC_CTX *mem_ctx, struct Binary_r *bin, uint32_t *count_p)
{
	enum ndr_err_code	ndr_err_code;
	struct ndr_pull		*ndr;
	struct SizedXid		*sized_xid_list, sized_xid;
	uint32_t		count;

	/* Sanity checks */
	if (!bin) return NULL;
	if (!bin->cb) return NULL;
	if (!bin->lpb) return NULL;

	ndr = talloc_zero(mem_ctx, struct ndr_pull);
	if (!ndr) return NULL;

	count = 0;
	sized_xid_list = talloc_zero_array(mem_ctx, struct SizedXid, 1);
	if (!sized_xid_list) {
		talloc_free(ndr);
		return NULL;
	}
	ndr->offset = 0;
	ndr->data = bin->lpb;
	ndr->data_size = bin->cb;

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
	while (ndr->offset < ndr->data_size) {
		ndr_err_code = ndr_pull_SizedXid(ndr, NDR_SCALARS, &sized_xid);
		if (ndr_err_code != NDR_ERR_SUCCESS) goto error;

		memcpy(&sized_xid_list[count], &sized_xid, sizeof(struct SizedXid));
		count++;

		sized_xid_list = talloc_realloc(mem_ctx, sized_xid_list, struct SizedXid, count + 1);
		if (!sized_xid_list) goto error;
	}
	talloc_free(ndr);

	if (count_p) {
		*count_p = count;
	}
	return sized_xid_list;

error:
	talloc_free(sized_xid_list);
	talloc_free(ndr);
	return NULL;
}

/**
   \details Return the effective value used in a TypedString
   structure.

   \param tstring pointer to TypedString structure

   \return pointer to a valid string on success, otherwise NULL
 */
_PUBLIC_ const char *get_TypedString(struct TypedString *tstring)
{
	if (!tstring) return NULL;

	switch (tstring->StringType) {
	case StringType_STRING8:
		return tstring->String.lpszA;
	case StringType_UNICODE_REDUCED:
		return tstring->String.lpszW_reduced;
	case StringType_UNICODE:
		return tstring->String.lpszW;
	case StringType_NONE:
	case StringType_EMPTY:
	default:
		return NULL;
	}

	return NULL;
}

/**
   \details Return the expected size of the utf8 string after
   conversion to utf16 by iconv() function.

   \param inbuf pointer to the input string

   \return expected length of the converted string

   \note This routine is based upon utf8_pull() function from
   samba4/lib/util/charset/iconv.c
 */
size_t get_utf8_utf16_conv_length(const char *inbuf)
{
	size_t		in_left;
	size_t		out_left;
	size_t		max_out;
	const uint8_t	*c = (const uint8_t *) inbuf;

	/* Sanity checks */
	if (!inbuf) return 0;

	in_left = strlen(inbuf);
	out_left = in_left;
	out_left = ( out_left * 3);
	/* includes null-termination bytes */
	max_out = out_left + 2;

	while (in_left >= 1 && out_left >= 2) {
		if ((c[0] & 0x80) == 0) {
			c += 1;
			in_left -= 1;
			out_left -= 2;
			continue;
		}

		if ((c[0] & 0xe0) == 0xc0) {
			if (in_left < 2 || (c[1] & 0xc0) != 0x80) {
				return -1;
			}
			c += 2;
			in_left -= 2;
			out_left -= 2;
			continue;
		}

		if ((c[0] & 0xf0) == 0xe0) {
			if (in_left < 3 ||
			    (c[1] & 0xc0) != 0x80 ||
			    (c[2] & 0xc0) != 0x80) {
				return -1;
			}
			c += 3;
			in_left -= 3;
			out_left -= 2;
			continue;
		}

		if ((c[0] & 0xf8) == 0xf0) {
			unsigned int codepoint;
			if (in_left < 4 ||
			    (c[1] & 0xc0) != 0x80 ||
			    (c[2] & 0xc0) != 0x80 ||
			    (c[3] & 0xc0) != 0x80) {
				return -1;
			}
			codepoint = 
				(c[3]&0x3f) | 
				((c[2]&0x3f)<<6) | 
				((c[1]&0x3f)<<12) |
				((c[0]&0x7)<<18);
			if (codepoint < 0x10000) {
				c += 4;
				in_left -= 4;
				out_left -= 2;
				continue;
			}

			codepoint -= 0x10000;

			if (out_left < 4) {
				return -1;
			}

			c += 4;
			in_left -= 4;
			out_left -= 4;
			continue;
		}
		
		/* we don't handle 5 byte sequences */
		return -1;
	}

	if (in_left > 0) {
		return -1;
	}

	return (max_out - out_left);
}

struct PropertyValue_r* get_PropertyValue_PropertyRow(struct PropertyRow_r *aRow, enum MAPITAGS ulPropTag)
{
	uint32_t	i;

	if (!aRow) {
		return NULL;
	}

	for (i = 0; i < aRow->cValues; i++) {
		if (ulPropTag == aRow->lpProps[i].ulPropTag) {
			return (&aRow->lpProps[i]);
		}
	}

	return NULL;
}

_PUBLIC_ struct PropertyValue_r *get_PropertyValue_PropertyRowSet(struct PropertyRowSet_r *RowSet, 
								  enum MAPITAGS ulPropTag)
{
	uint32_t	i;
	uint32_t	j;

	/* Sanity Checks */
	if (!RowSet) return NULL;

	for (i = 0; i != RowSet->cRows; i++) {
		for (j = 0; j < RowSet->aRow[i].cValues; j++) {
			if (ulPropTag == RowSet->aRow[i].lpProps[j].ulPropTag) {
				return (&RowSet->aRow[i].lpProps[j]);
			}
		}
	}

	return NULL;
}

_PUBLIC_ const void *get_PropertyValue_PropertyRowSet_data(struct PropertyRowSet_r *RowSet,
							   uint32_t ulPropTag)
{
	struct PropertyValue_r *lpProp;

	lpProp = get_PropertyValue_PropertyRowSet(RowSet, ulPropTag);
	return get_PropertyValue(lpProp, ulPropTag);
}

_PUBLIC_ bool set_PropertyValue(struct PropertyValue_r *lpProp, const void *data)
{
	if (data == NULL) {
		lpProp->ulPropTag = (lpProp->ulPropTag & 0xffff0000) | PT_ERROR;
		lpProp->value.err = MAPI_E_NOT_FOUND;
		return false;
	}
	switch (lpProp->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		lpProp->value.i = *((const uint16_t *)data);
		break;
	case PT_BOOLEAN:
		lpProp->value.b = *((const uint8_t *)data);
		break;
	case PT_LONG:
		lpProp->value.l = *((const uint32_t *)data);
		break;
	case PT_STRING8:
		lpProp->value.lpszA = (const char *) data;
		break;
	case PT_BINARY:
	case PT_SVREID:
		lpProp->value.bin = *((const struct Binary_r *)data);
		break;
	case PT_UNICODE:
		lpProp->value.lpszW = (const char *) data;
		break;
	case PT_CLSID:
		lpProp->value.lpguid = (struct FlatUID_r *) data;
		break;
	case PT_SYSTIME:
		lpProp->value.ft = *((const struct FILETIME *) data);
		break;
	case PT_ERROR:
		lpProp->value.err = *((enum MAPISTATUS *)data);
		break;
	case PT_MV_SHORT:
		lpProp->value.MVi = *((const struct ShortArray_r *)data);
		break;
	case PT_MV_LONG:
		lpProp->value.MVl = *((const struct LongArray_r *)data);
		break;
	case PT_MV_STRING8:
		lpProp->value.MVszA = *((const struct StringArray_r *)data);
		break;
	case PT_MV_BINARY:
		lpProp->value.MVbin = *((const struct BinaryArray_r *)data);
		break;
	case PT_MV_CLSID:
		lpProp->value.MVguid = *((const struct FlatUIDArray_r *)data);
		break;
	case PT_MV_UNICODE:
		lpProp->value.MVszW = *((const struct StringArrayW_r *)data);
		break;
	case PT_MV_SYSTIME:
		lpProp->value.MVft = *((const struct DateTimeArray_r *)data);
		break;
	case PT_NULL:
		lpProp->value.null = *((const uint32_t *)data);
		break;
	default:
		lpProp->ulPropTag = (lpProp->ulPropTag & 0xffff0000) | PT_ERROR;
		lpProp->value.err = MAPI_E_NOT_FOUND;

		return false;
	}

	return true;
}

_PUBLIC_ const void *get_PropertyValue(struct PropertyValue_r *lpProps, enum MAPITAGS ulPropTag)
{
	uint32_t	i;

	/* Sanity checks */
	if (!lpProps) return NULL;

	for (i = 0; lpProps[i].ulPropTag; i++) {
		if (ulPropTag == lpProps[i].ulPropTag) {
			return get_PropertyValue_data(&lpProps[i]);
		}
	}
	return NULL;
}

_PUBLIC_ const void *get_PropertyValue_data(struct PropertyValue_r *lpProps)
{
	if (lpProps->ulPropTag == 0) {
		return NULL;
	}

	switch(lpProps->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		return (const void *)&lpProps->value.i;
	case PT_BOOLEAN:
		return (const void *)&lpProps->value.b;
	case PT_STRING8:
		return (const void *)lpProps->value.lpszA;
	case PT_UNICODE:
		return (const void *)lpProps->value.lpszW;
	case PT_SYSTIME:
		return (const void *)(struct FILETIME *)&lpProps->value.ft;
	case PT_ERROR:
		return (const void *)&lpProps->value.err;
	case PT_LONG:
		return (const void *)&lpProps->value.l;
	case PT_CLSID:
		return (const void *)lpProps->value.lpguid;
	case PT_BINARY:
	case PT_SVREID:
		return (const void *)&lpProps->value.bin;
	case PT_MV_SHORT:
		return (const void *)(struct ShortArray_r *)&lpProps->value.MVi;
	case PT_MV_LONG:
		return (const void *)(struct LongArray_r *)&lpProps->value.MVl;
	case PT_MV_STRING8:
		return (const void *)(struct StringArray_r *)&lpProps->value.MVszA;
	case PT_MV_UNICODE:
		return (const void *)(struct StringArrayW_r *)&lpProps->value.MVszW;
	case PT_MV_BINARY:
		return (const void *)(struct BinaryArray_r *)&lpProps->value.MVbin;
	case PT_MV_SYSTIME:
		return (const void *)(struct DateTimeArray_r *)&lpProps->value.MVft;
	case PT_NULL:
		return (const void *)&lpProps->value.null;
	default:
		return NULL;
	}
}

/**
   \details add a PropertyValue_r structure to a PropertyRow_r array

   \param aRow pointer to the PropertyRow_r array where propValue should be appended
   \param propValue the PropertyValue_r structure to add to aRow

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_INVALID_PARAMETER.
 */
_PUBLIC_ enum MAPISTATUS PropertyRow_addprop(struct PropertyRow_r *aRow, struct PropertyValue_r propValue)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		cValues;
	struct PropertyValue_r	lpProp;
	uint32_t		i;
	
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!aRow, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *) aRow;

	/* If the property tag already exist, overwrite its value */
	for (i = 0; i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == propValue.ulPropTag) {
			aRow->lpProps[i] = propValue;
			return MAPI_E_SUCCESS;
		}
	}

	cValues = aRow->cValues + 1;
	aRow->lpProps = talloc_realloc(mem_ctx, aRow->lpProps, struct PropertyValue_r, cValues);
	lpProp = aRow->lpProps[cValues-1];
	lpProp.ulPropTag = propValue.ulPropTag;
	lpProp.dwAlignPad = 0;
	set_PropertyValue(&(lpProp), get_PropertyValue_data(&propValue));
	aRow->cValues = cValues;
	aRow->lpProps[cValues - 1] = lpProp;

	return MAPI_E_SUCCESS;
}

_PUBLIC_ const void *find_PropertyValue_data(struct PropertyRow_r *aRow, uint32_t mapitag)
{
	uint32_t i;

	if (!aRow) {
		return NULL;
	}

	for (i = 0; i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == mapitag) {
			return get_PropertyValue_data(&(aRow->lpProps[i]));
		}
	}
	return NULL;
}

/**
   \details Append a PropertyValue_r structure to given PropertyRowSet_r

   \param mem_ctx pointer to the memory context
   \param RowSet pointer to the PropertyRowSet_r array to update
   \param value the PropertyValue_r to append within SRowSet

   \return 0 on success, otherwise 1
 */
_PUBLIC_ uint32_t PropertyRowSet_propcpy(TALLOC_CTX *mem_ctx, struct PropertyRowSet_r *RowSet, struct PropertyValue_r value)
{
	uint32_t		rows;
	uint32_t		cValues;
	struct PropertyValue_r	lpProp;

	/* Sanity checks */
	if (!RowSet) return 1;

	for (rows = 0; rows < RowSet->cRows; rows++) {
		cValues = RowSet->aRow[rows].cValues + 1;
		RowSet->aRow[rows].lpProps = talloc_realloc(mem_ctx, RowSet->aRow[rows].lpProps, struct PropertyValue_r, cValues);
		lpProp = RowSet->aRow[rows].lpProps[cValues-1];
		lpProp.ulPropTag = value.ulPropTag;
		lpProp.dwAlignPad = 0;
		set_PropertyValue(&(lpProp), (void *)&value.value);
		RowSet->aRow[rows].cValues = cValues;
		RowSet->aRow[rows].lpProps[cValues - 1] = lpProp;
	}
	return 0;
}

/**
   \details Convenience function to convert a PropertyValue_r structure into a SPropValue structure.

   \param propvalue pointer to the PropertyValue_r structure to copy data to
   \param spropvalue pointer to the SPropValue structure to copy data from
 */
_PUBLIC_ void cast_PropertyValue_to_SPropValue(struct PropertyValue_r *propvalue, struct SPropValue *spropvalue)
{
	spropvalue->ulPropTag = propvalue->ulPropTag;

	switch (propvalue->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		spropvalue->value.b = propvalue->value.b;
		break;
	case PT_I2:
		spropvalue->value.i = propvalue->value.i;
		break;
	case PT_LONG:
		spropvalue->value.l = propvalue->value.l;
		break;
	case PT_STRING8:
		spropvalue->value.lpszA = propvalue->value.lpszA;
		break;
	case PT_UNICODE:
		spropvalue->value.lpszW = propvalue->value.lpszW;
		break;
	case PT_SYSTIME:
		spropvalue->value.ft = propvalue->value.ft;
		break;
	case PT_CLSID:
		spropvalue->value.lpguid = propvalue->value.lpguid;
		break;
	case PT_SVREID:
	case PT_BINARY:
		spropvalue->value.bin = propvalue->value.bin;
		break;
        case PT_ERROR:
                spropvalue->value.err = propvalue->value.err;
		break;
	case PT_MV_LONG:
		spropvalue->value.MVl = propvalue->value.MVl;
		break;
	case PT_MV_STRING8:
		spropvalue->value.MVszA = propvalue->value.MVszA;
		break;
        case PT_MV_UNICODE:
		spropvalue->value.MVszW = propvalue->value.MVszW;
		break;
	case PT_MV_CLSID:
		spropvalue->value.MVguid = propvalue->value.MVguid;
		break;
	case PT_MV_BINARY:
		spropvalue->value.MVbin = propvalue->value.MVbin;
		break;
        default:
                printf("unhandled conversion case in cast_PropvalueValue(): 0x%x\n", (propvalue->ulPropTag & 0xFFFF));
                OPENCHANGE_ASSERT();
	}
}

/**
   \details Convenience function to convert a PropertyRow_r structure into a SRow structure.

   \param mem_ctx pointer to the allocation context for structure members
   \param proprow pointer to the PropertyRow_r structure to copy data to
   \param srow pointer to the SRow structure to copy data from
 */
_PUBLIC_ void cast_PropertyRow_to_SRow(TALLOC_CTX *mem_ctx, struct PropertyRow_r *proprow, struct SRow *srow)
{
	uint32_t i;

	srow->cValues = proprow->cValues;
	srow->lpProps = talloc_array(mem_ctx, struct SPropValue, srow->cValues);
	for (i = 0; i < srow->cValues; i++) {
		cast_PropertyValue_to_SPropValue(proprow->lpProps + i, srow->lpProps + i);
	}
}

/**
   \details Convenience function to convert a PropertyRowSet_r structure into a SRowSet structure.

   \param mem_ctx pointer to the allocation context for structure members
   \param prowset pointer to the PropertyRowSet_r structure to copy data to
   \param srowset pointer to the SRowSet structure to copy data from
 */
_PUBLIC_ void cast_PropertyRowSet_to_SRowSet(TALLOC_CTX *mem_ctx, struct PropertyRowSet_r *prowset, struct SRowSet *srowset)
{
	uint32_t	i;

	/* Sanity checks */
	if (!srowset || !prowset) return;

	srowset->cRows = prowset->cRows;
	srowset->aRow = talloc_array(mem_ctx, struct SRow, srowset->cRows);
	for (i = 0; i < srowset->cRows; i++) {
		cast_PropertyRow_to_SRow(mem_ctx, prowset->aRow + i, srowset->aRow + i);
	}
}
