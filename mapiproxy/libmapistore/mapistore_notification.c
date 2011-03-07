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

#include "mapistore.h"

static struct mapistore_context *active_context = NULL;

struct mapistore_subscription *mapistore_new_subscription(TALLOC_CTX *mem_ctx, uint32_t handle,
                                                          uint16_t notification_types,
                                                          void *notification_parameters)
{
        struct mapistore_subscription *new_subscription;
        struct mapistore_table_subscription_parameters *table_parameters;
        struct mapistore_object_subscription_parameters *object_parameters;

        new_subscription = talloc_zero(mem_ctx, struct mapistore_subscription);
        new_subscription->handle = handle;
        new_subscription->notification_types = notification_types;
        if (notification_types == fnevTableModified) {
                table_parameters = notification_parameters;
                new_subscription->parameters.table_parameters = *table_parameters;
        }
        else {
                object_parameters = notification_parameters;
                new_subscription->parameters.object_parameters = *object_parameters;
        }

        return new_subscription;
}

void mapistore_notification_set_context(struct mapistore_context *new_context)
{
        active_context = new_context;
}

void mapistore_push_notification(uint8_t object_type, enum mapistore_notification_type event, void *parameters)
{
        struct mapistore_notification *new_notification;
        struct mapistore_notification_list *new_list;
        struct mapistore_table_notification_parameters *table_parameters;
        struct mapistore_object_notification_parameters *object_parameters;

        if (active_context) {
                new_list = talloc_zero(active_context, struct mapistore_notification_list);
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
                DLIST_ADD_END(active_context->notifications, new_list, void);
        }
        else {
                DEBUG(0, ("cannot push notification when no context is active\n"));
        }
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
                        && notification->event == MAPISTORE_OBJECT_DELETED)) {
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

struct mapistore_subscription_list *mapistore_find_matching_subscriptions(struct mapistore_context *mstore_ctx, struct mapistore_notification *notification)
{
        struct mapistore_subscription_list *matching_subscriptions, *new_element, *current_element;

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
