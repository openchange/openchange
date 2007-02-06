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

#include "openchange.h"
#include "ndr_exchange.h"
#include "mapitags.h"
#include "libmapi/include/mapidefs.h"
#include "libmapi/include/property.h"
#include "server/dcesrv_exchange.h"
#include "libmapi/include/proto.h"
#include "libmapi/include/mapi_proto.h"

BOOL	OnSection(const char *SectionName, void *UserData)
{
	struct SPropTagGBuild	*Data = (struct SPropTagGBuild *)UserData;

	Data->TagsT = talloc_realloc(Data->mem_ctx, Data->TagsT,
				     struct SPropTagByFunc, Data->cur_idx + 1);
	Data->TagsT[Data->cur_idx].FuncName = talloc_strdup(Data->mem_ctx, SectionName);
	Data->TagsT[Data->cur_idx].poped_yet = 0;
	Data->TagsT[Data->cur_idx].nb_tags = 0;
	Data->TagsT[Data->cur_idx].aulPropTag = talloc_array_size(Data->mem_ctx, sizeof(uint32_t), 0);
	Data->cur_idx += 1;
	Data->tag_idx = 0;

	return True;
}

BOOL	OnKey(const char *KeyName, const char *KeyValue, void *UserData)
{
	struct SPropTagGBuild	*Data = (struct SPropTagGBuild *)UserData;

	Data->TagsT[Data->cur_idx - 1].aulPropTag =
		talloc_realloc(Data->mem_ctx, Data->TagsT[Data->cur_idx - 1].aulPropTag,
			       uint32_t, Data->tag_idx + 1);
	Data->TagsT[Data->cur_idx - 1].aulPropTag[Data->tag_idx] = get_proptag_value(KeyName);
	Data->TagsT[Data->cur_idx - 1].nb_tags += 1;
	Data->tag_idx += 1;

	return True;
}

struct SPropTagArray *set_SPropTagFile(TALLOC_CTX *mem_ctx, const char *conf_path, const char *Function)
{
	static struct SPropTagGBuild	*Data = NULL;
	struct SPropTagArray	*SPropTag;
	uint32_t		i;
	uint32_t		j;

	if (Data == NULL) {
		if (conf_path == NULL)
			conf_path = lp_parm_string(-1, "pr_tags", "cf_path");
		Data = talloc(mem_ctx, struct SPropTagGBuild);
		Data->mem_ctx = Data;
		Data->cur_idx = 0;
		Data->tag_idx = 0;
		Data->TagsT = talloc_array_size(Data, sizeof(struct SPropTagByFunc), 0);
		if (pm_process(conf_path, OnSection, OnKey, Data) == False) {
			return NULL;
		}
		Data->TagsT = talloc_realloc(Data, Data->TagsT,
					     struct SPropTagByFunc, Data->cur_idx + 1);
		Data->TagsT[Data->cur_idx].FuncName = NULL;
		Data->TagsT[Data->cur_idx].aulPropTag = NULL;
		if (DEBUGLEVEL == 12) {
			for (i = 0; Data->TagsT[i].FuncName; i++) {
				printf("[%s]\n", Data->TagsT[i].FuncName);
				for (j = 0; j < Data->TagsT[i].nb_tags; j++)
					printf("%s\n", get_proptag_name(Data->TagsT[i].aulPropTag[j]));
				printf("\n");
			}
		}
	}
	if (Function) {
		for (i = 0; Data->TagsT[i].FuncName; i++) {
			if (!strcmp(Function, Data->TagsT[i].FuncName) && (Data->TagsT[i].poped_yet == 0)) {
				SPropTag = talloc(mem_ctx, struct SPropTagArray);
				SPropTag->aulPropTag = Data->TagsT[i].aulPropTag;
				SPropTag->cValues = Data->TagsT[i].nb_tags + 1;
				Data->TagsT[i].poped_yet = 1;
				if (DEBUGLEVEL == 12) {
					printf("Sended tags for %s :\n", Function);
					for (j = 0; j < Data->TagsT[i].nb_tags; j++) {
						printf("%s\n", get_proptag_name(Data->TagsT[i].aulPropTag[j]));
					}
				}

				return SPropTag;
			}
		}

		return NULL;
	}

	return NULL;
}

struct SPropTagArray *set_SPropTagArray(TALLOC_CTX *mem_ctx, uint32_t prop_nb, ...)
{
	struct SPropTagArray	*SPropTag;
	va_list			ap;
	uint32_t		i;
	uint32_t		*aulPropTag;

	aulPropTag = talloc_array_size(mem_ctx, sizeof (uint32_t), prop_nb);

	va_start(ap, prop_nb);
	for (i = 0; i != prop_nb; i++) {
		aulPropTag[i] = va_arg(ap, int);
	}
	va_end(ap);

	SPropTag = talloc(mem_ctx, struct SPropTagArray);
	SPropTag->aulPropTag = aulPropTag;
	SPropTag->cValues = prop_nb + 1;

	return SPropTag;
}

struct SPropValue *get_SPropValue_SRowSet(struct SRowSet *RowSet, uint32_t ulPropTag)
{
	uint32_t	i;
	uint32_t	j;

	for (i = 0; i != RowSet->cRows; i++) {
		for (j = 0; j != RowSet->aRow[i].cValues; j++) {
			if (ulPropTag == RowSet->aRow[i].lpProps[j].ulPropTag) {
				return (&RowSet->aRow[i].lpProps[j]);
			}
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

uint32_t MAPITAGS_delete_entries(enum MAPITAGS *mapitags, uint32_t prop_nb, ...)
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
				for (j = count; mapitags[j]; j++) {
					mapitags[j] = (mapitags[j+1]) ? mapitags[j+1] : 0;
				}
			}
		}
	}
	va_end(ap);

	return count;
}

void *find_SPropValue_data(struct SRow *aRow, uint32_t mapitag)
{
	uint32_t i;

	for (i = 0; i < aRow->cValues; i++) {
		if (aRow->lpProps[i].ulPropTag == mapitag) {
			return get_SPropValue_data(&(aRow->lpProps[i]));
		}
	}
	return NULL;
}

void *get_SPropValue_data(struct SPropValue *lpProps)
{
	if (lpProps->ulPropTag == 0) {
		return NULL;
	}
	switch(lpProps->ulPropTag & 0xFFFF) {
	case PT_STRING8:
		return (void *)lpProps->value.lpszA;
	default:
		return NULL;
	}
}

BOOL set_SPropValue_proptag(struct SPropValue* lpProps, uint32_t aulPropTag, void *data)
{
	lpProps->ulPropTag = aulPropTag;
	lpProps->dwAlignPad = 0x0;

	return (set_SPropValue(lpProps, data));
}

BOOL set_SPropValue(struct SPropValue* lpProps, void *data)
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
		lpProps->value.ft = *((struct FILETIME *)data);
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

uint32_t cast_mapi_SPropValue(struct mapi_SPropValue *mapi_sprop, struct SPropValue *sprop)
{
	mapi_sprop->ulPropTag = sprop->ulPropTag;

	switch(sprop->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		mapi_sprop->value.b = sprop->value.b;
		return sizeof(uint16_t);
	case PT_I2:
		mapi_sprop->value.i = sprop->value.i;
		return sizeof(uint16_t);
	case PT_LONG:
		mapi_sprop->value.l = sprop->value.l;
		return sizeof(uint32_t);
	case PT_STRING8:
		mapi_sprop->value.lpszA = sprop->value.lpszA;
		return (strlen(sprop->value.lpszA) + 1);
	}
	return 0;

}

uint32_t SRowSet_propcpy(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, struct SPropValue SPropValue)
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
