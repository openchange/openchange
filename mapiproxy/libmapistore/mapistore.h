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

#ifndef	__MAPISTORE_H
#define	__MAPISTORE_H

/**
   \file mapistore.h

   \brief MAPISTORE general API
   
   This header contains general functions, primarily for
   users of the store (rather than storage providers).
 */

#ifndef	_PUBLIC_
#define	_PUBLIC_
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#if 0
#include <mqueue.h>
#endif

#include <tdb.h>
#include <ldb.h>
#include <talloc.h>

#include "libmapi/libmapi.h"

/* forward declarations */
struct mapistore_mgmt_notif;

typedef	int (*init_backend_fn) (void);

#define	MAPISTORE_INIT_MODULE	"mapistore_init_backend"

#define MAPISTORE_FOLDER        1
#define	MAPISTORE_MESSAGE	2
#define	MAPISTORE_ATTACHMENT	3
#define	MAPISTORE_TABLE 	4

#define	MAPISTORE_SOFT_DELETE		1
#define	MAPISTORE_PERMANENT_DELETE	2

/* Default filename for named properties backend using ldb */
#define MAPISTORE_DB_NAMED  "named_properties.ldb"

struct mapistore_message {
	/* message props */
	char					*subject_prefix;
	char					*normalized_subject;

	/* recipients */
	struct SPropTagArray			*columns;
	uint32_t				recipients_count;
	struct mapistore_message_recipient	*recipients;
};

struct mapistore_message_recipient {
	enum ulRecipClass	type;
	char			*username;
	void			**data;
};

struct indexing_folders_list {
	uint64_t			*folderID;
	uint32_t			count;
};

enum mapistore_table_type {
	MAPISTORE_FOLDER_TABLE = 1,
	MAPISTORE_MESSAGE_TABLE = 2,
	MAPISTORE_FAI_TABLE = 3,
	MAPISTORE_RULE_TABLE = 4,
	MAPISTORE_ATTACHMENT_TABLE = 5,
	MAPISTORE_PERMISSIONS_TABLE = 6
};

enum mapistore_query_type {
	MAPISTORE_PREFILTERED_QUERY,
	MAPISTORE_LIVEFILTERED_QUERY,
};

/* proof of concept: a new structure to simplify property queries */
struct mapistore_property_data {
        void *data;
        int error; /* basically MAPISTORE_SUCCESS or MAPISTORE_ERR_NOT_FOUND */
};

struct mapistore_connection_info {
	char				*username;
	struct GUID			replica_guid;
	uint16_t			repl_id;
	struct mapistore_context	*mstore_ctx;
	struct ldb_context		*sam_ctx; /* samdb */
	struct openchangedb_context	*oc_ctx; /* openchangedb */
};

enum mapistore_context_role {
	MAPISTORE_MAIL_ROLE,
	MAPISTORE_DRAFTS_ROLE,
	MAPISTORE_SENTITEMS_ROLE,
	MAPISTORE_OUTBOX_ROLE,
	MAPISTORE_DELETEDITEMS_ROLE,
	MAPISTORE_CALENDAR_ROLE,
	MAPISTORE_CONTACTS_ROLE,
	MAPISTORE_TASKS_ROLE,
	MAPISTORE_NOTES_ROLE,
	MAPISTORE_JOURNAL_ROLE,
	MAPISTORE_FALLBACK_ROLE,
	MAPISTORE_MAX_ROLES
};

struct mapistore_contexts_list {
	char				*url;
	char				*name;
	bool				main_folder;
	enum mapistore_context_role	role;
	char				*tag;
	struct mapistore_contexts_list	*prev;
	struct mapistore_contexts_list	*next;
};

struct indexing_context {
	enum mapistore_error	(*add_fmid)(struct indexing_context *, const char *, uint64_t, const char *);
	enum mapistore_error	(*update_fmid)(struct indexing_context *, const char *, uint64_t, const char *);
	enum mapistore_error	(*del_fmid)(struct indexing_context *, const char *, uint64_t, uint8_t);
	enum mapistore_error	(*get_uri)(struct indexing_context *, const char *, TALLOC_CTX *, uint64_t, char **, bool *);
	enum mapistore_error	(*get_fmid)(struct indexing_context *, const char *, const char *, bool, uint64_t *, bool *);

	enum mapistore_error	(*allocate_fmid)(struct indexing_context *, const char *, uint64_t *);
	enum mapistore_error	(*allocate_fmids)(struct indexing_context *, const char *, int, uint64_t *);

	/* Backend URL */
	const char *url;

	/* Custom backend data */
	void *data;

	/* Custom backend cache */
	void *cache;
};

struct mapistore_backend {
	/** backend operations */
	struct {
		const char	*name;
		const char	*description;
		const char	*namespace;

		enum mapistore_error	(*init)(void);
		enum mapistore_error	(*list_contexts)(const char *, struct indexing_context *, TALLOC_CTX *, struct mapistore_contexts_list **);
		enum mapistore_error	(*create_context)(TALLOC_CTX *, struct mapistore_connection_info *, struct indexing_context *, const char *, void **);
		enum mapistore_error	(*create_root_folder)(const char *, enum mapistore_context_role, uint64_t, const char *, TALLOC_CTX *, char **);
	} backend;

	/** context operations */
	struct {
		enum mapistore_error	(*get_path)(void *, TALLOC_CTX *, uint64_t, char **);
		enum mapistore_error	(*get_root_folder)(void *, TALLOC_CTX *, uint64_t, void **);
	} context;

        /** oxcfold operations */
        struct {
		enum mapistore_error	(*open_folder)(void *, TALLOC_CTX *, uint64_t, void **);
		enum mapistore_error	(*create_folder)(void *, TALLOC_CTX *, uint64_t, struct SRow *, void **);
		enum mapistore_error	(*delete)(void *);
		enum mapistore_error	(*open_message)(void *, TALLOC_CTX *, uint64_t, bool, void **);
		enum mapistore_error	(*create_message)(void *, TALLOC_CTX *, uint64_t, uint8_t, void **);
		enum mapistore_error	(*delete_message)(void *, uint64_t, uint8_t);
		enum mapistore_error	(*move_copy_messages)(void *, void *, TALLOC_CTX *, uint32_t, uint64_t *, uint64_t *, struct Binary_r **, uint8_t);
 		enum mapistore_error	(*move_folder)(void *, void *, TALLOC_CTX *, const char *);
 		enum mapistore_error	(*copy_folder)(void *, void *, TALLOC_CTX *, bool, const char *);
		enum mapistore_error	(*get_deleted_fmids)(void *, TALLOC_CTX *, enum mapistore_table_type, uint64_t, struct UI8Array_r **, uint64_t *);
		enum mapistore_error	(*get_child_count)(void *, enum mapistore_table_type, uint32_t *);
                enum mapistore_error	(*open_table)(void *, TALLOC_CTX *, enum mapistore_table_type, uint32_t, void **, uint32_t *);
		enum mapistore_error	(*modify_permissions)(void *, uint8_t, uint16_t, struct PermissionData *);

		enum mapistore_error	(*preload_message_bodies)(void *, enum mapistore_table_type, const struct UI8Array_r *);
        } folder;

        /** oxcmsg operations */
        struct {
		enum mapistore_error	(*get_message_data)(void *, TALLOC_CTX *, struct mapistore_message **);
		enum mapistore_error	(*modify_recipients)(void *, struct SPropTagArray *, uint16_t, struct mapistore_message_recipient *);
                enum mapistore_error	(*set_read_flag)(void *, uint8_t);
		enum mapistore_error	(*save)(void *, TALLOC_CTX *);
		enum mapistore_error	(*submit)(void *, enum SubmitFlags);
                enum mapistore_error	(*open_attachment)(void *, TALLOC_CTX *, uint32_t, void **);
                enum mapistore_error	(*create_attachment)(void *, TALLOC_CTX *, void **, uint32_t *);
                enum mapistore_error	(*get_attachment_table)(void *, TALLOC_CTX *, void **, uint32_t *);

		/* attachment operations */
                enum mapistore_error	(*open_embedded_message)(void *, TALLOC_CTX *, void **, uint64_t *, struct mapistore_message **);
                enum mapistore_error	(*create_embedded_message)(void *, TALLOC_CTX *, void **, struct mapistore_message **);
	} message;

        /** oxctabl operations */
        struct {
                enum mapistore_error	(*get_available_properties)(void *, TALLOC_CTX *, struct SPropTagArray **);
                enum mapistore_error	(*set_columns)(void *, uint16_t, enum MAPITAGS *);
                enum mapistore_error	(*set_restrictions)(void *, struct mapi_SRestriction *, uint8_t *);
                enum mapistore_error	(*set_sort_order)(void *, struct SSortOrderSet *, uint8_t *);
                enum mapistore_error	(*get_row)(void *, TALLOC_CTX *, enum mapistore_query_type, uint32_t, struct mapistore_property_data **);
                enum mapistore_error	(*get_row_count)(void *, enum mapistore_query_type, uint32_t *);
		enum mapistore_error	(*handle_destructor)(void *, uint32_t);
        } table;

        /** oxcprpt operations */
        struct {
                enum mapistore_error	(*get_available_properties)(void *, TALLOC_CTX *, struct SPropTagArray **);
                enum mapistore_error	(*get_properties)(void *, TALLOC_CTX *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
                enum mapistore_error	(*set_properties)(void *, struct SRow *);
        } properties;

	/** manager operations */
	struct {
		enum mapistore_error	(*generate_uri)(TALLOC_CTX *, const char *, const char *, const char *, const char *, char **);
	} manager;
};

struct backend_context {
	const struct mapistore_backend	*backend;
	void				*backend_object;
	void				*root_folder_object;
	struct indexing_context		*indexing;
	uint32_t			context_id;
	uint32_t			ref_count;
	char				*uri;
};

struct backend_context_list {
	struct backend_context		*ctx;
	struct backend_context_list	*prev;
	struct backend_context_list	*next;
};

struct processing_context;

struct mapistore_context {
	struct processing_context		*processing_ctx;
	struct backend_context_list		*context_list;
	struct indexing_context_list		*indexing_list;
	struct replica_mapping_context_list	*replica_mapping_list;
	struct mapistore_subscription_list	*subscriptions;
	struct mapistore_notification_list	*notifications;
	struct namedprops_context		*nprops_ctx;
	struct mapistore_connection_info	*conn_info;
	const char				*cache;
#if 0
	mqd_t					mq_ipc;
#endif
};

struct mapistore_freebusy_properties {
	uint16_t	nbr_months;
	uint32_t	*months_ranges;
	struct Binary_r	*freebusy_free;
	struct Binary_r	*freebusy_tentative;
	struct Binary_r	*freebusy_busy;
	struct Binary_r	*freebusy_away;
	struct Binary_r	*freebusy_merged;
	uint32_t	publish_start;
	uint32_t	publish_end;
	// char		*email_address;
	struct FILETIME	timestamp;
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

/* definitions from mapistore_interface.c */

/* these 2 will soon disappear */
int mapistore_getprops(struct mapistore_context *, uint32_t, TALLOC_CTX *, uint64_t, uint8_t, struct SPropTagArray *, struct SRow *);
int mapistore_setprops(struct mapistore_context *, uint32_t, uint64_t, uint8_t, struct SRow *);

struct mapistore_context *mapistore_init(TALLOC_CTX *, struct loadparm_context *, const char *);
void mapistore_set_default_indexing_url(const char *);
void mapistore_set_default_cache_url(const char *);
char *mapistore_get_default_cache_url(void);
enum mapistore_error mapistore_release(struct mapistore_context *);
enum mapistore_error mapistore_set_connection_info(struct mapistore_context *, struct ldb_context *, struct openchangedb_context *, const char *);
enum mapistore_error mapistore_add_context(struct mapistore_context *, const char *, const char *, uint64_t, uint32_t *, void **);
enum mapistore_error mapistore_add_context_ref_count(struct mapistore_context *, uint32_t);
enum mapistore_error mapistore_del_context(struct mapistore_context *, uint32_t);
enum mapistore_error mapistore_search_context_by_uri(struct mapistore_context *, const char *, uint32_t *, void **);
const char *mapistore_errstr(enum mapistore_error);

enum mapistore_error mapistore_list_contexts_for_user(struct mapistore_context *, const char *, TALLOC_CTX *, struct mapistore_contexts_list **);
enum mapistore_error mapistore_create_root_folder(const char *, enum mapistore_context_role, uint64_t, const char *, TALLOC_CTX *, char **);

enum mapistore_error mapistore_folder_open_folder(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, void **);
enum mapistore_error mapistore_folder_create_folder(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, struct SRow *, void **);
enum mapistore_error mapistore_folder_delete(struct mapistore_context *, uint32_t, void *, uint8_t, TALLOC_CTX *, uint64_t **, uint32_t *);
enum mapistore_error mapistore_folder_open_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, bool, void **);
enum mapistore_error mapistore_folder_create_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, uint8_t, void **);
enum mapistore_error mapistore_folder_delete_message(struct mapistore_context *, uint32_t, void *, uint64_t, uint8_t);
enum mapistore_error mapistore_folder_move_copy_messages(struct mapistore_context *, uint32_t, void *, void *, TALLOC_CTX *, uint32_t, uint64_t *, uint64_t *, struct Binary_r **, uint8_t);
enum mapistore_error mapistore_folder_move_folder(struct mapistore_context *, uint32_t, void *, void *, TALLOC_CTX *, const char *);
enum mapistore_error mapistore_folder_copy_folder(struct mapistore_context *, uint32_t, void *, void *, TALLOC_CTX *, bool, const char *);
enum mapistore_error mapistore_folder_get_deleted_fmids(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, enum mapistore_table_type, uint64_t, struct UI8Array_r **, uint64_t *);
enum mapistore_error mapistore_folder_get_child_count(struct mapistore_context *, uint32_t, void *, enum mapistore_table_type, uint32_t *);
enum mapistore_error mapistore_folder_get_child_fmids(struct mapistore_context *, uint32_t, void *, enum mapistore_table_type, TALLOC_CTX *, uint64_t **, uint32_t *);
enum mapistore_error mapistore_folder_get_child_fid_by_name(struct mapistore_context *, uint32_t, void *, const char *, uint64_t *);
enum mapistore_error mapistore_folder_open_table(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, enum mapistore_table_type, uint32_t, void **, uint32_t *);
enum mapistore_error mapistore_folder_modify_permissions(struct mapistore_context *, uint32_t, void *, uint8_t, uint16_t, struct PermissionData *);
enum mapistore_error mapistore_folder_preload_message_bodies(struct mapistore_context *, uint32_t, void *, enum mapistore_table_type, const struct UI8Array_r *);
enum mapistore_error mapistore_folder_fetch_freebusy_properties(struct mapistore_context *, uint32_t, void *, struct tm *, struct tm *, TALLOC_CTX *, struct mapistore_freebusy_properties **);

enum mapistore_error mapistore_message_get_message_data(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, struct mapistore_message **);
enum mapistore_error mapistore_message_modify_recipients(struct mapistore_context *, uint32_t, void *, struct SPropTagArray *, uint16_t, struct mapistore_message_recipient *);
enum mapistore_error mapistore_message_set_read_flag(struct mapistore_context *, uint32_t, void *, uint8_t);
enum mapistore_error mapistore_message_save(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *);
enum mapistore_error mapistore_message_submit(struct mapistore_context *, uint32_t, void *, enum SubmitFlags);
enum mapistore_error mapistore_message_open_attachment(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint32_t, void **);
enum mapistore_error mapistore_message_create_attachment(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, uint32_t *);
enum mapistore_error mapistore_message_get_attachment_table(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, uint32_t *);
enum mapistore_error mapistore_message_attachment_open_embedded_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, uint64_t *, struct mapistore_message **msg);
enum mapistore_error mapistore_message_attachment_create_embedded_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, struct mapistore_message **msg);

enum mapistore_error mapistore_table_get_available_properties(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, struct SPropTagArray **);
enum mapistore_error mapistore_table_set_columns(struct mapistore_context *, uint32_t, void *, uint16_t, enum MAPITAGS *);
enum mapistore_error mapistore_table_set_restrictions(struct mapistore_context *, uint32_t, void *, struct mapi_SRestriction *, uint8_t *);
enum mapistore_error mapistore_table_set_sort_order(struct mapistore_context *, uint32_t, void *, struct SSortOrderSet *, uint8_t *);
enum mapistore_error mapistore_table_get_row(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, enum mapistore_query_type, uint32_t, struct mapistore_property_data **);
enum mapistore_error mapistore_table_get_row_count(struct mapistore_context *, uint32_t, void *, enum mapistore_query_type, uint32_t *);
enum mapistore_error mapistore_table_handle_destructor(struct mapistore_context *, uint32_t, void *, uint32_t);

enum mapistore_error mapistore_properties_get_available_properties(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, struct SPropTagArray **);
enum mapistore_error mapistore_properties_get_properties(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
enum mapistore_error mapistore_properties_set_properties(struct mapistore_context *, uint32_t, void *, struct SRow *);

enum MAPISTATUS mapistore_error_to_mapi(enum mapistore_error);
enum mapistore_error mapi_error_to_mapistore(enum MAPISTATUS);

/* definitions from mapistore_processing.c */
enum mapistore_error mapistore_set_mapping_path(const char *);

/* definitions from mapistore_backend.c */
enum mapistore_error mapistore_backend_register(const void *);
const char	*mapistore_backend_get_installdir(void);
init_backend_fn	*mapistore_backend_load(TALLOC_CTX *, const char *);
struct backend_context *mapistore_backend_lookup(struct backend_context_list *, uint32_t);
struct backend_context *mapistore_backend_lookup_by_uri(struct backend_context_list *, const char *);
struct backend_context *mapistore_backend_lookup_by_name(TALLOC_CTX *, const char *);
bool		mapistore_backend_run_init(init_backend_fn *);

/* definitions from mapistore_backend_defaults */
enum mapistore_error mapistore_backend_init_defaults(struct mapistore_backend *);

/* definitions from mapistore_indexing.c */
enum mapistore_error mapistore_indexing_record_add_fid(struct mapistore_context *, uint32_t, const char *, uint64_t);
enum mapistore_error mapistore_indexing_record_del_fid(struct mapistore_context *, uint32_t, const char *, uint64_t, uint8_t);
enum mapistore_error mapistore_indexing_record_add_mid(struct mapistore_context *, uint32_t, const char *, uint64_t);
enum mapistore_error mapistore_indexing_record_del_mid(struct mapistore_context *, uint32_t, const char *, uint64_t, uint8_t);
enum mapistore_error mapistore_indexing_record_add_fmid_for_uri(struct mapistore_context *, uint32_t, const char *, uint64_t, const char *);
enum mapistore_error mapistore_indexing_record_get_uri(struct mapistore_context *, const char *, TALLOC_CTX *, uint64_t, char **, bool *);
enum mapistore_error mapistore_indexing_record_get_fmid(struct mapistore_context *, const char *, const char *, bool, uint64_t *, bool *);

enum mapistore_error mapistore_indexing_get_new_folderID(struct mapistore_context *, uint64_t *);
enum mapistore_error mapistore_indexing_get_new_folderID_as_user(struct mapistore_context *, const char *, uint64_t *);
enum mapistore_error mapistore_indexing_get_new_folderIDs(struct mapistore_context *, TALLOC_CTX *, uint64_t, struct UI8Array_r **);
enum mapistore_error mapistore_indexing_reserve_fmid_range(struct mapistore_context *, uint64_t, uint64_t *);

/* definitions from mapistore_replica_mapping.c */
enum mapistore_error mapistore_replica_mapping_add(struct mapistore_context *, const char *, struct replica_mapping_context_list **);
enum mapistore_error mapistore_replica_mapping_guid_to_replid(struct mapistore_context *, const char *username, const struct GUID *, uint16_t *);
enum mapistore_error mapistore_replica_mapping_replid_to_guid(struct mapistore_context *, const char *username, uint16_t, struct GUID *);

struct namedprops_context;

/* definitions from mapistore_namedprops.c */
enum mapistore_error mapistore_namedprops_get_mapped_id(struct namedprops_context *, struct MAPINAMEID, uint16_t *);
enum mapistore_error mapistore_namedprops_next_unused_id(struct namedprops_context *, uint16_t *);
enum mapistore_error mapistore_namedprops_create_id(struct namedprops_context *, struct MAPINAMEID, uint16_t);
enum mapistore_error mapistore_namedprops_get_nameid(struct namedprops_context *, uint16_t, TALLOC_CTX *mem_ctx, struct MAPINAMEID **);
enum mapistore_error mapistore_namedprops_get_nameid_type(struct namedprops_context *, uint16_t, uint16_t *);
enum mapistore_error mapistore_namedprops_transaction_start(struct namedprops_context *);
enum mapistore_error mapistore_namedprops_transaction_commit(struct namedprops_context *);

/* definitions from mapistore_mgmt.c */
#if 0
enum mapistore_error mapistore_mgmt_backend_register_user(struct mapistore_connection_info *, const char *, const char *);
enum mapistore_error mapistore_mgmt_backend_unregister_user(struct mapistore_connection_info *, const char *, const char *);
enum mapistore_error mapistore_mgmt_interface_register_subscription(struct mapistore_connection_info *, struct mapistore_mgmt_notif *);
enum mapistore_error mapistore_mgmt_interface_unregister_subscription(struct mapistore_connection_info *, struct mapistore_mgmt_notif *);
enum mapistore_error mapistore_mgmt_interface_register_bind(struct mapistore_connection_info *, uint16_t, uint8_t *, uint16_t, uint8_t *);
#endif

/* definitions from mapistore_notifications.c (proof-of-concept) */

/* notifications subscriptions */
struct mapistore_subscription_list {
	struct mapistore_subscription *subscription;
	struct mapistore_subscription_list *next;
	struct mapistore_subscription_list *prev;
};

struct mapistore_table_subscription_parameters {
	uint8_t table_type;
	uint64_t folder_id; /* the parent folder id */
};

struct mapistore_object_subscription_parameters {
	bool whole_store;
	uint64_t folder_id;
	uint64_t object_id;
};

struct mapistore_subscription {
	uint32_t        handle;
	uint16_t        notification_types;
	union {
		struct mapistore_table_subscription_parameters table_parameters;
		struct mapistore_object_subscription_parameters object_parameters;
	} parameters;
#if 0
	char		*mqueue_name;
	mqd_t		mqueue;
#endif
};

struct mapistore_subscription *mapistore_new_subscription(TALLOC_CTX *, struct mapistore_context *, const char *, uint32_t, uint16_t, void *);

/* notifications (implementation) */

struct mapistore_notification_list {
	struct mapistore_notification *notification;
	struct mapistore_notification_list *next;
	struct mapistore_notification_list *prev;
};

enum mapistore_notification_type {
	MAPISTORE_OBJECT_CREATED = 1,
	MAPISTORE_OBJECT_MODIFIED = 2,
	MAPISTORE_OBJECT_DELETED = 3,
	MAPISTORE_OBJECT_COPIED = 4,
	MAPISTORE_OBJECT_MOVED = 5,
	MAPISTORE_OBJECT_NEWMAIL = 6
};

struct mapistore_table_notification_parameters {
	uint8_t table_type;
	uint32_t row_id;

	uint32_t handle;
	uint64_t folder_id; /* the parent folder id */
	uint64_t object_id; /* the folder/message id */
	uint32_t instance_id;
};

struct mapistore_object_notification_parameters {
	uint64_t folder_id;      /* the parent folder id */
	uint64_t object_id;      /* the folder/message id */
        uint64_t old_folder_id;  /* used for copy/move notifications */
        uint64_t old_object_id;  /* used for copy/move notifications */
	uint16_t tag_count;
	enum MAPITAGS *tags;
	bool new_message_count;
	uint32_t message_count;
};

struct mapistore_notification {
	uint32_t object_type;
	enum mapistore_notification_type event;
	union {
		struct mapistore_table_notification_parameters table_parameters;
		struct mapistore_object_notification_parameters object_parameters;
	} parameters;
};

struct mapistore_context;

struct mapistore_subscription_list *mapistore_find_matching_subscriptions(struct mapistore_context *, struct mapistore_notification *);
enum mapistore_error mapistore_delete_subscription(struct mapistore_context *, uint32_t, uint16_t);
void mapistore_push_notification(struct mapistore_context *, uint8_t, enum mapistore_notification_type, void *);
enum mapistore_error mapistore_get_queued_notifications(struct mapistore_context *, struct mapistore_subscription *, struct mapistore_notification_list **);
enum mapistore_error mapistore_get_queued_notifications_named(struct mapistore_context *, const char *, struct mapistore_notification_list **);

__END_DECLS

#endif	/* ! __MAPISTORE_H */
