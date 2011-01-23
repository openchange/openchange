/*
   OpenChange Storage Abstraction Layer library
   MAPIStore database backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2011
   Copyright (C) Brad Hards 2010-2011

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

#ifndef	__MAPISTORE_MSTOREDB_H
#define	__MAPISTORE_MSTOREDB_H

#define __STDC_FORMAT_MACROS	1
#include <inttypes.h>

#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/mapistore_defs.h"
#include "mapiproxy/libmapistore/mapistore_backend.h"
#include "mapiproxy/libmapistore/mapistore.h"

struct mstoredb_context {
	struct mapistore_backend_context	*mdb_ctx;
	void					*ldb_ctx;
	const char				*dbpath;
	char					*context_dn;
	uint64_t				context_fid;
	struct ldb_dn				*basedn;
};

#define	MAILBOX_BASE_URI	"CN=%s,%s"

struct mstoredb_dflt_folders {
	enum MAPISTORE_DFLT_FOLDERS	index;
	const char			*name;
};

const struct mstoredb_dflt_folders dflt_folders[] = {
	{ MDB_ROOT_FOLDER,		"CN=Mailbox Root,CN=Folders" },
	{ MDB_DEFERRED_ACTIONS,		"CN=Deferred Actions,CN=Mailbox Root,CN=Folders" },
	{ MDB_SPOOLER_QUEUE,		"CN=Spooler Queue,CN=Mailbox Root,CN=Folders" },
	{ MDB_TODO_SEARCH,		"CN=To-Do Search,CN=Mailbox Root,CN=Folders" },
	{ MDB_IPM_SUBTREE,		"CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_INBOX,			"CN=Inbox,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_OUTBOX,			"CN=Outbox,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_SENT_ITEMS,		"CN=Sent Items,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_DELETED_ITEMS,		"CN=Deleted Items,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_COMMON_VIEWS,		"CN=Common Views,CN=Mailbox Root,CN=Folders" },
	{ MDB_SCHEDULE,			"CN=Schedule,CN=Mailbox Root,CN=Folders" },
	{ MDB_SEARCH,			"CN=Search,CN=Mailbox Root,CN=Folders" },
	{ MDB_VIEWS,			"CN=Views,CN=Mailbox Root,CN=Folders" },
	{ MDB_SHORTCUTS,		"CN=Shortcuts,CN=Mailbox Root,CN=Folders" },
	{ MDB_REMINDERS,		"CN=Reminders,CN=Mailbox Root,CN=Folders" },
	{ MDB_CALENDAR,			"CN=Calendar,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_CONTACTS,			"CN=Contacts,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_JOURNAL,			"CN=Journal,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_NOTES,			"CN=Notes,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_TASKS,			"CN=Tasks,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_DRAFTS,			"CN=Drafts,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_TRACKED_MAIL,		"CN=Tracked Mail,CN=Mailbox Root,CN=Folders" },
	{ MDB_SYNC_ISSUES,		"CN=Sync Issues,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_CONFLICTS,		"CN=Conflicts,CN=Sync Issues,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_LOCAL_FAILURES,		"CN=Local Failures,CN=Sync Issues,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_SERVER_FAILURES,		"CN=Server Failures,CN=Sync Issues,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_JUNK_EMAIL,		"CN=Junk Emails,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_RSS_FEEDS,		"CN=RSS Feeds,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_CONVERSATION_ACT,		"CN=Conversation Actions,CN=IPM Subtree,CN=Mailbox Root,CN=Folders" },
	{ MDB_CUSTOM,			NULL }
};

__BEGIN_DECLS

/* CN=Inbox,CN=IPM SUbtree,CN=Mailbox Root,CN=Folders,CN=jkerihuel,CN=First Adminsitrative Group, ... */ 

enum MAPISTORE_ERROR mapistore_init_backend(void);

__END_DECLS

#endif /* ! __MAPISTORE_MSTOREDB_H */
