/*
   MAPI Implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2005-2007
   Copyright (C) Gregory Schiro 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <libmapi/mapitags.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>

_PUBLIC_ struct SPropTagArray *set_SPropTagArray(TALLOC_CTX *mem_ctx, 
						 uint32_t prop_nb, ...)
{
	struct SPropTagArray	*SPropTag;
	va_list			ap;
	uint32_t		i;
	uint32_t		*aulPropTag;

	aulPropTag = talloc_array(mem_ctx, uint32_t, prop_nb);

	va_start(ap, prop_nb);
	for (i = 0; i < prop_nb; i++) {
		aulPropTag[i] = va_arg(ap, int);
	}
	va_end(ap);

	SPropTag = talloc(mem_ctx, struct SPropTagArray);
	SPropTag->aulPropTag = aulPropTag;
	SPropTag->cValues = prop_nb;
	return SPropTag;
}

_PUBLIC_ struct SPropValue *get_SPropValue_SRowSet(struct SRowSet *RowSet, 
						   uint32_t ulPropTag)
{
	uint32_t	i;
	uint32_t	j;

	for (i = 0; i != RowSet->cRows; i++) {
		for (j = 0; j < RowSet->aRow[i].cValues; j++) {
			if (ulPropTag == RowSet->aRow[i].lpProps[j].ulPropTag) {
				return (&RowSet->aRow[i].lpProps[j]);
			}
		}
	}

	return NULL;
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

uint32_t MAPITAGS_delete_entries(enum MAPITAGS *mapitags, uint32_t final_count, uint32_t prop_nb, ...)
{
	va_list			ap;
	uint32_t		i,j;
	uint32_t		aulPropTag;
	uint32_t		count = 0;
	
	va_start(ap, prop_nb);
	for (i = 0; i != prop_nb; i++) {
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

_PUBLIC_ void *find_SPropValue_data(struct SRow *aRow, uint32_t mapitag)
{
	uint32_t i;

	for (i = 0; i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == mapitag) {
			return get_SPropValue_data(&(aRow->lpProps[i]));
		}
	}
	return NULL;
}

_PUBLIC_ void *find_mapi_SPropValue_data(struct mapi_SPropValue_array *properties, uint32_t mapitag)
{
	uint32_t i;

	for (i = 0; i < properties->cValues; i++) {
		if (properties->lpProps[i].ulPropTag == mapitag) {
			return get_mapi_SPropValue_data(&properties->lpProps[i]);
		}
	}
	return NULL;
}

void *get_mapi_SPropValue_data(struct mapi_SPropValue *lpProp)
{
	if (lpProp->ulPropTag == 0) {
		return NULL;
	}
	switch(lpProp->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		return (void *)(uint8_t *)&lpProp->value.b;
	case PT_I2:
		return (void *)(uint16_t *)&lpProp->value.i;
	case PT_LONG:
		return (void *)&lpProp->value.l;
	case PT_DOUBLE:
		return (void *)&lpProp->value.dbl;
	case PT_SYSTIME:
		return (void *)(struct FILETIME *)&lpProp->value.ft;
	case PT_ERROR:
		return (void *)lpProp->value.err;
	case PT_STRING8:
		return (void *)lpProp->value.lpszA;
	case PT_UNICODE:
		return (void *)lpProp->value.lpszW;
	case PT_BINARY:
		return (void *)(struct SBinary_short *)&lpProp->value.bin;
	case PT_MV_STRING8:
		return (void *)(struct mapi_SLPSTRArray *)&lpProp->value.MVszA;
	default:
		return NULL;
	}
}

void *get_SPropValue_data(struct SPropValue *lpProps)
{
	if (lpProps->ulPropTag == 0) {
		return NULL;
	}

	switch(lpProps->ulPropTag & 0xFFFF) {
	case PT_I8:
		return (void *)&lpProps->value.d;
	case PT_STRING8:
		return (void *)lpProps->value.lpszA;
	case PT_UNICODE:
		return (void *)lpProps->value.lpszW;
	case PT_ERROR:
	case PT_LONG:
		return (void *)&lpProps->value.l;
	case PT_DOUBLE:
		return (void *)&lpProps->value.dbl;
	default:
		return NULL;
	}
}

_PUBLIC_ BOOL set_SPropValue_proptag(struct SPropValue* lpProps, uint32_t aulPropTag, void *data)
{
	lpProps->ulPropTag = aulPropTag;
	lpProps->dwAlignPad = 0x0;

	return (set_SPropValue(lpProps, data));
}

_PUBLIC_ BOOL set_SPropValue(struct SPropValue* lpProps, void *data)
{
	if (data == NULL) {
		lpProps->value.err = MAPI_E_NOT_FOUND;
		return False;
	}
	switch (lpProps->ulPropTag & 0xFFFF) {
	case PT_SHORT:
		lpProps->value.i = *((uint16_t *)data);
		break;
	case PT_LONG:
		lpProps->value.l = *((uint32_t *)data);
		break;
	case PT_I8:
		lpProps->value.d = *((uint64_t *)data);
		break;
	case PT_BOOLEAN:
		lpProps->value.b = *((uint16_t *)data);
		break;
	case PT_STRING8:
		lpProps->value.lpszA = (char *) data;
		break;
	case PT_BINARY:
		lpProps->value.bin = *((struct SBinary *)data);
		break;
	case PT_UNICODE:
		lpProps->value.lpszW = (char *) data;
		break;
	case PT_CLSID:
		lpProps->value.lpguid = (struct MAPIUID *) data;
		break;
	case PT_SYSTIME:
		lpProps->value.ft = *((struct FILETIME *) data);
		break;
	case PT_ERROR:
		lpProps->value.err = *((uint32_t *)data);
		break;
	case PT_MV_SHORT:
		lpProps->value.err = *((uint32_t *)data);
		break;
	case PT_MV_LONG:
		lpProps->value.MVl = *((struct MV_LONG_STRUCT *)data);
		break;
	case PT_MV_STRING8:
		lpProps->value.MVszA = *((struct SLPSTRArray *)data);
		break;
	case PT_MV_BINARY:
		lpProps->value.MVbin = *((struct SBinaryArray *)data);
		break;
	case PT_MV_CLSID:
		lpProps->value.MVguid = *((struct SGuidArray *)data);
		break;
	case PT_MV_UNICODE:
		lpProps->value.MVszW = *((struct MV_UNICODE_STRUCT *)data);
		break;
	case PT_MV_SYSTIME:
		lpProps->value.MVft = *((struct SDateTimeArray *)data);
		break;
	case PT_NULL:
		lpProps->value.null = *((uint32_t *)data);
		break;
	case PT_OBJECT:
		lpProps->value.null = *((uint32_t *)data);
		break;
	default:
		lpProps->value.err = MAPI_E_NOT_FOUND;

		return False;
	}

	return True;
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
		return (strlen(sprop->value.lpszW) * 2 + 2);
	case PT_SYSTIME:
		mapi_sprop->value.ft.dwLowDateTime = sprop->value.ft.dwLowDateTime;
		mapi_sprop->value.ft.dwHighDateTime = sprop->value.ft.dwHighDateTime;
		return (sizeof (struct FILETIME));
	case PT_BINARY:
		mapi_sprop->value.bin.cb = sprop->value.bin.cb;
		mapi_sprop->value.bin.lpb = sprop->value.bin.lpb;
		return (mapi_sprop->value.bin.cb + sizeof(uint16_t));

	}
	return 0;

}

_PUBLIC_ enum MAPISTATUS SRow_addprop(struct SRow *aRow, struct SPropValue SPropValue)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		cValues;
	struct SPropValue	lpProp;
	
	MAPI_RETVAL_IF(!aRow, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = (TALLOC_CTX *) aRow;

	cValues = aRow->cValues + 1;
	aRow->lpProps = talloc_realloc(mem_ctx, aRow->lpProps, struct SPropValue, cValues);
	lpProp = aRow->lpProps[cValues];
	lpProp.ulPropTag = SPropValue.ulPropTag;
	lpProp.dwAlignPad = 0;
	set_SPropValue(&(lpProp), get_SPropValue_data(&SPropValue));
	aRow->cValues = cValues;
	aRow->lpProps[cValues - 1] = lpProp;

	return MAPI_E_SUCCESS;
}

_PUBLIC_ uint32_t SRowSet_propcpy(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, struct SPropValue SPropValue)
{
	uint32_t	rows;
	uint32_t	cValues;
	struct SPropValue lpProp;

	for (rows = 0; rows < SRowSet->cRows; rows++) {
		cValues = SRowSet->aRow[rows].cValues + 1;
		SRowSet->aRow[rows].lpProps = talloc_realloc(mem_ctx, SRowSet->aRow[rows].lpProps, struct SPropValue, cValues);
		lpProp = SRowSet->aRow[rows].lpProps[cValues];
		lpProp.ulPropTag = SPropValue.ulPropTag;
		lpProp.dwAlignPad = 0;
		set_SPropValue(&(lpProp), (void *)&SPropValue.value);
		SRowSet->aRow[rows].cValues = cValues;
		SRowSet->aRow[rows].lpProps[cValues - 1] = lpProp;
	}
	return 0;
}
