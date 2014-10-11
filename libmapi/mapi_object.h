/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2009.
   Copyright (C) Fabien Le Mentec 2007.

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

#ifndef	__MAPI_OBJECT_H
#define	__MAPI_OBJECT_H


#include <gen_ndr/exchange.h>


/* forward declarations
 */
struct mapi_session;

/* generic mapi object definition
 */

typedef int64_t mapi_id_t;
typedef uint32_t mapi_handle_t;

typedef struct mapi_object {
	bool			store;
	int64_t			id;
	mapi_handle_t		handle;
	uint8_t			logon_id;
	struct mapi_session	*session;
	void			*private_data;
} mapi_object_t;


/*
 * Interface objects
 */

/**
  IMsgStore store type
  */
enum MsgStoreType {
	PrivateFolderWithoutCachedFids, /*!< Private folder store without the cached folder values filled in */
	PrivateFolderWithCachedFids,  /*!< Private folder store with the cached folder values filled in */
	PublicFolder  /*!< Public folder store */
};

/**
 * IMsgStore object 
 */
typedef struct mapi_obj_store
{
	enum MsgStoreType	store_type;
	/* Mailbox */
	int64_t			fid_mailbox_root;
	int64_t			fid_deferred_actions;
	int64_t			fid_spooler_queue;
	int64_t			fid_top_information_store;
	int64_t			fid_inbox;
	int64_t			fid_outbox;
	int64_t			fid_sent_items;
	int64_t			fid_deleted_items;
	int64_t			fid_common_views;
	int64_t			fid_schedule;
	int64_t			fid_search;
	int64_t			fid_views;
	int64_t			fid_shortcuts;
	/* Public Folders */
	int64_t			fid_pf_public_root;
	int64_t			fid_pf_ipm_subtree;
	int64_t			fid_pf_non_ipm_subtree;
	int64_t			fid_pf_EFormsRegistryRoot;
	int64_t			fid_pf_FreeBusyRoot;
	int64_t			fid_pf_OfflineAB;
	int64_t			fid_pf_EFormsRegistry;
	int64_t			fid_pf_LocalSiteFreeBusy;
	int64_t			fid_pf_LocalSiteOfflineAB;
	int64_t			fid_pf_NNTPArticle;
	/* cached data */
	int64_t			fid_calendar;
	int64_t			fid_contact;
	int64_t			fid_journal;
	int64_t			fid_note;
	int64_t			fid_task;
	int64_t			fid_drafts;
	/* GUID */
	struct GUID		guid;
} mapi_object_store_t;


/**
 * IMAPITable object 
 */

typedef struct mapi_obj_bookmark {
	uint32_t			index;
	struct SBinary_short		bin;
	struct mapi_obj_bookmark	*prev;
	struct mapi_obj_bookmark	*next;
} mapi_object_bookmark_t;


typedef struct mapi_obj_table {
	uint32_t			bk_last;
	mapi_object_bookmark_t		*bookmark;
	struct SPropTagArray		proptags;
} mapi_object_table_t;


typedef struct mapi_obj_message {
	uint32_t			cValues;
	char				*SubjectPrefix;
	char				*NormalizedSubject;
	struct SPropTagArray   		SPropTagArray;
	struct SRowSet			SRowSet;
} mapi_object_message_t;

#endif /*!__MAPI_OBJECT_H */
