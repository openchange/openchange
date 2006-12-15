/*
   OpenChange Project

   ems address book provider implementation

   Copyright (C) Julien Kerihuel 2006
   Copyright (C) Pauline Khun 2006

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
#include "libmapi/include/mapidefs.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/mapi_proto.h"
#include "libmapi/include/proto.h"
#include "libmapi/mapitags.h"
#include "emsabp.h"

struct mapitags_x500 emsabp_single_x500_tags[] =
{
	/* GetMatches property tags */

        { PR_DISPLAY_NAME,		"displayName"		},
	{ PR_DISPLAY_NAME_UNICODE,	"displayName"		},
        { PR_TITLE,			"personalTitle"		},
        { PR_COMPANY_NAME,		"company"		},
        { PR_EMAIL_ADDRESS,		"legacyExchangeDN"	},
        { PR_ACCOUNT,			"sAMAccountName"	},
	{ PR_ACCOUNT_UNICODE,		"saMAccountName"	},

	/* QueryRows property tags */

	{ PR_EMS_AB_PROXY_ADDRESSES,	"proxyAddresses"	},

	/* GetProps property tags */

        { PR_EMS_AB_NETWORK_ADDRESS,	"networkAddress"	},

        { 0,				NULL			}
};

struct mapitags_x500 emsabp_multi_x500_tags[] =
{
	{ PR_EMS_AB_HOME_MDB,		"homeMDB"		},

        { 0,				NULL			}
};

struct emsabp_containerID emsabp_containerID[] = 
{
	{ "All Address Lists",	0x1b50 } ,
	{ "All Contacts",	0x1b53 } ,
	{ "All Groups",		0x1b52 } ,
	{ "All Users",		0x1b51 } ,
	{ "Public Folders",	0x1b54 } ,
	{ NULL,			0x0    }
};

/*
  Initialize the context data structure and open a connection on samdb database
*/

struct emsabp_ctx *emsabp_init(TALLOC_CTX *mem_ctx, struct dcesrv_call_state *dce_call)
{
	struct emsabp_ctx	*emsabp_ctx;

	emsabp_ctx = talloc_named(mem_ctx, sizeof (struct emsabp_ctx), EMSABP_CTX);
	if (!emsabp_ctx) return NULL;
	emsabp_ctx->mem_ctx = talloc_init(EMSABP_CTX);

	/* return an opaque context pointer on the configuration database */
	emsabp_ctx->conf_ctx = ldb_init(emsabp_ctx->mem_ctx);
	if (ldb_connect(emsabp_ctx->conf_ctx, 
			private_path(emsabp_ctx->mem_ctx, "configuration.ldb"), 
			LDB_FLG_RDONLY, NULL) != LDB_SUCCESS) {
		DEBUG(0, ("Connection to the configuration database failed\n"));
		exit (-1);
	}

	/* return an opaque context pointer on the samdb database */
	emsabp_ctx->sam_ctx = ldb_init(emsabp_ctx->mem_ctx);
	if (ldb_connect(emsabp_ctx->sam_ctx, 
			private_path(emsabp_ctx->mem_ctx, "sam.ldb"),
			LDB_FLG_RDONLY, NULL) != LDB_SUCCESS) {
		DEBUG(0, ("Connection to the sam database failed\n"));
		exit (-1);
	}

	/* return an opaque context pointer on the users database*/
	emsabp_ctx->users_ctx = ldb_init(emsabp_ctx->mem_ctx);
	if (ldb_connect(emsabp_ctx->users_ctx, 
			private_path(emsabp_ctx->mem_ctx, "users.ldb"),
			LDB_FLG_RDONLY, NULL) != LDB_SUCCESS) {
		DEBUG(0, ("Connection to the users database failed\n"));
		exit (-1);
	}

	emsabp_ctx->entry_ids = NULL;

	return emsabp_ctx;
}

/*
  Add an entry in the chain list
*/

BOOL emsabp_add_entry(struct emsabp_ctx *emsabp_ctx, uint8_t *instance_key,
		      struct ldb_message *ldb_recipient)
{
	struct entry_id		*entry;

	entry = talloc(emsabp_ctx->mem_ctx, struct entry_id);
	entry->guid = samdb_result_guid(ldb_recipient, "objectGUID");
	
	entry->instance_key =  talloc_size(emsabp_ctx->mem_ctx, 4 * sizeof (uint8_t));
	
	memset(entry->instance_key, 0, 4);
	entry->instance_key[0] = entry->guid.node[4];
	entry->instance_key[1] = entry->guid.node[5];

	entry->msg = ldb_recipient;
	entry->next = emsabp_ctx->entry_ids;

	if (instance_key) {
		memset(instance_key, 0, 4);
		instance_key[0] = entry->instance_key[0];
		instance_key[1] = entry->instance_key[1];
	}

	emsabp_ctx->entry_ids = entry;

	return True;
}


/*
  search and open an entry in the Active Directory
  PR_ANR is mapped under PR_ACCOUNT_NAME
  PR_ACCOUNT_NAME is represented in the AD by the samrAccountName attribute
*/

NTSTATUS emsabp_search(struct emsabp_ctx *emsabp_ctx, uint8_t ***instance_keys, struct SRestriction *restriction)
{
	enum ldb_scope			scope = LDB_SCOPE_SUBTREE;
	struct SPropertyRestriction	*res_prop = NULL;
	struct ldb_result		*res = NULL;
	struct SPropValue		*lpProp = NULL;
	const char * const		recipient_attrs[] = { "*", NULL};
	char				*recipient;
	char				*ldb_filter;
	int				ret;
	int				i;

	if (((uint32_t)(restriction->rt)) != RES_PROPERTY) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	res_prop = (struct SPropertyRestriction *)&(restriction->res);
	if ((res_prop->ulPropTag != PR_ANR) && (res_prop->ulPropTag != PR_ANR_UNICODE)) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	lpProp = res_prop->lpProp;
	if ((recipient = (char *)((res_prop->ulPropTag == PR_ANR) ? lpProp->value.lpszA : lpProp->value.lpszW))) {
		ldb_filter = talloc_asprintf(emsabp_ctx->mem_ctx, "(&(objectClass=user)(sAMAccountName=*%s*)(!(objectClass=computer)))", recipient);
	} else {
		return NT_STATUS_INVALID_PARAMETER;
	}
	ret = ldb_search(emsabp_ctx->users_ctx, ldb_get_default_basedn(emsabp_ctx->users_ctx), scope, ldb_filter, recipient_attrs, &res);

	if (ret != LDB_SUCCESS || !res->count) {
		return NT_STATUS_NO_SUCH_USER;
	}

	*instance_keys = talloc_size(emsabp_ctx->mem_ctx, sizeof(*instance_keys) * (res->count + 1));

	for (i = 0; i < res->count; i++) {
		(*instance_keys)[i] = talloc_size(emsabp_ctx->mem_ctx, sizeof (uint8_t) * 4);
		if (!emsabp_add_entry(emsabp_ctx, (*instance_keys)[i], res->msgs[i])) {
			/* FIXME: Change NTSTATUS value */
			return NT_STATUS_INVALID_PARAMETER;
		}
	}
	(*instance_keys)[res->count] = NULL;
	
	return NT_STATUS_OK;
}

/*
  search and open an entry in the Active Directory
  according to the given dn
*/

NTSTATUS emsabp_search_dn(struct emsabp_ctx *emsabp_ctx, struct ldb_message **ldb_res, uint8_t *instance_key, const char *dn)
{
	enum ldb_scope		scope = LDB_SCOPE_SUBTREE;
	struct ldb_dn		*ldb_dn = NULL;
	struct ldb_result	*res = NULL;
	const char * const	recipient_attrs[] = { "*", NULL};
	char			*ldb_filter;
	int			ret;

	if (dn == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}	
	/* Search for an Active Directory dn */
	ldb_dn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->conf_ctx, dn);
	if (ldb_dn_validate(ldb_dn)) {
		 ret = ldb_search(emsabp_ctx->conf_ctx, ldb_dn, scope, "", recipient_attrs, &res);
	 } else {
		 /* Search for an x400 dn which identifies an Exchange object on the Active Directory */
		 ldb_filter = talloc_asprintf(emsabp_ctx->mem_ctx, "(legacyExchangeDN=%s)", dn);
		 ret = ldb_search(emsabp_ctx->conf_ctx, ldb_get_default_basedn(emsabp_ctx->conf_ctx), scope, ldb_filter, recipient_attrs, &res);
	 }

	if (ret != LDB_SUCCESS || !res->count) {
		/* FIXME: Change NTSTATUS value */
		return NT_STATUS_NO_SUCH_USER;
	}

	if (ldb_res != NULL) {
		*ldb_res = res->msgs[0];
	}

	if (!emsabp_add_entry(emsabp_ctx, instance_key, res->msgs[0])) {
		/* FIXME: Change NTSTATUS value */
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

/*
  find the x500 attr matching the mapitag 
  query the message and retrieve the attribute value 
  if the attr doesn't exist, return NULL
*/

void *emsabp_query(TALLOC_CTX *mem_ctx, struct emsabp_ctx *emsabp_ctx, struct entry_id *entry, uint32_t mapitag)
{
	const char	       		*guid_str;
	struct GUID			*guid;
	struct ldb_message_element	*ldb_element;
	struct ldb_message		*ldb_res;
	struct SLPSTRArray		*mv_string;
	struct SBinary			*bin;
	const char			*ldb_str;
	const char			*x500 = NULL;
	NTSTATUS			status;
	BOOL				ismultix500 = False;
	uint32_t			ldb_int;
	uint32_t			i;
	uint32_t			*num;
	uint8_t				*instance_key;
	void				*data = (void *) NULL;

	switch (mapitag) {
	case PR_ADDRTYPE:
		data = talloc_strdup(mem_ctx, EMSABP_ADDRTYPE);
		return (data);
	case PR_ENTRYID:
		guid_str = lp_parm_string(-1, "exchange", "GUID");
		guid = talloc(mem_ctx, struct GUID);
		GUID_from_string(guid_str, guid);
		bin = talloc(mem_ctx, struct SBinary);

		bin->cb = 32;
		bin->lpb = talloc_size(mem_ctx, sizeof(uint8_t) * bin->cb);
		memset(bin->lpb, 0, bin->cb);
		bin->lpb[0] = 0x87;	/* Maybe a mask of bits */
		
		bin->lpb[4]  = (guid->time_low & 0xFF);
		bin->lpb[5]  = ((guid->time_low >>  8) & 0xFF);
		bin->lpb[6]  = ((guid->time_low >> 16) & 0xFF);
		bin->lpb[7]  = ((guid->time_low >> 24) & 0xFF);
		bin->lpb[8]  = (guid->time_mid &0xFF);
		bin->lpb[9]  = ((guid->time_mid >>  8) & 0xFF);
		bin->lpb[10] = (guid->time_hi_and_version & 0xFF);
		bin->lpb[11] = ((guid->time_hi_and_version >> 8) & 0xFF); 
		bin->lpb[12] = (guid->clock_seq[0] & 0xFF);
		bin->lpb[13] = (guid->clock_seq[1] & 0xFF);
		bin->lpb[14] = guid->node[0];
		bin->lpb[15] = guid->node[1];
		bin->lpb[16] = guid->node[2];
		bin->lpb[17] = guid->node[3];
		bin->lpb[18] = guid->node[4];
		bin->lpb[19] = guid->node[5];

		bin->lpb[20] = 0x01;
		/* 21 -> 27 0 bytes */
		bin->lpb[28] = entry->guid.node[4];
		bin->lpb[29] = entry->guid.node[5];
		/* 30 -> 31 0 bytes */
		
		talloc_free(guid);
		return (bin);
	case PR_OBJECT_TYPE:
		data = talloc(mem_ctx, uint32_t);
		*((uint32_t *)data) = MAPI_MAILUSER;
		return (data);
	case PR_DISPLAY_TYPE:
		data = talloc(mem_ctx, uint32_t);
		*((uint32_t *)data) = DT_MAILUSER;
		return (data);
	case PR_INSTANCE_KEY:
		bin = talloc(mem_ctx, struct SBinary);
		bin->cb = 4;
		bin->lpb = talloc_size(mem_ctx, sizeof(uint8_t) * bin->cb);
		memcpy(bin->lpb, entry->instance_key, 4);
		return (bin);
	}

	/* Check tag existence, otherwise return NULL */
	for (i = 0; emsabp_single_x500_tags[i].mapitag; i++) {
		if (emsabp_single_x500_tags[i].mapitag == mapitag) {
			x500 = emsabp_single_x500_tags[i].x500;
			break;
		}
	}
	if (x500 == NULL) {
		for (i = 0; emsabp_multi_x500_tags[i].mapitag; i++) {
			if (emsabp_multi_x500_tags[i].mapitag == mapitag) {
				ismultix500 = True;
				x500 = emsabp_multi_x500_tags[i].x500;
				break;
			}
		}
		if (x500 == NULL) {
			return NULL;
		}	
	}
	
	switch (mapitag & 0xFFFF) {
	case PT_STRING8:
	case PT_UNICODE:
		ldb_str = ldb_msg_find_attr_as_string(entry->msg, x500, NULL);
		if (ismultix500) {
			instance_key = talloc_size(mem_ctx, sizeof (uint8_t) * 4);
			status = emsabp_search_dn(emsabp_ctx, &ldb_res, instance_key, ldb_str);
			if (!NT_STATUS_IS_OK(status)) {
				return NULL;
			}
			ldb_str = ldb_msg_find_attr_as_string(ldb_res, "legacyExchangeDN", NULL);
			if (!ldb_str) {
				return NULL;
			}
		}
		data = talloc_strdup(mem_ctx, ldb_str);
		break;
	case PT_SHORT :
	case PT_LONG :
	case PT_BOOLEAN :
	case PT_MV_SHORT :
	case PT_NULL :
	case PT_OBJECT :
		ldb_int = ldb_msg_find_attr_as_int(entry->msg, x500, 0);
		num = talloc(mem_ctx, uint32_t);
		*num = ldb_int;
		data = (void *)num;
		break;
	case PT_MV_STRING8:
		mv_string = talloc(mem_ctx, struct SLPSTRArray);
		ldb_element = ldb_msg_find_element(entry->msg, x500);
		if (!ldb_element) {
			return NULL;
		}
		mv_string->cValues = ldb_element[0].num_values & 0xFFFFFFFF;
		mv_string->strings = talloc_size(mem_ctx, sizeof(struct SLPSTRArray *) * mv_string->cValues);
		for (i = 0; i < mv_string->cValues; i++) {
			mv_string->strings[i] = talloc(mem_ctx, struct LPSTR);
			mv_string->strings[i]->lppszA = talloc_strdup(mem_ctx, (char *)ldb_element->values[i].data);
		}
		data = mv_string;
		break;
 	}
	return data;
}

/*
  point on the entry_id matching the instance_key
  set the SRowSet with the requested information
 */

NTSTATUS emsabp_fetch_attrs(TALLOC_CTX *mem_ctx, struct emsabp_ctx *emsabp_ctx,
			    struct SRow *SRow, uint8_t *instance_key,
			    struct SPropTagArray *SPropTagArray)
{
	struct entry_id		*entry;
	uint32_t		i;
	struct SPropValue	*lpProps;
	uint32_t		ulPropTag;
	void			*data;
	
	entry = emsabp_ctx->entry_ids;
	while (entry != NULL) {
		DEBUG(3, ("emsabp_fetch_attrs: Comparing instance_key 0x%.2x%.2x%.2x%.2x with 0x%.2x%.2x%.2x%.2x\n",
			  instance_key[0], instance_key[1], instance_key[2], instance_key[3], entry->instance_key[0],
			  entry->instance_key[1],entry->instance_key[2],entry->instance_key[3]));
		if ((instance_key[0] == entry->instance_key[0]) &&
		    (instance_key[1] == entry->instance_key[1]) &&
		    (instance_key[2] == entry->instance_key[2]) &&
		    (instance_key[3] == entry->instance_key[3])) {
			DEBUG(3, ("emsabp_fetch_attrs: INSTANCE_KEY matches\n"));
			break;
		}
		entry = entry->next;
	}

	SRow->ulAdrEntryPad = 0x0;
	SRow->cValues = (SPropTagArray->cValues) ? (SPropTagArray->cValues - 1) : 0;

	lpProps = talloc_size(mem_ctx, sizeof(*lpProps) * SRow->cValues);
	for (i = 0; i < (SRow->cValues); i++) {
		ulPropTag = SPropTagArray->aulPropTag[i];
		if (entry) {
			data = emsabp_query(mem_ctx, emsabp_ctx, entry, SPropTagArray->aulPropTag[i]);
		} else {
			data = NULL;
		}
			
		ulPropTag = SPropTagArray->aulPropTag[i];
		if (!data) {
			ulPropTag &= 0xFFFF0000;
			ulPropTag += PT_ERROR;
		}
		lpProps[i].ulPropTag = ulPropTag;
		lpProps[i].dwAlignPad = 0x0;
		set_SPropValue(&lpProps[i], data);
	}
	SRow->lpProps = lpProps;

	return (entry) ?  NT_STATUS_OK : NT_STATUS_INVALID_PARAMETER;
}

/*
 * emsabp_hierarchy_get_entryID:
 * Generate the PR_ENTRYID SBinary structure for the given recipient
 *
 */

struct SBinary *emsabp_hierarchy_get_entryID(TALLOC_CTX *mem_ctx, struct GUID *guid, BOOL containerID)
{
	struct SBinary	*entryID;
	char		*guid_str = (char *) NULL;
	
	if (!containerID) {
		guid_str =  talloc_asprintf(mem_ctx,
					    PACKED_AB_GUID,
					    guid->time_low, guid->time_mid,
					    guid->time_hi_and_version,
					    guid->clock_seq[0],
					    guid->clock_seq[1],
					    guid->node[0], guid->node[1],
					    guid->node[2], guid->node[3],
					    guid->node[4], guid->node[5]);
		entryID = generate_recipient_entryid(mem_ctx, (const char *) guid_str);
		talloc_free(guid_str);
	} else {
		entryID = generate_recipient_entryid(mem_ctx, NULL);
	}	
	return (entryID);
}

/*
 * emsabp_hierarchy_get_containerID:
 * returns the containerID associated with the name passed as param
 */

uint32_t *emsabp_hierarchy_get_containerID(TALLOC_CTX *mem_ctx, const char *name)
{
	int 		i;
	uint32_t	*ret;

	ret = talloc(mem_ctx, uint32_t);
	*ret = 0;
	
	if (!name) {
		return ret;
	}
	for (i = 0; emsabp_containerID[i].name; i++) {
		if (name && emsabp_containerID[i].name && !strcmp(name, emsabp_containerID[i].name)) {
			*ret = emsabp_containerID[i].id;
			return ret;
		}
	}
	return ret;
}

/*
  
*/

void *emsabp_hierarchy_query(TALLOC_CTX *mem_ctx,
			     struct ldb_message *ldb_recipient,
			     struct ldb_message *ldb_recipient_parent,
			     uint32_t mapitag)
{
	void		*data = (void *) NULL;
	const char	*displayName;
	struct GUID	guid;

	switch (mapitag) {
	case PR_ENTRYID:
		guid = samdb_result_guid(ldb_recipient, "objectGUID");
		data = emsabp_hierarchy_get_entryID(mem_ctx,
						    &guid,
						    !strcmp(ldb_msg_find_attr_as_string(ldb_recipient, "displayName", NULL), "Address Lists Container"));
		break;
	case PR_CONTAINER_FLAGS:
		data = talloc(mem_ctx, uint32_t);
		if (!strcmp(ldb_msg_find_attr_as_string(ldb_recipient, "displayName", NULL), "All Address Lists")) {
			*((uint32_t *)data) = AB_UNMODIFIABLE | AB_SUBCONTAINERS | AB_RECIPIENTS;
		} else {
			*((uint32_t *)data) = AB_UNMODIFIABLE | AB_RECIPIENTS;
		}
		break;
	case PR_DEPTH:
		data = talloc(mem_ctx, uint32_t);
		if (ldb_recipient_parent) {
			*((uint32_t *)data) = 0x1;
		} else {
			*((uint32_t *)data) = 0x0;
		}
		break;
	case PR_EMS_AB_CONTAINERID:
		data = emsabp_hierarchy_get_containerID(mem_ctx, ldb_msg_find_attr_as_string(ldb_recipient, "displayName", NULL));
		break;
	case PR_DISPLAY_NAME:
	case PR_DISPLAY_NAME_UNICODE:
		displayName = ldb_msg_find_attr_as_string(ldb_recipient, "displayName", NULL);
		if (strcmp(displayName, "Address Lists Container")) {
			data = talloc_strdup(mem_ctx, displayName);
		} else {
			data = talloc_zero(mem_ctx, char *);
			data = (char *) NULL;
		}
		break;
	case PR_EMS_AB_IS_MASTER:
		data = talloc(mem_ctx, uint16_t);
		*((uint16_t *)data) = 0x0;
		break;
	case PR_EMS_AB_PARENT_ENTRYID:
		guid = samdb_result_guid(ldb_recipient_parent, "objectGUID");
		data = emsabp_hierarchy_get_entryID(mem_ctx,
						    &guid,
						    !strcmp(ldb_msg_find_attr_as_string(ldb_recipient_parent, "displayName", NULL), "Address Lists Container"));
		break;
	default:
		return NULL;
	}

	return data;
}

/*
  set the SRowSet with the requested information
*/

NTSTATUS emsabp_hierarchy_fetch_attrs(TALLOC_CTX *mem_ctx,
				      uint32_t flags,
				      struct SRow *SRow,
				      struct ldb_message *ldb_recipient,
				      struct ldb_message *ldb_recipient_parent)
{
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		ulPropTag, i;
	const char		*attribute = NULL;
	void			*data;
	
	SRow->ulAdrEntryPad = 0x0;
	attribute = ldb_msg_find_attr_as_string(ldb_recipient, "displayName", NULL);

	if (!strcmp(attribute, "All Address Lists") ||
	    !strcmp(attribute, "Address Lists Container")) {
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x6,
						  PR_ENTRYID,
						  PR_CONTAINER_FLAGS,
						  PR_DEPTH,
						  PR_EMS_AB_CONTAINERID,
						  ((flags) ? PR_DISPLAY_NAME_UNICODE : PR_DISPLAY_NAME),
						  PR_EMS_AB_IS_MASTER
			);
	} else {
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x7,
						  PR_ENTRYID,
						  PR_CONTAINER_FLAGS,
						  PR_DEPTH,
						  PR_EMS_AB_CONTAINERID,
						  ((flags) ? PR_DISPLAY_NAME_UNICODE : PR_DISPLAY_NAME),
						  PR_EMS_AB_IS_MASTER,
						  PR_EMS_AB_PARENT_ENTRYID
			);
	}
	
	SRow->cValues = SPropTagArray->cValues - 1;	

	lpProps = talloc_size(mem_ctx, sizeof(*lpProps) * SRow->cValues);

	for (i = 0; i < (SRow->cValues); i++) {
		ulPropTag = SPropTagArray->aulPropTag[i];
		data = emsabp_hierarchy_query(mem_ctx, ldb_recipient, ldb_recipient_parent, SPropTagArray->aulPropTag[i]);
		ulPropTag = SPropTagArray->aulPropTag[i];
		if (!data && ((SPropTagArray->aulPropTag[i] != PR_DISPLAY_NAME) && (SPropTagArray->aulPropTag[i] != PR_DISPLAY_NAME_UNICODE))) {
			ulPropTag &= 0xFFFF0000;
			ulPropTag += PT_ERROR;
		}
		lpProps[i].ulPropTag = ulPropTag;
		lpProps[i].dwAlignPad = 0x0;
		if ((SPropTagArray->aulPropTag[i] == PR_DISPLAY_NAME) || (SPropTagArray->aulPropTag[i] == PR_DISPLAY_NAME_UNICODE)) {
			    if (data) {
				    set_SPropValue(&lpProps[i], data);
			    } else {
				    switch (SPropTagArray->aulPropTag[i]) {
				    case PR_DISPLAY_NAME: 
					    lpProps[i].value.lpszA = (char *) NULL;
					    break;
				    case PR_DISPLAY_NAME_UNICODE:
					    lpProps[i].value.lpszW = (char *) NULL;
					    break;
				    default:
					    break;
				    }
			    }
		}
		else {
			set_SPropValue(&lpProps[i], data);
		}
	}

	SRow->lpProps = lpProps;
	
	return NT_STATUS_OK;
}

uint32_t emsabp_get_containers(TALLOC_CTX *mem_ctx, struct emsabp_ctx *emsabp_ctx, uint32_t flags,
			       struct SRow **SRows, struct ldb_message **ldb_recipient_parent,
			       const char *dn, const char *filter)
{
	enum ldb_scope scope = LDB_SCOPE_SUBTREE;
	const char * const	recipient_attrs[] = { "*", NULL};
	struct ldb_result	*res = NULL;
	struct ldb_dn		*basedn = NULL;
	uint32_t	       	i, ret;

	basedn = ldb_dn_new(emsabp_ctx->mem_ctx, emsabp_ctx->conf_ctx, dn);
	if (basedn == NULL)
		basedn = ldb_get_default_basedn(emsabp_ctx->conf_ctx);
	if ( ! ldb_dn_validate(basedn)) {
		DEBUG(3, ("Invalid Base DN format\n"));
		return -1;
	}

	ret = ldb_search(emsabp_ctx->conf_ctx, basedn, scope, filter, recipient_attrs, &res);

	if ((ret != LDB_SUCCESS) || ((res != NULL) && (res->count == 0))) {
		return 0;
	}

	*SRows = talloc_array(mem_ctx, struct SRow, res->count);

	for (i = 0; i < res->count; i++) {
	  emsabp_hierarchy_fetch_attrs(mem_ctx, flags, &((*SRows)[i]), res->msgs[i], *ldb_recipient_parent);
	}

	if (res->count == 1) {
		*ldb_recipient_parent = res->msgs[0];
	} else {
		*ldb_recipient_parent = NULL;
	}

	return res->count;
}

NTSTATUS emsabp_get_hierarchytable(TALLOC_CTX *mem_ctx, struct emsabp_ctx *emsabp_ctx, uint32_t flags, 
				   struct SRowSet **RowSet)
{
	struct ldb_message	*ldb_recipient_parent = NULL;
	struct SRow		*SRow_root, *SRow_subroot, *SRow_containers;
	int			i, count;
	const char		*dn;

/* 	RowSet[0] = talloc(mem_ctx, struct SRowSet); */

	/* Set 'Address Lists Container' object */
	count = emsabp_get_containers(mem_ctx, emsabp_ctx, flags, &SRow_root, &ldb_recipient_parent,
				      NULL, "(cn=Address Lists Container)");
	if (count != 1) {
		return NT_STATUS(MAPI_E_CORRUPT_STORE);
	}

	/* Set 'All Address Lists' object */

	dn = ldb_msg_find_attr_as_string(ldb_recipient_parent, "dn", NULL);
	ldb_recipient_parent = NULL;

	count = emsabp_get_containers(mem_ctx, emsabp_ctx, flags, &SRow_subroot, &ldb_recipient_parent,
				      dn, "(cn=All Address Lists)");
	if (count != 1) {
		return NT_STATUS(MAPI_E_CORRUPT_STORE);
	}

	/* Set 'All Address Lists' subcontainers object */
	count = emsabp_get_containers(mem_ctx, emsabp_ctx, flags, &SRow_containers, &ldb_recipient_parent,
				      ldb_msg_find_attr_as_string(ldb_recipient_parent, "dn", NULL), "(&(objectClass=addressbookContainer)(!(cn=All Address Lists)))");

	if (count < 1) {
		return NT_STATUS(MAPI_E_CORRUPT_STORE);
	}

	printf("emsabp_get_hierarchytable: count = %d\n", count);
	RowSet[0]->cRows = count + 2;
	RowSet[0]->aRow = talloc_array(mem_ctx, struct SRow, RowSet[0]->cRows);

	/* Add all root SRow to the SRowSet */
	memcpy(&(RowSet[0]->aRow[0]), &(SRow_root[0]), sizeof(struct SRow));

	/* Add all subroot SRow to the SRowSet */
	memcpy(&(RowSet[0]->aRow[1]), &(SRow_subroot[0]), sizeof(struct SRow));

	/* Add all subcontainers SRow to the SRowSet */
	for (i = 2; i < (count + 2); i++) {
		memcpy(&(RowSet[0]->aRow[i]), &(SRow_containers[i - 2]), sizeof(struct SRow));
	}
	
	return NT_STATUS_OK;
}
