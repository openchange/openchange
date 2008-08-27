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
   \details This function replaces network address from the binding
   strings returned by Exchange for the PR_EMS_AB_NETWORK_ADDRESS
   property and limit the binding strings scope to ncacn_ip_tcp.

   \return true on success, otherwise false
 */
bool mapiproxy_NspiGetProps(struct dcesrv_call_state *dce_call, struct NspiGetProps *r)
{
	uint32_t		i;
	uint32_t		propid = -1;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SRow		*SRow;
	struct SLPSTRArray	*slpstr;
	struct SPropValue	*lpProp;

	/* Sanity checks */
	if (!r->out.REPL_values) return false;
	if (!(*r->out.REPL_values)->cValues) return false;

	/* Step 1. Find PR_EMS_AB_NETWORK_ADDRESS index */
	propid = -1;
	SPropTagArray = r->in.REQ_properties;
	for (i = 0; i < SPropTagArray->cValues - 1; i++) {
		if (SPropTagArray->aulPropTag[i] == PR_EMS_AB_NETWORK_ADDRESS) {
			propid = i;
			break;
		}
	}
	if (propid == -1) return false;

	/* Step 2. Retrieve the SLPSTRArray */
	SRow = *r->out.REPL_values;
	lpProp = &SRow->lpProps[propid];

	if (!lpProp) return false;
	if (lpProp->ulPropTag != PR_EMS_AB_NETWORK_ADDRESS) return false;

	slpstr = &(lpProp->value.MVszA);

	/* Step 3. Modify Exchange binding strings and only return ncacn_ip_tcp */
	slpstr->cValues = 1;
	slpstr->strings[0]->lppszA = talloc_asprintf(dce_call, "ncacn_ip_tcp:%s", lp_realm(dce_call->conn->dce_ctx->lp_ctx));

	return true;
}
