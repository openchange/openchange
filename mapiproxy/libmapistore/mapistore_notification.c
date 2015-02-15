/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Wolfgang Sourdeau 2011

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

#include <talloc.h>
#include "utils/dlinklist.h"

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mgmt/mapistore_mgmt.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.h"

#if 0
static int mapistore_subscription_destructor(void *data)
{
	struct mapistore_subscription	*subscription = (struct mapistore_subscription *) data;

	DEBUG(0, ("#### DELETING SUBSCRIPTION ###\n"));

	if (!data) return -1;

	DEBUG(0, ("### 1. NotificationFlags = 0x%x\n", subscription->notification_types));
	DEBUG(0, ("### 2. handle = 0x%x\n", subscription->handle));

	if (subscription->mqueue != -1) {
		if (mq_unlink(subscription->mqueue_name) == -1) {
			perror("mq_unlink");
		}
		DEBUG(0, ("%s unlinked\n", subscription->mqueue_name));
		talloc_free(subscription->mqueue_name);
	}
	return 0;
}
#endif

struct mapistore_subscription *mapistore_new_subscription(TALLOC_CTX *mem_ctx, 
							  struct mapistore_context *mstore_ctx,
							  const char *username,
							  uint32_t handle,
                                                          uint16_t notification_types,
                                                          void *notification_parameters)
{
#if 0
	int						ret;
	struct mapistore_connection_info		c;
	struct mapistore_mgmt_notif			n;
        struct mapistore_subscription			*new_subscription;
        struct mapistore_table_subscription_parameters	*table_parameters;
        struct mapistore_object_subscription_parameters *object_parameters;
	unsigned int					prio;
	struct mq_attr					attr;
	DATA_BLOB					data;

        new_subscription = talloc_zero(mem_ctx, struct mapistore_subscription);
        new_subscription->handle = handle;
        new_subscription->notification_types = notification_types;
	new_subscription->mqueue = -1;
	new_subscription->mqueue_name = NULL;
        if (notification_types == fnevTableModified) {
                table_parameters = notification_parameters;
                new_subscription->parameters.table_parameters = *table_parameters;
        }
        else {
                object_parameters = notification_parameters;
                new_subscription->parameters.object_parameters = *object_parameters;

		/* NewMail POC: open newmail mail queue */
		if (notification_types & fnevNewMail || notification_types & fnevObjectCreated) {
			new_subscription->mqueue_name = talloc_asprintf((TALLOC_CTX *)new_subscription, 
									MAPISTORE_MQUEUE_NEWMAIL_FMT, username);
			new_subscription->mqueue = mq_open(new_subscription->mqueue_name, 
							   O_RDONLY|O_NONBLOCK|O_CREAT, 0777, NULL);
			if (new_subscription->mqueue == -1) {
				perror("mq_open");
				talloc_free(new_subscription->mqueue_name);
				return new_subscription;
			}
			
			/* Empty queue since we only want to retrieve new data from now */
			ret = mq_getattr(new_subscription->mqueue, &attr);
			if (ret == -1) {
				perror("mq_getattr");
			} else {
				data.data = talloc_size(mem_ctx, attr.mq_msgsize);
				while ((data.length = mq_receive(new_subscription->mqueue, (char *)data.data,
								 attr.mq_msgsize, &prio)) != -1) {
					dump_data(0, data.data, data.length);
					talloc_free(data.data);
					data.data = talloc_size(mem_ctx, attr.mq_msgsize);
				}
				talloc_free(data.data);
			}
			
			/* Set destructor on new_subscription as we want to unlink the queue upon release */
			talloc_set_destructor((void *)new_subscription, (int (*)(void *))mapistore_subscription_destructor);
			
			/* Send notification to tell about newmail */
			n.WholeStore = object_parameters->whole_store;
			n.NotificationFlags = notification_types;
			if (n.WholeStore == true) {
				n.FolderID = 0;
				n.MessageID = 0;
				n.MAPIStoreURI = NULL;
			} else {
				n.FolderID = object_parameters->folder_id;
				n.MessageID = object_parameters->object_id;
				/* FIXME */
				n.MAPIStoreURI = NULL;
			}

			c.username = (char *)username;
			c.mstore_ctx = mstore_ctx;
			
			ret = mapistore_mgmt_interface_register_subscription(&c, &n);
			DEBUG(0, ("[%s:%d]: registering notification: %d\n", __FUNCTION__, __LINE__, ret));
		}
	}

        return new_subscription;
#endif

	return NULL;
}

_PUBLIC_ void mapistore_push_notification(struct mapistore_context *mstore_ctx, uint8_t object_type, enum mapistore_notification_type event, void *parameters)
{
#if 0
        struct mapistore_notification *new_notification;
        struct mapistore_notification_list *new_list;
        struct mapistore_table_notification_parameters *table_parameters;
        struct mapistore_object_notification_parameters *object_parameters;

        if (!mstore_ctx) return;

	new_list = talloc_zero(mstore_ctx, struct mapistore_notification_list);
	new_notification = talloc_zero(new_list, struct mapistore_notification);
	new_list->notification = new_notification;
	new_notification->object_type = object_type;
	new_notification->event = event;
	if (object_type == MAPISTORE_TABLE) {
		table_parameters = parameters;
		new_notification->parameters.table_parameters = *table_parameters;
	}
	else {
		object_parameters = parameters;
		new_notification->parameters.object_parameters = *object_parameters;
		if (new_notification->parameters.object_parameters.tag_count > 0
		    && new_notification->parameters.object_parameters.tag_count != 0xffff) {
			new_notification->parameters.object_parameters.tags
				= talloc_memdup(new_notification, new_notification->parameters.object_parameters.tags,
						sizeof(enum MAPITAGS) * new_notification->parameters.object_parameters.tag_count);
		}
	}
	DLIST_ADD_END(mstore_ctx->notifications, new_list, void);
#endif
}

#if 0
static struct mapistore_notification_list *mapistore_notification_process_mqueue_notif(TALLOC_CTX *mem_ctx, 
										       DATA_BLOB data)
										       
{
	struct mapistore_notification_list	*nl;
	struct mapistore_mgmt_command		command;
	struct ndr_pull				*ndr_pull = NULL;

	ndr_pull = ndr_pull_init_blob(&data, mem_ctx);
	ndr_pull_mapistore_mgmt_command(ndr_pull, NDR_SCALARS|NDR_BUFFERS, &command);

	/* verbose */
	{
		struct ndr_print	*ndr_print;
		
		ndr_print = talloc_zero(mem_ctx, struct ndr_print);
		ndr_print->print = ndr_print_printf_helper;
		ndr_print->depth = 1;
		ndr_print_mapistore_mgmt_command(ndr_print, "command", &command);
		talloc_free(ndr_print);
	}

	if (command.type != MAPISTORE_MGMT_NOTIF) {
		DEBUG(0, ("[%s:%d]: Invalid command type received: 0x%x\n",
			  __FUNCTION__, __LINE__, command.type));
		return NULL;
	}

	if (command.command.notification.status != MAPISTORE_MGMT_SEND) {
		DEBUG(0, ("[%s:%d]: Invalid notification status: 0x%x\n",
			  __FUNCTION__, __LINE__, command.command.notification.status));
		return NULL;
	}

	nl = talloc_zero(mem_ctx, struct mapistore_notification_list);
	nl->notification = talloc_zero((TALLOC_CTX *)nl, struct mapistore_notification);

	/* On newmail notification received, trigger 3 notifications:
	   1. FolderModifiedNotification (0x3010)
	   2. MessageObjectCreated (0x8004)
	   3. FolderModification (0x10)
	 */

	switch (command.command.notification.NotificationFlags) {
	case 0x8004:
		nl->notification->object_type = MAPISTORE_MESSAGE;
		nl->notification->event = MAPISTORE_OBJECT_CREATED;
		nl->notification->parameters.object_parameters.folder_id = command.command.notification.FolderID;
		nl->notification->parameters.object_parameters.object_id = command.command.notification.MessageID;
		nl->notification->parameters.object_parameters.tag_count = 24;
		nl->notification->parameters.object_parameters.tags = talloc_array(nl->notification, enum MAPITAGS, nl->notification->parameters.object_parameters.tag_count);
		nl->notification->parameters.object_parameters.tags[0] = PR_RCVD_REPRESENTING_ENTRYID;
		nl->notification->parameters.object_parameters.tags[1] = PR_RCVD_REPRESENTING_ADDRTYPE;
		nl->notification->parameters.object_parameters.tags[2] = PR_RCVD_REPRESENTING_EMAIL_ADDRESS;
		nl->notification->parameters.object_parameters.tags[3] = PR_RCVD_REPRESENTING_NAME;
		nl->notification->parameters.object_parameters.tags[4] = PR_RCVD_REPRESENTING_SEARCH_KEY;
		nl->notification->parameters.object_parameters.tags[5] = PidTagReceivedRepresentingFlags;
		nl->notification->parameters.object_parameters.tags[6] = 0x67BA0102;
		nl->notification->parameters.object_parameters.tags[7] = PR_CONTENT_FILTER_SCL;
		nl->notification->parameters.object_parameters.tags[8] = PR_LAST_MODIFICATION_TIME;
		nl->notification->parameters.object_parameters.tags[9] = PR_LAST_MODIFIER_ENTRYID;
		nl->notification->parameters.object_parameters.tags[10] = PR_LAST_MODIFIER_NAME;
		nl->notification->parameters.object_parameters.tags[11] = PR_MODIFIER_FLAGS;
		nl->notification->parameters.object_parameters.tags[12] = 0x67BE0102;
		nl->notification->parameters.object_parameters.tags[13] = PR_LOCAL_COMMIT_TIME;
		nl->notification->parameters.object_parameters.tags[14] = PR_RECEIVED_BY_SEARCH_KEY;
		nl->notification->parameters.object_parameters.tags[15] = PR_RECEIVED_BY_ENTRYID;
		nl->notification->parameters.object_parameters.tags[16] = PR_RECEIVED_BY_ADDRTYPE;
		nl->notification->parameters.object_parameters.tags[17] = PR_RECEIVED_BY_EMAIL_ADDRESS;
		nl->notification->parameters.object_parameters.tags[18] = PR_RECEIVED_BY_NAME;
		nl->notification->parameters.object_parameters.tags[19] = PidTagReceivedByFlags;
		nl->notification->parameters.object_parameters.tags[20] = 0x67B90102;
		nl->notification->parameters.object_parameters.tags[21] = PR_MESSAGE_FLAGS;
		nl->notification->parameters.object_parameters.tags[22] = PR_MESSAGE_SIZE;
		nl->notification->parameters.object_parameters.tags[23] = PR_INTERNET_ARTICLE_NUMBER;
		break;
	case 0x3010:
		nl->notification->object_type = MAPISTORE_FOLDER;
		nl->notification->event = MAPISTORE_OBJECT_MODIFIED;
		nl->notification->parameters.object_parameters.folder_id = command.command.notification.FolderID;
		nl->notification->parameters.object_parameters.tag_count = 0x5;
		nl->notification->parameters.object_parameters.tags = talloc_array(nl->notification, enum MAPITAGS, nl->notification->parameters.object_parameters.tag_count);
		nl->notification->parameters.object_parameters.tags[0] = PR_CONTENT_COUNT;
		nl->notification->parameters.object_parameters.tags[1] = PR_CONTENT_UNREAD;
		nl->notification->parameters.object_parameters.tags[2] = PR_MESSAGE_SIZE;
		nl->notification->parameters.object_parameters.tags[3] = PR_RECIPIENT_ON_NORMAL_MSG_COUNT;
		nl->notification->parameters.object_parameters.tags[4] = PR_NORMAL_MESSAGE_SIZE;
		nl->notification->parameters.object_parameters.message_count = command.command.notification.TotalNumberOfMessages;
		break;
	default:
		DEBUG(3, ("Unsupported Notification Type: 0x%x\n", command.command.notification.NotificationFlags));
		break;

		/* TODO: Finir de faire les notifications
		  FIX dcesrv_exchange_emsmdb.c: fill_notification 
		  Faire le test allelouia */
	}
	  

	/* HACK: we only support NewMail notifications for now */
	return nl;
}
#endif

/**
   \details Return the list of pending mapistore notifications
   available on the queue name specified in argument.

   \param mstore_ctx pointer to the mapistore context
   \param mqueue_name the name of the queue to open
   \param nl pointer on pointer to the list of mapistore notifications to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error.
 */
_PUBLIC_ enum mapistore_error mapistore_get_queued_notifications_named(struct mapistore_context *mstore_ctx,
								       const char *mqueue_name,
								       struct mapistore_notification_list **nl)
{
	bool					found = false;
#if 0
	int					ret;
	mqd_t					mqueue;
	struct mapistore_notification_list	*nlist = NULL;
	struct mapistore_notification_list	*el = NULL;
	unsigned int				prio;
	struct mq_attr				attr;
	DATA_BLOB				data;

	printf("[%s:%d]: queue name = %s\n", __FUNCTION__, __LINE__, ((mqueue_name) ? mqueue_name : NULL));
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!nl, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mqueue = mq_open(mqueue_name, O_RDONLY|O_NONBLOCK|O_CREAT, 0777, NULL);
	if (mqueue == -1) {
		perror("mq_open");
		return MAPISTORE_ERR_NOT_INITIALIZED;
	}

	/* Retrieve queue attributes */
	ret = mq_getattr(mqueue, &attr);
	if (ret == -1) {
		perror("mq_getattr");
		/* set proper error message here and remove above */
		if (mq_close(mqueue) == -1) {
			perror("mq_close");
		}
		MAPISTORE_RETVAL_IF(ret == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
	}

	data.data = talloc_size((TALLOC_CTX *)mstore_ctx, attr.mq_msgsize);
	while ((data.length = mq_receive(mqueue, (char *)data.data, attr.mq_msgsize, &prio)) != -1) {
		printf("* we received a notification on queue %s\n", mqueue_name);
		if (!nlist) {
			nlist = talloc_zero((TALLOC_CTX *)mstore_ctx, struct mapistore_notification_list);
		}
		el = mapistore_notification_process_mqueue_notif((TALLOC_CTX *)nlist, data);
		printf("* processing notification returned %p\n", el);
		if (el) {
			DLIST_ADD_END(nlist, el, struct mapistore_notification_list);
		}
		talloc_free(data.data);
		found = true;
		data.data = talloc_size((TALLOC_CTX *)mstore_ctx, attr.mq_msgsize);
	}
	talloc_free(data.data);

	if (found == true) {
		*nl = nlist;
	}

	if (mq_close(mqueue) == -1) {
		perror("mq_close");
	}
#endif

	return (found == false) ? MAPISTORE_ERR_NOT_FOUND : MAPISTORE_SUCCESS;
}

/**
   \details Return the list of pending mapistore notifications within
   the queue pointed by the mapistore subscription structure.

   \param mstore_ctx pointer to the mapistore context
   \param s pointer to the mapistore subscription where the mqueue file descriptor is stored
   \param nl pointer on pointer to the list of mapistore noficiations to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_get_queued_notifications(struct mapistore_context *mstore_ctx,
								 struct mapistore_subscription *s,
								 struct mapistore_notification_list **nl)
{
	bool					found = false;
#if 0
	int					ret;
	struct mapistore_notification_list	*nlist = NULL;
	struct mapistore_notification_list	*el = NULL;
	unsigned int				prio;
	struct mq_attr				attr;
	DATA_BLOB				data;

	DEBUG(0, ("mapistore_get_queued_notifications: queue name = %s\n", s->mqueue_name));
	DEBUG(0, ("mapistore_get_queued_notifications: before sanity checks\n"));
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!s, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!nl, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	MAPISTORE_RETVAL_IF(s->mqueue == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
	DEBUG(0, ("mapistore_get_queued_notifications: after sanity checks\n"));

	/* Retrieve queue attributes */
	ret = mq_getattr(s->mqueue, &attr);
	if (ret == -1) {
		perror("mq_getattr");
		/* set proper error message here and remove above */
		MAPISTORE_RETVAL_IF(ret == -1, MAPISTORE_ERR_NOT_FOUND, NULL);
	}

	data.data = talloc_size((TALLOC_CTX *)mstore_ctx, attr.mq_msgsize);
	while ((data.length = mq_receive(s->mqueue, (char *)data.data, attr.mq_msgsize, &prio)) != -1) {
		if (!nlist) {
			nlist = talloc_zero((TALLOC_CTX *)mstore_ctx, struct mapistore_notification_list);
		}
		el = mapistore_notification_process_mqueue_notif((TALLOC_CTX *)nlist, data);
		if (el) {
			DLIST_ADD_END(nlist, el, struct mapistore_notification_list);
		}
		talloc_free(data.data);
		found = true;
		data.data = talloc_size((TALLOC_CTX *)mstore_ctx, attr.mq_msgsize);
	}
	talloc_free(data.data);

	if (found == true) {
		*nl = nlist;
	}

	DEBUG(0, ("mapistore_get_queued_notification: found = %s\n", 
		  (found == false) ? "MAPISTORE_ERR_NOT_FOUND" : "MAPISTORE_SUCCESS"));
#endif
	return (found == false) ? MAPISTORE_ERR_NOT_FOUND : MAPISTORE_SUCCESS;
}

#if 0
static bool notification_matches_subscription(struct mapistore_notification *notification, struct mapistore_subscription *subscription)
{
        bool result;
        struct mapistore_table_notification_parameters *n_table_parameters;
        struct mapistore_table_subscription_parameters *s_table_parameters;
        struct mapistore_object_notification_parameters *n_object_parameters;
        struct mapistore_object_subscription_parameters *s_object_parameters;

        result = false;

        if (notification->object_type == MAPISTORE_TABLE) {
                if (subscription->notification_types & fnevTableModified) {
                        n_table_parameters = &notification->parameters.table_parameters;
                        if (subscription->handle == n_table_parameters->handle) {
                                s_table_parameters = &subscription->parameters.table_parameters;
                                result = (n_table_parameters->table_type == s_table_parameters->table_type
                                          && n_table_parameters->folder_id == s_table_parameters->folder_id);
                        }
                }
        }
        else {
                if (((subscription->notification_types & fnevObjectCreated)
                     && notification->event == MAPISTORE_OBJECT_CREATED)
                    || ((subscription->notification_types & fnevObjectModified)
                        && notification->event == MAPISTORE_OBJECT_MODIFIED)
                    || ((subscription->notification_types & fnevObjectDeleted)
                        && notification->event == MAPISTORE_OBJECT_DELETED)
		    || ((subscription->notification_types & fnevObjectCopied)
                        && notification->event == MAPISTORE_OBJECT_COPIED)
		    || ((subscription->notification_types & fnevObjectMoved)
                        && notification->event == MAPISTORE_OBJECT_MOVED)) {
                        n_object_parameters = &notification->parameters.object_parameters;
                        s_object_parameters = &subscription->parameters.object_parameters;
                        if (s_object_parameters->whole_store)
                                result = true;
                        else {
                                if (notification->object_type == MAPISTORE_FOLDER) {
                                        result = (n_object_parameters->object_id == s_object_parameters->folder_id);
                                }
                                else if (notification->object_type == MAPISTORE_MESSAGE) {
                                        result = (n_object_parameters->folder_id == s_object_parameters->folder_id
                                                  && (s_object_parameters->object_id == 0
                                                      || n_object_parameters->object_id == s_object_parameters->object_id));
                                }
                                else {
                                        DEBUG(5, ("[%s] warning: considering notification for unhandled object: %d...\n",
                                                  __PRETTY_FUNCTION__, notification->object_type));
                                }
                        }
                }
        }

        return result;
}
#endif

_PUBLIC_ enum mapistore_error mapistore_delete_subscription(struct mapistore_context *mstore_ctx, uint32_t identifier, 
							    uint16_t NotificationFlags)
{
	struct mapistore_subscription_list *el;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	for (el = mstore_ctx->subscriptions; el; el = el->next) {
		if ((el->subscription->handle == identifier) &&
		    (el->subscription->notification_types == NotificationFlags)) {
			OC_DEBUG(0, "*** DELETING SUBSCRIPTION ***");
			OC_DEBUG(0, "subscription: handle = 0x%x", el->subscription->handle);
			OC_DEBUG(0, "subscription: types = 0x%x", el->subscription->notification_types);
#if 0
			DEBUG(0, ("subscription: mqueue = %d\n", el->subscription->mqueue));
			DEBUG(0, ("subscription: mqueue name = %s\n", el->subscription->mqueue_name));
#endif
			DLIST_REMOVE(mstore_ctx->subscriptions, el);
			talloc_free(el);
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}

_PUBLIC_ struct mapistore_subscription_list *mapistore_find_matching_subscriptions(struct mapistore_context *mstore_ctx, struct mapistore_notification *notification)
{
#if 0
        struct mapistore_subscription_list *matching_subscriptions, *new_element, *current_element;

	if (!mstore_ctx) return NULL;

        matching_subscriptions = NULL;

        current_element = mstore_ctx->subscriptions;
        while (current_element) {
                if (notification_matches_subscription(notification, current_element->subscription)) {
                        new_element = talloc_memdup(mstore_ctx, current_element, sizeof(struct mapistore_subscription_list));
                        DLIST_ADD_END(matching_subscriptions, new_element, void);
                }
                current_element = current_element->next;
        }
        
        return matching_subscriptions;
#endif
	return NULL;
}
