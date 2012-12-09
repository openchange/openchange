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

#ifndef	__MAPISTORE_PRIVATE_H__
#define	__MAPISTORE_PRIVATE_H__

#include <talloc.h>

#ifndef	ISDOT
#define ISDOT(path) ( \
			*((const char *)(path)) == '.' && \
			*(((const char *)(path)) + 1) == '\0' \
		    )
#endif

#ifndef	ISDOTDOT
#define ISDOTDOT(path)	( \
			    *((const char *)(path)) == '.' && \
			    *(((const char *)(path)) + 1) == '.' && \
			    *(((const char *)(path)) + 2) == '\0' \
			)
#endif


struct tdb_wrap {
	struct tdb_context	*tdb;
	const char		*name;
	struct tdb_wrap		*prev;
	struct tdb_wrap		*next;
};


struct ldb_wrap {
  struct ldb_wrap			*next;
	struct ldb_wrap			*prev;
	struct ldb_wrap_context {
		const char		*url;
		struct tevent_context	*ev;
		unsigned int		flags;
	} context;
	struct ldb_context		*ldb;
};

/**
   Identifier mapping context.

   This structure stores PR_MID and PR_FID identifiers to backend
   identifiers mapping. It points to a database containing the used
   identifiers.

   The last_id structure member references the last identifier value
   which got created. There is no identifier available with a value
   higher than last_id.
 */
struct id_mapping_context {
	struct tdb_wrap		*used_ctx;
	uint64_t		last_id;
};


/**
   Free context identifier list

   This structure is a double chained list storing unused context
   identifiers.
 */
struct context_id_list {
	uint32_t		context_id;
	struct context_id_list	*prev;
	struct context_id_list	*next;
};


struct processing_context {
	struct id_mapping_context	*mapping_ctx;
	struct context_id_list		*free_ctx;
	uint32_t			last_context_id;
	uint64_t			dflt_start_id;
};


/**
   Indexing identifier list
 */
struct indexing_context_list {
	struct tdb_wrap			*index_ctx;
	char				*username;
	// uint32_t			ref_count;
	struct indexing_context_list	*prev;
	struct indexing_context_list	*next;
};

#define	MAPISTORE_DB_NAMED		"named_properties.ldb"
#define	MAPISTORE_DB_INDEXING		"indexing.tdb"
#define	MAPISTORE_SOFT_DELETED_TAG	"SOFT_DELETED:"

struct replica_mapping_context_list {
	struct tdb_context		*tdb;
	char				*username;
	uint32_t			ref_count;
	struct replica_mapping_context_list	*prev;
	struct replica_mapping_context_list	*next;
};
#define	MAPISTORE_DB_REPLICA_MAPPING	"replica_mapping.tdb"

/**
   The database name where in use ID mappings are stored
 */
#define	MAPISTORE_DB_LAST_ID_KEY	"mapistore_last_id"
#define	MAPISTORE_DB_LAST_ID_VAL	0x15000

#define	MAPISTORE_DB_NAME_USED_ID	"mapistore_id_mapping_used.tdb"

/**
   MAPIStore management defines
 */
#define	MAPISTORE_MQUEUE_IPC		"/mapistore_ipc"
#define	MAPISTORE_MQUEUE_NEWMAIL_FMT	"/%s#newmail"

__BEGIN_DECLS

/**
   Properties used in mapistore but not referenced anymore in MS-OXCPROPS.pdf
 */
#define PR_ATTACH_ID				0x3f880014
#define PR_MODIFIER_FLAGS			0x405a0003
#define	PR_RECIPIENT_ON_NORMAL_MSG_COUNT	0x66af0003

/* definitions from mapistore_processing.c */
const char *mapistore_get_mapping_path(void);
enum mapistore_error mapistore_init_mapping_context(struct processing_context *);
enum mapistore_error mapistore_get_context_id(struct processing_context *, uint32_t *);
enum mapistore_error mapistore_free_context_id(struct processing_context *, uint32_t);


/* definitions from mapistore_backend.c */
enum mapistore_error mapistore_backend_init(TALLOC_CTX *, const char *);
enum mapistore_error mapistore_backend_registered(const char *);
enum mapistore_error mapistore_backend_list_contexts(const char *, struct tdb_wrap *, TALLOC_CTX *, struct mapistore_contexts_list **);
enum mapistore_error mapistore_backend_create_context(TALLOC_CTX *, struct mapistore_connection_info *, struct tdb_wrap *, const char *, const char *, uint64_t, struct backend_context **);
enum mapistore_error mapistore_backend_create_root_folder(const char *, enum mapistore_context_role, uint64_t, const char *, TALLOC_CTX *, char **);
enum mapistore_error mapistore_backend_add_ref_count(struct backend_context *);
enum mapistore_error mapistore_backend_delete_context(struct backend_context *);
enum mapistore_error mapistore_backend_get_path(struct backend_context *, TALLOC_CTX *, uint64_t, char **);

enum mapistore_error mapistore_backend_folder_open_folder(struct backend_context *, void *, TALLOC_CTX *, uint64_t, void **);
enum mapistore_error mapistore_backend_folder_create_folder(struct backend_context *, void *, TALLOC_CTX *, uint64_t, struct SRow *, void **);
enum mapistore_error mapistore_backend_folder_delete(struct backend_context *, void *);
enum mapistore_error mapistore_backend_folder_open_message(struct backend_context *, void *, TALLOC_CTX *, uint64_t, bool, void **);
enum mapistore_error mapistore_backend_folder_create_message(struct backend_context *, void *, TALLOC_CTX *, uint64_t, uint8_t, void **);
enum mapistore_error mapistore_backend_folder_delete_message(struct backend_context *, void *, uint64_t, uint8_t);
enum mapistore_error mapistore_backend_folder_move_copy_messages(struct backend_context *, void *, void *, TALLOC_CTX *, uint32_t, uint64_t *, uint64_t *, struct Binary_r **, uint8_t);
enum mapistore_error mapistore_backend_folder_move_folder(struct backend_context *, void *, void *, TALLOC_CTX *, const char *);
enum mapistore_error mapistore_backend_folder_copy_folder(struct backend_context *, void *, void *, TALLOC_CTX *, bool, const char *);
enum mapistore_error mapistore_backend_folder_get_deleted_fmids(struct backend_context *, void *, TALLOC_CTX *, enum mapistore_table_type, uint64_t, struct UI8Array_r **, uint64_t *);
enum mapistore_error mapistore_backend_folder_get_child_count(struct backend_context *, void *, enum mapistore_table_type, uint32_t *);
enum mapistore_error mapistore_backend_folder_get_child_fid_by_name(struct backend_context *, void *, const char *, uint64_t *);
enum mapistore_error mapistore_backend_folder_open_table(struct backend_context *, void *, TALLOC_CTX *, enum mapistore_table_type, uint32_t, void **, uint32_t *);
enum mapistore_error mapistore_backend_folder_modify_permissions(struct backend_context *, void *, uint8_t, uint16_t, struct PermissionData *);
enum mapistore_error mapistore_backend_folder_preload_message_bodies(struct backend_context *, void *, enum mapistore_table_type, const struct UI8Array_r *);

enum mapistore_error mapistore_backend_message_get_message_data(struct backend_context *, void *, TALLOC_CTX *, struct mapistore_message **);
enum mapistore_error mapistore_backend_message_modify_recipients(struct backend_context *, void *, struct SPropTagArray *, uint16_t, struct mapistore_message_recipient *);
enum mapistore_error mapistore_backend_message_set_read_flag(struct backend_context *, void *, uint8_t);
enum mapistore_error mapistore_backend_message_save(struct backend_context *, void *);
enum mapistore_error mapistore_backend_message_submit(struct backend_context *, void *, enum SubmitFlags);
enum mapistore_error mapistore_backend_message_get_attachment_table(struct backend_context *, void *, TALLOC_CTX *, void **, uint32_t *);
enum mapistore_error mapistore_backend_message_open_attachment(struct backend_context *, void *, TALLOC_CTX *, uint32_t, void **);
enum mapistore_error mapistore_backend_message_create_attachment(struct backend_context *, void *, TALLOC_CTX *, void **, uint32_t *);
enum mapistore_error mapistore_backend_message_attachment_open_embedded_message(struct backend_context *, void *, TALLOC_CTX *, void **, uint64_t *, struct mapistore_message **msg);
enum mapistore_error mapistore_backend_message_attachment_create_embedded_message(struct backend_context *, void *, TALLOC_CTX *, void **, struct mapistore_message **msg);

enum mapistore_error mapistore_backend_table_get_available_properties(struct backend_context *, void *, TALLOC_CTX *, struct SPropTagArray **);
enum mapistore_error mapistore_backend_table_set_columns(struct backend_context *, void *, uint16_t, enum MAPITAGS *);
enum mapistore_error mapistore_backend_table_set_restrictions(struct backend_context *, void *, struct mapi_SRestriction *, uint8_t *);
enum mapistore_error mapistore_backend_table_set_sort_order(struct backend_context *, void *, struct SSortOrderSet *, uint8_t *);
enum mapistore_error mapistore_backend_table_get_row(struct backend_context *, void *, TALLOC_CTX *, enum mapistore_query_type, uint32_t, struct mapistore_property_data **);
enum mapistore_error mapistore_backend_table_get_row_count(struct backend_context *, void *, enum mapistore_query_type, uint32_t *);
enum mapistore_error mapistore_backend_table_handle_destructor(struct backend_context *, void *, uint32_t);

enum mapistore_error mapistore_backend_properties_get_available_properties(struct backend_context *, void *, TALLOC_CTX *, struct SPropTagArray **);
enum mapistore_error mapistore_backend_properties_get_properties(struct backend_context *, void *, TALLOC_CTX *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
enum mapistore_error mapistore_backend_properties_set_properties(struct backend_context *, void *, struct SRow *);

enum mapistore_error mapistore_backend_manager_generate_uri(struct backend_context *, TALLOC_CTX *, const char *, const char *, const char *, const char *, char **);

/* definitions from mapistore_tdb_wrap.c */
struct tdb_wrap *mapistore_tdb_wrap_open(TALLOC_CTX *, const char *, int, int, int, mode_t);

/* definitions from mapistore_ldb_wrap.c */
struct ldb_context *mapistore_ldb_wrap_connect(TALLOC_CTX *, struct tevent_context *, const char *, unsigned int);

/* definitions from mapistore_indexing.c */
struct indexing_context_list *mapistore_indexing_search(struct mapistore_context *, const char *);
enum mapistore_error mapistore_indexing_add(struct mapistore_context *, const char *, struct indexing_context_list **);
enum mapistore_error mapistore_indexing_search_existing_fmid(struct indexing_context_list *, uint64_t, bool *);
enum mapistore_error mapistore_indexing_record_add(TALLOC_CTX *, struct indexing_context_list *, uint64_t, const char *);
enum mapistore_error mapistore_indexing_record_add_fmid(struct mapistore_context *, uint32_t, const char *, uint64_t);
enum mapistore_error mapistore_indexing_record_del_fmid(struct mapistore_context *, uint32_t, const char *, uint64_t, uint8_t);
// enum mapistore_error mapistore_indexing_add_ref_count(struct indexing_context_list *);
// enum mapistore_error mapistore_indexing_del_ref_count(struct indexing_context_list *);

/* definitions from mapistore_namedprops.c */
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *, struct ldb_context **);

__END_DECLS

#endif	/* ! __MAPISTORE_PRIVATE_H__ */
