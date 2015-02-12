/*
   MAPI Proxy

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

#include "config.h"

#include <sys/types.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <talloc.h>
#include <dcerpc.h>

#include "gen_ndr/exchange.h"

#include <dcerpc_server.h>
#include <param.h>

#include "gen_ndr/ndr_exchange.h"
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/dcesrv_mapiproxy_proto.h"

#include <sys/types.h>

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

/*
   endpoint server for the exchange_store_admin3 pipe
*/

/*
  ec_store_admin3_dummy
*/
void dcesrv_ec_store_admin3_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_ec_store_admin2_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_ec_store_admin1_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
enum MAPISTATUS dcesrv_RfrGetNewDSA(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct RfrGetNewDSA *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  RfrGetFQDNFromLegacyDN
*/
enum MAPISTATUS dcesrv_RfrGetFQDNFromLegacyDN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					      struct RfrGetFQDNFromLegacyDN *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
   endpoint server for the exchange_sysatt_cluster pipe
*/

/*
  sysatt_cluster_dummy
*/
void dcesrv_sysatt_cluster_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_sysatt_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_MtaBind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct MtaBind *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  MtaBindAck
*/
void dcesrv_MtaBindAck(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_ds_abandon(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_abandon *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_add_entry
*/
void dcesrv_ds_add_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_add_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_bind
*/
void dcesrv_ds_bind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_bind *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_compare
*/
void dcesrv_ds_compare(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_compare *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_list
*/
void dcesrv_ds_list(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_list *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_modify_entry
*/
void dcesrv_ds_modify_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_modify_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_modify_rdn
*/
void dcesrv_ds_modify_rdn(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_modify_rdn *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_read
*/
void dcesrv_ds_read(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_read *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_receive_result
*/
void dcesrv_ds_receive_result(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_receive_result *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_remove_entry
*/
void dcesrv_ds_remove_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_remove_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_search
*/
void dcesrv_ds_search(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_search *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_unbind
*/
void dcesrv_ds_unbind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_unbind *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  ds_wait
*/
void dcesrv_ds_wait(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct ds_wait *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_replica_add
*/
void dcesrv_dra_replica_add(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_replica_add *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_replica_delete
*/
void dcesrv_dra_replica_delete(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_replica_delete *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_replica_synchronize
*/
void dcesrv_dra_replica_synchronize(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_replica_synchronize *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_reference_update
*/
void dcesrv_dra_reference_update(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_reference_update *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_authorize_replica
*/
void dcesrv_dra_authorize_replica(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_authorize_replica *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_unauthorize_replica
*/
void dcesrv_dra_unauthorize_replica(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_unauthorize_replica *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_adopt
*/
void dcesrv_dra_adopt(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_adopt *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_set_status
*/
void dcesrv_dra_set_status(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_set_status *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_modify_entry
*/
void dcesrv_dra_modify_entry(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct dra_modify_entry *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  dra_delete_subref
*/
void dcesrv_dra_delete_subref(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_xds_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_exchange_mta_qadmin(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
void dcesrv_exchange_store_information_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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

enum MAPISTATUS dcesrv_NspiBind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiBind *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiUnbind 
*/
enum MAPISTATUS dcesrv_NspiUnbind(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiUnbind *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiUpdateStat 
*/
enum MAPISTATUS dcesrv_NspiUpdateStat(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiUpdateStat *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiQueryRows 
*/
enum MAPISTATUS dcesrv_NspiQueryRows(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiQueryRows *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiSeekEntries 
*/
enum MAPISTATUS dcesrv_NspiSeekEntries(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct NspiSeekEntries *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetMatches 
*/
enum MAPISTATUS dcesrv_NspiGetMatches(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				      struct NspiGetMatches *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiResortRestriction 
*/
enum MAPISTATUS dcesrv_NspiResortRestriction(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					     struct NspiResortRestriction *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiDNToMId
*/
enum MAPISTATUS dcesrv_NspiDNToMId(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				   struct NspiDNToMId *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetPropList 
*/
enum MAPISTATUS dcesrv_NspiGetPropList(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct NspiGetPropList *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetProps 
*/
enum MAPISTATUS dcesrv_NspiGetProps(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct NspiGetProps *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiCompareMIds 
*/
enum MAPISTATUS dcesrv_NspiCompareMIds(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct NspiCompareMIds *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiModProps 
*/
enum MAPISTATUS dcesrv_NspiModProps(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				    struct NspiModProps *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetSpecialTable 
*/
enum MAPISTATUS dcesrv_NspiGetSpecialTable(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetSpecialTable *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetTemplateInfo 
*/
enum MAPISTATUS dcesrv_NspiGetTemplateInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiGetTemplateInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiModLInkAtt 
*/
enum MAPISTATUS dcesrv_NspiModLinkAtt(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				      struct NspiModLinkAtt *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiDeleteEntries 
*/
enum MAPISTATUS dcesrv_NspiDeleteEntries(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					 struct NspiDeleteEntries *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiQueryColumns 
*/
enum MAPISTATUS dcesrv_NspiQueryColumns(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiQueryColumns *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetNamesFromIDs 
*/
enum MAPISTATUS dcesrv_NspiGetNamesFromIDs(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					   struct NspiGetNamesFromIDs *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiGetIDsFromNames 
*/
enum MAPISTATUS dcesrv_NspiGetIDsFromNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					   struct NspiGetIDsFromNames *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiResolveNames 
*/
enum MAPISTATUS dcesrv_NspiResolveNames(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiResolveNames *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  NspiResolveNamesW 
*/
enum MAPISTATUS dcesrv_NspiResolveNamesW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct NspiResolveNamesW *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_emsmdb pipe
*/

/* 
  EcDoConnect 
*/
enum MAPISTATUS dcesrv_EcDoConnect(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoConnect *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDoDisconnect 
*/
enum MAPISTATUS dcesrv_EcDoDisconnect(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoDisconnect *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDoRpc 
*/
enum MAPISTATUS dcesrv_EcDoRpc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoRpc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcGetMoreRpc 
*/
void dcesrv_EcGetMoreRpc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcGetMoreRpc *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRRegisterPushNotification 
*/
enum MAPISTATUS dcesrv_EcRRegisterPushNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRRegisterPushNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRUnregisterPushNotification 
*/
enum MAPISTATUS dcesrv_EcRUnregisterPushNotification(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRUnregisterPushNotification *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDummyRpc 
*/
enum MAPISTATUS dcesrv_EcDummyRpc(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		                  struct EcDummyRpc *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRGetDCName 
*/
void dcesrv_EcRGetDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRGetDCName *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcRNetGetDCName 
*/
void dcesrv_EcRNetGetDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcRNetGetDCName *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  EcDoRpcExt 
*/
enum MAPISTATUS dcesrv_EcDoRpcExt(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct EcDoRpcExt *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
  EcDoConnect Ex
*/
enum MAPISTATUS dcesrv_EcDoConnectEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct EcDoConnectEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
   EcDoRpcExt2
 */
enum MAPISTATUS dcesrv_EcDoRpcExt2(struct dcesrv_call_state  *dce_call, TALLOC_CTX *mem_ctx,
				   struct EcDoRpcExt2 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  EcDoUnknown0xc
 */
void dcesrv_EcUnknown0xC(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			struct EcUnknown0xC *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  EcDoAsyncConnectEx
 */
void dcesrv_EcUnknown0xD(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			 struct EcUnknown0xD *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  EcDoAsyncConnectEx
 */
enum MAPISTATUS dcesrv_EcDoAsyncConnectEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					  struct EcDoAsyncConnectEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  endpoint server for the exchange_async_emsmdb pipe
 */
enum MAPISTATUS dcesrv_EcDoAsyncWaitEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct EcDoAsyncWaitEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/* 
   endpoint server for the exchange_unknown pipe
*/

/* 
  unknown_dummy 
*/
void dcesrv_unknown_dummy(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct unknown_dummy *r)
{
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}
