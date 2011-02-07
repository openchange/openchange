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
	MDB_ROOT_FOLDER		= 1,	/**< The root folder */
	MDB_DEFERRED_ACTIONS	= 2,	/**< Deferred actions special folder  */
	MDB_SPOOLER_QUEUE	= 3,	/**< Mail spooler queue special folder */
	MDB_TODO_SEARCH		= 4,	/**< Special folder for "todo" search */
	MDB_IPM_SUBTREE		= 5,	/**< Root folder for inter-personal messages */
	MDB_INBOX		= 6,	/**< Inbox special folder */
	MDB_OUTBOX		= 7,	/**< Outbox (to be send) special folder */
	MDB_SENT_ITEMS		= 8,	/**< Sent items (previously sent) special folder */
	MDB_DELETED_ITEMS	= 9,	/**< Deleted items special folder */
	MDB_COMMON_VIEWS	= 10,	/**< Special folder for common views metadata */
	MDB_SCHEDULE		= 11,	/**< Schedule (Free/busy) special folder */
	MDB_SEARCH		= 12,	/**< Search root folder */
	MDB_VIEWS		= 13,	/**< Special folder for views metadata */
	MDB_SHORTCUTS		= 14,	/**< Special folder for "shortcuts" action storage */
	MDB_REMINDERS		= 15,	/**< Special folder for reminders */
	MDB_CALENDAR		= 16,	/**< The user calendar */
	MDB_CONTACTS		= 17,	/**< The user's private address book (contacts) */
	MDB_JOURNAL		= 18,	/**< The user's journal */
	MDB_NOTES		= 19,	/**< The user's short notes */
	MDB_TASKS		= 20,	/**< The user's tasks ("todo list") */
	MDB_DRAFTS		= 21,	/**< Special folder for draft messages */
	MDB_TRACKED_MAIL	= 22,	/**< Special folder for tracked mail */
	MDB_SYNC_ISSUES		= 23,	/**< Special folder to handle messages that failed synchronization */
	MDB_CONFLICTS		= 24,	/**< Special folder for server-side conflicts (from synchronization)  */
	MDB_LOCAL_FAILURES	= 25,	/**< Special folder for client side failures */
	MDB_SERVER_FAILURES	= 26,	/**< Special folder for server side failures */
	MDB_JUNK_EMAIL		= 27,	/**< Special folder for junk ("spam") email */
	MDB_RSS_FEEDS		= 28,	/**< Special folder for RSS feeds */
	MDB_CONVERSATION_ACT	= 29,	/**< Conversation Actions folder */
	MDB_LAST_SPECIALFOLDER	= MDB_CONVERSATION_ACT, /**< the last identifier, used for iteration */
	MDB_CUSTOM		= 999	/**< This is a custom (or generic) folder with no special meaning */
};


/**
   \brief Named properties provisioning types

   This list identifies the different provisioning types a backend can
   use for Internal named properties provisioning purposes
 */
enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE {
	MAPISTORE_NAMEDPROPS_PROVISION_NONE	= 0, /** < No LDIF data associated */
	MAPISTORE_NAMEDPROPS_PROVISION_FILE	= 1, /** < LDIF information are retrieved from a file */
	MAPISTORE_NAMEDPROPS_PROVISION_BUFFER	= 2  /** < LDIF information are retrieved from a buffer */
};

/**
  A message object
  
  This is used to handle the contents of a message. Note that the body of the message
  is just another property.
  
  Attachments are handled by a separate table, and are not available here.
 */
struct mapistore_message {
	struct SRowSet			*recipients; /**< the list of recipient rows */
	struct SRow			*properties; /**< the properties of the message */
};

/**
  Table types
 */
enum MAPISTORE_TABLE_TYPE {
	MAPISTORE_FOLDER_TABLE = 1,	/**< This table is for a folder */
	MAPISTORE_MESSAGE_TABLE = 2	/**< This table is for a message */
};

/* TODO: convert this to an enum */
#define	MAPISTORE_FOLDER		1
#define	MAPISTORE_MESSAGE		2

/**
  Deletion types
 */
enum MAPISTORE_DELETION_TYPE {
	MAPISTORE_SOFT_DELETE = 1,	/**< The item is "soft" deleted */
	MAPISTORE_PERMANENT_DELETE = 2	/**< The item is hard deleted */
};

/**
   Default message ID allocation range
 */
#define	MAPISTORE_INDEXING_DFLT_ALLOC_RANGE_VAL	0x1000

#endif /* __MAPISTORE_DEFS_H */
