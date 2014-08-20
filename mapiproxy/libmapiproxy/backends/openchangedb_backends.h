/*
   MAPI Proxy - OpenchangeDB interface definition

   OpenChange Project

   Copyright (C) Jesús García Sáez 2014

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

#ifndef __OPENCHANGEDB_BACKEND_H__
#define __OPENCHANGEDB_BACKEND_H__

#include <stdint.h>
#include <stdbool.h>
#include <talloc.h>
#include <gen_ndr/exchange.h>

struct openchangedb_context {
	enum MAPISTATUS (*get_new_changeNumber)(struct openchangedb_context *, const char *, uint64_t *);
	enum MAPISTATUS (*get_new_changeNumbers)(struct openchangedb_context *, TALLOC_CTX *, const char *, uint64_t, struct UI8Array_r **);
	enum MAPISTATUS (*get_next_changeNumber)(struct openchangedb_context *, const char *, uint64_t *);
	enum MAPISTATUS (*get_SpecialFolderID)(struct openchangedb_context *, const char *, uint32_t, uint64_t *);
	enum MAPISTATUS (*get_SystemFolderID)(struct openchangedb_context *, const char *, uint32_t, uint64_t *);
	enum MAPISTATUS (*get_PublicFolderID)(struct openchangedb_context *, const char *, uint32_t, uint64_t *);
	enum MAPISTATUS (*get_distinguishedName)(TALLOC_CTX *, struct openchangedb_context *, uint64_t, char **);
	enum MAPISTATUS (*get_MailboxGuid)(struct openchangedb_context *, const char *, struct GUID *);
	enum MAPISTATUS (*get_MailboxReplica)(struct openchangedb_context *, const char *, uint16_t *, struct GUID *);
	enum MAPISTATUS (*get_PublicFolderReplica)(struct openchangedb_context *, const char *, uint16_t *, struct GUID *);
	enum MAPISTATUS (*get_parent_fid)(struct openchangedb_context *, const char *, uint64_t, uint64_t *, bool);
	enum MAPISTATUS (*get_MAPIStoreURIs)(struct openchangedb_context *, const char *, TALLOC_CTX *, struct StringArrayW_r **);
	enum MAPISTATUS (*get_mapistoreURI)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint64_t, char **, bool);
	enum MAPISTATUS (*set_mapistoreURI)(struct openchangedb_context *, const char *, uint64_t, const char *);
	enum MAPISTATUS (*get_fid)(struct openchangedb_context *, const char *, uint64_t *);
	enum MAPISTATUS (*get_ReceiveFolder)(TALLOC_CTX *, struct openchangedb_context *, const char *, const char *, uint64_t *, const char **);
	enum MAPISTATUS (*get_ReceiveFolderTable)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t *, struct ReceiveFolder **);
	enum MAPISTATUS (*get_TransportFolder)(struct openchangedb_context *, const char *, uint64_t *);
	enum MAPISTATUS (*lookup_folder_property)(struct openchangedb_context *, uint32_t, uint64_t);
	enum MAPISTATUS (*set_folder_properties)(struct openchangedb_context *, const char *, uint64_t, struct SRow *);
	enum MAPISTATUS (*get_folder_property)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t, uint64_t, void **);
	enum MAPISTATUS (*get_folder_count)(struct openchangedb_context *, const char *, uint64_t, uint32_t *);
	enum MAPISTATUS (*get_message_count)(struct openchangedb_context *, const char *, uint64_t, uint32_t *, bool);
	enum MAPISTATUS (*get_system_idx)(struct openchangedb_context *, const char *, uint64_t, int *);
	enum MAPISTATUS (*get_table_property)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t, uint32_t, void **);
	enum MAPISTATUS (*get_fid_by_name)(struct openchangedb_context *, const char *, uint64_t, const char*, uint64_t *);
	enum MAPISTATUS (*get_mid_by_subject)(struct openchangedb_context *, const char *, uint64_t, const char *, bool, uint64_t *);
	enum MAPISTATUS (*set_ReceiveFolder)(struct openchangedb_context *, const char *, const char *, uint64_t);
	enum MAPISTATUS (*create_mailbox)(struct openchangedb_context *, const char *, const char *, const char *, int, uint64_t, const char *);
	enum MAPISTATUS (*create_folder)(struct openchangedb_context *, const char *, uint64_t, uint64_t, uint64_t, const char *, int);
	enum MAPISTATUS (*delete_folder)(struct openchangedb_context *, const char *, uint64_t);
	enum MAPISTATUS (*get_fid_from_partial_uri)(struct openchangedb_context *, const char *, uint64_t *);
	enum MAPISTATUS (*get_users_from_partial_uri)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t *, char ***, char ***);

	enum MAPISTATUS (*table_init)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint8_t, uint64_t, void **);
	enum MAPISTATUS (*table_set_sort_order)(struct openchangedb_context *, void *, struct SSortOrderSet *);
	enum MAPISTATUS (*table_set_restrictions)(struct openchangedb_context *, void *, struct mapi_SRestriction *);
	enum MAPISTATUS (*table_get_property)(TALLOC_CTX *, struct openchangedb_context *, void *, enum MAPITAGS, uint32_t, bool, void **);

	enum MAPISTATUS (*message_create)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint64_t, uint64_t, bool, void **);
	enum MAPISTATUS (*message_save)(struct openchangedb_context *, void *, uint8_t);
	enum MAPISTATUS (*message_open)(TALLOC_CTX *, struct openchangedb_context *, const char *, uint64_t, uint64_t, void **, void **);
	enum MAPISTATUS (*message_get_property)(TALLOC_CTX *, struct openchangedb_context *, void *, uint32_t, void **);
	enum MAPISTATUS (*message_set_properties)(TALLOC_CTX *, struct openchangedb_context *, void *, struct SRow *);

	enum MAPISTATUS (*transaction_start)(struct openchangedb_context *);
	enum MAPISTATUS (*transaction_commit)(struct openchangedb_context *);

	enum MAPISTATUS (*get_new_public_folderID)(struct openchangedb_context *, const char *, uint64_t *);
	bool (*is_public_folder_id)(struct openchangedb_context *, uint64_t);

	enum MAPISTATUS (*get_indexing_url)(struct openchangedb_context *, const char *, const char **);

	bool (*set_locale)(struct openchangedb_context *, const char *, uint32_t);

	const char **(*get_folders_names)(TALLOC_CTX *, struct openchangedb_context *, const char *, const char *);
	const char *backend_type;
	void *data;
};

const char *nil_string;


#define TRANSPORT_FOLDER_SYSTEM_IDX 14

#endif /* __OPENCHANGEDB_BACKEND_H__ */
