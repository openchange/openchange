/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008-2013.

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

#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"
#include "libocpf/ocpf_private.h"

/**
   \file ocpf_api.c

   \brief ocpf Private API
 */


void ocpf_do_debug(struct ocpf_context *ctx, const char *format, ...)
{
	va_list ap;
	char	*s = NULL;
	int	ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	if (ret == -1) {
		printf("%s:%d: [Debug dump failure]\n", ctx->filename, ctx->lineno);
		fflush(0);
		return;
	}
	if (ctx) {
		printf("%s:%d: %s\n", ctx->filename, ctx->lineno, s);
		fflush(0);
	} else {
		printf("%s\n", s);
		fflush(0);
	}
	free(s);
}


int ocpf_propvalue_var(struct ocpf_context *ctx,
		       const char *propname, 
		       uint32_t proptag, 
		       const char *variable, 
		       bool unescape,
		       int scope)
{
	struct ocpf_var		*vel;
	struct ocpf_property	*element;
	uint32_t		aulPropTag;
	uint32_t		cRows;
	struct SRow		aRow;
	struct SPropValue	lpProps;
	void			*value;
	int			i;

	if (!ocpf || !ocpf->mem_ctx) return -1;
	if (!propname && !proptag) return -1;
	if (propname && proptag) return -1;

	/* Sanity check: do not insert the same property twice */
	if (proptag) {
		aulPropTag = proptag;
	} else {
		aulPropTag = get_proptag_value(propname);
	}

	switch (scope) {
	case kw_PROPERTY:
		for (element = ctx->props; element->next; element = element->next) {
			OCPF_RETVAL_IF(element->aulPropTag == aulPropTag, ctx, OCPF_WARN_PROP_REGISTERED, NULL);
		}
		for (vel = ctx->vars; vel->next; vel = vel->next) {
			if (vel->name && !strcmp(vel->name, variable)) {
				OCPF_RETVAL_IF(vel->propType != (aulPropTag & 0xFFFF), ctx, OCPF_WARN_PROPVALUE_MISMATCH, NULL);
				element = NULL;
				element = talloc_zero(ctx->vars, struct ocpf_property);
				element->aulPropTag = aulPropTag;
				if (unescape && (((aulPropTag & 0xFFFF) == PT_STRING8) || 
						 ((aulPropTag & 0xFFFF) == PT_UNICODE))) {
					element->value = ocpf_write_unescape_string(ctx, vel->value);
				} else {
					element->value = vel->value;
				}
				DLIST_ADD(ctx->props, element);
				return OCPF_SUCCESS;
			}
		}
		break;
	case kw_RECIPIENT:
		cRows = ctx->recipients->cRows;
		aRow = ctx->recipients->aRow[cRows];
		for (i = 0; i < aRow.cValues; i++) {
			OCPF_RETVAL_IF(aRow.lpProps[i].ulPropTag == aulPropTag, ctx, OCPF_WARN_PROP_REGISTERED, NULL);
		}
		for (vel = ctx->vars; vel->next; vel = vel->next) {
			if (vel->name && !strcmp(vel->name, variable)) {
				OCPF_RETVAL_IF(vel->propType != (aulPropTag & 0xFFFF), ctx, OCPF_WARN_PROPVALUE_MISMATCH, NULL);
				lpProps.ulPropTag = aulPropTag;
				lpProps.dwAlignPad = 0;
				if (unescape && (((aulPropTag & 0xFFFF) == PT_STRING8) ||
						 ((aulPropTag & 0xFFFF) == PT_UNICODE))) {
					value = ocpf_write_unescape_string(ctx, vel->value);
				} else {
					value = (void *)vel->value;
				}
				set_SPropValue(&lpProps, value);

				if (!aRow.cValues) {
					aRow.lpProps = talloc_array((TALLOC_CTX *)ctx->recipients->aRow, struct SPropValue, 2);
				} else {
					aRow.lpProps = talloc_realloc(aRow.lpProps, aRow.lpProps, 
								      struct SPropValue, aRow.cValues + 2);
				}
				aRow.lpProps[aRow.cValues] = lpProps;
				aRow.cValues += 1;
				ctx->recipients->aRow[cRows] = aRow;
				return OCPF_SUCCESS;
			}
		}
		break;
	default:
		break;
	}

	OCPF_RETVAL_IF(1, ctx, OCPF_WARN_VAR_NOT_REGISTERED, NULL);
}


int ocpf_set_propvalue(TALLOC_CTX *mem_ctx, 
		       struct ocpf_context *ctx,
		       const void **value, 
		       uint16_t proptype, 
		       uint16_t sproptype, 
		       union SPropValue_CTR lpProp, 
		       bool unescape)
{
	char	*str = NULL;

	OCPF_RETVAL_IF(proptype != sproptype, ctx, OCPF_WARN_PROPVALUE_MISMATCH, NULL);

	switch (proptype) {
	case PT_STRING8:
		if (unescape) {
			str = ocpf_write_unescape_string(ctx, (const char *) lpProp.lpszA);
		} else {
			str = talloc_strdup(ctx, (const char *) lpProp.lpszA);
		}
		*value = talloc_memdup(ctx, str, strlen(str) + 1);
		talloc_free(str);
		return OCPF_SUCCESS;
	case PT_UNICODE:
		if (unescape) {
			str = ocpf_write_unescape_string(ctx, lpProp.lpszW);
		} else {
			str = talloc_strdup(ctx, lpProp.lpszW);
		}
		*value = talloc_memdup(ctx, str, strlen(str) + 1);
		talloc_free(str);
		return OCPF_SUCCESS;
	case PT_SHORT:
		*value = talloc_memdup(ctx, (const void *)&lpProp.i, sizeof (uint16_t));
		return OCPF_SUCCESS;
	case PT_LONG:
		*value = talloc_memdup(ctx, (const void *)&lpProp.l, sizeof (uint32_t));
		return OCPF_SUCCESS;
	case PT_DOUBLE:
		*value = talloc_memdup(ctx, (const void *)&lpProp.dbl, sizeof (uint64_t));
		return OCPF_SUCCESS;
	case PT_BOOLEAN:
		*value = talloc_memdup(ctx, (const void *)&lpProp.b, sizeof (uint8_t));
		return OCPF_SUCCESS;
	case PT_ERROR:
		*value = talloc_memdup(ctx, (const void *)&lpProp.err, sizeof (uint32_t));
		return OCPF_SUCCESS;
	case PT_I8:
		*value = talloc_memdup(ctx, (const void *)&lpProp.d, sizeof (uint64_t));
		return OCPF_SUCCESS;
	case PT_SYSTIME:
		*value = talloc_memdup(ctx, (const void *)&lpProp.ft, sizeof (struct FILETIME));
		return OCPF_SUCCESS;
	case PT_SVREID:
	case PT_BINARY:
		*value = (const void *)talloc_zero(ctx, struct Binary_r);
		((struct Binary_r *)*value)->cb = lpProp.bin.cb;
		((struct Binary_r *)*value)->lpb = talloc_memdup(ctx, (const void *)lpProp.bin.lpb, lpProp.bin.cb);
		return OCPF_SUCCESS;
	case PT_MV_LONG:
		*value = (const void *)talloc_zero(ctx, struct LongArray_r);
		((struct LongArray_r *)*value)->cValues = lpProp.MVl.cValues;
		((struct LongArray_r *)*value)->lpl = talloc_array(ctx, uint32_t, lpProp.MVl.cValues);
		{
			uint32_t	i;
			
			for (i = 0; i < lpProp.MVl.cValues; i++) {
				((struct LongArray_r *)*value)->lpl[i] = lpProp.MVl.lpl[i];
			}
			return OCPF_SUCCESS;
		}
	case PT_MV_STRING8:
		*value = (const void *)talloc_zero(ctx, struct StringArray_r);
		((struct StringArray_r *)*value)->cValues = lpProp.MVszA.cValues;
		((struct StringArray_r *)*value)->lppszA = talloc_array(ctx, uint8_t *, lpProp.MVszA.cValues);
		{
			uint32_t	i;

			for (i = 0; i < lpProp.MVszA.cValues; i++) {
				if (unescape) {
					str = ocpf_write_unescape_string(ctx, (const char *) lpProp.MVszA.lppszA[i]);
				} else {
					str = (char *)lpProp.MVszA.lppszA[i];
				}
				((struct StringArray_r *)*value)->lppszA[i] = (uint8_t *) talloc_strdup(ctx, str);
				talloc_free(str);
			}
		}
		return OCPF_SUCCESS;
	case PT_MV_UNICODE:
		*value = (const void *)talloc_zero(ctx, struct StringArrayW_r);
		((struct StringArrayW_r *)*value)->cValues = lpProp.MVszW.cValues;
		((struct StringArrayW_r *)*value)->lppszW = talloc_array(ctx, const char *, lpProp.MVszW.cValues);
		{
			uint32_t	i;

			for (i = 0; i < lpProp.MVszW.cValues; i++) {
				if (unescape) {
					str = ocpf_write_unescape_string(ctx, lpProp.MVszW.lppszW[i]);
				} else {
					str = (char *)lpProp.MVszW.lppszW[i];
				}
				((struct StringArrayW_r *)*value)->lppszW[i] = talloc_strdup(ctx, str);
				talloc_free(str);
			}
		}
		return OCPF_SUCCESS;
	case PT_MV_BINARY:
		*value = (const void *)talloc_zero(ctx, struct BinaryArray_r);
		((struct BinaryArray_r *)*value)->cValues = lpProp.MVbin.cValues;
		((struct BinaryArray_r *)*value)->lpbin = talloc_array(ctx, struct Binary_r, lpProp.MVbin.cValues);
		{
			uint32_t	i;

			for (i = 0; i < lpProp.MVbin.cValues; i++) {
				((struct BinaryArray_r *)*value)->lpbin[i].cb = lpProp.MVbin.lpbin[i].cb;
				((struct BinaryArray_r *)*value)->lpbin[i].lpb = talloc_memdup(((struct BinaryArray_r *)*value)->lpbin,
											       lpProp.MVbin.lpbin[i].lpb, lpProp.MVbin.lpbin[i].cb);
			}
		}
		return OCPF_SUCCESS;
	default:
		ocpf_do_debug(ctx, "%s (0x%.4x)", OCPF_WARN_PROP_TYPE, proptype);
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
	case PT_MV_LONG:
		talloc_free(lpProp.MVl.lpl);
		break;
	case PT_MV_STRING8:
		talloc_free(lpProp.MVszA.lppszA);
		break;
	case PT_MV_UNICODE:
		talloc_free(lpProp.MVszW.lppszW);
		break;
	case PT_MV_BINARY:
		talloc_free(lpProp.MVbin.lpbin);
		break;
	}
	return OCPF_SUCCESS;
}

int ocpf_propvalue(struct ocpf_context *ctx,
		   uint32_t aulPropTag, 
		   union SPropValue_CTR lpProp, 
		   uint16_t proptype, 
		   bool unescape,
		   int scope)
{
	struct ocpf_property	*element;
	int			ret;
	uint32_t		cRows;
	struct SRow		aRow;
	struct SPropValue	lpProps;
	void			*value;
	int			i;

	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!ctx) return OCPF_ERROR;

	switch (scope) {
	case kw_PROPERTY:
		/* Sanity check: do not insert the same property twice */
		for (element = ctx->props; element->next; element = element->next) {
			OCPF_RETVAL_IF(element->aulPropTag == aulPropTag, ctx, OCPF_WARN_PROP_REGISTERED, NULL);
		}

		element = NULL;
		element = talloc_zero(ctx->props, struct ocpf_property);
		if ((aulPropTag & 0xFFFF) == PT_STRING8) {
			element->aulPropTag = (aulPropTag & 0xFFFF0000) + PT_UNICODE;
		} else {
			element->aulPropTag = aulPropTag;
		}
		ret = ocpf_set_propvalue((TALLOC_CTX *)element, ctx, &element->value, 
					 (uint16_t)aulPropTag & 0xFFFF, proptype, lpProp, unescape);
		if (ret == -1) {
			talloc_free(element);
			return OCPF_ERROR;
		}

		DLIST_ADD(ctx->props, element);
		break;
	case kw_RECIPIENT:
		cRows = ctx->recipients->cRows;
		aRow = ctx->recipients->aRow[cRows];
		for (i = 0; i < aRow.cValues; i++) {
			OCPF_RETVAL_IF(aRow.lpProps[i].ulPropTag == aulPropTag, ctx, OCPF_WARN_PROP_REGISTERED, NULL);
		}

		lpProps.ulPropTag = aulPropTag;
		ret = ocpf_set_propvalue((TALLOC_CTX *)ctx->recipients->aRow, ctx, (const void **)&value, 
					 (uint16_t)aulPropTag & 0xFFFF, proptype, lpProp, unescape);
		if (ret == -1) {
			return OCPF_ERROR;
		}
		set_SPropValue(&lpProps, value);

		if (!aRow.cValues) {
			aRow.lpProps = talloc_array((TALLOC_CTX *)ctx->recipients->aRow, struct SPropValue, 2);
		} else {
			aRow.lpProps = talloc_realloc(aRow.lpProps, aRow.lpProps, struct SPropValue, aRow.cValues + 2);
		}
		aRow.lpProps[aRow.cValues] = lpProps;
		aRow.cValues += 1;
		ctx->recipients->aRow[cRows] = aRow;
		break;
	default:
		break;
	}

	return OCPF_SUCCESS;
}

int ocpf_new_recipient(struct ocpf_context *ctx)
{
	uint32_t	cRows;
	
	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!ctx->recipients || !ctx->recipients->aRow) return OCPF_ERROR;

	ctx->recipients->cRows += 1;
	cRows = ctx->recipients->cRows;

	ctx->recipients->aRow = talloc_realloc(ctx->recipients->aRow, ctx->recipients->aRow, struct SRow, cRows + 2);
	ctx->recipients->aRow[cRows].ulAdrEntryPad = 0;
	ctx->recipients->aRow[cRows].lpProps = NULL;
	ctx->recipients->aRow[cRows].cValues = 0;

	return OCPF_SUCCESS;
}

int ocpf_recipient_set_class(struct ocpf_context *ctx, enum ulRecipClass class)
{
	struct SPropValue	lpProps;
	uint32_t		cRows;
	struct SRow		aRow;
	int			i;

	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!ctx->recipients || !ctx->recipients->aRow) return OCPF_ERROR;

	cRows = ctx->recipients->cRows;
	aRow = ctx->recipients->aRow[cRows];

	/* Check if PidTagRecipientType has not been declared as a block property */
	for (i = 0; i < aRow.cValues; i++) {
		if (aRow.lpProps[i].ulPropTag == PidTagRecipientType) {
			if (aRow.lpProps[i].value.l == class) {
				return OCPF_SUCCESS;
			} else {
				OCPF_RETVAL_IF(1, ctx, OCPF_WARN_PROP_REGISTERED, NULL);
			}
		}
	}

	lpProps.ulPropTag = PidTagRecipientType;
	lpProps.dwAlignPad = 0;
	set_SPropValue(&lpProps, (void *)&class);
	
	if (!aRow.cValues) {
		aRow.lpProps = talloc_array((TALLOC_CTX *)ctx->recipients->aRow, struct SPropValue, 2);
	} else {
		aRow.lpProps = talloc_realloc(aRow.lpProps, aRow.lpProps, struct SPropValue, aRow.cValues + 2);
	}

	aRow.lpProps[aRow.cValues] = lpProps;
	aRow.cValues += 1;
	ctx->recipients->aRow[cRows] = aRow;

	return OCPF_SUCCESS;
}

void ocpf_propvalue_s(struct ocpf_context *ctx,
		      const char *propname, 
		      union SPropValue_CTR lpProp, 
		      uint16_t proptype, 
		      bool unescape,
		      int scope)
{
	uint32_t	aulPropTag;

	aulPropTag = get_proptag_value(propname);
	ocpf_propvalue(ctx, aulPropTag, lpProp, proptype, unescape, scope);
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
int ocpf_nproperty_add(struct ocpf_context *ctx,
		       struct ocpf_nprop *nprop, 
		       union SPropValue_CTR lpProp,
		       const char *var_name, 
		       uint16_t proptype, 
		       bool unescape)
{
	enum MAPISTATUS		retval;
	int			ret = 0;
	struct ocpf_nproperty	*element;
	struct ocpf_nproperty	*el;
	struct ocpf_var		*vel;

	if (!ocpf || !ocpf->mem_ctx) return -1;

	element = talloc_zero(ctx, struct ocpf_nproperty);

	if (nprop->guid) {
		ret = ocpf_oleguid_check(ctx, nprop->guid, &element->oleguid);
		OCPF_RETVAL_IF(ret == -1, ctx, OCPF_WARN_OLEGUID_UNREGISTERED, element);
	}

	if (nprop->OOM) {
		/*
		 * Sanity check: do not insert twice the same
		 * (OOM,oleguid) couple
		 */
		for (el = ctx->nprops; el->next; el = el->next) {
			OCPF_RETVAL_IF((el->OOM && !strcmp(el->OOM, nprop->OOM)) &&
				       (el->oleguid && nprop->guid && !strcmp(el->oleguid, nprop->guid)),
				       ctx, OCPF_WARN_OOM_REGISTERED, element);
		}

		element->kind = OCPF_OOM;
		element->OOM = nprop->OOM;
		retval = mapi_nameid_OOM_lookup(element->OOM, element->oleguid,
						&element->propType);
		OCPF_RETVAL_IF(retval != MAPI_E_SUCCESS, ctx, OCPF_WARN_OOM_UNKNOWN, element);
	} else if (nprop->mnid_string) {
		/* 
		 * Sanity check: do not insert twice the same
		 * (mnid_string,oleguid) couple 
		 */
		for (el = ctx->nprops; el->next; el = el->next) {
			OCPF_RETVAL_IF((el->mnid_string && !strcmp(el->mnid_string, nprop->mnid_string)) &&
				       (el->oleguid && nprop->guid && !strcmp(el->oleguid, nprop->guid)),
				       ctx, OCPF_WARN_STRING_REGISTERED, element);
		}

		element->kind = OCPF_MNID_STRING;
		element->mnid_string = nprop->mnid_string;
		if (nprop->registered == true) {
			retval = mapi_nameid_string_lookup(element->mnid_string, 
							   element->oleguid,
							   &element->propType);
			OCPF_RETVAL_IF(retval != MAPI_E_SUCCESS, ctx, OCPF_WARN_STRING_UNKNOWN, element);
		} else {
			element->propType = nprop->propType;
		}
	} else if (nprop->mnid_id) {
		/* 
		 * Sanity check: do not insert twice the same
		 * (mnid_id-oleguid) couple 
		 */
		for (el = ctx->nprops; el->next; el = el->next) {
			OCPF_RETVAL_IF((el->mnid_id == nprop->mnid_id) && 
				       (el->oleguid && nprop->guid && !strcmp(el->oleguid, nprop->guid)),
				       ctx, OCPF_WARN_LID_REGISTERED, element);
		}
		element->kind = OCPF_MNID_ID;
		element->mnid_id = nprop->mnid_id;
		if (nprop->registered == true) {
			retval = mapi_nameid_lid_lookup(element->mnid_id,
							element->oleguid,
							&element->propType);
			OCPF_RETVAL_IF(retval != MAPI_E_SUCCESS, ctx, OCPF_WARN_LID_UNKNOWN, element);
		} else {
			element->propType = nprop->propType;
		}
	}

	if (var_name) {
		for (vel = ctx->vars; vel->next; vel = vel->next) {
			if (vel->name && !strcmp(vel->name, var_name)) {
				OCPF_RETVAL_IF(element->propType != vel->propType, ctx, OCPF_WARN_PROPVALUE_MISMATCH, element);
				element->value = vel->value;
			}
		}
		OCPF_RETVAL_IF(!element->value, ctx, OCPF_WARN_VAR_NOT_REGISTERED, element);
	} else {
		ret = ocpf_set_propvalue((TALLOC_CTX *)element, ctx, &element->value, 
					 element->propType, proptype, lpProp, unescape);
		if (ret == -1) {
			talloc_free(element);
			return OCPF_ERROR;
		}
	}

	DLIST_ADD(ctx->nprops, element);

	return OCPF_SUCCESS;
}

/**
   \details Register OCPF message type
   
   Register OCPF message type

   \param type message type to register

   \return OCPF_SUCCESS on success, otherwise OCPF_ERROR
 */
int ocpf_type_add(struct ocpf_context *ctx, const char *type)
{
	if (!ocpf || !ocpf->mem_ctx || !type) return OCPF_ERROR;

	if (ctx->type) {
		talloc_free((void *)ctx->type);
		ctx->type = NULL;
	}
	ctx->type = talloc_strdup(ctx, type);

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

static int ocpf_folder_name_to_id(const char *name, uint64_t *id)
{
	uint32_t	i;

	if (!name) return OCPF_ERROR;

	for (i = 0; olfolders[i].name; i++) {
		if (olfolders[i].name && !strcmp(olfolders[i].name, name)) {
			*id =  olfolders[i].id;
			return OCPF_SUCCESS;
		}
	}
	/* not found */
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
int ocpf_folder_add(struct ocpf_context *ctx, 
		    const char *name, 
		    uint64_t id, 
		    const char *var_name)
{
	struct ocpf_var		*element;

	/* Sanity check */
	if ((name && id) || (name && var_name) || (id && var_name)) return OCPF_ERROR;
	if (!name && !id && !var_name) return OCPF_ERROR;

	if (name) {
		int res = ocpf_folder_name_to_id(name, &(ctx->folder));
		OCPF_RETVAL_IF(res == OCPF_ERROR, ctx, OCPF_WARN_FOLDER_ID_UNKNOWN, NULL);
	} else if (id) {
		ctx->folder = id;
	} else if (var_name) {
		for (element = ctx->vars; element->next; element = element->next) {
			if (element->name && !strcmp(element->name, var_name)) {
				/* WARNING: we assume var data is double */
				ctx->folder = *((uint64_t *)element->value);
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
int ocpf_oleguid_add(struct ocpf_context *ctx,
		     const char *name, 
		     const char *oleguid)
{
	NTSTATUS		status;
	struct ocpf_oleguid	*element;
	struct GUID		guid;

	/* Sanity checks */
	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!name) return OCPF_ERROR;

	/* Sanity check: Do not insert twice the same name or guid */
	for (element = ctx->oleguid; element->next; element = element->next) {
		OCPF_RETVAL_IF(element->name && !strcmp(element->name, name),
			       ctx, OCPF_WARN_OLEGUID_N_REGISTERED, NULL);

		OCPF_RETVAL_IF(element->guid && !strcmp(element->guid, oleguid),
			       ctx, OCPF_WARN_OLEGUID_G_REGISTERED, NULL);
	}

	element = talloc_zero(ctx->oleguid, struct ocpf_oleguid);

	status = GUID_from_string(oleguid, &guid);
	OCPF_RETVAL_IF(!NT_STATUS_IS_OK(status), ctx, OCPF_WARN_OLEGUID_INVALID, element);

	element->name = talloc_strdup(element, name);
	element->guid = talloc_strdup(element, oleguid);

	DLIST_ADD(ctx->oleguid, element);

	return OCPF_SUCCESS;
}


/**
   \details Check if the given OLEGUID has been registered

   \param name the OLEGUID to check
   \param guid pointer on pointer to the guid result

   \result OCPF_SUCCESS on success, otherwise OCPF_ERROR;
 */
int ocpf_oleguid_check(struct ocpf_context *ctx,
		       const char *name, 
		       const char **guid)
{
	struct ocpf_oleguid	*element;

	for (element = ctx->oleguid; element->next; element = element->next) {	
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

	memset(&tm, 0, sizeof(struct tm));
	if (!strptime(date, DATE_FORMAT, &tm)) {
		printf("Invalid data format: Tyyy-mm-dd hh:mm:ss (e.g.: T2008-03-06 23:30:00");
		return OCPF_ERROR;
	}
	
	unix_to_nt_time(&nt, mktime(&tm));
	ft->dwLowDateTime = (nt << 32) >> 32;
	ft->dwHighDateTime = (nt >> 32);

	return OCPF_SUCCESS;
}


int ocpf_variable_add(struct ocpf_context *ctx,
		      const char *name, 
		      union SPropValue_CTR lpProp, 
		      uint16_t propType, 
		      bool unescape)
{
	struct ocpf_var		*element;
	int			ret;

	if (!ocpf || !ocpf->mem_ctx) return OCPF_ERROR;
	if (!name) return OCPF_ERROR;

	/* Sanity check: Do not insert twice the same variable */
	for (element = ctx->vars; element->next; element = element->next) {
		OCPF_RETVAL_IF(element->name && !strcmp(element->name, name),
			       ctx, OCPF_WARN_VAR_REGISTERED, NULL);
	}

	element = talloc_zero(ctx->vars, struct ocpf_var);
	element->name = talloc_strdup((TALLOC_CTX *)element, name);
	element->propType = propType;

	ret = ocpf_set_propvalue((TALLOC_CTX *)element, ctx, &element->value, propType, 
				 propType, lpProp, unescape);
	OCPF_RETVAL_IF(ret == -1, ctx, OCPF_WARN_VAR_TYPE, element);

	DLIST_ADD(ctx->vars, element);

	return OCPF_SUCCESS;
}


int ocpf_binary_add(struct ocpf_context *ctx, const char *filename, struct Binary_r *bin)
{
	int		fd;
	int		ret;
	struct stat	sb;

	fd = open(filename, O_RDONLY);
	OCPF_RETVAL_IF(fd == -1, ctx, OCPF_WARN_FILENAME_INVALID, NULL);

	ret = fstat(fd, &sb);
	if (ret == -1) {
		ocpf_do_debug(ctx, "%s", OCPF_WARN_FILENAME_STAT);
		close(fd);
		return OCPF_ERROR;
	}
	
	bin->lpb = talloc_size(ctx, sb.st_size);
	bin->cb = read(fd, bin->lpb, sb.st_size);

	close(fd);

	return OCPF_SUCCESS;
}
