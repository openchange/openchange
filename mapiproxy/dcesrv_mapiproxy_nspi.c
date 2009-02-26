/*
   MAPI Proxy - NSPI

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/dcesrv_mapiproxy_proto.h"

/**
   \file dcesrv_mapiproxy_nspi.c

   \brief NSPI hook functions
 */

/**
   \details Retrieve the servername from a DN string
 
   \param dn the DN string

   \return a talloc'd server name
 */
static char *x500_get_servername(const char *dn)
{
	char *pdn;
	char *servername;

	if (!dn) {
		return NULL;
	}

	pdn = strcasestr(dn, SERVERNAME);
	if (pdn == NULL) return NULL;

	pdn += strlen(SERVERNAME);
	servername = strsep(&pdn, "/");

	return (talloc_strdup(NULL, servername));
}


/**
   \details This function replaces network address from the binding
   strings returned by Exchange for the PR_EMS_AB_NETWORK_ADDRESS
   property and limit the binding strings scope to ncacn_ip_tcp.

   \param dce_call pointer to the session context
   \param r pointer to the NspiGetProps structure

   \return true on success, otherwise false
 */
bool mapiproxy_NspiGetProps(struct dcesrv_call_state *dce_call, struct NspiGetProps *r)
{
	uint32_t		i;
	uint32_t		propID = -1;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRow		*SRow;
	struct StringArray_r	*slpstr;
	struct SPropValue	*lpProp;

	/* Sanity checks */
	if (!r->out.ppRows) return false;
	if (!(*r->out.ppRows)->cValues) return false;

	/* Step 1. Find PR_EMS_AB_NETWORK_ADDRESS index */
	propID = -1;
	SPropTagArray = r->in.pPropTags;
	for (i = 0; i < SPropTagArray->cValues; i++) {
		if (SPropTagArray->aulPropTag[i] == PR_EMS_AB_NETWORK_ADDRESS) {
			propID = i;
			break;
		}
	}
	if (propID == -1) return false;

	/* Step 2. Retrieve the SLPSTRArray */
	SRow = *r->out.ppRows;
	lpProp = &SRow->lpProps[propID];

	if (!lpProp) return false;
	if (lpProp->ulPropTag != PR_EMS_AB_NETWORK_ADDRESS) return false;

	slpstr = &(lpProp->value.MVszA);

	/* Step 3. Modify Exchange binding strings and only return ncacn_ip_tcp */
	slpstr->cValues = 1;
	slpstr->lppszA[0] = talloc_asprintf(dce_call, "ncacn_ip_tcp:%s.%s", 
					    lp_netbios_name(dce_call->conn->dce_ctx->lp_ctx), 
					    lp_realm(dce_call->conn->dce_ctx->lp_ctx));
	strlower_m((char *)slpstr->lppszA[0]);

	return true;
}


/**
   \details This function replaces the Exchange server name with
   mapiproxy netbios name for the PR_EMS_AB_HOME_MDB property and
   saves the original name in a global variable for further usage -
   such as mapiproxy_NspiDNToMId.

   \param dce_call pointer to the session context
   \param r pointer to the NspiQueryRows structure

   \sa mapiproxy_NspiDNToMId
*/
bool mapiproxy_NspiQueryRows(struct dcesrv_call_state *dce_call, struct NspiQueryRows *r)
{
	struct dcesrv_mapiproxy_private	*private;
	uint32_t		i;
	uint32_t		propID = -1;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRowSet		*SRowSet;
	struct SPropValue	*lpProp;
	char			*lpszA;
	char			*exchname;

	private = dce_call->context->private_data;

	/* Sanity checks */
	if (!r->out.ppRows) return false;
	if (!(*r->out.ppRows)->cRows) return false;
	if (!r->in.pPropTags) return false;

	/* Step 1. Find PR_EMS_AB_HOME_MDB index */
	propID = -1;
	SPropTagArray = r->in.pPropTags;
	for (i = 0; i < SPropTagArray->cValues; i++) {
		if (SPropTagArray->aulPropTag[i] == PR_EMS_AB_HOME_MDB) {
			propID = i;
			break;
		}
	}
	if (propID == -1) return false;

	/* Retrieve the lpszA */
	SRowSet = *r->out.ppRows;
	lpProp = &(SRowSet->aRow->lpProps[propID]);

	if (!lpProp) return false;
	if (lpProp->ulPropTag != PR_EMS_AB_HOME_MDB) return false;

	if (private->exchname) {
		if (strstr(lpProp->value.lpszA, private->exchname)) {
			lpProp->value.lpszA = string_sub_talloc((TALLOC_CTX *) dce_call, lpProp->value.lpszA, private->exchname, 
								lp_netbios_name(dce_call->conn->dce_ctx->lp_ctx));	
		}
	} else {
		lpszA = talloc_strdup(dce_call, lpProp->value.lpszA);
		if ((exchname = x500_get_servername(lpszA))) {
			private->exchname = talloc_strdup(NULL, exchname);
			lpProp->value.lpszA = string_sub_talloc((TALLOC_CTX *) dce_call, lpProp->value.lpszA, exchname, 
								lp_netbios_name(dce_call->conn->dce_ctx->lp_ctx));
			talloc_free(exchname);
		}
		talloc_free(lpszA);
	}

	return true;
}


/**
   \details This function looks if the server DN string in the request
   holds the mapiproxy netbios name and replaces it with the original
   Exchange server one fetched from NspiQueryRows or NspiGetProps.

   \param dce_call pointer to the session context
   \param r pointer to the NspiDNToMId structure

   \return true on success or false if no occurrence of the mapiproxy
   netbios name was found.
*/
bool mapiproxy_NspiDNToMId(struct dcesrv_call_state *dce_call, struct NspiDNToMId *r)
{
	struct dcesrv_mapiproxy_private	*private;
	const char			*proxyname;
	uint32_t			i;

	private = dce_call->context->private_data;
	proxyname = lp_netbios_name(dce_call->conn->dce_ctx->lp_ctx);

	if (!private->exchname) return false;

	for (i = 0; i < r->in.pNames->Count; i++) {
		if (strstr(r->in.pNames->Strings[i], proxyname)) {
			r->in.pNames->Strings[i] = string_sub_talloc((TALLOC_CTX *) dce_call, r->in.pNames->Strings[i], proxyname, private->exchname);
			return true;
		}
	}

	return false;
}
