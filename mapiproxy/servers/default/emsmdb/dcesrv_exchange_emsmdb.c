/*
   MAPI Proxy - Exchange EMSMDB Server

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
   \file dcesrv_exchange_emsmdb.c

   \brief OpenChange EMSMDB Server implementation
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_emsmdb.h"
#include <util/debug.h>


/**
   \details exchange_emsmdb EcDoConnect (0x0) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoConnect request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoConnect(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct EcDoConnect *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDoConnect (0x0) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDoDisconnect (0x1) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoDisconnect request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoDisconnect(struct dcesrv_call_state *dce_call,
					     TALLOC_CTX *mem_ctx,
					     struct EcDoDisconnect *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDoDisconnect (0x1) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDoRpc (0x2) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoRpc request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoRpc(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx,
				      struct EcDoRpc *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDoRpc (0x2) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcGetMoreRpc (0x3) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcGetMoreRpc request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcGetMoreRpc(struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct EcGetMoreRpc *r)
{
	DEBUG(3, ("exchange_emsmdb: EcGetMoreRpc (0x3) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcRRegisterPushNotification (0x4) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRRegisterPushNotification request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcRRegisterPushNotification(struct dcesrv_call_state *dce_call,
							  TALLOC_CTX *mem_ctx,
							  struct EcRRegisterPushNotification *r)
{
	DEBUG(3, ("exchange_emsmdb: EcRRegisterPushNotification (0x4) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcRUnregisterPushNotification (0x5) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRUnregisterPushNotification request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcRUnregisterPushNotification(struct dcesrv_call_state *dce_call,
							    TALLOC_CTX *mem_ctx,
							    struct EcRUnregisterPushNotification *r)
{
	DEBUG(3, ("exchange_emsmdb: EcRUnregisterPushNotification (0x5) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDummyRpc (0x6) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDummyRpc request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcDummyRpc(struct dcesrv_call_state *dce_call,
			      TALLOC_CTX *mem_ctx,
			      struct EcDummyRpc *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDummyRpc (0x6) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcRGetDCName (0x7) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRGetDCName request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcRGetDCName(struct dcesrv_call_state *dce_call,
				TALLOC_CTX *mem_ctx,
				struct EcRGetDCName *r)
{
	DEBUG(3, ("exchange_emsmdb: EcRGetDCName (0x7) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcRNetGetDCName (0x8) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcRNetGetDCName request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcRNetGetDCName(struct dcesrv_call_state *dce_call,
				   TALLOC_CTX *mem_ctx,
				   struct EcRNetGetDCName *r)
{
	DEBUG(3, ("exchange_emsmdb: EcRNetGetDCName (0x8) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDoRpcExt (0x9) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoRpcExt request data

   \return MAPI_E_SUCCESS on success
 */
static void dcesrv_EcDoRpcExt(struct dcesrv_call_state *dce_call,
			      TALLOC_CTX *mem_ctx,
			      struct EcDoRpcExt *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDoRpcExt (0x9) not implemented\n"));
	DCESRV_FAULT_VOID(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details exchange_emsmdb EcDoConnectEx (0xA) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoConnectEx request data

   \return MAPI_E_SUCCESS on success
 */
static enum MAPISTATUS dcesrv_EcDoConnectEx(struct dcesrv_call_state *dce_call,
					    TALLOC_CTX *mem_ctx,
					    struct EcDoConnectEx *r)
{
	DEBUG(3, ("exchange_emsmdb: EcDoConnectEx (0xA) not implemented\n"));
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/**
   \details Dispatch incoming EMSMDB call to the correct OpenChange
   server function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r generic pointer on EMSMDB data
   \param mapiproxy pointer to the mapiproxy structure controlling
   mapiproxy behavior

   \return NT_STATUS_OK;
 */
static NTSTATUS dcesrv_exchange_emsmdb_dispatch(struct dcesrv_call_state *dce_call,
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
	if (table->name && strcmp(table->name, NDR_EXCHANGE_EMSMDB_NAME)) return NT_STATUS_UNSUCCESSFUL;

	switch (opnum) {
	case NDR_ECDOCONNECT:
		retval = dcesrv_EcDoConnect(dce_call, mem_ctx, (struct EcDoConnect *)r);
		break;
	case NDR_ECDODISCONNECT:
		retval = dcesrv_EcDoDisconnect(dce_call, mem_ctx, (struct EcDoDisconnect *)r);
		break;
	case NDR_ECDORPC:
		retval = dcesrv_EcDoRpc(dce_call, mem_ctx, (struct EcDoRpc *)r);
		break;
	case NDR_ECGETMORERPC:
		dcesrv_EcGetMoreRpc(dce_call, mem_ctx, (struct EcGetMoreRpc *)r);
		break;
	case NDR_ECRREGISTERPUSHNOTIFICATION:
		retval = dcesrv_EcRRegisterPushNotification(dce_call, mem_ctx, (struct EcRRegisterPushNotification *)r);
		break;
	case NDR_ECRUNREGISTERPUSHNOTIFICATION:
		retval = dcesrv_EcRUnregisterPushNotification(dce_call, mem_ctx, (struct EcRUnregisterPushNotification *)r);
		break;
	case NDR_ECDUMMYRPC:
		dcesrv_EcDummyRpc(dce_call, mem_ctx, (struct EcDummyRpc *)r);
		break;
	case NDR_ECRGETDCNAME:
		dcesrv_EcRGetDCName(dce_call, mem_ctx, (struct EcRGetDCName *)r);
		break;
	case NDR_ECRNETGETDCNAME:
		dcesrv_EcRNetGetDCName(dce_call, mem_ctx, (struct EcRNetGetDCName *)r);
		break;
	case NDR_ECDORPCEXT:
		dcesrv_EcDoRpcExt(dce_call, mem_ctx, (struct EcDoRpcExt *)r);
		break;
	case NDR_ECDOCONNECTEX:
		retval = dcesrv_EcDoConnectEx(dce_call, mem_ctx, (struct EcDoConnectEx *)r);
		break;
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize the EMSMDB OpenChange server

   \param dce_ctx pointer to the server context

   \return NT_STATUS_OK on success
 */
static NTSTATUS dcesrv_exchange_emsmdb_init(struct dcesrv_context *dce_ctx)
{
	return NT_STATUS_OK;
}


/**
   \details Terminate the EMSMDB connection

   \param server_id reference to the server identifier structure
   \param context_id the connection context identifier

   \return NT_STATUS_OK on success
 */
static NTSTATUS dcesrv_exchange_emsmdb_unbind(struct server_id server_id, uint32_t context_id)
{
	return NT_STATUS_OK;
}


/**
   \details Entry point for the default OpenChange EMSMDB server

   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	server;
	NTSTATUS		ret;

	/* Fill in our name */
	server.name = "exchange_emsmdb";
	server.status = MAPIPROXY_DEFAULT;
	server.description = "OpenChange EMSMDB server";
	server.endpoint = "exchange_emsmdb";

	/* Fill in all the operations */
	server.init = dcesrv_exchange_emsmdb_init;
	server.unbind = dcesrv_exchange_emsmdb_unbind;
	server.dispatch = dcesrv_exchange_emsmdb_dispatch;
	server.push = NULL;
	server.pull = NULL;
	server.ndr_pull = NULL;

	/* Register ourselves with the MAPIPROXY server subsystem */
	ret = mapiproxy_server_register(&server);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0, ("Failed to register the 'exchange_emsmdb' default mapiproxy server!\n"));
		return ret;
	}

	return ret;
}
