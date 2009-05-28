/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008.

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

#include "libocpf/ocpf_private.h"
#include <libocpf/ocpf.h>
#include <libocpf/ocpf_api.h>
#include <libmapi/defs_private.h>

#include <time.h>

struct ocpf_guid {
	char		*name;
	const char	*oleguid;
};

static const struct ocpf_guid ocpf_guid[] = {
	{ "PSETID_Appointment",  "00062002-0000-0000-c000-000000000046" },
	{ "PSETID_Task",	 "00062003-0000-0000-c000-000000000046" },
	{ "PSETID_Address",    	 "00062004-0000-0000-c000-000000000046" },
	{ "PSETID_Common",	 "00062008-0000-0000-c000-000000000046" },
	{ "PSETID_Note",       	 "0006200e-0000-0000-c000-000000000046" },
	{ "PSETID_Log",		 "0006200a-0000-0000-c000-000000000046" },
	{ "PS_PUBLIC_STRINGS",   "00020329-0000-0000-c000-000000000046" },
	{ "PS_INTERNET_HEADERS", "00020386-0000-0000-c000-000000000046" },
	{ NULL,			 NULL					}
};

static char *ocpf_write_get_guid_name(const char *oleguid)
{
	uint32_t			i;
	static int			idx = 0;
	static struct ocpf_oleguid	*guid = NULL;
	struct ocpf_oleguid		*element;
	char				*name;

	if (!oleguid) return NULL;

	if (!guid) {
		guid = talloc_zero(ocpf->mem_ctx, struct ocpf_oleguid);
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

	element->name = talloc_asprintf(ocpf->mem_ctx, "PSETID_Custom_%d", idx);
	element->guid = talloc_strdup(ocpf->mem_ctx, oleguid);
	DLIST_ADD(guid, element);
	name = talloc_strdup(ocpf->mem_ctx, element->name);
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

static void ocpf_write_propname(FILE *fp, uint32_t ulPropTag)
{
	const char	*propname;
	char		*line;
	ssize_t		len;

	propname = get_proptag_name(ulPropTag);
	if (propname) {
		line = talloc_asprintf(ocpf->mem_ctx, "\t%s = ", propname);
	} else {
		line = talloc_asprintf(ocpf->mem_ctx, "\t0x%x = ", ulPropTag);
	}
	len = fwrite(line, strlen(line), 1, fp);
	talloc_free(line);
}

static char *ocpf_write_systime(const struct FILETIME *ft)
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
	line = talloc_strdup(ocpf->mem_ctx, tempTime);

	return line;
}

static char *ocpf_write_binary(const struct Binary_r *bin)
{
	uint32_t	i;
	char		*line;
	
	line = talloc_asprintf(ocpf->mem_ctx, "{");
	for (i = 0; i < bin->cb; i++) {
		line = talloc_asprintf_append(line, " 0x%.2x", bin->lpb[i]);
	}
	line = talloc_asprintf_append(line, " }\n");

	return line;
}

static char *ocpf_write_escape_string(const char *value)
{
	char	*str = NULL;
	char	*stmp = NULL;
	int	value_len;
	int	len = 0;
	int	tmp = 0;

	value_len = strlen(value);
	tmp = strcspn(value, "\\\"");

	if (tmp == value_len) {
		str = talloc_strdup(ocpf->mem_ctx, value);
		return str;
	} else {
		str = talloc_strndup(ocpf->mem_ctx, value, tmp);
		str = talloc_asprintf_append_buffer(str, "\\\%c", value[tmp]);
	}
	len += tmp + 1;

	while (len < value_len) {
		tmp = strcspn(value + len, "\\\"");
		
		if ((tmp + len) == value_len) {
			str = talloc_asprintf_append_buffer(str, "%s", value + len);
			break;
		} else {
			stmp = talloc_strndup(ocpf->mem_ctx, value + len, tmp);
			str = talloc_asprintf_append_buffer(str, "%s\\\%c", stmp, value[len + tmp]);
			talloc_free(stmp);
			len += tmp + 1;
		}
	}

	return str;
}

char *ocpf_write_unescape_string(const char *value)
{
	char	*str = NULL;
	char	*stmp = NULL;
	int	value_len;
	int	len = 0;
	int	tmp = 0;

	value_len = strlen(value);
	tmp = strcspn(value, "\\");

	if (tmp == value_len) {
		str = talloc_strdup(ocpf->mem_ctx, value);
		return str;
	}
	
	str = talloc_strndup(ocpf->mem_ctx, value, tmp + 1);
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
			
		stmp = talloc_strndup(ocpf->mem_ctx, value + len, tmp + 1);
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

static char *ocpf_write_mv_string8(const struct StringArray_r *value)
{
	char		*str = NULL;
	char		*tmp = NULL;
	uint32_t	i;

	str = talloc_asprintf(ocpf->mem_ctx, "{ ");
	for (i = 0; i < value->cValues; i++) {
		tmp = ocpf_write_escape_string((const char *)value->lppszA[i]);
		if (i != value->cValues - 1) {
			str = talloc_asprintf_append_buffer(str, "\"%s\", ", tmp);
		} else {
			str = talloc_asprintf_append_buffer(str, "\"%s\" }", tmp);
		}
		talloc_free(tmp);
	}

	return str;
}

static char *ocpf_write_property(bool *found, uint32_t ulPropTag, const void *value)
{
	char	*line = NULL;
	char	*str = NULL;

	switch (ulPropTag & 0xFFFF) {
	case PT_STRING8:
		str = ocpf_write_escape_string((const char *)value);
		line = talloc_asprintf(ocpf->mem_ctx, "\"%s\"\n", str);
		talloc_free(str);
		*found = true;
		break;
	case PT_UNICODE:
		str = ocpf_write_escape_string((const char *)value);
		line = talloc_asprintf(ocpf->mem_ctx, "U\"%s\"\n", str);
		talloc_free(str);
		*found = true;
		break;
	case PT_SHORT:
		line = talloc_asprintf(ocpf->mem_ctx, "S%d\n", *((const uint16_t *)value));
		*found = true;
		break;
	case PT_LONG:
		line = talloc_asprintf(ocpf->mem_ctx, "%d\n", *((const uint32_t *)value));			
		*found = true;
		break;
	case PT_BOOLEAN:
		line = talloc_asprintf(ocpf->mem_ctx, "B\"%s\"\n", (*((const uint8_t *)value) == true) ? "true" : "false");
		*found = true;
		break;
	case PT_DOUBLE:
		line = talloc_asprintf(ocpf->mem_ctx, "D0x%"PRIx64"\n", *(const uint64_t *)value);
		*found = true;
		break;
	case PT_SYSTIME:
		line = ocpf_write_systime((const struct FILETIME *)value);
		*found = true;
		break;
	case PT_BINARY:
		line = ocpf_write_binary((const struct Binary_r *)value);
		*found = true;
		break;
	case PT_MV_STRING8:
		line = ocpf_write_mv_string8((const struct StringArray_r *)value);
		*found = true;
		break;
	}

	return line;
}


static char *ocpf_write_recipients(enum ocpf_recipClass recipClass)
{
	struct ocpf_recipients	*element;
	char			*line = NULL;
	bool			found = false;

	line = talloc_zero(ocpf->mem_ctx, char);
	for (element = ocpf->recipients, found = false; element->next; element = element->next) {
		if (found && element->class == recipClass) {
			line = talloc_asprintf_append(line, ";");
			found = false;
		}
		if (element->class == recipClass) {
			line = talloc_asprintf_append(line, "\"%s\"", element->name);
			found = true;
		}
	}
	return line;
}


static int ocpf_write_add_recipients(enum ocpf_recipClass recipClass, const char *recipients)
{
	char		*tmp = NULL;
	uint32_t	i = 0;

	if (!recipients) return OCPF_ERROR;

	if ((tmp = strtok((char *)recipients, ";")) == NULL) {
		return OCPF_ERROR;
	}

	ocpf_recipient_add(recipClass, tmp);

	for (i = 1; (tmp = strtok(NULL, ";")) != NULL; i++) {
		ocpf_recipient_add(recipClass, tmp);
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

   \param filename output filename
   \param folder_id the folder 

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_init
 */
_PUBLIC_ int ocpf_write_init(const char *filename, mapi_id_t folder_id)
{
	OCPF_RETVAL_IF(!filename, OCPF_WRITE_NOT_INITIALIZED, NULL);
	OCPF_RETVAL_IF(!folder_id, OCPF_WRITE_NOT_INITIALIZED, NULL);
	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, OCPF_NOT_INITIALIZED, NULL);

	ocpf->filename = talloc_strdup(ocpf->mem_ctx, filename);
	ocpf->folder = folder_id;

	return OCPF_SUCCESS;
}


/**
   \details Create the OCPF structure required for the commit
   operation

   This function process properties and named properties from the
   specified mapi_SPropValue_array and generates an OCPF structure
   with all the attributes required to create an OCPF file in the
   commit operation.

   \param obj_message the message object
   \param mapi_lpProps the array of mapi properties returned by
   GetPropsAll

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa GetPropsAll, ocpf_write_commit
 */
_PUBLIC_ int ocpf_write_auto(mapi_object_t *obj_message,
			     struct mapi_SPropValue_array *mapi_lpProps)
{
	enum MAPISTATUS		retval;
	uint32_t		i;
	uint16_t		propID;
	struct SPropValue	lpProps;
	const char		*type;
	const char		*recipient;
	char			*tmp_guid;
	const char     		*guid;
	struct MAPINAMEID	*nameid;
	uint16_t		count;
	struct ocpf_nprop	nprop;

	OCPF_RETVAL_IF(!ocpf->filename, OCPF_WRITE_NOT_INITIALIZED, NULL);
	OCPF_RETVAL_IF(!ocpf || !ocpf->mem_ctx, OCPF_NOT_INITIALIZED, NULL);
	OCPF_RETVAL_IF(!mapi_lpProps, OCPF_INVALID_PROPARRAY, NULL);

	/* store message type */
	type = (const char *) find_mapi_SPropValue_data(mapi_lpProps, PR_MESSAGE_CLASS);
	ocpf_type_add(type);

	/* store recipients */
	recipient = (const char *) find_mapi_SPropValue_data(mapi_lpProps, PR_DISPLAY_TO);
	ocpf_write_add_recipients(OCPF_MAPI_TO, recipient);

	recipient = (const char *) find_mapi_SPropValue_data(mapi_lpProps, PR_DISPLAY_CC);
	ocpf_write_add_recipients(OCPF_MAPI_CC, recipient);

	recipient = (const char *) find_mapi_SPropValue_data(mapi_lpProps, PR_DISPLAY_BCC);
	ocpf_write_add_recipients(OCPF_MAPI_BCC, recipient);

	/* store properties and OLEGUID in OCPF context */
	for (i = 0; i < mapi_lpProps->cValues; i++) {
		propID = mapi_lpProps->lpProps[i].ulPropTag >> 16;
		cast_SPropValue(&mapi_lpProps->lpProps[i], &lpProps);
		
		if (propID < 0x8000) {
			if (ocpf_write_exclude_property(lpProps.ulPropTag) == false) {
				/* HACK: replace PR_CONVERSATION_TOPIC with PR_SUBJECT */
				if (lpProps.ulPropTag == PR_CONVERSATION_TOPIC) {
					lpProps.ulPropTag = PR_SUBJECT;
					ocpf_propvalue(lpProps.ulPropTag, lpProps.value, lpProps.ulPropTag & 0xFFFF, false);
					cast_SPropValue(&mapi_lpProps->lpProps[i], &lpProps);
				}
				ocpf_propvalue(mapi_lpProps->lpProps[i].ulPropTag, 
					       lpProps.value, mapi_lpProps->lpProps[i].ulPropTag & 0xFFFF, false);
			}
		} else {
			nameid = talloc_zero(ocpf->mem_ctx, struct MAPINAMEID);
			retval = GetNamesFromIDs(obj_message, ((lpProps.ulPropTag & 0xFFFF0000) | PT_NULL),
						 &count, &nameid);
			memset(&nprop, 0, sizeof (struct ocpf_nprop));
			switch (nameid->ulKind) {
			case MNID_ID:
				nprop.mnid_id = nameid->kind.lid;
				break;
			case MNID_STRING:
				nprop.mnid_string = talloc_strdup(ocpf->mem_ctx, nameid->kind.lpwstr.Name);
				break;
			}
			nprop.propType = lpProps.ulPropTag & 0xFFFF;
			tmp_guid = GUID_string(ocpf->mem_ctx, &nameid->lpguid);
			nprop.guid = ocpf_write_get_guid_name(tmp_guid);

			/* OLEGUID has to be inserted prior named properties */
			if (ocpf_oleguid_check(nprop.guid, &guid) != OCPF_SUCCESS)
				ocpf_oleguid_add(nprop.guid, tmp_guid);
			
			nprop.registered = false;
			ocpf_nproperty_add(&nprop, lpProps.value, NULL, nprop.propType, false);

			talloc_free(nameid);
		}
	}

	return OCPF_SUCCESS;
}


/**
   \details Write OCPF structure to OCPF file

   This function dumps the OCPF structure content into the OCPF file
   defined in ocpf_write_init.

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR

   \sa ocpf_write_init, ocpf_write_auto
 */
_PUBLIC_ int ocpf_write_commit(void)
{
	FILE			*fp;
	struct ocpf_property	*element;
	struct ocpf_nproperty	*nelement;
	struct ocpf_oleguid	*nguid;
	char			*line;
	bool			found = false;
	char			*definition = NULL;
	ssize_t			len;

	fp = fopen(ocpf->filename, "w+");
	OCPF_RETVAL_IF(!fp, OCPF_INVALID_FILEHANDLE, NULL);

	/* message type */
	line = talloc_asprintf(ocpf->mem_ctx, "TYPE   \"%s\"\n\n", ocpf->type);
	len = fwrite(line, strlen(line), 1, fp);
	talloc_free(line);

	/* folder id */
	line = talloc_asprintf(ocpf->mem_ctx, "FOLDER D0x%"PRIx64"\n\n", ocpf->folder);
	len = fwrite(line, strlen(line), 1, fp);
	talloc_free(line);

	/* OLEGUID */
	for (nguid = ocpf->oleguid; nguid->next; nguid = nguid->next) {
		line = talloc_asprintf(ocpf->mem_ctx, "OLEGUID %-25s \"%s\"\n", nguid->name, nguid->guid);
		len = fwrite(line, strlen(line), 1, fp);
		talloc_free(line);
	}
	len = fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);

	/* RECIPIENT TO */
	line = ocpf_write_recipients(OCPF_MAPI_TO);
	if (line && strlen(line)) {
		len = fwrite(OCPF_RECIPIENT_TO, strlen(OCPF_RECIPIENT_TO), 1, fp);
		len = fwrite(line, strlen(line), 1, fp);
		len = fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);
		talloc_free(line);
	}

	/* RECIPIENT CC */
	line = ocpf_write_recipients(OCPF_MAPI_CC);
	if (line && strlen(line)) {
		len = fwrite(OCPF_RECIPIENT_CC, strlen(OCPF_RECIPIENT_CC), 1, fp);
		len = fwrite(line, strlen(line), 1, fp);
		len = fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);
		talloc_free(line);
	}

	/* RECIPIENT BCC */
	line = ocpf_write_recipients(OCPF_MAPI_BCC);
	if (line && strlen(line)) {
		len = fwrite(OCPF_RECIPIENT_BCC, strlen(OCPF_RECIPIENT_BCC), 1, fp);
		len = fwrite(line, strlen(line), 1, fp);
		len = fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);
		talloc_free(line);
	}

	len = fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);

	/* known properties */
	len = fwrite(OCPF_PROPERTY_BEGIN, strlen(OCPF_PROPERTY_BEGIN), 1, fp);
	for (element = ocpf->props; element->next; element = element->next) {
		line = ocpf_write_property(&found, element->aulPropTag, element->value);
		if (found == true) {
			ocpf_write_propname(fp, element->aulPropTag);
			len = fwrite(line, strlen(line), 1, fp);
			talloc_free(line);
			found = false;
		}
	}
	len = fwrite(OCPF_END, strlen(OCPF_END), 1, fp);
	len = fwrite(OCPF_NEWLINE, strlen(OCPF_NEWLINE), 1, fp);

	/* named properties */
	len = fwrite(OCPF_NPROPERTY_BEGIN, strlen(OCPF_NPROPERTY_BEGIN), 1, fp);
	for (nelement = ocpf->nprops; nelement->next; nelement = nelement->next) {
		line = ocpf_write_property(&found, nelement->propType, nelement->value);
		if (found == true) {
			if (nelement->mnid_id) {
				definition = talloc_asprintf(ocpf->mem_ctx, "\tMNID_ID:0x%.4x:%s:%s = ",
							     nelement->mnid_id,
							     ocpf_write_get_proptype_name(nelement->propType),
							     ocpf_write_get_guid_name(nelement->oleguid));
			} else if (nelement->mnid_string) {
				definition = talloc_asprintf(ocpf->mem_ctx, "\tMNID_STRING:\"%s\":%s:%s = ",
							     nelement->mnid_string,
							     ocpf_write_get_proptype_name(nelement->propType),
							     ocpf_write_get_guid_name(nelement->oleguid));
			}
			
			if (definition) {
				len = fwrite(definition, strlen(definition), 1, fp);
				talloc_free(definition);
			}

			len = fwrite(line, strlen(line), 1, fp);
			talloc_free(line);
			found = false;
		}
	}
	len = fwrite(OCPF_END, strlen(OCPF_END), 1, fp);

	fclose(fp);
	return OCPF_SUCCESS;
}
