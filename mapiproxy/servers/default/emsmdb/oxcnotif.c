/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2015

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
   \file oxcnotif.c

   \brief Core Notifications routines and Rops
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "mapiproxy/libmapistore/gen_ndr/mapistore_notification.h"
#include "dcesrv_exchange_emsmdb.h"


/**
   \details EcDoRpc RegisterNotification (0x29) Rop. This operation
   subscribes for specified notifications on the server and returns a
   handle of the subscription to the client.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param mapi_req pointer to the RegisterNotification
   EcDoRpc_MAPI_REQ structure
   \param mapi_repl pointer to the RegisterNotification
   EcDoRpc_MAPI_REPL structure
   \param handles pointer to the MAPI handles array
   \param size pointer to the mapi_response size to update

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS EcDoRpc_RopRegisterNotification(TALLOC_CTX *mem_ctx,
							 struct emsmdbp_context *emsmdbp_ctx,
							 struct EcDoRpc_MAPI_REQ *mapi_req,
							 struct EcDoRpc_MAPI_REPL *mapi_repl,
							 uint32_t *handles, uint16_t *size)
{
	enum MAPISTATUS		retval;
	enum mapistore_error	mretval;
	struct emsmdbp_object	*parent_object;
	struct emsmdbp_object	*subscription_object;
	struct mapi_handles	*parent_rec = NULL;
	struct mapi_handles	*subscription_rec = NULL;
	uint32_t		handle;
        void                    *data;
	uint16_t		flags;
	uint64_t		fid = 0;
	uint64_t		mid = 0;

	OC_DEBUG(4, "exchange_emsmdb: [OXCNOTIF] RegisterNotification (0x29)\n");

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_req, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_repl, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!handles, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!size, MAPI_E_INVALID_PARAMETER, NULL);

	mapi_repl->opnum = mapi_req->opnum;
	mapi_repl->handle_idx = mapi_req->u.mapi_RegisterNotification.handle_idx;
	mapi_repl->error_code = MAPI_E_SUCCESS;

	handle = handles[mapi_req->handle_idx];
	retval = mapi_handles_search(emsmdbp_ctx->handles_ctx, handle, &parent_rec);
	if (retval) {
		mapi_repl->error_code = ecNullObject;
		OC_DEBUG(5, "  handle (%x) not found: %x\n", handle, mapi_req->handle_idx);
		goto end;
	}

	/* Check we have a logon user */
	if (!emsmdbp_ctx->logon_user) {
		mapi_repl->error_code = MAPI_E_LOGON_FAILED;
		goto end;
	}

        retval = mapi_handles_get_private_data(parent_rec, &data);
	if (retval) {
		mapi_repl->error_code = retval;
		OC_DEBUG(5, "  handle data not found, idx = %x\n", mapi_req->handle_idx);
		goto end;
	}

	parent_object = (struct emsmdbp_object *) data;

	retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &subscription_rec);
	if (retval) {
		mapi_repl->error_code = retval;
		goto end;
	}
	/* Notification subscriptions */
	flags = mapi_req->u.mapi_RegisterNotification.NotificationFlags;
	if (mapi_req->u.mapi_RegisterNotification.WantWholeStore) {
		flags |= sub_WholeStore;
	} else {
		fid = mapi_req->u.mapi_RegisterNotification.FolderId.ID;
		mid = mapi_req->u.mapi_RegisterNotification.MessageId.ID;
	}

	mretval = mapistore_notification_subscription_add(emsmdbp_ctx->mstore_ctx,
							  emsmdbp_ctx->session_uuid,
							  subscription_rec->handle,
							  flags, fid, mid, 0, NULL);
	if (mretval != MAPISTORE_SUCCESS) {
		/* MS-OXCROPS section 2.2.14.1 does not describe a
		 * failure response buffer for RegisterNotification
		 * Rop */
		OC_DEBUG(0, "Failed to add subscription: %s", mapistore_errstr(mretval));
	}

	handles[mapi_repl->handle_idx] = subscription_rec->handle;

	/* Create emsmdbp subscription object */
	subscription_object = emsmdbp_object_subscription_init(subscription_rec, emsmdbp_ctx, parent_object);
	if (!subscription_object) {
		OC_DEBUG(0, "Unable to create subscription object");
	}
	mapi_handles_set_private_data(subscription_rec, subscription_object);
	subscription_object->object.subscription->handle = subscription_rec->handle;

end:
	*size += libmapiserver_RopRegisterNotification_size();

	return MAPI_E_SUCCESS;
}
