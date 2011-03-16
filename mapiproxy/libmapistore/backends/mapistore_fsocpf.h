/*
   OpenChange Storage Abstraction Layer library
   MAPIStore FSOCPF backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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

#ifndef	__MAPISTORE_FSOCPF_H
#define	__MAPISTORE_FSOCPF_H

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include "libocpf/ocpf.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_defs.h"
#include "mapiproxy/libmapistore/mapistore_common.h"
#include "mapiproxy/libmapistore/mapistore_backend.h"
#include <dlinklist.h>
#include <dirent.h>

struct fsocpf_folder {
	DIR				*dir;
	char				*uri;
};

struct fsocpf_folder_list {
	struct fsocpf_folder		*folder;
	struct fsocpf_folder_list	*next;
	struct fsocpf_folder_list	*prev;
};

struct fsocpf_message {
	char				*uri;
	char				*folder_uri;
	uint32_t			ocpf_context_id;
	char				*path;
};

struct fsocpf_message_list {
	struct fsocpf_message		*message;
	struct fsocpf_message_list	*prev;
	struct fsocpf_message_list	*next;
};

struct fsocpf_context {
	void					*private_data;
	struct mapistore_backend_context	*mstoredb_ctx;
	char					*login_user;
	char					*username;
	char					*uri;
	struct fsocpf_folder_list		*folders;
	struct fsocpf_message_list		*messages;
	// char					*root_uri;
	DIR					*dir;
};


struct fsocpf_dflt_folders {
	enum MAPISTORE_DFLT_FOLDERS	index;
	const char			*name;
};

const struct fsocpf_dflt_folders dflt_folders[] = {
	{ MDB_ROOT_FOLDER,		"Root" },
	{ MDB_DEFERRED_ACTIONS,		"Deferred Actions" },
	{ MDB_SPOOLER_QUEUE,		"Spooler Queue" },
	{ MDB_TODO_SEARCH,		"TODO Search" },
	{ MDB_IPM_SUBTREE,		"IPM Subtree" },
	{ MDB_INBOX,			"Inbox" },
	{ MDB_OUTBOX,			"Outbox" },
	{ MDB_SENT_ITEMS,		"Sent Items" },
	{ MDB_DELETED_ITEMS,		"Deleted Items" },
	{ MDB_COMMON_VIEWS,		"Common Views" },
	{ MDB_SCHEDULE,			"Schedule" },
	{ MDB_SEARCH,			"Search" },
	{ MDB_VIEWS,			"Views" },
	{ MDB_SHORTCUTS,		"Shortcuts" },
	{ MDB_REMINDERS,		"Reminders" },
	{ MDB_CALENDAR,			"Calendar" },
	{ MDB_CONTACTS,			"Contacts" },
	{ MDB_JOURNAL,			"Journal" },
	{ MDB_NOTES,			"Notes" },
	{ MDB_TASKS,			"Tasks" },
	{ MDB_DRAFTS,			"Drafts" },
	{ MDB_TRACKED_MAIL,		"Tracked Mail" },
	{ MDB_SYNC_ISSUES,		"Synchronization Issues" },
	{ MDB_CONFLICTS,		"Conflicts" },
	{ MDB_LOCAL_FAILURES,		"Local Failures" },
	{ MDB_SERVER_FAILURES,		"Server Failures" },
	{ MDB_JUNK_EMAIL,		"Junk Email" },
	{ MDB_RSS_FEEDS,		"RSS Feeds" },
	{ MDB_CONVERSATION_ACT,		"Conversation Actions" },
	{ MDB_CUSTOM,			NULL }
};

__BEGIN_DECLS

enum MAPISTORE_ERROR	mapistore_init_backend(void);

__END_DECLS

#endif /* ! __MAPISTORE_FSOCPF_H */
