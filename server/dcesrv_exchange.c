/* 
   Unix SMB/CIFS implementation.

   endpoint server for the exchange_* pipes

   Copyright (C) Julien Kerihuel 2005-2006
   Copyright (C) Gregory Schiro 2006
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
#include <dcerpc_server.h>
#include <talloc.h>
#include <core.h>
#include <util.h>
#include <param.h>
#include <core/nterr.h>
#include <dcerpc.h>
#include "ndr_exchange.h"
#include "server/dcesrv_exchange.h"
#include <dcerpc_server/common.h>
#include "ndr_mapi.h"
#include "libmapi/include/proto.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/mapi_proto.h"
#include "libmapi/include/mapitables.h"
#include "providers/emsabp.h"
#include "providers/providers_proto.h"

enum exchange_handle {
	EXCHANGE_HANDLE_NSP,
	EXCHANGE_HANDLE_EMSMDB
};

/* 
   endpoint server for the exchange_store_admin3 pipe
*/

/* 
  ec_store_admin3_dummy 
*/
static void ec_store_admin3_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ec_store_admin3_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
   endpoint server for the exchange_store_admin2 pipe
*/

/* 
  ec_store_admin2_dummy 
*/
static void ec_store_admin2_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ec_store_admin2_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
   endpoint server for the exchange_store_admin1 pipe
*/

/* 
  ec_store_admin1_dummy 
*/
static void ec_store_admin1_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ec_store_admin1_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
   endpoint server for the exchange_ds_rfr pipe
*/

/* 
  RfrGetNewDSA 
*/
static void RfrGetNewDSA(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct RfrGetNewDSA *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  RfrGetFQDNFromLegacyDN 
*/
static void RfrGetFQDNFromLegacyDN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct RfrGetFQDNFromLegacyDN *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_sysatt_cluster pipe
*/

/* 
  sysatt_cluster_dummy 
*/
static void sysatt_cluster_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct sysatt_cluster_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_system_attendant pipe
*/

/* 
  sysatt_dummy 
*/
static void sysatt_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct sysatt_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_mta pipe
*/

/* 
  MtaBind 
*/
static void MtaBind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct MtaBind *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  MtaBindAck 
*/
static void MtaBindAck(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct MtaBindAck *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_drs pipe
*/

/* 
  ds_abandon 
*/
static void ds_abandon(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_abandon *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_add_entry 
*/
static void ds_add_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_add_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_bind 
*/
static void ds_bind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_bind *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_compare 
*/
static void ds_compare(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_compare *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_list 
*/
static void ds_list(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_list *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_modify_entry 
*/
static void ds_modify_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_modify_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_modify_rdn 
*/
static void ds_modify_rdn(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_modify_rdn *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_read 
*/
static void ds_read(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_read *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_receive_result 
*/
static void ds_receive_result(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_receive_result *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_remove_entry 
*/
static void ds_remove_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_remove_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_search 
*/
static void ds_search(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_search *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_unbind 
*/
static void ds_unbind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_unbind *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  ds_wait 
*/
static void ds_wait(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_wait *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_replica_add 
*/
static void dra_replica_add(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_replica_add *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_replica_delete 
*/
static void dra_replica_delete(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_replica_delete *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_replica_synchronize 
*/
static void dra_replica_synchronize(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_replica_synchronize *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_reference_update 
*/
static void dra_reference_update(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_reference_update *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_authorize_replica 
*/
static void dra_authorize_replica(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_authorize_replica *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_unauthorize_replica 
*/
static void dra_unauthorize_replica(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_unauthorize_replica *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_adopt 
*/
static void dra_adopt(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_adopt *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_set_status 
*/
static void dra_set_status(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_set_status *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_modify_entry 
*/
static void dra_modify_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_modify_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  dra_delete_subref 
*/
static void dra_delete_subref(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_delete_subref *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_xds pipe
*/

/* 
  xds_dummy 
*/
static void xds_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct xds_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_mta_qadmin pipe
*/

/* 
  exchange_mta_qadmin 
*/
static void exchange_mta_qadmin(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct exchange_mta_qadmin *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_store_information pipe
*/

/* 
  exchange_store_information_dummy 
*/
static void exchange_store_information_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct exchange_store_information_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_nsp pipe
*/

/* 
  NspiBind 
*/

static enum MAPISTATUS NspiBind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiBind *r)
{
	struct GUID		*guid = (struct GUID *) NULL;
	const char		*exchange_GUID = lp_parm_string(-1, "exchange", "GUID");
	struct emsabp_ctx	*emsabp_context;
	struct dcesrv_handle	*handle;
	struct policy_handle	wire_handle;

	DEBUG(0, ("##### in NspiBind ####\n"));

	if (!NTLM_AUTH_IS_OK(dce_call)) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));

		/* Create an empty policy handle */
		wire_handle.handle_type = EXCHANGE_HANDLE_NSP;
		wire_handle.uuid = GUID_zero();
		*r->out.handle = wire_handle;

		r->out.mapiuid = r->in.mapiuid;
		r->out.result = MAPI_E_LOGON_FAILED;
		return MAPI_E_LOGON_FAILED;
	}

	emsabp_context = emsabp_init((void *)dce_call->conn, dce_call);
	if (!emsabp_context) {
	  return MAPI_E_FAILONEPROVIDER;
	}

	/* check if a valid CPID has been provided */
	if (valid_codepage(r->in.settings->codepage) == False) {
		DEBUG(1, ("Invalid CPID\n"));
		r->out.mapiuid = r->in.mapiuid;
		r->out.result = MAPI_E_UNKNOWN_CPID;
		return MAPI_E_UNKNOWN_CPID;
	}
	
	guid = talloc(mem_ctx, struct GUID);
	if (!NT_STATUS_IS_OK(GUID_from_string(exchange_GUID, guid))) {
		DEBUG(1, ("No Exchange default GUID specified"));
		r->out.mapiuid = r->in.mapiuid;
		r->out.result = MAPI_E_LOGON_FAILED;
		return MAPI_E_LOGON_FAILED;
	}

	handle = dcesrv_handle_new(dce_call->context, EXCHANGE_HANDLE_NSP);
	if (!handle) {
		/* replaces NT_STATUS_NO_MEMORY */
		return MAPI_E_NOT_ENOUGH_RESOURCES;
	}

	handle->data = (void *) emsabp_context;
	*r->out.handle = handle->wire_handle;
	r->out.mapiuid = guid;
	r->out.result = MAPI_E_SUCCESS;

	DEBUG(0, ("NspiBind : Success\n"));

	return MAPI_E_SUCCESS;
}


/* 
  NspiUnbind 
*/
static enum MAPISTATUS NspiUnbind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiUnbind *r)
{
/* 	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR); */
	return MAPI_E_SUCCESS;
}


/* 
  NspiUpdateStat 
*/
static void NspiUpdateStat(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiUpdateStat *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiQueryRows 
*/
static enum MAPISTATUS NspiQueryRows(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiQueryRows *r)
{
	struct emsabp_ctx	*emsabp_context;
	struct dcesrv_handle	*h;
	NTSTATUS		status;
	int			row_nb, i = 0;

	DEBUG(0, ("##### in NspiQueryRows ####\n"));

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	emsabp_context = (struct emsabp_ctx *) h->data;

        /* MAPI_SETTINGS */
	r->out.settings = r->in.settings;
	r->out.settings->service_provider.ab[0] = (uint8_t)((*r->in.instance_key) & 0xFF);
	r->out.settings->service_provider.ab[1] = (uint8_t)(((*r->in.instance_key) >> 8) & 0xFF);

	row_nb = r->in.lRows;

	/* Row Set */
	r->out.RowSet = talloc(mem_ctx, struct SRowSet *);
	r->out.RowSet[0] = talloc(mem_ctx, struct SRowSet);
	r->out.RowSet[0]->cRows = row_nb;
	r->out.RowSet[0]->aRow = talloc_size(mem_ctx, sizeof(struct SRow) * row_nb);
	while (i < row_nb) {
		status = emsabp_fetch_attrs(mem_ctx, emsabp_context, &(r->out.RowSet[0]->aRow[i]), r->in.instance_key[i], r->in.REQ_properties);
		if (!NT_STATUS_IS_OK(status))  /* FIXME */
			return MAPI_E_LOGON_FAILED;
		i++;
	}

        r->out.result = MAPI_E_SUCCESS;

	DEBUG(0, ("NspiQueryRows : Success\n"));

        return MAPI_E_SUCCESS;
}


/* 
  NspiSeekEntries 
*/
static void NspiSeekEntries(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiSeekEntries *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetMatches 
*/
static enum MAPISTATUS NspiGetMatches(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				      struct NspiGetMatches *r)
{
	struct dcesrv_handle	*h;
	struct emsabp_ctx	*emsabp_context;
	struct instance_key	*instance_keys;
	NTSTATUS		status;
	int			nbrows = 0;

	DEBUG(0, ("##### in NspiGetMatches ####\n"));

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	emsabp_context = (struct emsabp_ctx *) h->data;

        /* Settings */
        r->out.settings = r->in.settings;

	/* Search the provider for the requested recipient */
	instance_keys = talloc(emsabp_context->mem_ctx, struct instance_key);
	status = emsabp_search(emsabp_context, instance_keys, r->in.restrictions);
	if (!NT_STATUS_IS_OK(status)) {
		return MAPI_E_LOGON_FAILED;
	}
	
        /* Row Set */
        r->out.RowSet = talloc(mem_ctx, struct SRowSet *);
	r->out.RowSet[0] = talloc(mem_ctx, struct SRowSet);
	r->out.RowSet[0]->cRows = instance_keys->cValues - 1;
	r->out.RowSet[0]->aRow = talloc_size(mem_ctx, sizeof(struct SRow) * (instance_keys->cValues - 1));
	/* Instance keys */
	r->out.instance_key = instance_keys;

	DEBUG(0,("All NspiGetMatches instance_keys(%d)\n", instance_keys->cValues));
	nbrows = 0;
	while (nbrows < (instance_keys->cValues - 1)) {
		DEBUG(0,("instance_keys[%d] = 0x%x\n", nbrows, instance_keys->value[nbrows]));
		status = emsabp_fetch_attrs(mem_ctx, emsabp_context, &(r->out.RowSet[0]->aRow[nbrows]), instance_keys->value[nbrows], r->in.REQ_properties);
		if (!NT_STATUS_IS_OK(status))	/* FIXME */
			return MAPI_E_LOGON_FAILED;

		DEBUG(0,("NspiGetMatches after set: instance_keys[%d] = 0x%x\n", nbrows, r->out.instance_key->value[nbrows]));

		nbrows++;
	}

        r->out.result = MAPI_E_SUCCESS;
	
	DEBUG(0, ("NspiGetMatches : Success\n"));
	
        return MAPI_E_SUCCESS;
}


/* 
  NspiResortRestriction 
*/
static void NspiResortRestriction(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiResortRestriction *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiDNToEph 
*/
static enum MAPISTATUS NspiDNToEph(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiDNToEph *r)
{
	uint32_t		instance_key;
	struct dcesrv_handle	*h;
	struct emsabp_ctx	*emsabp_context;
	NTSTATUS		status;

 	DEBUG(0, ("##### in NspiDNToEph ####\n"));

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	emsabp_context = (struct emsabp_ctx *) h->data;

	/* Search the server identifier according to the given legacyExchangeDN */

	/* Instance key */
        r->out.instance_key = talloc(mem_ctx, struct instance_key);
        r->out.instance_key->value = talloc_size(mem_ctx, sizeof (uint32_t));

	status = emsabp_search_dn(emsabp_context, NULL, &(instance_key), r->in.server_dn->str);
	if (!NT_STATUS_IS_OK(status)) {
		/* Microsoft Exchange returns success even when the research failed */
		memset(r->out.instance_key->value, 0, sizeof(uint32_t));
		r->out.instance_key->cValues = 0x2;
		return MAPI_E_SUCCESS;
	}

	r->out.instance_key->value[0] = instance_key;

        r->out.instance_key->cValues = 0x2;

        r->out.result = MAPI_E_SUCCESS;

	DEBUG(0, ("NspiDNToEph : Success\n"));	

        return MAPI_E_SUCCESS;
}


/* 
  NspiGetPropList 
*/
static void NspiGetPropList(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetPropList *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetProps 
*/
static enum MAPISTATUS NspiGetProps(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct NspiGetProps *r)
{
	uint32_t		instance_key;
	struct dcesrv_handle	*h;
	struct emsabp_ctx	*emsabp_context;
	NTSTATUS		status;

	DEBUG(0, ("##### in NspiGetProps ####\n"));

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
	emsabp_context = (struct emsabp_ctx *) h->data;

	/* Convert instance_key */
	instance_key = r->in.settings->service_provider.ab[1];
	instance_key <<= 8;
	instance_key |= r->in.settings->service_provider.ab[0];

	r->out.REPL_values = talloc_size(mem_ctx, sizeof(struct SRow *));
	r->out.REPL_values[0] = talloc_size(mem_ctx, sizeof(struct SRow));

	status = emsabp_fetch_attrs(mem_ctx, emsabp_context, &(r->out.REPL_values[0][0]), instance_key, r->in.REQ_properties);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.result = MAPI_W_ERRORS_RETURNED;
		return MAPI_W_ERRORS_RETURNED;
	}

        r->out.result = MAPI_E_SUCCESS;

	DEBUG(0, ("NspiGetProps : Success\n"));	
	
	return MAPI_E_SUCCESS;
}


/* 
  NspiCompareDNTs 
*/
static void NspiCompareDNTs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiCompareDNTs *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiModProps 
*/
static void NspiModProps(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiModProps *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetHierarchyInfo 
*/
static enum MAPISTATUS NspiGetHierarchyInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetHierarchyInfo *r)
{
	struct dcesrv_handle    *h;
	struct emsabp_ctx       *emsabp_context;

	DEBUG(0, ("##### in NspiGetHierarchyInfo ####\n"));

	h = dcesrv_handle_fetch(dce_call->context, r->in.handle, DCESRV_HANDLE_ANY);
        emsabp_context = (struct emsabp_ctx *) h->data;

	r->out.unknown2 = talloc(mem_ctx, uint32_t);
	*(r->out.unknown2) = 0x1;
	
	r->out.RowSet = talloc(mem_ctx, struct SRowSet *);
	r->out.RowSet[0] = talloc(mem_ctx, struct SRowSet);
	emsabp_get_hierarchytable(mem_ctx, emsabp_context, r->in.unknown1, r->out.RowSet);

	DEBUG(0, ("NspiGetHierarchyInfo : success\n"));

	return MAPI_E_SUCCESS;

}


/* 
  NspiGetTemplateInfo 
*/
static void NspiGetTemplateInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetTemplateInfo *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiModLInkAtt 
*/
static void NspiModLInkAtt(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiModLInkAtt *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiDeleteEntries 
*/
static void NspiDeleteEntries(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiDeleteEntries *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiQueryColumns 
*/
static void NspiQueryColumns(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiQueryColumns *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetNamesFromIDs 
*/
static void NspiGetNamesFromIDs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetNamesFromIDs *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetIDsFromNames 
*/
static void NspiGetIDsFromNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetIDsFromNames *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiResolveNames 
*/
static void NspiResolveNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiResolveNames *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiResolveNamesW 
*/
static void NspiResolveNamesW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiResolveNamesW *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_emsmdb pipe
*/

/* 
  EcDoConnect 
*/
static enum MAPISTATUS EcDoConnect(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoConnect *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDoDisconnect 
*/
static enum MAPISTATUS EcDoDisconnect(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoDisconnect *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDoRpc 
*/
static enum MAPISTATUS EcDoRpc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoRpc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcGetMoreRpc 
*/
static void EcGetMoreRpc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcGetMoreRpc *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRRegisterPushNotification 
*/
static enum MAPISTATUS EcRRegisterPushNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRRegisterPushNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRUnregisterPushNotification 
*/
static enum MAPISTATUS EcRUnregisterPushNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRUnregisterPushNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDummyRpc 
*/
static void EcDummyRpc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDummyRpc *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRGetDCName 
*/
static void EcRGetDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRGetDCName *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRNetGetDCName 
*/
static void EcRNetGetDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRNetGetDCName *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDoRpcExt 
*/
static void EcDoRpcExt(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoRpcExt *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_unknown pipe
*/

/* 
  unknown_dummy 
*/
static void unknown_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct unknown_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "ndr_exchange_s.c"

NTSTATUS init_module(void)
{
	NTSTATUS ret;

 	ret = dcerpc_server_exchange_store_admin3_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_store_admin2_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_store_admin1_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_ds_rfr_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_sysatt_cluster_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_system_attendant_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_mta_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_drs_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_xds_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_mta_qadmin_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

 	ret = dcerpc_server_exchange_store_information_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

	ret = dcerpc_server_exchange_nsp_init();
	NT_STATUS_NOT_OK_RETURN(ret);

	ret = dcerpc_server_exchange_emsmdb_init();
	NT_STATUS_NOT_OK_RETURN(ret);

 	ret = dcerpc_server_exchange_unknown_init(); 
 	NT_STATUS_NOT_OK_RETURN(ret); 

	return NT_STATUS_OK;
}
