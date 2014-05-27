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

#ifndef	__DCESRV_MAPIPROXY_PROTO_H__
#define	__DCESRV_MAPIPROXY_PROTO_H__

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* definitions from dcesrv_mapiproxy.c */
NTSTATUS dcerpc_server_mapiproxy_init(void);
NTSTATUS samba_init_module(void);

NTSTATUS dcerpc_server_exchange_nsp_init(void);
NTSTATUS dcerpc_server_exchange_emsmdb_init(void);

/* definitions from dcesrv_mapiproxy_nspi.c */
bool mapiproxy_NspiGetProps(struct dcesrv_call_state *, struct NspiGetProps *);
bool mapiproxy_NspiQueryRows(struct dcesrv_call_state *, struct NspiQueryRows *);
bool mapiproxy_NspiDNToMId(struct dcesrv_call_state *, struct NspiDNToMId *);

/* definitions from dcesrv_mapiproxy_rfr.c */
bool mapiproxy_RfrGetNewDSA(struct dcesrv_call_state *, struct RfrGetNewDSA *);

/* init functions definitions from gen_ndr/ndr_exchange_s.c */

NTSTATUS dcerpc_server_exchange_store_admin3_init(void);
NTSTATUS dcerpc_server_exchange_store_admin2_init(void);
NTSTATUS dcerpc_server_exchange_store_admin1_init(void);
NTSTATUS dcerpc_server_exchange_ds_rfr_init(void);
NTSTATUS dcerpc_server_exchange_sysatt_cluster_init(void);
NTSTATUS dcerpc_server_exchange_system_attendant_init(void);
NTSTATUS dcerpc_server_exchange_mta_init(void);
NTSTATUS dcerpc_server_exchange_drs_init(void);
NTSTATUS dcerpc_server_exchange_xds_init(void);
NTSTATUS dcerpc_server_exchange_mta_qadmin_init(void);
NTSTATUS dcerpc_server_exchange_store_information_init(void);
NTSTATUS dcerpc_server_exchange_nsp_init(void);
NTSTATUS dcerpc_server_exchange_emsmdb_init(void);
NTSTATUS dcerpc_server_exchange_async_emsmdb_init(void);
NTSTATUS dcerpc_server_exchange_unknown_init(void);

/* definitions from samba4: librpc/ndr/ndr_table.c */
NTSTATUS				ndr_table_init(void);
NTSTATUS				ndr_table_register(const struct ndr_interface_table *);
const struct ndr_interface_table	*ndr_table_by_uuid(const struct GUID *);
const struct ndr_interface_list		*ndr_table_list(void);
const struct ndr_interface_table	*ndr_table_by_name(const char *);

/* The following definitions come from dcesrv_mapiproxy_unused.c  */
void dcesrv_ec_store_admin3_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct ec_store_admin3_dummy *);
void dcesrv_ec_store_admin2_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct ec_store_admin2_dummy *);
void dcesrv_ec_store_admin1_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct ec_store_admin1_dummy *);
enum MAPISTATUS dcesrv_RfrGetNewDSA(struct dcesrv_call_state *, TALLOC_CTX *,struct RfrGetNewDSA *);
enum MAPISTATUS dcesrv_RfrGetFQDNFromLegacyDN(struct dcesrv_call_state *, TALLOC_CTX *,struct RfrGetFQDNFromLegacyDN *);
void dcesrv_sysatt_cluster_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct sysatt_cluster_dummy *);
void dcesrv_sysatt_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct sysatt_dummy *);
void dcesrv_MtaBind(struct dcesrv_call_state *, TALLOC_CTX *,struct MtaBind *);
void dcesrv_MtaBindAck(struct dcesrv_call_state *, TALLOC_CTX *,struct MtaBindAck *);
void dcesrv_ds_abandon(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_abandon *);
void dcesrv_ds_add_entry(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_add_entry *);
void dcesrv_ds_bind(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_bind *);
void dcesrv_ds_compare(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_compare *);
void dcesrv_ds_list(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_list *);
void dcesrv_ds_modify_entry(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_modify_entry *);
void dcesrv_ds_modify_rdn(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_modify_rdn *);
void dcesrv_ds_read(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_read *);
void dcesrv_ds_receive_result(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_receive_result *);
void dcesrv_ds_remove_entry(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_remove_entry *);
void dcesrv_ds_search(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_search *);
void dcesrv_ds_unbind(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_unbind *);
void dcesrv_ds_wait(struct dcesrv_call_state *, TALLOC_CTX *,struct ds_wait *);
void dcesrv_dra_replica_add(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_replica_add *);
void dcesrv_dra_replica_delete(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_replica_delete *);
void dcesrv_dra_replica_synchronize(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_replica_synchronize *);
void dcesrv_dra_reference_update(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_reference_update *);
void dcesrv_dra_authorize_replica(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_authorize_replica *);
void dcesrv_dra_unauthorize_replica(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_unauthorize_replica *);
void dcesrv_dra_adopt(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_adopt *);
void dcesrv_dra_set_status(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_set_status *);
void dcesrv_dra_modify_entry(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_modify_entry *);
void dcesrv_dra_delete_subref(struct dcesrv_call_state *, TALLOC_CTX *,struct dra_delete_subref *);
void dcesrv_xds_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct xds_dummy *);
void dcesrv_exchange_mta_qadmin(struct dcesrv_call_state *, TALLOC_CTX *,struct exchange_mta_qadmin *);
void dcesrv_exchange_store_information_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct exchange_store_information_dummy *);


/* NSPI protocol functions */
enum MAPISTATUS dcesrv_NspiBind(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiBind *);
enum MAPISTATUS dcesrv_NspiUnbind(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiUnbind *);
enum MAPISTATUS dcesrv_NspiUpdateStat(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiUpdateStat *);
enum MAPISTATUS dcesrv_NspiQueryRows(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiQueryRows *);
enum MAPISTATUS dcesrv_NspiSeekEntries(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiSeekEntries *);
enum MAPISTATUS dcesrv_NspiGetMatches(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetMatches *);
enum MAPISTATUS dcesrv_NspiResortRestriction(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiResortRestriction *);
enum MAPISTATUS dcesrv_NspiDNToMId(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiDNToMId *);
enum MAPISTATUS dcesrv_NspiGetPropList(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetPropList *);
enum MAPISTATUS dcesrv_NspiGetProps(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetProps *);
enum MAPISTATUS dcesrv_NspiCompareMIds(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiCompareMIds *);
enum MAPISTATUS  dcesrv_NspiModProps(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiModProps *);
enum MAPISTATUS dcesrv_NspiGetSpecialTable(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetSpecialTable *);
enum MAPISTATUS dcesrv_NspiGetTemplateInfo(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetTemplateInfo *);
enum MAPISTATUS dcesrv_NspiModLinkAtt(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiModLinkAtt *);
enum MAPISTATUS dcesrv_NspiDeleteEntries(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiDeleteEntries *);
enum MAPISTATUS dcesrv_NspiQueryColumns(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiQueryColumns *);
enum MAPISTATUS dcesrv_NspiGetNamesFromIDs(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetNamesFromIDs *);
enum MAPISTATUS dcesrv_NspiGetIDsFromNames(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiGetIDsFromNames *);
enum MAPISTATUS dcesrv_NspiResolveNames(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiResolveNames *);
enum MAPISTATUS dcesrv_NspiResolveNamesW(struct dcesrv_call_state *, TALLOC_CTX *,struct NspiResolveNamesW *);

/* EMSMDB protocol functions */
enum MAPISTATUS dcesrv_EcDoConnect(struct dcesrv_call_state *, TALLOC_CTX *,struct EcDoConnect *);
enum MAPISTATUS dcesrv_EcDoDisconnect(struct dcesrv_call_state *, TALLOC_CTX *,struct EcDoDisconnect *);
enum MAPISTATUS dcesrv_EcDoRpc(struct dcesrv_call_state *, TALLOC_CTX *,struct EcDoRpc *);
void dcesrv_EcGetMoreRpc(struct dcesrv_call_state *, TALLOC_CTX *,struct EcGetMoreRpc *);
enum MAPISTATUS dcesrv_EcRRegisterPushNotification(struct dcesrv_call_state *, TALLOC_CTX *,struct EcRRegisterPushNotification *);
enum MAPISTATUS dcesrv_EcRUnregisterPushNotification(struct dcesrv_call_state *, TALLOC_CTX *,struct EcRUnregisterPushNotification *);
enum MAPISTATUS dcesrv_EcDummyRpc(struct dcesrv_call_state *, TALLOC_CTX *,struct EcDummyRpc *);
void dcesrv_EcRGetDCName(struct dcesrv_call_state *, TALLOC_CTX *,struct EcRGetDCName *);
void dcesrv_EcRNetGetDCName(struct dcesrv_call_state *, TALLOC_CTX *,struct EcRNetGetDCName *);
enum MAPISTATUS dcesrv_EcDoRpcExt(struct dcesrv_call_state *, TALLOC_CTX *,struct EcDoRpcExt *);
enum MAPISTATUS dcesrv_EcDoConnectEx(struct dcesrv_call_state *, TALLOC_CTX *, struct EcDoConnectEx *);
enum MAPISTATUS dcesrv_EcDoRpcExt2(struct dcesrv_call_state *, TALLOC_CTX *, struct EcDoRpcExt2 *);
void dcesrv_EcUnknown0xC(struct dcesrv_call_state *, TALLOC_CTX *, struct EcUnknown0xC *);
void dcesrv_EcUnknown0xD(struct dcesrv_call_state *, TALLOC_CTX *, struct EcUnknown0xD *);
enum MAPISTATUS dcesrv_EcDoAsyncConnectEx(struct dcesrv_call_state *, TALLOC_CTX *, struct EcDoAsyncConnectEx *);
void dcesrv_unknown_dummy(struct dcesrv_call_state *, TALLOC_CTX *,struct unknown_dummy *);

/* AsyncEMSMDB protocol function */
enum MAPISTATUS dcesrv_EcDoAsyncWaitEx(struct dcesrv_call_state *, TALLOC_CTX *, struct EcDoAsyncWaitEx *);

__END_DECLS

#endif	/* ! __DCESRV_MAPIPROXY_PROTO_H__ */
