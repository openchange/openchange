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
   \file mapistore_mgmt_messages.c

   \brief Process IPC messages received on command message queue.
 */

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mgmt/mapistore_mgmt.h"
#include "mapiproxy/libmapistore/mgmt/gen_ndr/ndr_mapistore_mgmt.h"

static int mapistore_mgmt_message_user_command_add(struct mapistore_mgmt_context *mgmt_ctx,
						   struct mapistore_mgmt_user_cmd user_cmd)
{
	struct mapistore_mgmt_users	*el;

	el = talloc_zero((TALLOC_CTX *)mgmt_ctx, struct mapistore_mgmt_users);
	if (!el) {
		DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
		return MAPISTORE_ERR_NO_MEMORY;
	}
	el->info = talloc_zero((TALLOC_CTX *)el, struct mapistore_mgmt_user_cmd);
	if (!el->info) {
		talloc_free(el);
		DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
		return MAPISTORE_ERR_NO_MEMORY;
	}
	el->info->backend = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.backend);
	el->info->username = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.username);
	el->info->vuser = talloc_strdup((TALLOC_CTX *)el->info, user_cmd.vuser);
	el->ref_count = 1;
	DLIST_ADD_END(mgmt_ctx->users, el, struct mapistore_mgmt_users);

	return MAPISTORE_SUCCESS;
}

int mapistore_mgmt_message_user_command(struct mapistore_mgmt_context *mgmt_ctx,
					struct mapistore_mgmt_user_cmd user_cmd)
{
	struct mapistore_mgmt_users	*el;
	bool				found = false;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(user_cmd.backend == NULL || user_cmd.username == NULL || 
			    user_cmd.vuser == NULL, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mgmt_ctx->users == NULL) {
		if (user_cmd.status == MAPISTORE_MGMT_REGISTER) {
			return mapistore_mgmt_message_user_command_add(mgmt_ctx, user_cmd);
		} else {
			DEBUG(0, ("[%s:%d]: Trying to unregister user %s in empty list\n", 
				  __FUNCTION__, __LINE__, user_cmd.username));
			return MAPISTORE_SUCCESS;
		}
	} else {
		/* Search users list and perform action */
		for (el = mgmt_ctx->users; el; el = el->next) {
			/* Case where the record exists */
			if ((!strcmp(el->info->backend, user_cmd.backend)) &&
			    (!strcmp(el->info->username, user_cmd.username)) &&
			    (!strcmp(el->info->vuser, user_cmd.vuser))) {
				found = true;
				switch (user_cmd.status) {
				case MAPISTORE_MGMT_REGISTER:
					el->ref_count += 1;
					break;
				case MAPISTORE_MGMT_UNREGISTER:
					el->ref_count -= 1;
					/* Delete record if ref_count is 0 */
					if (el->ref_count == 0) {
						DLIST_REMOVE(mgmt_ctx->users, el);
						talloc_free(el);
						break;
					}
					break;
				default:
					DEBUG(0, ("[%s:%d]: Invalid user command status: %d\n",
						  __FUNCTION__, __LINE__, user_cmd.status));
					break;
				}
			}
		}
		/* Case where no matching record was found: insert */
		if (found == false) {
			switch (user_cmd.status) {
			case MAPISTORE_MGMT_REGISTER:
				return mapistore_mgmt_message_user_command_add(mgmt_ctx, user_cmd);
				break;
			case MAPISTORE_MGMT_UNREGISTER:
				DEBUG(0, ("[%s:%d]: Trying to unregister non-existing users %s\n",
					  __FUNCTION__, __LINE__, user_cmd.username));
				break;
			default:
				DEBUG(0, ("[%s:%d]: Invalid user command status: %d\n",
					  __FUNCTION__, __LINE__, user_cmd.status));
				break;
			}
		}
	}

	return MAPISTORE_SUCCESS;
}

static int mapistore_mgmt_message_notification_command_add(struct mapistore_mgmt_users *user_cmd,
							   struct mapistore_mgmt_notification_cmd notif)
{
	struct mapistore_mgmt_notif	*el;

	el = talloc_zero((TALLOC_CTX *)user_cmd, struct mapistore_mgmt_notif);
	if (!el) {
		DEBUG(0, ("[%s:%d]: Not enough memory\n", __FUNCTION__, __LINE__));
		return MAPISTORE_ERR_NO_MEMORY;
	}
	el->WholeStore = notif.WholeStore;
	el->NotificationFlags = notif.NotificationFlags;
	el->ref_count = 1;
	if (el->WholeStore == false) {
		el->MAPIStoreURI = talloc_strdup((TALLOC_CTX *)el, notif.MAPIStoreURI);
		el->FolderID = notif.FolderID;
		el->MessageID = notif.MessageID;
	}
	DLIST_ADD_END(user_cmd->notifications, el, struct mapistore_mgmt_notif);

	return MAPISTORE_SUCCESS;
}

static bool mapistore_mgmt_message_notification_wholestore(struct mapistore_mgmt_users *user_cmd,
							   struct mapistore_mgmt_notification_cmd notif)
{
	struct mapistore_mgmt_notif	*el;
	bool				found = false;

	if (notif.WholeStore == false) return false;

	switch (notif.status) {
	case MAPISTORE_MGMT_REGISTER:
		for (el = user_cmd->notifications; el; el = el->next) {
			if ((el->WholeStore == true) && 
			    (el->NotificationFlags == notif.NotificationFlags)) {
				found = true;
				el->ref_count += 1;
				break;
			}
		}
		if (found == false) {
			mapistore_mgmt_message_notification_command_add(user_cmd, notif);
		}
		break;
	case MAPISTORE_MGMT_UNREGISTER:
		for (el = user_cmd->notifications; el; el = el->next) {
			if ((el->WholeStore == true) &&
			    (el->NotificationFlags == notif.NotificationFlags)) {
				el->ref_count -= 1;
				if (!el->ref_count) {
					DEBUG(0, ("[%s:%d]: Deleting WholeStore subscription\n", 
						  __FUNCTION__, __LINE__));
					DLIST_REMOVE(user_cmd->notifications, el);
					talloc_free(el);
					return true;
				}
			}
		}
		DEBUG(0, ("[%s:%d]: Unregistered subscription found\n", __FUNCTION__, __LINE__));
		break;
	}

	return true;
}

static bool mapistore_mgmt_message_notification_message(struct mapistore_mgmt_users *user_cmd,
							struct mapistore_mgmt_notification_cmd notif)
{
	struct mapistore_mgmt_notif	*el;
	bool				found = true;

	if (!notif.MessageID) return false;

	switch (notif.status) {
	case MAPISTORE_MGMT_REGISTER:
		for (el = user_cmd->notifications; el; el = el->next) {
			if ((el->MessageID == notif.MessageID) &&
			    (el->NotificationFlags == notif.NotificationFlags)) {
				found = true;
				el->ref_count += 1;
				break;
			}
		}
		if (found == false) {
			mapistore_mgmt_message_notification_command_add(user_cmd, notif);
		}
		break;
	case MAPISTORE_MGMT_UNREGISTER:
		for (el = user_cmd->notifications; el; el = el->next) {
			if ((el->MessageID == notif.MessageID) &&
			    (el->NotificationFlags == notif.NotificationFlags)) {
				el->ref_count -= 1;
				if (!el->ref_count) {
					DEBUG(0, ("[%s:%d]: Deleting Message subscription\n", 
						  __FUNCTION__, __LINE__));
					DLIST_REMOVE(user_cmd->notifications, el);
					talloc_free(el);
					return true;
				}
			}
		}
		DEBUG(0, ("[%s:%d]: Unregistered subscription found\n", __FUNCTION__, __LINE__));
		break;
	}

	return true;
}

static bool mapistore_mgmt_message_notification_folder(struct mapistore_mgmt_users *user_cmd,
						       struct mapistore_mgmt_notification_cmd notif)
{
	struct mapistore_mgmt_notif	*el;
	bool				found = true;

	if (!notif.FolderID) return false;

	switch (notif.status) {
	case MAPISTORE_MGMT_REGISTER:
		for (el = user_cmd->notifications; el; el = el->next) {
			if (!el->MessageID && (el->FolderID == notif.FolderID) &&
			    (el->NotificationFlags == notif.NotificationFlags)) {
				found = true;
				el->ref_count += 1;
				break;
			}
		}
		if (found == false) {
			mapistore_mgmt_message_notification_command_add(user_cmd, notif);
		}
		break;
	case MAPISTORE_MGMT_UNREGISTER:
		for (el = user_cmd->notifications; el; el = el->next) {
			if (!el->MessageID && (el->FolderID == notif.FolderID) &&
			    (el->NotificationFlags == notif.NotificationFlags)) {
				el->ref_count -= 1;
				if (!el->ref_count) {
					DEBUG(0, ("[%s:%d]: Deleting Folder subscription\n", 
						  __FUNCTION__, __LINE__));
					DLIST_REMOVE(user_cmd->notifications, el);
					talloc_free(el);
					return true;
				}
			}
		}
		DEBUG(0, ("[%s:%d]: Unregistered subscription found\n", __FUNCTION__, __LINE__));
		break;
	}

	return true;
}

int mapistore_mgmt_message_notification_command(struct mapistore_mgmt_context *mgmt_ctx,
						struct mapistore_mgmt_notification_cmd notif)
{
	struct mapistore_mgmt_users	*el;
	bool				ret;


	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mgmt_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!mgmt_ctx->users, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!notif.username || !notif.MAPIStoreURI || 
			    (!notif.FolderID && notif.WholeStore == false), MAPI_E_INVALID_PARAMETER, NULL);

	for (el = mgmt_ctx->users; el; el = el->next) {
		if (!strcmp(el->info->username, notif.username)) {
			/* Case where no notifications has been registered yet */
			if (el->notifications == NULL) {
				mapistore_mgmt_message_notification_command_add(el, notif);
			} else {
				/* subscription on wholestore case */
				ret = mapistore_mgmt_message_notification_wholestore(el, notif);
				if (ret == false) {
					/* subscription on message case */
					ret = mapistore_mgmt_message_notification_message(el, notif);
					if (ret == false) {
						/* subscription on folder case */
						ret = mapistore_mgmt_message_notification_folder(el, notif);
					}
				}
			}
		}
	}
	
	return MAPISTORE_SUCCESS;
}
