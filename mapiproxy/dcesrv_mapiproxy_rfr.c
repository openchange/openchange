/*
   MAPI Proxy - RFR

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
   \file dcesrv_mapiproxy_rfr.c

   \brief NSPI Referral hook functions
 */


/**
   \details This function replaces the Exchange server FQDN with
   mapiproxy one.

   \return true on success, otherwise false
 */
bool mapiproxy_RfrGetNewDSA(struct dcesrv_call_state *dce_call, struct RfrGetNewDSA *r)
{
	/* Sanity checks */
	if (!r->out.ppszServer) return false;

	*r->out.ppszServer = (uint8_t *) talloc_asprintf(dce_call, "%s.%s", 
					     lpcfg_netbios_name(dce_call->conn->dce_ctx->lp_ctx), 
					     lpcfg_realm(dce_call->conn->dce_ctx->lp_ctx));
	*r->out.ppszServer = (uint8_t *) strlower_talloc(dce_call, (const char *) *r->out.ppszServer);

	return true;
}
