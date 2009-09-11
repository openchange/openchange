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

#include "libocpf/ocpf_private.h"
#include <libocpf/ocpf.h>
#include <libocpf/ocpf_api.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

/**
   \file ocpf_api.c

   \brief ocpf Private API
 */


void ocpf_do_debug(const char *format, ...)
{
	va_list ap;
	char	*s = NULL;
	int	ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	printf("%s:%d: %s\n", ocpf_get_filename(), lineno, s);
	free(s);
}

const char *ocpf_get_filename(void)
{
	return ocpf->filename;
}


int ocpf_propvalue_var(const char *propname, uint32_t proptag, const char *variable, bool unescape)
{
	struct ocpf_var		*vel;
	struct ocpf_property	*element;
	uint32_t		aulPropTag;

	if (!ocpf || !ocpf->mem_ctx) return -1;
	if (!propname && !proptag) return -1;
	if (propname && proptag) return -1;

	/* Sanity check: do not insert the same property twice */
	if (proptag) {
		aulPropTag = proptag;
	} else {
		aulPropTag = get_proptag_value(propname);
	}

	for (element = ocpf->props; element->next; element = element->next) {
		OCPF_RETVAL_IF(element->aulPropTag == aulPropTag, OCPF_WARN_PROP_REGISTERED, NULL);
	}

	for (vel = ocpf->vars; vel->next; vel = vel->next) {
		if (vel->name && !strcmp(vel->name, variable)) {
			OCPF_RETVAL_IF(vel->propType != (aulPropTag & 0xFFFF), OCPF_WARN_PROPVALUE_MISMATCH, NULL);
			element = NULL;
			element = talloc_zero(ocpf->mem_ctx, struct ocpf_property);
			element->aulPropTag = aulPropTag;
			if (unescape && (((aulPropTag & 0xFFFF) == PT_STRING8) || 
					 ((aulPropTag & 0xFFFF) == PT_UNICODE))) {
				element->value = ocpf_write_unescape_string(vel->value);
			} else {
				element->value = vel->value;
			}
			DLIST_ADD(ocpf->props, element);
			return OCPF_SUCCESS;
		}
	}

	OCPF_RETVAL_IF(1, OCPF_WARN_VAR_NOT_REGISTERED, NULL);
}


int ocpf_set_propvalue(TALLOC_CTX *mem_ctx, const void **value, uint16_t proptype, uint16_t sproptype, 
		       union SPropValue_CTR lpProp, bool unescape)
{
	char	*str = NULL;

	OCPF_RETVAL_IF(proptype != sproptype, OCPF_WARN_PROPVALUE_MISMATCH, NULL);

	switch (proptype) {
	case PT_STRING8:
		if (unescape) {
			str = ocpf_write_unescape_string(lpProp.lpszA);
		} else {
			str = talloc_strdup(ocpf->mem_ctx, lpProp.lpszA);
		}
		*value = talloc_memdup(mem_ctx, str, strlen(str) + 1);
		talloc_free(str);
		return OCPF_SUCCESS;
	case PT_UNICODE:
		if (unescape) {
			str = ocpf_write_unescape_string(lpProp.lpszW);
		} else {
			str = talloc_strdup(ocpf->mem_ctx, lpProp.lpszW);
		}
		*value = talloc_memdup(mem_ctx, str, strlen(str) + 1);
		talloc_free(str);
		return OCPF_SUCCESS;
	case PT_SHORT:
		*value = talloc_memdup(mem_ctx, (const void *)&lpProp.i, sizeof (uint16_t));
		return OCPF_SUCCESS;
	case PT_LONG:
		*value = talloc_memdup(mem_ctx, (const void *)&lpProp.l, sizeof (uint32_t));
		return OCPF_SUCCESS;
	case PT_BOOLEAN:
		*value = talloc_memdup(mem_ctx, (const void *)&lpProp.b, sizeof (uint8_t));
		return OCPF_SUCCESS;
	case PT_ERROR:
		*value = talloc_memdup(mem_ctx, (const void *)&lpProp.err, sizeof (uint32_t));
		return OCPF_SUCCESS;
	case PT_DOUBLE:
		*value = talloc_memdup(mem_ctx, (const void *)&lpProp.d, sizeof (uint64_t));
		return OCPF_SUCCESS;
	case PT_SYSTIME:
		*value = talloc_memdup(mem_ctx, (const void *)&lpProp.ft, sizeof (struct FILETIME));
		return OCPF_SUCCESS;
	case PT_BINARY:
		*value = (const void *)talloc_zero(mem_ctx, struct Binary_r);
		((struct Binary_r *)*value)->cb = lpProp.bin.cb;
		((struct Binary_r *)*value)->lpb = talloc_memdup(mem_ctx, (const void *)lpProp.bin.lpb, lpProp.bin.cb);
		return OCPF_SUCCESS;
	case PT_MV_STRING8:
		*value = (const void *)talloc_zero(mem_ctx, struct StringArray_r);
		((struct StringArray_r *)*value)->cValues = lpProp.MVszA.cValues;
		((struct StringArray_r *)*value)->lppszA = talloc_array(mem_ctx, const char *, lpProp.MVszA.cValues);
		{
			uint32_t	i;

			for (i = 0; i < lpProp.MVszA.cValues; i++) {
				if (unescape) {
					str = ocpf_write_unescape_string(lpProp.MVszA.lppszA[i]);
				} else {
					str = (char *)lpProp.MVszA.lppszA[i];
				}
				((struct StringArray_r *)*value)->lppszA[i] = talloc_strdup(mem_ctx, str);
				talloc_free(str);
			}
		}
		return OCPF_SUCCESS;
	default:
		OCPF_WARN(("%s (0x%.4x)", OCPF_WARN_PROP_TYPE, proptype));
		return OCPF_ERROR;
	}
}

int ocpf_propvalue_free(union SPropValue_CTR lpProp, uint16_t proptype)
{
	switch (proptype) {
	case PT_STRING8:
		talloc_free((char *)lpProp.lpszA);
		break;
	case PT_UNICODE:
		talloc_free((char *)lpProp.lpszW);
		break;
	case PT_MV_STRING8:
		talloc_free(lpProp.MVszA.lppszA);
		break;
	}
	return OCPF_SUCCESS;
}

int ocpf_propvalue(uint32_t aulPropTag, union SPropValue_CTR lpProp, uint16_t proptype, bool unescape)
{
	struct ocpf_property	*element;
	int			ret;

	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;

	/* Sanity check: do not insert the same property twice */
	for (element = ocpf->props; element->next; element = element->next) {
		OCPF_RETVAL_IF(element->aulPropTag == aulPropTag, OCPF_WARN_PROP_REGISTERED, NULL);
	}

	element = NULL;
	element = talloc_zero(ocpf->mem_ctx, struct ocpf_property);
	element->aulPropTag = aulPropTag;
	ret = ocpf_set_propvalue((TALLOC_CTX *)element, &element->value, (uint16_t)aulPropTag & 0xFFFF, proptype, lpProp, unescape);
	if (ret == -1) {
		talloc_free(element);
		return OCPF_ERROR;
	}

	DLIST_ADD(ocpf->props, element);
	return OCPF_SUCCESS;
}


void ocpf_propvalue_s(const char *propname, union SPropValue_CTR lpProp, uint16_t proptype, bool unescape)
{
	uint32_t	aulPropTag;

	aulPropTag = get_proptag_value(propname);
	ocpf_propvalue(aulPropTag, lpProp, proptype, unescape);
}


/**
   \details Add a named property

   This function adds either a custom or a known named property and
   supplies either the lpProp value or substitute with registered
   variable.

   \param nprop pointer on a ocpf named property entry
   \param lpProp named property value
   \param var_name variable name
   \param proptype variable property type
   \param unescape whether the property value should be escaped

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR.
 */
int ocpf_nproperty_add(struct ocpf_nprop *nprop, union SPropValue_CTR lpProp,
		       const char *var_name, uint16_t proptype, bool unescape)
{
	enum MAPISTATUS		retval;
	int			ret = 0;
	struct ocpf_nproperty	*element;
	struct ocpf_nproperty	*el;
	struct ocpf_var		*vel;

	if (!ocpf || !ocpf->mem_ctx) return -1;

	element = talloc_zero(ocpf->mem_ctx, struct ocpf_nproperty);

	if (nprop->guid) {
		ret = ocpf_oleguid_check(nprop->guid, &element->oleguid);
		OCPF_RETVAL_IF(ret == -1, OCPF_WARN_OLEGUID_UNREGISTERED, element);
	}

	if (nprop->OOM) {
		/*
		 * Sanity check: do not insert twice the same
		 * (OOM,oleguid) couple
		 */
		for (el = ocpf->nprops; el->next; el = el->next) {
			OCPF_RETVAL_IF((el->OOM && !strcmp(el->OOM, nprop->OOM)) &&
				       (el->oleguid && nprop->guid && !strcmp(el->oleguid, nprop->guid)),
				       OCPF_WARN_OOM_REGISTERED, element);
		}

		element->kind = OCPF_OOM;
		element->OOM = nprop->OOM;
		retval = mapi_nameid_OOM_lookup(element->OOM, element->oleguid,
						&element->propType);
		OCPF_RETVAL_IF(retval != MAPI_E_SUCCESS, OCPF_WARN_OOM_UNKNOWN, element);
	} else if (nprop->mnid_string) {
		/* 
		 * Sanity check: do not insert twice the same
		 * (mnid_string,oleguid) couple 
		 */
		for (el = ocpf->nprops; el->next; el = el->next) {
			OCPF_RETVAL_IF((el->mnid_string && !strcmp(el->mnid_string, nprop->mnid_string)) &&
				       (el->oleguid && nprop->guid && !strcmp(el->oleguid, nprop->guid)),
				       OCPF_WARN_STRING_REGISTERED, element);
		}

		element->kind = OCPF_MNID_STRING;
		element->mnid_string = nprop->mnid_string;
		if (nprop->registered == true) {
			retval = mapi_nameid_string_lookup(element->mnid_string, 
							   element->oleguid,
							   &element->propType);
			OCPF_RETVAL_IF(retval != MAPI_E_SUCCESS, OCPF_WARN_STRING_UNKNOWN, element);
		} else {
			element->propType = nprop->propType;
		}
	} else if (nprop->mnid_id) {
		/* 
		 * Sanity check: do not insert twice the same
		 * (mnid_id-oleguid) couple 
		 */
		for (el = ocpf->nprops; el->next; el = el->next) {
			OCPF_RETVAL_IF((el->mnid_id == nprop->mnid_id) && 
				       (el->oleguid && nprop->guid && !strcmp(el->oleguid, nprop->guid)),
				       OCPF_WARN_LID_REGISTERED, element);
		}
		element->kind = OCPF_MNID_ID;
		element->mnid_id = nprop->mnid_id;
		if (nprop->registered == true) {
			retval = mapi_nameid_lid_lookup(element->mnid_id,
							element->oleguid,
							&element->propType);
			OCPF_RETVAL_IF(retval != MAPI_E_SUCCESS, OCPF_WARN_LID_UNKNOWN, element);
		} else {
			element->propType = nprop->propType;
		}
	}

	if (var_name) {
		for (vel = ocpf->vars; vel->next; vel = vel->next) {
			if (vel->name && !strcmp(vel->name, var_name)) {
				OCPF_RETVAL_IF(element->propType != vel->propType, OCPF_WARN_PROPVALUE_MISMATCH, element);
				element->value = vel->value;
			}
		}
		OCPF_RETVAL_IF(!element->value, OCPF_WARN_VAR_NOT_REGISTERED, element);
	} else {
		ret = ocpf_set_propvalue((TALLOC_CTX *)element, &element->value, element->propType, proptype, lpProp, unescape);
		if (ret == -1) {
			talloc_free(element);
			return OCPF_ERROR;
		}
	}

	DLIST_ADD(ocpf->nprops, element);

	return OCPF_SUCCESS;
}


/**
   \details Register OCPF message type
   
   Register OCPF message type

   \param type message type to register

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
int ocpf_type_add(const char *type)
{
	if (!ocpf || !ocpf->mem_ctx || !type) return OCPF_ERROR;

	ocpf->type = talloc_strdup(ocpf->mem_ctx, type);

	return OCPF_SUCCESS;
}


/* WARNING: This array doesn't hold all possible values */
static struct ocpf_olfolder olfolders[] = {
	{ olFolderTopInformationStore,	"olFolderTopInformationStore"	},
	{ olFolderDeletedItems,		"olFolderDeletedItems"		},
	{ olFolderOutbox,		"olFolderOutbox"		},
	{ olFolderSentMail,		"olFolderSentMail"		},
	{ olFolderInbox,		"olFolderInbox"			},
	{ olFolderCommonView,		"olFolderCommonView"		},
	{ olFolderCalendar,		"olFolderCalendar"		},
	{ olFolderContacts,		"olFolderContacts"		},
	{ olFolderJournal,		"olFolderJournal"		},
	{ olFolderNotes,		"olFolderNotes"			},
	{ olFolderTasks,		"olFolderTasks"			},
	{ 0, NULL }
};

static int64_t ocpf_folder_name_to_id(const char *name)
{
	uint32_t	i;

	if (!name) return OCPF_ERROR;

	for (i = 0; olfolders[i].name; i++) {
		if (olfolders[i].name && !strcmp(olfolders[i].name, name)) {
			return olfolders[i].id;
		}
	}
	return OCPF_ERROR;
}

/**
   \details Register OCPF folder

   Register the folder where the OCPF message needs to be saved

   \param name the name of the default folder if specified
   \param id the folder id of the message if specified
   \param var_name the substitution variable to use for folder ID if
   specified

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
int ocpf_folder_add(const char *name, uint64_t id, const char *var_name)
{
	struct ocpf_var		*element;

	/* Sanity check */
	if ((name && id) || (name && var_name) || (id && var_name)) return OCPF_ERROR;
	if (!name && !id && !var_name) return OCPF_ERROR;

	if (name) {
		ocpf->folder = (uint64_t) ocpf_folder_name_to_id(name);
		OCPF_RETVAL_IF(ocpf->folder == -1, OCPF_WARN_FOLDER_ID_UNKNOWN, NULL);
	} else if (id) {
		ocpf->folder = id;
	} else if (var_name) {
		for (element = ocpf->vars; element->next; element = element->next) {
			if (element->name && !strcmp(element->name, var_name)) {
				/* WARNING: we assume var data is double */
				ocpf->folder = *((uint64_t *)element->value);
			}
		}
	}

	return OCPF_SUCCESS;
}


/**
   \details Register new OLEGUID in ocpf context

   This function registers a OLEGUID couple name, value in ocpf.

   \param name the OLEGUID name
   \param oleguid the guid string value

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
int ocpf_oleguid_add(const char *name, const char *oleguid)
{
	NTSTATUS		status;
	struct ocpf_oleguid	*element;
	struct GUID		guid;

	/* Sanity checks */
	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!name) return OCPF_ERROR;

	/* Sanity check: Do not insert twice the same name or guid */
	for (element = ocpf->oleguid; element->next; element = element->next) {
		OCPF_RETVAL_IF(element->name && !strcmp(element->name, name),
			       OCPF_WARN_OLEGUID_N_REGISTERED, NULL);

		OCPF_RETVAL_IF(element->guid && !strcmp(element->guid, oleguid),
			       OCPF_WARN_OLEGUID_G_REGISTERED, NULL);
	}

	element = talloc_zero(ocpf->mem_ctx, struct ocpf_oleguid);

	status = GUID_from_string(oleguid, &guid);
	OCPF_RETVAL_IF(!NT_STATUS_IS_OK(status), OCPF_WARN_OLEGUID_INVALID, element);

	element->name = talloc_strdup(ocpf->mem_ctx, name);
	element->guid = talloc_strdup(ocpf->mem_ctx, oleguid);

	DLIST_ADD(ocpf->oleguid, element);

	return OCPF_SUCCESS;
}


/**
   \details Check if the given OLEGUID has been registered

   \param name the OLEGUID to check
   \param guid pointer on pointer to the guid result

   \result OCPF_SUCCESS on success, otherwise OCPF_ERROR;
 */
int ocpf_oleguid_check(const char *name, const char **guid)
{
	struct ocpf_oleguid	*element;

	for (element = ocpf->oleguid; element->next; element = element->next) {	
		if (element->name && !strcmp(element->name, name)) {
			*guid = element->guid;
			return OCPF_SUCCESS;
		}
	}

	return OCPF_ERROR;
}


/**
   \details convert a string to FILETIME structure

   This function converts a string - representing a date under the
   following format "Tyyy-mm-dd hh:mm:ss" - into a FILETIME structure.

   \param date the date to convert
   \param ft pointer on the converted date

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
int ocpf_add_filetime(const char *date, struct FILETIME *ft)
{
	NTTIME		nt;
	struct tm	tm;

	if (!strptime(date, DATE_FORMAT, &tm)) {
		printf("Invalid data format: Tyyy-mm-dd hh:mm:ss (e.g.: T2008-03-06 23:30:00");
		return OCPF_ERROR;
	}
	
	unix_to_nt_time(&nt, mktime(&tm));
	ft->dwLowDateTime = (nt << 32) >> 32;
	ft->dwHighDateTime = (nt >> 32);

	return OCPF_SUCCESS;
}


int ocpf_variable_add(const char *name, union SPropValue_CTR lpProp, uint16_t propType, bool unescape)
{
	struct ocpf_var		*element;
	int			ret;

	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!name) return OCPF_ERROR;

	/* Sanity check: Do not insert twice the same variable */
	for (element = ocpf->vars; element->next; element = element->next) {
		OCPF_RETVAL_IF(element->name && !strcmp(element->name, name),
			       OCPF_WARN_VAR_REGISTERED, NULL);
	}

	element = talloc_zero(ocpf->mem_ctx, struct ocpf_var);
	element->name = talloc_strdup((TALLOC_CTX *)element, name);
	element->propType = propType;

	ret = ocpf_set_propvalue((TALLOC_CTX *)element, &element->value, propType, propType, lpProp, unescape);
	OCPF_RETVAL_IF(ret == -1, OCPF_WARN_VAR_TYPE, element);

	DLIST_ADD(ocpf->vars, element);

	return OCPF_SUCCESS;
}


int ocpf_binary_add(const char *filename, struct Binary_r *bin)
{
	int		fd;
	struct stat	sb;

	fd = open(filename, O_RDONLY);
	OCPF_RETVAL_IF(fd == -1, OCPF_WARN_FILENAME_INVALID, NULL);
	OCPF_RETVAL_IF(fstat(fd, &sb), OCPF_WARN_FILENAME_STAT, NULL);
	
	bin->lpb = talloc_size(ocpf->mem_ctx, sb.st_size);
	bin->cb = read(fd, bin->lpb, sb.st_size);

	close(fd);

	return OCPF_SUCCESS;
}

int ocpf_recipient_add(uint8_t recipClass, char *recipient)
{
	struct ocpf_recipients	*element;

	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!recipient) return OCPF_ERROR;

	element = talloc_zero(ocpf->mem_ctx, struct ocpf_recipients);
	element->name = talloc_strdup((TALLOC_CTX *)element, recipient);
	element->class = recipClass;

	DLIST_ADD(ocpf->recipients, element);

	return OCPF_SUCCESS;
}
