/*
   MAPI Implementation

   OpenChange Project

   Copyright (C) Julien Kerihuel 2005-2006
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

BOOL set_SPropValue_proptag(struct SPropValue* lpProps, uint32_t aulPropTag, void *data)
{
	lpProps->ulPropTag = aulPropTag;
	lpProps->dwAlignPad = 0x0;

	return set_SPropValue(lpProps, data);
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
  set_SPropValues : Returns a pointer on struct SPropValue with all data set
*/
/* struct SPropValue *set_SPropValues(TALLOC_CTX *mem_ctx, struct SPropTagArray* pPropTagArray, void **data) */
/* { */
/* 	struct SPropValue	*lpProps; */
/* 	uint32_t		size; */
/* 	uint32_t		i; */

/* 	size = (pPropTagArray->cValues) ? pPropTagArray->cValues - 1 : 0; */
/* 	lpProps = talloc_size(mem_ctx, sizeof(*lpProps) * size); */
/* 	for (i = 0; i < size; i++) { */
/* 		lpProps[i].ulPropTag = pPropTagArray->aulPropTag[i]; */
/* 		lpProps[i].dwAlignPad = 0x0; */
/* 		set_SPropValue(&(lpProps[i]), data[i]); */
/* 	} */

/* 	return lpProps; */
/* } */

