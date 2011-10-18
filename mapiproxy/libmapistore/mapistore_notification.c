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
#include <dlinklist.h>

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.h"

static int mapistore_subscription_destructor(void *data)
{
	struct mapistore_subscription	*subscription = (struct mapistore_subscription *) data;

	if (!data) return -1;

	if (subscription->mqueue != -1) {
		if (mq_unlink(subscription->mqueue_name) == -1) {
			perror("mq_unlink");
		}
		DEBUG(0, ("%s unlinked\n", subscription->mqueue_name));
		talloc_free(subscription->mqueue_name);
	}
	return 0;
}

struct mapistore_subscription *mapistore_new_subscription(TALLOC_CTX *mem_ctx, 
							  const char *username,
							  uint32_t handle,
                                                          uint16_t notification_types,
                                                          void *notification_parameters)
{
	int						ret;
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
        }

	/* NewMail POC: open newmail mail queue */
	if (notification_types == fnevNewMail) {
		new_subscription->mqueue_name = talloc_asprintf((TALLOC_CTX *)new_subscription, 
								MAPISTORE_MQUEUE_NEWMAIL_FMT, username);
		new_subscription->mqueue = mq_open(new_subscription->mqueue_name, 
						   O_RDONLY|O_NONBLOCK|O_CREAT, 0755, NULL);
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
	}

        return new_subscription;
}

_PUBLIC_ void mapistore_push_notification(struct mapistore_context *mstore_ctx, uint8_t object_type, enum mapistore_notification_type event, void *parameters)
{
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
}

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

	/* HACK: we only support NewMail notifications for now */
	nl->notification->object_type = MAPISTORE_MESSAGE;
	nl->notification->event = MAPISTORE_OBJECT_NEWMAIL;
	nl->notification->parameters.object_parameters.folder_id = command.command.notification.FolderID;
	nl->notification->parameters.object_parameters.object_id = command.command.notification.MessageID;

	return nl;
}

_PUBLIC_ enum MAPISTATUS mapistore_get_queued_notifications(struct mapistore_context *mstore_ctx,
							    struct mapistore_subscription *s,
							    struct mapistore_notification_list **nl)
{
	int					ret;
	struct mapistore_notification_list	*nlist = NULL;
	struct mapistore_notification_list	*el = NULL;
	unsigned int				prio;
	struct mq_attr				attr;
	DATA_BLOB				data;
	bool					found = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!s, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!nl, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	MAPISTORE_RETVAL_IF(s->mqueue == -1, MAPISTORE_ERR_NOT_FOUND, NULL);

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
			talloc_zero((TALLOC_CTX *)mstore_ctx, struct mapistore_notification_list);
		}
		el = mapistore_notification_process_mqueue_notif((TALLOC_CTX *)mstore_ctx, data);
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

	return (found == false) ? MAPISTORE_ERR_NOT_FOUND : MAPISTORE_SUCCESS;
}

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

_PUBLIC_ int mapistore_delete_subscription(struct mapistore_context *mstore_ctx, uint32_t identifier, 
					   uint16_t NotificationFlags)
{
	struct mapistore_subscription_list *el;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstore_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	for (el = mstore_ctx->subscriptions; el; el = el->next) {
		if ((el->subscription->handle == identifier) &&
		    (el->subscription->notification_types == NotificationFlags)) {
			DLIST_REMOVE(mstore_ctx->subscriptions, el);
			talloc_free(el);
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}

_PUBLIC_ struct mapistore_subscription_list *mapistore_find_matching_subscriptions(struct mapistore_context *mstore_ctx, struct mapistore_notification *notification)
{
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
}
