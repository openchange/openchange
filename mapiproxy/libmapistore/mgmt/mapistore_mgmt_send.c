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

enum mapistore_error mapistore_mgmt_send_udp_notification(struct mapistore_mgmt_context *mgmt_ctx,
					 const char *username)
{
	struct mapistore_mgmt_users	*el;
	ssize_t				len;
	bool				found = false;

	printf("mapistore_mgmt_send_udp_notification\n");
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mgmt_ctx->users, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	for (el = mgmt_ctx->users; el; el = el->next) {
		if (!strcmp(el->info->username, username) && el->notify_ctx) {
			len = send(el->notify_ctx->fd, (const void *)el->notify_ctx->context_data, 
				   el->notify_ctx->context_len, MSG_DONTWAIT);
			if (len == -1) {
				perror("send");
			} else {
				printf("UDP NOTIFICATION SENT, size is: %zu\n", len);
				found = true;
			}
		}
	}

	return (found == true) ? MAPISTORE_SUCCESS : MAPISTORE_ERR_NOT_FOUND;
}


/* /\**  */
/*     \details Send a newmail notification over irpc to the listener */
/*     process */

/*     \param mgmt_ctx pointer to the mapistore management context */
/*     \param username the openchange user to deliver the notification to */
/*     \param FolderID the identifier of the folder which received the newmail */
/*     \param MessageID the identifier of the received message */
/*     \param MAPIStoreURI the URI of the new message */

/*     \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error. */
/*  *\/ */
/* enum mapistore_error mapistore_mgmt_send_newmail_notification(struct mapistore_mgmt_context *mgmt_ctx, */
/* 					     const char *username, */
/* 					     uint64_t FolderID, */
/* 					     uint64_t MessageID, */
/* 					     const char *MAPIStoreURI) */
/* { */
/* 	mqd_t					mqfd; */
/* 	int					ret; */
/* 	TALLOC_CTX				*mem_ctx; */
/* 	DATA_BLOB				data; */
/* 	struct mapistore_mgmt_command		cmd; */
/* 	enum ndr_err_code			ndr_err; */
/* 	char					*queue; */

/* 	/\* Sanity checks *\/ */
/* 	printf("mapistore_mgmt_send_newmail_notification\n"); */
/* 	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL); */
/* 	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL); */
/* 	MAPISTORE_RETVAL_IF(!MAPIStoreURI, MAPISTORE_ERR_INVALID_PARAMETER, NULL); */

/* 	mem_ctx = talloc_new(NULL); */

/* 	/\* Try to open the newmail queue for specified user *\/ */
/* 	queue = talloc_asprintf(mem_ctx, MAPISTORE_MQUEUE_NEWMAIL_FMT, username); */
/* 	mqfd = mq_open(queue, O_WRONLY|O_NONBLOCK); */
/* 	if (mqfd == -1) { */
/* 		ret = mapistore_mgmt_send_udp_notification(mgmt_ctx, username); */
/* 		printf("[%s:%d] mapistore_mgmt_send_udp_notification: %d\n", __FUNCTION__, __LINE__, ret); */
/* 		perror("mq_open"); */
/* 		talloc_free(mem_ctx); */
/* 		return MAPISTORE_ERR_NOT_FOUND; */
/* 	} */

/* 	/\* Prepare DATA_BLOB notification *\/ */
/* 	cmd.type = MAPISTORE_MGMT_NOTIF; */
/* 	cmd.command.notification.status = MAPISTORE_MGMT_SEND; */
/* 	cmd.command.notification.NotificationFlags = mgmt_notification_type_newmail; */
/* 	cmd.command.notification.username = username; */
/* 	cmd.command.notification.WholeStore = 0; */
/* 	cmd.command.notification.FolderID = FolderID; */
/* 	cmd.command.notification.MessageID = MessageID; */
/* 	cmd.command.notification.MAPIStoreURI = MAPIStoreURI; */

/* 	ndr_err = ndr_push_struct_blob(&data, mem_ctx, &cmd, (ndr_push_flags_fn_t)ndr_push_mapistore_mgmt_command); */
/* 	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) { */
/* 		OC_DEBUG(0, ("! [%s:%d][%s]: Failed to push mapistore_mgmt_command into NDR blob\n", */
/* 			  __FUNCTION__, __LINE__, __FUNCTION__)); */
/* 		mq_close(mqfd); */
/* 		talloc_free(mem_ctx); */
/* 		return MAPISTORE_ERR_INVALID_DATA; */
/* 	} */

/* 	/\* Send the message *\/ */
/* 	ret = mq_send(mqfd, (const char *)data.data, data.length, 0); */
/* 	if (ret == -1) { */
/* 		perror("mq_send"); */
/* 		talloc_free(mem_ctx); */
/* 		mq_close(mqfd); */
/* 		return MAPISTORE_ERR_MSG_SEND; */
/* 	} */

	
/* 	mq_close(mqfd); */
/* 	talloc_free(mem_ctx); */

/* 	/\* Send UDP notification *\/ */
/* 	ret = mapistore_mgmt_send_udp_notification(mgmt_ctx, username); */
/* 	printf("[%s:%d] mapistore_mgmt_send_udp_notification: %d\n", __FUNCTION__, __LINE__, ret); */

/* 	return MAPISTORE_SUCCESS; */
/* } */


static enum mapistore_error mapistore_mgmt_push_send(TALLOC_CTX *mem_ctx, mqd_t mqfd, struct mapistore_mgmt_command cmd)
{
	DATA_BLOB		data;
	enum ndr_err_code	ndr_err;
	int			ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mqfd, MAPISTORE_ERR_NOT_FOUND, NULL);

	/* Prepare DATA_BLOB with data pushed */
	ndr_err = ndr_push_struct_blob(&data, mem_ctx, &cmd, (ndr_push_flags_fn_t)ndr_push_mapistore_mgmt_command);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		OC_DEBUG(0, "Failed to push mapistore_mgmt_command into NDR blob\n");
		mq_close(mqfd);
		return MAPISTORE_ERR_INVALID_DATA;
	}

	/* Send the message */
	ret = mq_send(mqfd, (const char *)data.data, data.length, 0);
	if (ret == -1) {
		printf("Notification Pushed with error: ret = %d\n", ret);
		perror("mq_send");
		mq_close(mqfd);
		return MAPISTORE_ERR_MSG_SEND;
	}

	printf("Notification Pushed successfully: ret = %d\n", ret);
	return MAPISTORE_SUCCESS;
}


/**
   \details Send notifications 
 */
enum mapistore_error mapistore_mgmt_send_newmail_notification(struct mapistore_mgmt_context *mgmt_ctx,
							      const char *username,
							      uint64_t FolderID,
							      uint64_t MessageID,
							      const char *MAPIStoreURI)
{
	mqd_t					mqfd;
	int					ret;
	TALLOC_CTX				*mem_ctx;
	struct mapistore_mgmt_command		cmd;
	char					*queue;

	/* Sanity checks */
	printf("[%s:%d]: mapistore_mgmt_send_newmail_global_notification\n", __FUNCTION__, __LINE__);
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!MAPIStoreURI, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);

	/* Try to open the newmail queue for specified user */
	queue = talloc_asprintf(mem_ctx, MAPISTORE_MQUEUE_NEWMAIL_FMT, username);
	mqfd = mq_open(queue, O_WRONLY|O_NONBLOCK);
	if (mqfd == -1) {
		ret = mapistore_mgmt_send_udp_notification(mgmt_ctx, username);
		perror("mq_open");
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	/* fnevNewmail subscription case */
	ret = mapistore_mgmt_registered_folder_subscription(mgmt_ctx, username, NULL, 
							    mgmt_notification_type_newmail);
	/* if (ret == MAPISTORE_SUCCESS) { */
	/* 	memset(&cmd, 0x0, sizeof (struct mapistore_mgmt_command)); */
	/* 	cmd.type = MAPISTORE_MGMT_NOTIF; */
	/* 	cmd.command.notification.status = MAPISTORE_MGMT_SEND; */
	/* 	cmd.command.notification.NotificationFlags = mgmt_notification_type_newmail | mgmt_notification_type_Mbit; */
	/* 	cmd.command.notification.username = username; */
	/* 	cmd.command.notification.WholeStore = 0; */
	/* 	cmd.command.notification.FolderID = FolderID; */
	/* 	cmd.command.notification.MessageID = MessageID; */
	/* 	cmd.command.notification.MAPIStoreURI = MAPIStoreURI; */
	/* 	cmd.command.notification.TotalNumberOfMessages = 0; */
	/* 	cmd.command.notification.UnreadNumberOfMessages = 0; */
	/* 	mapistore_mgmt_push_send(mem_ctx, mqfd, cmd); */
	/* 	printf("NEWMAIL notification sent on %s\n", queue); */
	/* } */

	/* fnevObjectModified subscription case (fntevTbit + fnevUbit) (0x3010) but only 0x10 looked up */
	ret = mapistore_mgmt_registered_folder_subscription(mgmt_ctx, username, NULL,
							    mgmt_notification_type_objectmodified);
	if (ret == MAPISTORE_SUCCESS) {
		memset(&cmd, 0x0, sizeof (struct mapistore_mgmt_command));
		cmd.type = MAPISTORE_MGMT_NOTIF;
		cmd.command.notification.status = MAPISTORE_MGMT_SEND;
		cmd.command.notification.NotificationFlags = mgmt_notification_type_objectmodified | 
			mgmt_notification_type_Tbit | mgmt_notification_type_Ubit;
		cmd.command.notification.WholeStore = 0;
		cmd.command.notification.FolderID = FolderID;
		cmd.command.notification.MessageID = MessageID;
		cmd.command.notification.MAPIStoreURI = NULL;
		/* Calculate the total number of messages and unread messages */
		cmd.command.notification.TotalNumberOfMessages = 4;
		cmd.command.notification.UnreadNumberOfMessages = 1;
		mapistore_mgmt_push_send(mgmt_ctx, mqfd, cmd);
		printf("0x3010 notification sent on %s\n", queue);

	}

	/* fnevObjectCreated subscription case (0x8004) but only 0x4 looked up */
	ret = mapistore_mgmt_registered_folder_subscription(mgmt_ctx, username, NULL,
							    mgmt_notification_type_objectcreated);
	if (ret == MAPISTORE_SUCCESS) {
		memset(&cmd, 0x0, sizeof (struct mapistore_mgmt_command));
		cmd.type = MAPISTORE_MGMT_NOTIF;
		cmd.command.notification.status = MAPISTORE_MGMT_SEND;
		cmd.command.notification.NotificationFlags = mgmt_notification_type_objectcreated | mgmt_notification_type_Mbit;
		cmd.command.notification.WholeStore = 0;
		cmd.command.notification.FolderID = FolderID;
		cmd.command.notification.MessageID = MessageID;
		cmd.command.notification.MAPIStoreURI = MAPIStoreURI;
		cmd.command.notification.TotalNumberOfMessages = 0;
		cmd.command.notification.UnreadNumberOfMessages = 0;
		mapistore_mgmt_push_send(mgmt_ctx, mqfd, cmd);
		printf("0x8004 notification sent on %s\n", queue);
	}

	/* fnevObjectModified (0x10) subscription case */
	ret = mapistore_mgmt_registered_folder_subscription(mgmt_ctx, username, NULL,
							    mgmt_notification_type_objectmodified);
	if (ret == MAPISTORE_SUCCESS) {
		memset(&cmd, 0x0, sizeof (struct mapistore_mgmt_command));
		cmd.type = MAPISTORE_MGMT_NOTIF;
		cmd.command.notification.status = MAPISTORE_MGMT_SEND;
		cmd.command.notification.NotificationFlags = mgmt_notification_type_objectmodified;
		cmd.command.notification.FolderID = FolderID;
		cmd.command.notification.MessageID = MessageID;
		cmd.command.notification.MAPIStoreURI = MAPIStoreURI;
		cmd.command.notification.TotalNumberOfMessages = 0;
		cmd.command.notification.UnreadNumberOfMessages = 0;
	}

	/* Send UDP notification */
	ret = mapistore_mgmt_send_udp_notification(mgmt_ctx, username);
	printf("[%s:%d] mapistore_mgmt_send_udp_notification: %d\n", __FUNCTION__, __LINE__, ret);

	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}
