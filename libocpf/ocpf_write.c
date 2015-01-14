/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008-2011.

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
   \file ocpf_write.c

   \brief public OCPF write API
 */

#include <time.h>

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"
#include "libmapi/libmapi_private.h"
#include "libmapi/mapidefs.h"

struct ocpf_guid {
	char		*name;
	const char	*oleguid;
};

static const struct ocpf_guid ocpf_guid[] = {
	{ "PSETID_Appointment",		PSETID_Appointment },
	{ "PSETID_Task",		PSETID_Task },
	{ "PSETID_Address",		PSETID_Address },
	{ "PSETID_Common",		PSETID_Common },
	{ "PSETID_Note",		PSETID_Note },
	{ "PSETID_Log",			PSETID_Log },
	{ "PSETID_Sharing",		PSETID_Sharing },
	{ "PSETID_PostRss",		PSETID_PostRss },
	{ "PSETID_UnifiedMessaging",	PSETID_UnifiedMessaging },
	{ "PSETID_Meeting",		PSETID_Meeting },
	{ "PSETID_Airsync",		PSETID_AirSync },
	{ "PSETID_Messaging",		PSETID_Messaging },
	{ "PSETID_Attachment",		PSETID_Attachment },
	{ "PSETID_CalendarAssistant",	PSETID_CalendarAssistant },
	{ "PS_PUBLIC_STRINGS",		PS_PUBLIC_STRINGS },
	{ "PS_INTERNET_HEADERS",	PS_INTERNET_HEADERS },
	{ "PS_MAPI",			PS_MAPI },
	{ NULL,			 NULL					}
};

static char *ocpf_write_get_guid_name(struct ocpf_context *ctx, const char *oleguid)
{
	uint32_t			i;
	static int			idx = 0;
	static struct ocpf_oleguid	*guid = NULL;
	struct ocpf_oleguid		*element;
	char				*name;

	if (!oleguid) return NULL;

	if (!guid) {
		guid = talloc_zero(ctx, struct ocpf_oleguid);
	}

	for (i = 0; ocpf_guid[i].oleguid; i++) {
		if (!strcmp(oleguid, ocpf_guid[i].oleguid)) {
			return ocpf_guid[i].name;
		}
	}

	for (element = guid; element->next; element = element->next) {
		if (!strcmp(oleguid, element->guid)) {
			return (char *)element->name;
		}
	}

	element->name = talloc_asprintf(ctx, "PSETID_Custom_%d", idx);
	element->guid = talloc_strdup(ctx, oleguid);
	DLIST_ADD(guid, element);
	name = talloc_strdup(ctx, element->name);
	idx++;

	return name;
}

struct ocpf_proptype {
	uint16_t	type;
	const char	*name;
};

static const struct ocpf_proptype ocpf_proptype[] = {
	{ 0x2,		"PT_SHORT"	},
	{ 0x3,		"PT_LONG"	},
	{ 0x4,		"PT_FLOAT"	},
	{ 0x5,		"PT_DOUBLE"	},
	{ 0x6,		"PT_CURRENCY"	},
	{ 0x7,		"PT_APPTIME"	},
	{ 0xa,		"PT_ERROR"	},
	{ 0xb,		"PT_BOOLEAN"	},
	{ 0xd,		"PT_OBJECT"	},
	{ 0x14,		"PT_I8"		},
	{ 0x1e,		"PT_STRING8"	},
	{ 0x1f,		"PT_UNICODE"	},
	{ 0x40,		"PT_SYSTIME"	},
	{ 0x48,		"PT_CLSID"	},
	{ 0x102,	"PT_BINARY"	},
	{ 0x1002,	"PT_MV_SHORT"	},
	{ 0x1003,	"PT_MV_LONG"	},
	{ 0x101e,	"PT_MV_STRING8"	},
	{ 0x101f,	"PT_MV_UNICODE"	},
	{ 0,		NULL}
};

static const char *ocpf_write_get_proptype_name(uint16_t type)
{
	uint32_t	i;

	for (i = 0; ocpf_proptype[i].name; i++) {
		if (type == ocpf_proptype[i].type) {
			return ocpf_proptype[i].name;
		}
	}
	return NULL;
}

static void ocpf_write_propname(struct ocpf_context *ctx, FILE *fp, uint32_t ulPropTag)
{
	const char	*propname;
	char		*line;

	propname = get_proptag_name(ulPropTag);
	if (propname) {
		line = talloc_asprintf(ctx, "\t%s = ", propname);
	} else {
		line = talloc_asprintf(ctx, "\t0x%x = ", ulPropTag);
	}
	fwrite(line, strlen(line), 1, fp);
	talloc_free(line);
}

static char *ocpf_write_systime(struct ocpf_context *ctx, const struct FILETIME *ft)
{
	char		*line;
	char		tempTime[60];
	NTTIME		nt;
	time_t		t;
	struct tm	*tm;

	nt = ft->dwHighDateTime;
	nt = (nt << 32) | ft->dwLowDateTime;
	t = nt_time_to_unix(nt);
	tm = localtime(&t);

	strftime(tempTime, sizeof(tempTime)-1, "T%Y-%m-%d %H:%M:%S\n", tm);
	line = talloc_strdup(ctx, tempTime);

	return line;
}

static char *ocpf_write_binary(struct ocpf_context *ctx, const struct Binary_r *bin)
{
	uint32_t	i;
	char		*line;
	
	line = talloc_asprintf(ctx, "{");
	for (i = 0; i < bin->cb; i++) {
		line = talloc_asprintf_append(line, " 0x%.2x", bin->lpb[i]);
	}
	line = talloc_asprintf_append(line, " }\n");

	return line;
}

static char *ocpf_write_mv_binary(struct ocpf_context *ctx, const struct BinaryArray_r *value)
{
	uint32_t	i;
	uint32_t	j;
	char		*line;

	line = talloc_asprintf(ctx, "{");
	for (i = 0; i < value->cValues; i++) {
		line = talloc_asprintf_append(line, " {");
		for (j = 0; j < value->lpbin[i].cb; j++) {
			line = talloc_asprintf_append(line, " 0x%.2x", value->lpbin[i].lpb[j]);
		}
		if (i != value->cValues - 1) {
			line = talloc_asprintf_append(line, " },");
		} else {
			line = talloc_asprintf_append(line, " }");
		}
	}
	line = talloc_asprintf_append(line, " }\n");

	return line;
}

static char *ocpf_write_mv_long(struct ocpf_context *ctx, const struct LongArray_r *value)
{
	char		*str = NULL;
	uint32_t	i;

	str = talloc_asprintf(ctx, "{ ");
	for (i = 0; i < value->cValues; i++) {
		if (i != value->cValues - 1) {
			str = talloc_asprintf_append_buffer(str, "%d, ", value->lpl[i]);
		} else {
			str = talloc_asprintf_append_buffer(str, "%d", value->lpl[i]);
		}
	}

	str = talloc_asprintf_append_buffer(str, " }\n");

	return str;
}

static char *ocpf_write_escape_string(struct ocpf_context *ctx, const char *value)
{
	char	*str = NULL;
	char	*stmp = NULL;
	int	value_len;
	int	len = 0;
	int	tmp = 0;

	value_len = strlen(value);
	tmp = strcspn(value, "\\\"");

	if (tmp == value_len) {
		str = talloc_strdup(ctx, value);
		return str;
	} else {
		str = talloc_strndup(ctx, value, tmp);
		str = talloc_asprintf_append_buffer(str, "\\%c", value[tmp]);
	}
	len += tmp + 1;

	while (len < value_len) {
		tmp = strcspn(value + len, "\\\"");
		
		if ((tmp + len) == value_len) {
			str = talloc_asprintf_append_buffer(str, "%s", value + len);
			break;
		} else {
			stmp = talloc_strndup(ctx, value + len, tmp);
			str = talloc_asprintf_append_buffer(str, "%s\\%c", stmp, value[len + tmp]);
			talloc_free(stmp);
			len += tmp + 1;
		}
	}

	return str;
}

char *ocpf_write_unescape_string(TALLOC_CTX *mem_ctx, const char *value)
{
	char	*str = NULL;
	char	*stmp = NULL;
	int	value_len;
	int	len = 0;
	int	tmp = 0;

	value_len = strlen(value);
	tmp = strcspn(value, "\\");

	if (tmp == value_len) {
		str = talloc_strdup(mem_ctx, value);
		return str;
	}
	
	str = talloc_strndup(mem_ctx, value, tmp + 1);
	if (value[tmp + 1] && value[tmp + 1] == '\\') {
		len += tmp + 2;
	} else {
		len += tmp + 1;
	}

	while (len < value_len) {
		tmp = strcspn(value + len, "\\");
		
		if ((tmp + len) == value_len) {
			str = talloc_asprintf_append(str, "%s", value + len);
			break;
		}
			
		stmp = talloc_strndup(mem_ctx, value + len, tmp + 1);
		str = talloc_asprintf_append(str, "%s", stmp);
		if (value[len + tmp + 1] && 
		    (value[len + tmp + 1] == '\\' || value[len + tmp + 1] == '"')) {
			len += tmp + 2;
		} else {
			len += tmp + 1;
		}
		talloc_free(stmp);
	}

	return str;
}

static char *ocpf_write_mv_string8(struct ocpf_context *ctx, const struct StringArray_r *value)
{
	char		*str = NULL;
	char		*tmp = NULL;
	uint32_t	i;

	str = talloc_asprintf(ctx, "{ ");
	for (i = 0; i < value->cValues; i++) {
		tmp = ocpf_write_escape_string(ctx, (const char *)value->lppszA[i]);
		if (i != value->cValues - 1) {
			str = talloc_asprintf_append_buffer(str, "\"%s\", ", tmp);
		} else {
			str = talloc_asprintf_append_buffer(str, "\"%s\" }", tmp);
		}
		talloc_free(tmp);
	}

	return str;
}


static char *ocpf_write_mv_unicode(struct ocpf_context *ctx, const struct StringArrayW_r *value)
{
	char		*str = NULL;
	char		*tmp = NULL;
	uint32_t	i;

	str = talloc_asprintf(ctx, "{ ");
	for (i = 0; i < value->cValues; i++) {
		tmp = ocpf_write_escape_string(ctx, (const char *)value->lppszW[i]);
		if (i != value->cValues - 1) {
			str = talloc_asprintf_append_buffer(str, "\"%s\", ", tmp);
		} else {
			str = talloc_asprintf_append_buffer(str, "\"%s\" }", tmp);
		}
		talloc_free(tmp);
	}

	return str;	
}

static char *ocpf_write_property(struct ocpf_context *ctx, bool *found, uint32_t ulPropTag, const void *value)
{
	char	*line = NULL;
	char	*str = NULL;

	switch (ulPropTag & 0xFFFF) {
	case PT_STRING8:
		str = ocpf_write_escape_string(ctx, (const char *)value);
		line = talloc_asprintf(ctx, "\"%s\"\n", str);
		talloc_free(str);
		*found = true;
		break;
	case PT_UNICODE:
		str = ocpf_write_escape_string(ctx, (const char *)value);
		line = talloc_asprintf(ctx, "W\"%s\"\n", str);
		talloc_free(str);
		*found = true;
		break;
	case PT_SHORT:
		line = talloc_asprintf(ctx, "S%d\n", *((const uint16_t *)value));
		*found = true;
		break;
	case PT_LONG:
		line = talloc_asprintf(ctx, "%d\n", *((const uint32_t *)value));			
		*found = true;
		break;
	case PT_DOUBLE:
		line = talloc_asprintf(ctx, "F%e\n", *((const double *)value));
		*found = true;
		break;
	case PT_BOOLEAN:
		line = talloc_asprintf(ctx, "B\"%s\"\n", (*((const uint8_t *)value) == true) ? "true" : "false");
		*found = true;
		break;
	case PT_I8:
		line = talloc_asprintf(ctx, "D0x%.16"PRIx64"\n", *(const uint64_t *)value);
		*found = true;
		break;
	case PT_SYSTIME:
		line = ocpf_write_systime(ctx, (const struct FILETIME *)value);
		*found = true;
		break;
	case PT_BINARY:
		line = ocpf_write_binary(ctx, (const struct Binary_r *)value);
		*found = true;
		break;
	case PT_MV_LONG:
		line = ocpf_write_mv_long(ctx, (const struct LongArray_r *)value);
		*found = true;
		break;
	case PT_MV_STRING8:
		line = ocpf_write_mv_string8(ctx, (const struct StringArray_r *)value);
		*found = true;
		break;
	case PT_MV_UNICODE:
		line = ocpf_write_mv_unicode(ctx, (const struct StringArrayW_r *)value);
		*found = true;
		break;
	case PT_MV_BINARY:
		line = ocpf_write_mv_binary(ctx, (const struct BinaryArray_r *)value);
		*found = true;
		break;
	}

	return line;
}


static int ocpf_write_recipients(struct ocpf_context *ctx, 
				 FILE *fp,
				 enum ulRecipClass recipClass)
{
	int			i;
	int			j;
	char			*line = NULL;
	const void		*value_data;
	bool			found = false;
	uint32_t		*RecipClass;
	enum MAPITAGS		ulPropTag;
	struct SPropValue	*lpProps;

	for (i = 0; i < ctx->recipients->cRows; i++) {
		lpProps = get_SPropValue_SRow(&(ctx->recipients->aRow[i]), PidTagRecipientType);
		if (lpProps) {
			RecipClass = (uint32_t *)get_SPropValue_data(lpProps);
			if (RecipClass && *RecipClass == recipClass) {
				switch (recipClass) {
				case MAPI_TO:
					fwrite(OCPF_RECIPIENT_TO, strlen(OCPF_RECIPIENT_TO), 1, fp);
					break;
				case MAPI_CC:
					fwrite(OCPF_RECIPIENT_CC, strlen(OCPF_RECIPIENT_CC), 1, fp);
					break;
				case MAPI_BCC:
					fwrite(OCPF_RECIPIENT_BCC, strlen(OCPF_RECIPIENT_BCC), 1, fp);
					break;
				default:
					break;
				}

				for (j = 0; j < ctx->recipients->aRow[i].cValues; j++) {
					ulPropTag = ctx->recipients->aRow[i].lpProps[j].ulPropTag;
					value_data = get_SPropValue_data(&(ctx->recipients->aRow[i].lpProps[j]));
					if (value_data) {
						line = ocpf_write_property(ctx, &found, ulPropTag, (void *)value_data);
						if (found == true) {
							ocpf_write_propname(ctx, fp, ulPropTag);
							fwrite(line, strlen(line), 1, fp);
							talloc_free(line);
							found = false;
						}
					}
				}

				fwrite(OCPF_END, strlen(OCPF_END), 1, fp);
				fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);
			}
		}
	}

	return OCPF_SUCCESS;
}


static bool ocpf_write_exclude_property(uint32_t ulPropTag)
{
	uint32_t	i;
	uint32_t	propArray[] = { PR_DISPLAY_TO, 
					PR_DISPLAY_CC, 
					PR_DISPLAY_BCC, 
					0};

	for (i = 0; propArray[i]; i++) {
		if (propArray[i] == ulPropTag) {
			return true;
		}
	}

	return false;
}

/**
   \details Specify the OCPF file name to write
 
   Specify the ocpf file to create

   \param context_id the identifier representing the context
   \param folder_id the folder 

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_init
 */
_PUBLIC_ int ocpf_write_init(uint32_t context_id, 
			     mapi_id_t folder_id)
{
	struct ocpf_context	*ctx;

	/* Sanity checks */
	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL);

	/* Search the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);	

	ctx->folder = folder_id;

	return OCPF_SUCCESS;
}


/**
   \details Create the OCPF structure required for the commit
   operation

   This function process properties and named properties from the
   specified mapi_SPropValue_array and generates an OCPF structure
   with all the attributes required to create an OCPF file in the
   commit operation.

   \param context_id the identifier representing the context
   \param obj_message the message object
   \param mapi_lpProps the array of mapi properties returned by
   GetPropsAll

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa GetPropsAll, ocpf_write_commit
 */
_PUBLIC_ int ocpf_write_auto(uint32_t context_id,
			     mapi_object_t *obj_message,
			     struct mapi_SPropValue_array *mapi_lpProps)
{
	enum MAPISTATUS		retval;
	int			ret;
	struct ocpf_context	*ctx;
	uint32_t		i;
	uint16_t		propID;
	struct SPropValue	lpProps;
	struct SPropTagArray	SPropTagArray;
	const char		*type;
	char			*tmp_guid;
	const char     		*guid;
	struct MAPINAMEID	*nameid;
	uint16_t		count;
	struct ocpf_nprop	nprop;

	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL);
	OCPF_RETVAL_IF(!mapi_lpProps, NULL, OCPF_INVALID_PROPARRAY, NULL);
	
	/* Find the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);
	OCPF_RETVAL_IF(!ctx->filename, ctx, OCPF_WRITE_NOT_INITIALIZED, NULL);

	/* store message type */
	type = (const char *) find_mapi_SPropValue_data(mapi_lpProps, PidTagMessageClass);
	if (type) {
		ret = ocpf_type_add(ctx, type);
		if (ret) return ret;
	}

	/* store recipients */
	if (obj_message) {
		retval = GetRecipientTable(obj_message, ctx->recipients, &SPropTagArray);
		OCPF_RETVAL_IF(retval, ctx, OCPF_INVALID_RECIPIENTS, NULL);
	}

	/* store properties and OLEGUID in OCPF context */
	for (i = 0; i < mapi_lpProps->cValues; i++) {
		propID = mapi_lpProps->lpProps[i].ulPropTag >> 16;
		cast_SPropValue(ctx, &mapi_lpProps->lpProps[i], &lpProps);
		
		if (propID < 0x8000) {
			if (ocpf_write_exclude_property(lpProps.ulPropTag) == false) {
				/* HACK: replace PR_CONVERSATION_TOPIC with PR_SUBJECT */
				if (lpProps.ulPropTag == PR_CONVERSATION_TOPIC) {
					lpProps.ulPropTag = PR_SUBJECT;
					ocpf_propvalue(ctx, lpProps.ulPropTag, lpProps.value, 
						       lpProps.ulPropTag & 0xFFFF, false, kw_PROPERTY);
					cast_SPropValue(ctx, &mapi_lpProps->lpProps[i], &lpProps);
				}
				ocpf_propvalue(ctx, mapi_lpProps->lpProps[i].ulPropTag, 
					       lpProps.value, mapi_lpProps->lpProps[i].ulPropTag & 0xFFFF, false, kw_PROPERTY);
			}
		} else {
			nameid = talloc_zero(ctx, struct MAPINAMEID);
			retval = GetNamesFromIDs(obj_message, ((lpProps.ulPropTag & 0xFFFF0000) | PT_NULL),
						 &count, &nameid);
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			switch (nameid->ulKind) {
			case MNID_ID:
				nprop.mnid_id = nameid->kind.lid;
				break;
			case MNID_STRING:
				nprop.mnid_string = talloc_strdup(ctx, nameid->kind.lpwstr.Name);
				break;
			}
			nprop.propType = lpProps.ulPropTag & 0xFFFF;
			tmp_guid = GUID_string(ctx, &nameid->lpguid);
			nprop.guid = ocpf_write_get_guid_name(ctx, tmp_guid);

			/* OLEGUID has to be inserted prior named properties */
			if (ocpf_oleguid_check(ctx, nprop.guid, &guid) != OCPF_SUCCESS)
				ocpf_oleguid_add(ctx, nprop.guid, tmp_guid);
			
			nprop.registered = false;
			ocpf_nproperty_add(ctx, &nprop, lpProps.value, NULL, nprop.propType, false);

			talloc_free(nameid);
		}
	}

	return OCPF_SUCCESS;
}


/**
   \details Write OCPF structure to OCPF file

   This function dumps the OCPF structure content into the OCPF file
   defined in ocpf_write_init.

   \param context_id the identifier representing the context

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_write_init, ocpf_write_auto
 */
_PUBLIC_ int ocpf_write_commit(uint32_t context_id)
{
	FILE			*fp;
	struct ocpf_context	*ctx;
	struct ocpf_property	*element;
	struct ocpf_nproperty	*nelement;
	struct ocpf_oleguid	*nguid;
	char			*line;
	bool			found = false;
	char			*definition = NULL;

	/* Find the context */
	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	OCPF_RETVAL_IF(!ctx, NULL, OCPF_INVALID_CONTEXT, NULL);
	OCPF_RETVAL_IF(!ctx->filename, ctx, OCPF_WRITE_NOT_INITIALIZED, NULL);
	OCPF_RETVAL_IF(ctx->flags == OCPF_FLAGS_READ, ctx, OCPF_WRITE_NOT_INITIALIZED, NULL);

	if (ctx->flags == OCPF_FLAGS_CREATE) {
		ctx->fp = fopen(ctx->filename, "w+");
	} 

	/* Position the file at the beginning of the stream */
	fseek(ctx->fp, 0, SEEK_SET);

	fp = ctx->fp;
	OCPF_RETVAL_IF(!fp, ctx, OCPF_INVALID_FILEHANDLE, NULL);

	/* message type */
	if (ctx->type) {
		line = talloc_asprintf(ctx, "TYPE   \"%s\"\n\n", ctx->type);
		fwrite(line, strlen(line), 1, fp);
		talloc_free(line);
	}

	/* folder id */
	if (ctx->folder) {
		line = talloc_asprintf(ctx, "FOLDER D0x%.16"PRIx64"\n\n", ctx->folder);
		fwrite(line, strlen(line), 1, fp);
		talloc_free(line);
	}

	/* OLEGUID */
	for (nguid = ctx->oleguid; nguid->next; nguid = nguid->next) {
		line = talloc_asprintf(ctx, "OLEGUID %-25s \"%s\"\n", nguid->name, nguid->guid);
		fwrite(line, strlen(line), 1, fp);
		talloc_free(line);
	}
	fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);

	/* RECIPIENT */
	if (ctx->recipients && ctx->recipients->cRows) {
		fwrite(OCPF_RECIPIENT_BEGIN, strlen(OCPF_RECIPIENT_BEGIN), 1, fp);
		ocpf_write_recipients(ctx, fp, MAPI_TO);
		ocpf_write_recipients(ctx, fp, MAPI_CC);
		ocpf_write_recipients(ctx, fp, MAPI_BCC);

		fwrite(OCPF_END, strlen(OCPF_END), 1, fp);
		fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);
	}

	/* known properties */
	fwrite(OCPF_PROPERTY_BEGIN, strlen(OCPF_PROPERTY_BEGIN), 1, fp);
	for (element = ctx->props; element->next; element = element->next) {
		line = ocpf_write_property(ctx, &found, element->aulPropTag, element->value);
		if (found == true) {
			ocpf_write_propname(ctx, fp, element->aulPropTag);
			fwrite(line, strlen(line), 1, fp);
			talloc_free(line);
			found = false;
		}
	}
	fwrite(OCPF_END, strlen(OCPF_END), 1, fp);
	fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);

	/* named properties */
	fwrite(OCPF_NPROPERTY_BEGIN, strlen(OCPF_NPROPERTY_BEGIN), 1, fp);
	for (nelement = ctx->nprops; nelement->next; nelement = nelement->next) {
		line = ocpf_write_property(ctx, &found, nelement->propType, nelement->value);
		if (found == true) {
			if (nelement->mnid_id) {
				definition = talloc_asprintf(ctx, "\tMNID_ID:0x%.4x:%s:%s = ",
							     nelement->mnid_id,
							     ocpf_write_get_proptype_name(nelement->propType),
							     ocpf_write_get_guid_name(ctx, nelement->oleguid));
			} else if (nelement->mnid_string) {
				definition = talloc_asprintf(ctx, "\tMNID_STRING:\"%s\":%s:%s = ",
							     nelement->mnid_string,
							     ocpf_write_get_proptype_name(nelement->propType),
							     ocpf_write_get_guid_name(ctx, nelement->oleguid));
			}
			
			if (definition) {
				fwrite(definition, strlen(definition), 1, fp);
				talloc_free(definition);
			}

			fwrite(line, strlen(line), 1, fp);
			talloc_free(line);
			found = false;
		}
	}
	fwrite(OCPF_END, strlen(OCPF_END), 1, fp);

	return OCPF_SUCCESS;
}
