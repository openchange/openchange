/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011

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
   \file mapistore_mgmt_send.c

   \brief Provides mapistore management routine for sending
   messages/notifications from administrative/services tools to
   OpenChange Server/listener process. Using this interface virtually
   restrict mapistore features to the specific management functions
   subset.
 */

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mgmt/mapistore_mgmt.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.h"

/** 
    \details Send a newmail notification over irpc to the listener
    process

    \param mgmt_ctx pointer to the mapistore management context
    \param username the openchange user to deliver the notification to
    \param FolderID the identifier of the folder which received the newmail
    \param MessageID the identifier of the received message
    \param MAPIStoreURI the URI of the new message

    \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error.
 */
int mapistore_mgmt_send_newmail_notification(struct mapistore_mgmt_context *mgmt_ctx,
					     const char *username,
					     uint64_t FolderID,
					     uint64_t MessageID,
					     const char *MAPIStoreURI)
{
	mqd_t					mqfd;
	int					ret;
	TALLOC_CTX				*mem_ctx;
	DATA_BLOB				data;
	struct mapistore_mgmt_command		cmd;
	enum ndr_err_code			ndr_err;
	char					*queue;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!MAPIStoreURI, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);

	/* Try to open the newmail queue for specified user */
	queue = talloc_asprintf(mem_ctx, MAPISTORE_MQUEUE_NEWMAIL_FMT, username);
	mqfd = mq_open(queue, O_WRONLY|O_NONBLOCK, 0755, NULL);
	if (mqfd == -1) {
		perror("mq_open");
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	/* Prepare DATA_BLOB notification */
	cmd.type = MAPISTORE_MGMT_NOTIF;
	cmd.command.notification.status = MAPISTORE_MGMT_SEND;
	cmd.command.notification.NotificationFlags = mgmt_notification_type_newmail;
	cmd.command.notification.username = username;
	cmd.command.notification.WholeStore = 0;
	cmd.command.notification.FolderID = FolderID;
	cmd.command.notification.MessageID = MessageID;
	cmd.command.notification.MAPIStoreURI = MAPIStoreURI;

	ndr_err = ndr_push_struct_blob(&data, mem_ctx, &cmd, (ndr_push_flags_fn_t)ndr_push_mapistore_mgmt_command);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("! [%s:%d][%s]: Failed to push mapistore_mgmt_command into NDR blob\n",
			  __FUNCTION__, __LINE__, __FUNCTION__));
		mq_close(mqfd);
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_INVALID_DATA;
	}

	/* Send the message */
	ret = mq_send(mqfd, (const char *)data.data, data.length, 0);
	if (ret == -1) {
		perror("mq_send");
		talloc_free(mem_ctx);
		mq_close(mqfd);
		return MAPISTORE_ERR_MSG_SEND;
	}

	
	mq_close(mqfd);
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

