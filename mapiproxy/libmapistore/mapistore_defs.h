/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2011
   Copyright (C) Brad Hards <bradh@openchange.org> 2010-2011

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
   \file mapistore_defs.h

   \brief MAPISTORE common definitions
   
   This header contains the shared definitions for data structures,
   enumerations and defines that are used across the MAPISTORE API.
 */

#ifndef	__MAPISTORE_DEFS_H
#define	__MAPISTORE_DEFS_H

/**
  \brief Special Folder identifiers
  
  This list identifies each of the folder identifiers.
 */
enum MAPISTORE_DFLT_FOLDERS {
	MDB_ROOT_FOLDER		= 1,
	MDB_DEFERRED_ACTIONS	= 2,
	MDB_SPOOLER_QUEUE	= 3,
	MDB_TODO_SEARCH		= 4,
	MDB_IPM_SUBTREE		= 5,
	MDB_INBOX		= 6,
	MDB_OUTBOX		= 7,
	MDB_SENT_ITEMS		= 8,
	MDB_DELETED_ITEMS	= 9,
	MDB_COMMON_VIEWS	= 10,
	MDB_SCHEDULE		= 11,
	MDB_SEARCH		= 12,
	MDB_VIEWS		= 13,
	MDB_SHORTCUTS		= 14,
	MDB_REMINDERS		= 15,
	MDB_CALENDAR		= 16,
	MDB_CONTACTS		= 17,
	MDB_JOURNAL		= 18,
	MDB_NOTES		= 19,
	MDB_TASKS		= 20,
	MDB_DRAFTS		= 21,
	MDB_TRACKED_MAIL	= 22,
	MDB_SYNC_ISSUES		= 23,
	MDB_CONFLICTS		= 24,
	MDB_LOCAL_FAILURES	= 25,
	MDB_SERVER_FAILURES	= 26,
	MDB_JUNK_EMAIL		= 27,
	MDB_RSS_FEEDS		= 28,
	MDB_CONVERSATION_ACT	= 29, /**< Conversation Actions folder */
	MDB_LAST_SPECIALFOLDER	= MDB_CONVERSATION_ACT, /**< the last identifier, used for iteration */
	MDB_CUSTOM		= 999 /**< This is a custom (or generic) folder with no special meaning */
};

struct mapistore_message {
	struct SRowSet			*recipients;
	struct SRow			*properties;
};

#define	MAPISTORE_FOLDER_TABLE		1
#define	MAPISTORE_MESSAGE_TABLE		2

#define	MAPISTORE_FOLDER		1
#define	MAPISTORE_MESSAGE		2

#define	MAPISTORE_SOFT_DELETE		1
#define	MAPISTORE_PERMANENT_DELETE	2

/* TODO: perhaps this should be in another header */
const char		*mapistore_get_mapping_path(void);

#endif /* __MAPISTORE_DEFS_H */
