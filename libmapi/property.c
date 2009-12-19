/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2008.
   Copyright (C) Gregory Schiro 2006

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
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
					   uint32_t aulPropTag)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!SPropTagArray->cValues, MAPI_E_INVALID_PARAMETER, NULL);

	SPropTagArray->cValues += 1;
	SPropTagArray->aulPropTag = (enum MAPITAGS *) talloc_realloc(mem_ctx, SPropTagArray->aulPropTag,
								     uint32_t, SPropTagArray->cValues + 1);
	SPropTagArray->aulPropTag[SPropTagArray->cValues - 1] = aulPropTag;
	SPropTagArray->aulPropTag[SPropTagArray->cValues] = 0;

	return MAPI_E_SUCCESS;
}

_PUBLIC_ const void *get_SPropValue(struct SPropValue *lpProps, 
				    uint32_t ulPropTag)
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

_PUBLIC_ enum MAPISTATUS set_default_error_SPropValue_SRow(struct SRow *aRow, uint32_t ulPropTag, void *data)
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

enum MAPITAGS *get_MAPITAGS_SRow(TALLOC_CTX *mem_ctx, struct SRow *aRow)
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
	mapitags[idx] = 0;
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
					mapitags[j] = (mapitags[j+1]) ? mapitags[j+1] : 0;
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

	for (i = 0; i < properties->cValues; i++) {
		if (properties->lpProps[i].ulPropTag == mapitag) {
			return get_mapi_SPropValue_data(&properties->lpProps[i]);
		}
	}
	return NULL;
}

_PUBLIC_ const void *get_mapi_SPropValue_data(struct mapi_SPropValue *lpProp)
{
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
		return (const void *)&lpProps->value.bin;
	case PT_MV_SHORT:
		return (const void *)(struct ShortArray_r *)&lpProps->value.MVi;
	case PT_MV_LONG:
		return (const void *)(struct LongArray_r *)&lpProps->value.MVl;
	case PT_MV_STRING8:
		return (const void *)(struct StringArray_r *)&lpProps->value.MVszA;
	case PT_MV_UNICODE:
		return (const void *)(struct WStringArray_r *)&lpProps->value.MVszW;
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

_PUBLIC_ bool set_SPropValue_proptag(struct SPropValue *lpProps, uint32_t aulPropTag, const void *data)
{
	lpProps->ulPropTag = aulPropTag;
	lpProps->dwAlignPad = 0x0;

	return (set_SPropValue(lpProps, data));
}

_PUBLIC_ struct SPropValue *add_SPropValue(TALLOC_CTX *mem_ctx, struct SPropValue *lpProps, uint32_t *cValues, uint32_t aulPropTag, const void * data)
{
	lpProps = talloc_realloc(mem_ctx, lpProps, struct SPropValue, *cValues + 2);

	set_SPropValue_proptag(&lpProps[*cValues], aulPropTag, data);
	*cValues = *cValues + 1;

	return lpProps;
}
/*
  TODO: should this be public?
*/
_PUBLIC_ bool set_SPropValue(struct SPropValue *lpProps, const void *data)
{
	if (data == NULL) {
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
		lpProps->value.err = *((const uint32_t *)data);
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
		lpProps->value.MVszW = *((const struct WStringArray_r *)data);
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
	case PT_I8:
		return sizeof (uint64_t);
	case PT_STRING8:
		return strlen(lpProp->value.lpszA) + 1;
	case PT_UNICODE:
		return strlen(lpProp->value.lpszW) * 2 + 2;
	case PT_SYSTIME:
		return sizeof (struct FILETIME);
	case PT_BINARY:
		return (lpProp->value.bin.cb + sizeof(uint16_t));
	}
	return 0;
}

/*
  convenient function which cast a SPropValue structure in a mapi_SPropValue one and return the associated size
*/

_PUBLIC_ uint32_t cast_mapi_SPropValue(struct mapi_SPropValue *mapi_sprop, struct SPropValue *sprop)
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
		return sizeof(uint64_t);
	case PT_STRING8:
		mapi_sprop->value.lpszA = sprop->value.lpszA;
		if (!mapi_sprop->value.lpszA) return 0;
		return (strlen(sprop->value.lpszA) + 1);
	case PT_UNICODE:
		mapi_sprop->value.lpszW = sprop->value.lpszW;
		if (!mapi_sprop->value.lpszW) return 0;
		return (strlen(sprop->value.lpszW) * 2 + 2);
	case PT_SYSTIME:
		mapi_sprop->value.ft.dwLowDateTime = sprop->value.ft.dwLowDateTime;
		mapi_sprop->value.ft.dwHighDateTime = sprop->value.ft.dwHighDateTime;
		return (sizeof (struct FILETIME));
	case PT_BINARY:
		mapi_sprop->value.bin.cb = sprop->value.bin.cb;
		mapi_sprop->value.bin.lpb = sprop->value.bin.lpb;
		return (mapi_sprop->value.bin.cb + sizeof(uint16_t));
	case PT_MV_STRING8:
		{
			uint32_t	i;
			uint32_t	size = 0;

			mapi_sprop->value.MVszA.cValues = sprop->value.MVszA.cValues;
			size += 4;

			mapi_sprop->value.MVszA.strings = talloc_array(global_mapi_ctx->mem_ctx, struct mapi_LPSTR, mapi_sprop->value.MVszA.cValues);
			for (i = 0; i < mapi_sprop->value.MVszA.cValues; i++) {
				mapi_sprop->value.MVszA.strings[i].lppszA = sprop->value.MVszA.lppszA[i];
				size += strlen(mapi_sprop->value.MVszA.strings[i].lppszA) + 1;
			}
			return size;
		}
	}
	return 0;

}

/*
 *
 */
_PUBLIC_ uint32_t cast_SPropValue(struct mapi_SPropValue *mapi_sprop, struct SPropValue *sprop)
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
		return sizeof(uint64_t);
	case PT_STRING8:
		sprop->value.lpszA = mapi_sprop->value.lpszA;
		if (!mapi_sprop->value.lpszA) return 0;
		return (strlen(sprop->value.lpszA) + 1);
	case PT_UNICODE:
		sprop->value.lpszW = mapi_sprop->value.lpszW;
		if (!sprop->value.lpszW) return 0;
		return (strlen(mapi_sprop->value.lpszW) * 2 + 2);
	case PT_SYSTIME:
		sprop->value.ft.dwLowDateTime = mapi_sprop->value.ft.dwLowDateTime;
		sprop->value.ft.dwHighDateTime = mapi_sprop->value.ft.dwHighDateTime;
		return (sizeof (struct FILETIME));
	case PT_BINARY:
		sprop->value.bin.cb = mapi_sprop->value.bin.cb;
		sprop->value.bin.lpb = mapi_sprop->value.bin.lpb;
		return (sprop->value.bin.cb + sizeof(uint16_t));

	case PT_MV_STRING8:
		{
		uint32_t	i;
		uint32_t	size = 0;

		sprop->value.MVszA.cValues = mapi_sprop->value.MVszA.cValues;
		size += 4;

		sprop->value.MVszA.lppszA = talloc_array(global_mapi_ctx->mem_ctx, const char *, sprop->value.MVszA.cValues);
		for (i = 0; i < sprop->value.MVszA.cValues; i++) {
			sprop->value.MVszA.lppszA[i] = mapi_sprop->value.MVszA.strings[i].lppszA;
			size += strlen(sprop->value.MVszA.lppszA[i]) + 1;
		}
		return size;
		}
	}
	return 0;
}


/**
   \details add a SPropValue structure to a SRow array

   \param aRow pointer to the SRow array where SPropBalue should be
   appended
   \param SPropValue reference to the SPropValue structure to add to
   aRow

   \return MAPI_E_SUCCESS on success, otherwise
   MAPI_E_INVALID_PARAMETER.
 */
_PUBLIC_ enum MAPISTATUS SRow_addprop(struct SRow *aRow, struct SPropValue SPropValue)
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
		if (aRow->lpProps[i].ulPropTag == SPropValue.ulPropTag) {
			aRow->lpProps[i] = SPropValue;
			return MAPI_E_SUCCESS;
		}
	}

	cValues = aRow->cValues + 1;
	aRow->lpProps = talloc_realloc(mem_ctx, aRow->lpProps, struct SPropValue, cValues);
	lpProp = aRow->lpProps[cValues-1];
	lpProp.ulPropTag = SPropValue.ulPropTag;
	lpProp.dwAlignPad = 0;
	set_SPropValue(&(lpProp), get_SPropValue_data(&SPropValue));
	aRow->cValues = cValues;
	aRow->lpProps[cValues - 1] = lpProp;

	return MAPI_E_SUCCESS;
}


/**
   \details Append a SPropValue structure to given SRowSet

   \param mem_ctx pointer to the memory context
   \param SRowSet pointer to the SRowSet array to update
   \param SPropValue the SPropValue to append within SRowSet

   \return 0 on success, otherwise 1
 */
_PUBLIC_ uint32_t SRowSet_propcpy(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, struct SPropValue SPropValue)
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
		lpProp.ulPropTag = SPropValue.ulPropTag;
		lpProp.dwAlignPad = 0;
		set_SPropValue(&(lpProp), (void *)&SPropValue.value);
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

	mem_ctx = talloc_named(NULL, 0, "mapi_SPropValue_array_named");

	for (i = 0; i < props->cValues; i++) {
		if ((props->lpProps[i].ulPropTag & 0xFFFF0000) > 0x80000000) {
			propID = props->lpProps[i].ulPropTag;
			propID = (propID & 0xFFFF0000) | PT_NULL;
			nameid = talloc_zero(mem_ctx, struct MAPINAMEID);
			retval = GetNamesFromIDs(obj, propID, &count, &nameid);
			if (retval != MAPI_E_SUCCESS) goto end;

			if (count) {
				/* Display property given its propID */
				switch (nameid->ulKind) {
				case MNID_ID:
					props->lpProps[i].ulPropTag = (nameid->kind.lid << 16) | 
						(props->lpProps[i].ulPropTag & 0x0000FFFF);
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

_PUBLIC_ bool set_SPropValue_proptag_date_timeval(struct SPropValue *lpProps, uint32_t aulPropTag, const struct timeval *t) 
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
        ndr->iconv_convenience = smb_iconv_convenience_init(mem_ctx, "CP850", "UTF8", true);
        RecurrencePattern = talloc_zero(mem_ctx, struct RecurrencePattern);
        ndr_err_code = ndr_pull_RecurrencePattern(ndr, NDR_SCALARS, RecurrencePattern);

        talloc_free(ndr);

        if (ndr_err_code != NDR_ERR_SUCCESS) {
                talloc_free(RecurrencePattern);
                return NULL;
        }
	
	/*Copy DeletedInstanceDates and ModifiedInstanceDates into memory*/ 
	RecurrencePattern->DeletedInstanceDates=talloc_memdup(mem_ctx, RecurrencePattern->DeletedInstanceDates, 
							      sizeof(uint32_t) * RecurrencePattern->DeletedInstanceCount);
							      
	RecurrencePattern->ModifiedInstanceDates=talloc_memdup(mem_ctx, RecurrencePattern->ModifiedInstanceDates, 
							      sizeof(uint32_t) * RecurrencePattern->ModifiedInstanceCount);
	
	/*Set reference to parent so arrays get free with RecurrencePattern struct*/
	RecurrencePattern->DeletedInstanceDates=talloc_reference(RecurrencePattern, RecurrencePattern->DeletedInstanceDates);
	RecurrencePattern->ModifiedInstanceDates=talloc_reference(RecurrencePattern, RecurrencePattern->ModifiedInstanceDates);

        return RecurrencePattern;
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
        ndr->iconv_convenience = smb_iconv_convenience_init(mem_ctx, "CP850", "UTF8", true);
        arp = talloc_zero(mem_ctx, struct AppointmentRecurrencePattern);
        ndr_err_code = ndr_pull_AppointmentRecurrencePattern(ndr, NDR_SCALARS, arp);

        talloc_free(ndr);

        if (ndr_err_code != NDR_ERR_SUCCESS) {
                talloc_free(arp);
                return NULL;
        }

	/*Copy ExceptionInfo array into memory*/ 
	arp->ExceptionInfo=talloc_memdup(mem_ctx,arp->ExceptionInfo, sizeof(struct ExceptionInfo) * arp->ExceptionCount);
	
	/*Copy DeletedInstanceDates and ModifiedInstanceDates into memory*/ 
	arp->RecurrencePattern.DeletedInstanceDates=talloc_memdup(mem_ctx, arp->RecurrencePattern.DeletedInstanceDates, 
							      sizeof(uint32_t) * arp->RecurrencePattern.DeletedInstanceCount);
							      
	arp->RecurrencePattern.ModifiedInstanceDates=talloc_memdup(mem_ctx, arp->RecurrencePattern.ModifiedInstanceDates, 
							      sizeof(uint32_t) * arp->RecurrencePattern.ModifiedInstanceCount);
	
	/*Set reference to parent so arrays get free with rest*/
	arp->ExceptionInfo = talloc_reference(arp, arp->ExceptionInfo);
	arp->RecurrencePattern.DeletedInstanceDates = talloc_reference(arp,arp->RecurrencePattern.DeletedInstanceDates);
	arp->RecurrencePattern.ModifiedInstanceDates = talloc_reference(arp, arp->RecurrencePattern.ModifiedInstanceDates);
	

	

        return arp;
}


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
	ndr->iconv_convenience = smb_iconv_convenience_init(mem_ctx, "CP850", "UTF8", true);
	TimeZoneStruct = talloc_zero(mem_ctx, struct TimeZoneStruct);
	ndr_err_code = ndr_pull_TimeZoneStruct(ndr, NDR_SCALARS, TimeZoneStruct);

	talloc_free(ndr);
	
	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(TimeZoneStruct);
		return NULL;
	}

	return TimeZoneStruct;
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
	ndr->iconv_convenience = smb_iconv_convenience_init(mem_ctx, "CP850", "UTF8", true);
	GlobalObjectId = talloc_zero(mem_ctx, struct GlobalObjectId);
	ndr_err_code = ndr_pull_GlobalObjectId(ndr, NDR_SCALARS, GlobalObjectId);

	talloc_free(ndr);

	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(GlobalObjectId);
		return NULL;
	}

	return GlobalObjectId;
}
