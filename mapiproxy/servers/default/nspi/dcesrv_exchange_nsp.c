/*
   MAPI Proxy - Exchange NSPI Server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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
   \file dcesrv_exchange_nsp.c

   \brief OpenChange NSPI Server implementation
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_nsp.h"
#include <util/debug.h>


/**
   \details exchange_nsp NspiBind (0x0) function
 */
static enum MAPISTATUS dcesrv_NspiBind(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct NspiBind *r)
{
	DEBUG(3, ("exchange_nsp: NspiBind (0x0) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiUnbind (0x1) function
 */
static enum MAPISTATUS dcesrv_NspiUnbind(struct dcesrv_call_state *dce_call,
					 TALLOC_CTX *mem_ctx,
					 struct NspiUnbind *r)
{
	DEBUG(3, ("exchange_nsp: NspiUnbind (0x1) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiUpdateStat (0x2) function
*/
static enum MAPISTATUS dcesrv_NspiUpdateStat(struct dcesrv_call_state *dce_call, 
					     TALLOC_CTX *mem_ctx,
					     struct NspiUpdateStat *r)
{
	DEBUG(3, ("exchange_nsp: NspiUpdateStat (0x2) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiQueryRows (0x3) function
 */
static enum MAPISTATUS dcesrv_NspiQueryRows(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct NspiQueryRows *r)
{
	DEBUG(3, ("exchange_nsp: NspiQueryRows (0x3) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiSeekEntries (0x4) function
 */
static enum MAPISTATUS dcesrv_NspiSeekEntries(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiSeekEntries *r)
{
	DEBUG(3, ("exchange_nsp: NspiSeekEntries (0x4) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetMatches (0x5) function
 */
static enum MAPISTATUS dcesrv_NspiGetMatches(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct NspiGetMatches *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetMatches (0x5) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiResortRestriction (0x6) function
 */
static enum MAPISTATUS dcesrv_NspiResortRestriction(struct dcesrv_call_state *dce_call,
						    TALLOC_CTX *mem_ctx,
						    struct NspiResortRestriction *r)
{
	DEBUG(3, ("exchange_nsp: NspiResortRestriction (0x6) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiDNToMId (0x7) function
 */
static enum MAPISTATUS dcesrv_NspiDNToMId(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct NspiDNToMId *r)
{
	DEBUG(3, ("exchange_nsp: NspiDNToMId (0x7) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetPropList (0x8) function
 */
static enum MAPISTATUS dcesrv_NspiGetPropList(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiGetPropList *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetPropList (0x8) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetProps (0x9) function
 */
static enum MAPISTATUS dcesrv_NspiGetProps(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct NspiGetProps *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetProps (0x9) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiCompareMIds (0xA) function
*/
static enum MAPISTATUS dcesrv_NspiCompareMIds(struct dcesrv_call_state *dce_call,
					      TALLOC_CTX *mem_ctx,
					      struct NspiCompareMIds *r)
{
	DEBUG(3, ("exchange_nsp: NspiCompareMIds (0xA) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiModProps (0xB) function
 */
static enum MAPISTATUS dcesrv_NspiModProps(struct dcesrv_call_state *dce_call,
					   TALLOC_CTX *mem_ctx,
					   struct NspiModProps *r)
{
	DEBUG(3, ("exchange_nsp: NspiModProps (0xB) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetSpecialTable (0xC) function
 */
static enum MAPISTATUS dcesrv_NspiGetSpecialTable(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetSpecialTable *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetSpecialTable (0xC) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetTemplateInfo (0xD) function
 */
static enum MAPISTATUS dcesrv_NspiGetTemplateInfo(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetTemplateInfo *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetTemplateInfo (0xD) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiModLinkAtt (0xE) function
 */
static enum MAPISTATUS dcesrv_NspiModLinkAtt(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct NspiModLinkAtt *r)
{
	DEBUG(3, ("exchange_nsp: NspiModLinkAtt (0xE) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiDeleteEntries (0xF) function
 */
static enum MAPISTATUS dcesrv_NspiDeleteEntries(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						struct NspiDeleteEntries *r)
{
	DEBUG(3, ("exchange_nsp: NspiDeleteEntries (0xF) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiQueryColumns (0x10) function
 */
static enum MAPISTATUS dcesrv_NspiQueryColumns(struct dcesrv_call_state *dce_call,
					       TALLOC_CTX *mem_ctx,
					       struct NspiQueryColumns *r)
{
	DEBUG(3, ("exchange_nsp: NspiQueryColumns (0x10) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetNamesFromIDs (0x11) function
 */
static enum MAPISTATUS dcesrv_NspiGetNamesFromIDs(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetNamesFromIDs *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetNamesFromIDs (0x11) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiGetIDsFromNames (0x12) function
 */
static enum MAPISTATUS dcesrv_NspiGetIDsFromNames(struct dcesrv_call_state *dce_call,
						  TALLOC_CTX *mem_ctx,
						  struct NspiGetIDsFromNames *r)
{
	DEBUG(3, ("exchange_nsp: NspiGetIDsFromNames (0x12) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiResolveNames (0x13) function
 */
static enum MAPISTATUS dcesrv_NspiResolveNames(struct dcesrv_call_state *dce_call,
					       TALLOC_CTX *mem_ctx,
					       struct NspiResolveNames *r)
{
	DEBUG(3, ("exchange_nsp: NspiResolveNames (0x13) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_nsp NspiResolveNamesW (0x14) function
 */
static enum MAPISTATUS dcesrv_NspiResolveNamesW(struct dcesrv_call_state *dce_call,
						TALLOC_CTX *mem_ctx,
						struct NspiResolveNamesW *r)
{
	DEBUG(3, ("exchange_nsp: NspiResolveNamesW (0x14) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


static NTSTATUS dcesrv_exchange_nsp_dispatch(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     void *r, struct mapiproxy *mapiproxy)
{
	enum MAPISTATUS				retval;
	const struct ndr_interface_table	*table;
	uint16_t				opnum;

	table = (const struct ndr_interface_table *) dce_call->context->iface->private;
	opnum = dce_call->pkt.u.request.opnum;

	/* Sanity checks */
	if (!table) return NT_STATUS_UNSUCCESSFUL;
	if (table->name && strcmp(table->name, NDR_EXCHANGE_NSP_NAME)) return NT_STATUS_UNSUCCESSFUL;

	switch (opnum) {
	case NDR_NSPIBIND:
		retval = dcesrv_NspiBind(dce_call, mem_ctx, (struct NspiBind *)r);
		break;
	case NDR_NSPIUNBIND:
		retval = dcesrv_NspiUnbind(dce_call, mem_ctx, (struct NspiUnbind *)r);
		break;
	case NDR_NSPIUPDATESTAT:
		retval = dcesrv_NspiUpdateStat(dce_call, mem_ctx, (struct NspiUpdateStat *)r);
		break;
	case NDR_NSPIQUERYROWS:
		retval = dcesrv_NspiQueryRows(dce_call, mem_ctx, (struct NspiQueryRows *)r);
		break;
	case NDR_NSPISEEKENTRIES:
		retval = dcesrv_NspiSeekEntries(dce_call, mem_ctx, (struct NspiSeekEntries *)r);
		break;
	case NDR_NSPIGETMATCHES:
		retval = dcesrv_NspiGetMatches(dce_call, mem_ctx, (struct NspiGetMatches *)r);
		break;
	case NDR_NSPIRESORTRESTRICTION:
		retval = dcesrv_NspiResortRestriction(dce_call, mem_ctx, (struct NspiResortRestriction *)r);
		break;
	case NDR_NSPIDNTOMID:
		retval = dcesrv_NspiDNToMId(dce_call, mem_ctx, (struct NspiDNToMId *)r);
		break;
	case NDR_NSPIGETPROPLIST:
		retval = dcesrv_NspiGetPropList(dce_call, mem_ctx, (struct NspiGetPropList *)r);
		break;
	case NDR_NSPIGETPROPS:
		retval = dcesrv_NspiGetProps(dce_call, mem_ctx, (struct NspiGetProps *)r);
		break;
	case NDR_NSPICOMPAREMIDS:
		retval = dcesrv_NspiCompareMIds(dce_call, mem_ctx, (struct NspiCompareMIds *)r);
		break;
	case NDR_NSPIMODPROPS:
		retval = dcesrv_NspiModProps(dce_call, mem_ctx, (struct NspiModProps *)r);
		break;
	case NDR_NSPIGETSPECIALTABLE:
		retval = dcesrv_NspiGetSpecialTable(dce_call, mem_ctx, (struct NspiGetSpecialTable *)r);
		break;
	case NDR_NSPIGETTEMPLATEINFO:
		retval = dcesrv_NspiGetTemplateInfo(dce_call, mem_ctx, (struct NspiGetTemplateInfo *)r);
		break;
	case NDR_NSPIMODLINKATT:
		retval = dcesrv_NspiModLinkAtt(dce_call, mem_ctx, (struct NspiModLinkAtt *)r);
		break;
	case NDR_NSPIDELETEENTRIES:
		retval = dcesrv_NspiDeleteEntries(dce_call, mem_ctx, (struct NspiDeleteEntries *)r);
		break;
	case NDR_NSPIQUERYCOLUMNS:
		retval = dcesrv_NspiQueryColumns(dce_call, mem_ctx, (struct NspiQueryColumns *)r);
		break;
	case NDR_NSPIGETNAMESFROMIDS:
		retval = dcesrv_NspiGetNamesFromIDs(dce_call, mem_ctx, (struct NspiGetNamesFromIDs *)r);
		break;
	case NDR_NSPIGETIDSFROMNAMES:
		retval = dcesrv_NspiGetIDsFromNames(dce_call, mem_ctx, (struct NspiGetIDsFromNames *)r);
		break;
	case NDR_NSPIRESOLVENAMES:
		retval = dcesrv_NspiResolveNames(dce_call, mem_ctx, (struct NspiResolveNames *)r);
		break;
	case NDR_NSPIRESOLVENAMESW:
		retval = dcesrv_NspiResolveNamesW(dce_call, mem_ctx, (struct NspiResolveNamesW *)r);
		break;
	}

	return NT_STATUS_OK;
}


static NTSTATUS dcesrv_exchange_nsp_init(struct dcesrv_context *dce_ctx)
{
	return NT_STATUS_OK;
}


static NTSTATUS dcesrv_exchange_nsp_unbind(struct server_id server_id, uint32_t context_id)
{
	return NT_STATUS_OK;
}


/**
   \details Entry point for the default OpenChange NSPI server

   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	server;
	NTSTATUS		ret;

	/* Fill in our name */
	server.name = "exchange_nsp";
	server.status = MAPIPROXY_DEFAULT;
	server.description = "OpenChange NSPI server";
	server.endpoint = "exchange_nsp";

	/* Fill in all the operations */
	server.init = dcesrv_exchange_nsp_init;
	server.unbind = dcesrv_exchange_nsp_unbind;
	server.dispatch = dcesrv_exchange_nsp_dispatch;
	server.push = NULL;
	server.pull = NULL;
	server.ndr_pull = NULL;

	/* Register ourselves with the MAPIPROXY server subsystem */
	ret = mapiproxy_server_register(&server);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Failed to register the 'exchange_nsp' default mapiproxy server!\n"));
		return ret;
	}

	return ret;
}
