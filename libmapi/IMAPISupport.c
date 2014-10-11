/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

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

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"


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
   \param connection connection identifier for callback function
   \param NotificationFlags mask for events to provide notifications
   for (see below)
   \param WholeStore whether the scope for this notification is whole
   database
   \param notify_callback notification callback function.
   \param private_data the data to be passed at the callback function
   when invoked
   
   The Notification Flags can take the following values:
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
   
   \return MAPI_E_SUCCESS on success, otherwise MAPI error.  
   
   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa RegisterNotification, Unsubscribe, MonitorNotification,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS Subscribe(mapi_object_t *obj, uint32_t	*connection, 
				   uint16_t NotificationFlags,
				   bool WholeStore,
				   mapi_notify_callback_t notify_callback,
				   void	*private_data)
{
	TALLOC_CTX			*mem_ctx;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	struct RegisterNotification_req	request;
	struct notifications		*notification;
	struct mapi_notify_ctx		*notify_ctx;
	struct mapi_session		*session;
	enum MAPISTATUS			retval;
	NTSTATUS			status;
	uint32_t			size = 0;
	static uint32_t			ulConnection = 0;
	uint8_t 			logon_id = 0;

	/* Sanity Checks */
	OPENCHANGE_RETVAL_IF(!connection, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!obj, MAPI_E_INVALID_PARAMETER, NULL);

	session = mapi_object_get_session(obj);
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session->notify_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	if ((retval = mapi_object_get_logon_id(obj, &logon_id)) != MAPI_E_SUCCESS)
		return retval;

	mem_ctx = talloc_named(session, 0, "Subscribe");

	/* Fill the Subscribe operation */
	request.handle_idx = 0x1;
	size += sizeof (uint8_t);

	request.NotificationFlags = NotificationFlags;
	size += sizeof (uint16_t);

	request.WantWholeStore = WholeStore;
	size += sizeof (uint8_t);

	if (WholeStore == false) {
		request.FolderId.ID = mapi_object_get_id(obj);
		size += sizeof (int64_t);

		request.MessageId.ID = 0x0;
		size += sizeof (int64_t);
	}
	/* Fill the MAPI_REQ request */
	mapi_req = talloc_zero(mem_ctx, struct EcDoRpc_MAPI_REQ);
	mapi_req->opnum = op_MAPI_RegisterNotification;
	mapi_req->logon_id = logon_id;
	mapi_req->handle_idx = 0;
	mapi_req->u.mapi_RegisterNotification = request;
	size += 5;

	/* Fill the mapi_request structure */
	mapi_request = talloc_zero(mem_ctx, struct mapi_request);
	mapi_request->mapi_len = size + sizeof (uint32_t) * 2;
	mapi_request->length = size;
	mapi_request->mapi_req = mapi_req;
	mapi_request->handles = talloc_array(mem_ctx, uint32_t, 2);
	mapi_request->handles[0] = mapi_object_get_handle(obj);
	mapi_request->handles[1] = 0xffffffff;

	status = emsmdb_transaction_wrapper(session, mem_ctx, mapi_request, &mapi_response);
	OPENCHANGE_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_CALL_FAILED, mem_ctx);
	retval = mapi_response->mapi_repl->error_code;
	OPENCHANGE_RETVAL_IF(retval, retval, mem_ctx);

	/* add the notification to the list */
	ulConnection++;
	notify_ctx = session->notify_ctx;
	notification = talloc_zero((TALLOC_CTX *)session, struct notifications);

	notification->ulConnection = ulConnection;
	notification->parentID = mapi_object_get_id(obj);
	*connection = ulConnection;

	/* set notification handle */
	mapi_object_init(&notification->obj_notif);
	mapi_object_set_handle(&notification->obj_notif, mapi_response->handles[1]);
	mapi_object_set_session(&notification->obj_notif, session);

	notification->NotificationFlags = NotificationFlags;
	notification->callback = notify_callback;
	notification->private_data = private_data;

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

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.  
   
   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
   transaction
   
   \sa RegisterNotification, Subscribe, MonitorNotification,
   GetLastError
*/
_PUBLIC_ enum MAPISTATUS Unsubscribe(struct mapi_session *session, uint32_t ulConnection)
{
	enum MAPISTATUS		retval;
	struct mapi_notify_ctx	*notify_ctx;
	struct notifications	*notification;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session->notify_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	notify_ctx = session->notify_ctx;
	notification = notify_ctx->notifications;

	while (notification) {
		if (notification->ulConnection == ulConnection) {
			retval = Release(&notification->obj_notif);
			OPENCHANGE_RETVAL_IF(retval, retval, NULL);
			DLIST_REMOVE(notify_ctx->notifications, notification);
			break;
		}
		notification = notification->next;
	}

	return MAPI_E_SUCCESS;
}

enum MAPISTATUS ProcessNotification(struct mapi_notify_ctx *notify_ctx, 
					   struct mapi_response *mapi_response)
{
	struct notifications	*notification;
	void			*NotificationData;
	uint32_t		i;

	OPENCHANGE_RETVAL_IF(!mapi_response, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapi_response->mapi_repl, MAPI_E_SUCCESS, NULL);

	for (i = 0; mapi_response->mapi_repl[i].opnum; i++) {
		if (mapi_response->mapi_repl[i].opnum == op_MAPI_Notify) {
			mapi_handle_t	handle = mapi_response->mapi_repl[i].u.mapi_Notify.NotificationHandle;

			switch(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationType) {
			case fnevNewMail:
			case fnevMbit|fnevNewMail:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.NewMailNotification);
				break;
			case fnevObjectCreated:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderCreatedNotification);
				break;
			case fnevObjectDeleted:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderDeletedNotification);
				break;
			case fnevObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderModifiedNotification_10);
				break;
			case fnevObjectMoved:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderMoveNotification);
				break;
			case fnevObjectCopied:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderCopyNotification);
				break;			
			case fnevSearchComplete:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.SearchCompleteNotification);
				break;
			case fnevTableModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.HierarchyTableChange);
				break;
			case fnevStatusObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.IcsNotification);
				break;
			case fnevTbit|fnevObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderModifiedNotification_1010);
				break;
			case fnevUbit|fnevObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderModifiedNotification_2010);
				break;
			case fnevTbit|fnevUbit|fnevObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.FolderModifiedNotification_3010);
				break;
			case fnevMbit|fnevObjectCreated:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.MessageCreatedNotification);
				break;
			case fnevMbit|fnevObjectDeleted:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.MessageDeletedNotification);
				break;
			case fnevMbit|fnevObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.MessageModifiedNotification);
				break;
			case fnevMbit|fnevObjectMoved:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.MessageMoveNotification);
				break;
			case fnevMbit|fnevObjectCopied:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.MessageCopyNotification);
				break;
			case fnevMbit|fnevTableModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.ContentsTableChange);
				break;
			case fnevMbit|fnevSbit|fnevObjectDeleted:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.SearchMessageRemovedNotification);
				break;
			case fnevMbit|fnevSbit|fnevObjectModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.SearchMessageModifiedNotification);
				break;
			case fnevMbit|fnevSbit|fnevTableModified:
				NotificationData = (void *)&(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationData.SearchTableChange);
				break;			
			default:
				NotificationData = NULL;
				break;
			}
			notification = notify_ctx->notifications;

			while (notification->ulConnection) {
				if ((notification->NotificationFlags & mapi_response->mapi_repl[i].u.mapi_Notify.NotificationType) && 
				    (handle == mapi_object_get_handle(&(notification->obj_notif)))) {
					if (notification->callback && NotificationData) {
						notification->callback(mapi_response->mapi_repl[i].u.mapi_Notify.NotificationType,
								       (void *)NotificationData,
								       notification->private_data);
					}
				}
				notification = notification->next;
			}
		}
	}
	return MAPI_E_SUCCESS;
}

/**
   \details Force notification of pending events

   This function force the server to send any pending notificaion and
   process them. These MAPI notifications are next compared to the
   registered ones and the callback specified in Subscribe() called if
   it matches.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.  

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa RegisterNotification, Subscribe, Unsubscribe, GetLastError
*/
_PUBLIC_ enum MAPISTATUS DispatchNotifications(struct mapi_session *session)
{
	struct mapi_response	*mapi_response;
	enum MAPISTATUS		retval;
	NTSTATUS		status;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session->notify_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	status = emsmdb_transaction_null((struct emsmdb_context *)session->emsmdb->ctx, &mapi_response);
	if (!NT_STATUS_IS_OK(status))
		return MAPI_E_CALL_FAILED;

	retval = ProcessNotification(session->notify_ctx, mapi_response);
	talloc_free(mapi_response);
	return retval;
}


/**
   \details Wait for notifications and process them

   This function waits for notifications on the UDP port
   and generates the traffic needed to receive MAPI
   notifications. These MAPI notifications are next compared to the
   registered ones and the callback specified in Subscribe() called if
   it matches.

   The function takes a callback in cb_data to check if it should 
   continue to process notifications. Timeval in cb_data can be
   used to control the behavior of select.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.  

   \note Developers may also call GetLastError() to retrieve the last
   MAPI error code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction

   \sa RegisterNotification, Subscribe, Unsubscribe, GetLastError

   \note This code is experimental. The current implementation is
   non-threaded, only supports fnevNewmail and fnevCreatedObject
   notifications and will block your process until you send a signal.
*/
_PUBLIC_ enum MAPISTATUS MonitorNotification(struct mapi_session *session, void *private_data, 
					     struct mapi_notify_continue_callback_data *cb_data)
{
	struct mapi_response	*mapi_response;
	struct mapi_notify_ctx	*notify_ctx;
	NTSTATUS		status;
	int			is_done;
	int			err;
	char			buf[512];
	fd_set                  read_fds;
	int                     nread;
        mapi_notify_continue_callback_t callback;
	void                    *data;
	struct timeval          *tv;
	struct timeval          tvi;
	enum MAPISTATUS		retval;

	/* sanity checks */
	OPENCHANGE_RETVAL_IF(!session, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!session->notify_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	notify_ctx = session->notify_ctx;
	callback = cb_data ? cb_data->callback : NULL;
	data = cb_data ? cb_data->data : NULL;
	tv = cb_data ? &tvi : NULL;

	nread = 0;
	is_done = 0;
	while (!is_done) {
		FD_ZERO(&read_fds);
		FD_SET(notify_ctx->fd, &read_fds);
		if( cb_data ) tvi = cb_data->tv;

		err = select(notify_ctx->fd + 1, &read_fds, NULL, NULL, tv);
		if (FD_ISSET(notify_ctx->fd, &read_fds)) {
		        do {
			         nread = read(notify_ctx->fd, buf, sizeof(buf));
				 if (nread > 0) {
			                status = emsmdb_transaction_null((struct emsmdb_context *)session->emsmdb->ctx,
									 &mapi_response);
					if (!NT_STATUS_IS_OK(status))
					         err = -1;
					else {
					         retval = ProcessNotification(notify_ctx, mapi_response);
						 OPENCHANGE_RETVAL_IF(retval, retval, NULL);
					}
				 }
			} while (nread > 0 && err != -1);
		}
		if ((callback != NULL && callback (data)) || err < 0)
		        is_done = 1;
	}

	return MAPI_E_SUCCESS;
}
