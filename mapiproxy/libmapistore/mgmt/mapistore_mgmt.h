/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2010

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

#ifndef	__MAPISTORE_MGMT_H
#define	__MAPISTORE_MGMT_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

#include <dlinklist.h>

#include "gen_ndr/mapistore_mgmt.h"

/* forward declaration */
struct mapistore_context;

struct mapistore_mgmt_users_list {
	uint32_t	count;
	const char	**user;
};

struct mapistore_mgmt_notif {
	bool				WholeStore;
	uint16_t			NotificationFlags;
	int64_t				FolderID;
	int64_t				MessageID;
	const char			*MAPIStoreURI;
	uint32_t			ref_count;
	struct mapistore_mgmt_notif	*prev;
	struct mapistore_mgmt_notif	*next;
};

struct mapistore_mgmt_notify_context {
	int				fd;
	struct sockaddr			*addr;
	uint16_t			context_len;
	uint8_t				*context_data;
};

struct mapistore_mgmt_users {
	struct mapistore_mgmt_user_cmd		*info;
	struct mapistore_mgmt_notif		*notifications;
	uint32_t				ref_count;
	struct mapistore_mgmt_notify_context	*notify_ctx;
	struct mapistore_mgmt_users		*prev;
	struct mapistore_mgmt_users		*next;
};

struct mapistore_mgmt_context {
	struct mapistore_context	*mstore_ctx;
	struct mapistore_mgmt_users	*users;
	mqd_t				mq_ipc;
	bool				verbose;
};

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

__BEGIN_DECLS

/* definitions from mapistore_mgmt.c */
struct mapistore_mgmt_context *mapistore_mgmt_init(struct mapistore_context *);
enum mapistore_error mapistore_mgmt_release(struct mapistore_mgmt_context *);
enum mapistore_error mapistore_mgmt_registered_backend(struct mapistore_mgmt_context *, const char *);
struct mapistore_mgmt_users_list *mapistore_mgmt_existing_users(struct mapistore_mgmt_context *, void *, const char *, const char *, const char *);
struct mapistore_mgmt_users_list *mapistore_mgmt_registered_users(struct mapistore_mgmt_context *, const char *, const char *);
enum mapistore_error mapistore_mgmt_set_verbosity(struct mapistore_mgmt_context *, bool);

enum mapistore_error mapistore_mgmt_generate_uri(struct mapistore_mgmt_context *, const char *, const char *, const char *, const char *, const char *, char **);
enum mapistore_error mapistore_mgmt_registered_message(struct mapistore_mgmt_context *, const char *, const char *, const char *,const char *, const char *, const char *);
enum mapistore_error mapistore_mgmt_register_message(struct mapistore_mgmt_context *, const char *, const char *, int64_t, const char *, const char *, char **);
enum mapistore_error mapistore_mgmt_registered_folder_subscription(struct mapistore_mgmt_context *, const char *, const char *, uint16_t);

/* definitions from mapistore_mgmt_messages.c */
enum mapistore_error mapistore_mgmt_message_user_command(struct mapistore_mgmt_context *, struct mapistore_mgmt_user_cmd);
enum mapistore_error mapistore_mgmt_message_notification_command(struct mapistore_mgmt_context *, struct mapistore_mgmt_notification_cmd);
enum mapistore_error mapistore_mgmt_message_bind_command(struct mapistore_mgmt_context *, struct mapistore_mgmt_bind_cmd);

/* definitions from mapistore_mgmt_send.c */
enum mapistore_error mapistore_mgmt_send_newmail_notification(struct mapistore_mgmt_context *, const char *, int64_t, int64_t, const char *);
enum mapistore_error mapistore_mgmt_send_udp_notification(struct mapistore_mgmt_context *, const char *);

__END_DECLS

#endif /* ! __MAPISTORE_MGMT_H */
