/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>


/**
   \file IMAPISupport.c

   \brief MAPI notifications functions
 */


/**
   \details Register an object to receive notifications
 
   This function registers notifications on the Exchange server for an
   object.  The function holds the notifications intended to be
   monitored in as a bitmask.

   \param obj the object to get notifications for
   \param connection connection identifier for callabck function
   \param ulEventMask mask for events to provide notifications for (see
   below)
   \param notify_callback notification callback function.
   
   The event bitmask can take the following values:
   - fnevCriticalError
   - fnevNewMail
   - fnevObjectCreated
   - fnevObjectDeleted
   - fnevObjectModified
   - fnevObjectMoved
   - fnevObjectCopied
   - fnevSearchComplete
   - fnevTableModified
   - fnevStatusObjectModified
   - fnevReservedForMapi
   - fnevExtended
   
   \return MAPI_E_SUCCESS on success, otherwise -1.  
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa RegisterNotification, Unsubscribe, MonitorNotification,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS Subscribe(mapi_object_t *obj, uint32_t	*connection, 
				   uint32_t ulEventMask, 
				   mapi_notify_callback_t notify_callback)
{
	TALLOC_CTX		*mem_ctx;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct Advise_req	request;
	struct notifications	*notification;
	struct mapi_notify_ctx	*notify_ctx;
	enum MAPISTATUS		retval;
	NTSTATUS		status;
	uint32_t		size = 0;
	mapi_ctx_t		*mapi_ctx;
	static uint32_t		ulConnection = 0;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = global_mapi_ctx;
	mem_ctx = talloc_init("Subscribe");

	/* Fill the Subscribe operation */
	request.handle_idx = 0x1;
	request.notification_type = ulEventMask;
	request.layout = 0x0; /* FIXME: Neither the right nor the correct usage */
	memset(request.u.entryids.entryid, 0, sizeof (struct notif_entryid));
	request.u.entryids.entryid[0] = mapi_object_get_id(obj);
	size += 20;

	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_Advise;
	mapi_req->mapi_flags = 0;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_Advise = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction(mapi_ctx->session->emsmdb->ctx, mapi_request, &mapi_response);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	/* add the notification to the list */
	ulConnection++;
	notify_ctx = global_mapi_ctx->session->notify_ctx;
	notification = talloc_zero((TALLOC_CTX *)global_mapi_ctx->mem_ctx, struct notifications);

	notification->ulConnection = ulConnection;
	notification->parentID = mapi_object_get_id(obj);
	*connection = ulConnection;

	/* set notification handle */
	mapi_object_init(&notification->obj_notif);
	mapi_object_set_handle(&notification->obj_notif, mapi_response->handles[1]);

	notification->ulEventMask = ulEventMask;
	notification->callback = notify_callback;

	DLIST_ADD(notify_ctx->notifications, notification);

	talloc_free(mapi_response);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


/**
   \details Unregister notifications on a given object.

   Cancel any notification registrations associated with the notify
   object.  This function unregisters notifications on the Exchange
   server for the object specified with its connection number
   ulConnection. The function will releases the notification on the
   Exchange server and remove the entry from the internal
   notifications list.

   The function takes a callback to execute when such notification
   occurs and returns the ulConnection identifier we can use in
   further management.

   \return MAPI_E_SUCCESS on success, otherwise -1.  
   
   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa RegisterNotification, Subscribe, MonitorNotification,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS Unsubscribe(uint32_t ulConnection)
{
	enum MAPISTATUS	retval;
	struct mapi_notify_ctx	*notify_ctx;
	struct notifications	*notification;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	notify_ctx = global_mapi_ctx->session->notify_ctx;
	notification = notify_ctx->notifications;

	while (notification) {
		if (notification->ulConnection == ulConnection) {
			retval = Release(&notification->obj_notif);
			mapi_errstr("Release", GetLastError());
			MAPI_RETVAL_IF(retval, retval, NULL);
			DLIST_REMOVE(notify_ctx->notifications, notification);
			break;
		}
		notification = notification->next;
	}

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS ProcessNotification(struct mapi_notify_ctx *notify_ctx, 
					   struct mapi_response *mapi_response,
					   void *private_data)
{
	struct notifications	*notification;
	void			*notif_data;
	uint32_t		i;

	if (!mapi_response || !mapi_response->mapi_repl) return MAPI_E_INVALID_PARAMETER;

	for (i = 0; mapi_response->mapi_repl[i].opnum; i++) {
		switch(mapi_response->mapi_repl[i].u.mapi_Notify.ulEventType) {
		case fnevNewMail:
			notif_data = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.info.newmail);
			break;
		default:
			notif_data = NULL;
		}
		notification = notify_ctx->notifications;
		while (notification->ulConnection) {
			if (notification->ulEventMask & mapi_response->mapi_repl[i].u.mapi_Notify.ulEventType) {
				if (notification->callback) {
					notification->callback(mapi_response->mapi_repl[i].u.mapi_Notify.ulEventType,
							       (void *)notif_data,
							       (void *)private_data);
				}
			}
			notification = notification->next;
		}
	}
	return MAPI_E_SUCCESS;
}


/**
   \details Wait for notifications and process them

   This function indefinively waits for notifications on the UDP port
   and generates the traffic needed to receive MAPI
   notifications. These MAPI notifications are next compared to the
   registered ones and the callback specified in Subscribe() called if
   it matches.

   Note that the function will loop indefinitively until an error
   occurs.

   \return MAPI_E_SUCCESS on success, otherwise -1.  

   \note Developers should call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa RegisterNotification, Subscribe, Unsubscribe, GetLastError

   \note This code is experimental. The current implementation is
   non-threaded, only supports fnevNewmail and fnevCreatedObject
   notifications and will block your process until you send a signal.
*/
_PUBLIC_ enum MAPISTATUS MonitorNotification(void *private_data)
{
	struct mapi_response	*mapi_response;
	struct mapi_notify_ctx	*notify_ctx;
	enum MAPISTATUS		retval;
	NTSTATUS		status;
	int			is_done;
	int			err;
	char			buf[512];
	
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	notify_ctx = global_mapi_ctx->session->notify_ctx;

	is_done = 0;
	while (!is_done) {
		err = read(notify_ctx->fd, buf, sizeof(buf));
		if (err > 0) {
			status = emsmdb_transaction_null((struct emsmdb_context *)global_mapi_ctx->session->emsmdb->ctx, &mapi_response);
			if (!NT_STATUS_IS_OK(status)) {
				err = -1;
			} else {
				retval = ProcessNotification(notify_ctx, mapi_response, private_data);
			}
		}
		if (err <= 0) is_done = 1;
	}

	return MAPI_E_SUCCESS;
}
