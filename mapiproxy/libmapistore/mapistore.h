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

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE
#endif

#ifndef	_PUBLIC_
#define	_PUBLIC_
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <mqueue.h>

#include <tdb.h>
#include <ldb.h>
#include <talloc.h>
#include <util/debug.h>

#include "libmapi/libmapi.h"

#define	MAPISTORE_SUCCESS	0

typedef	int (*init_backend_fn) (void);

#define	MAPISTORE_INIT_MODULE	"mapistore_init_backend"

#define	MAPISTORE_FOLDER_TABLE  	1
#define	MAPISTORE_MESSAGE_TABLE 	2
#define	MAPISTORE_FAI_TABLE             3
#define	MAPISTORE_RULE_TABLE            4
#define	MAPISTORE_ATTACHMENT_TABLE      5
#define	MAPISTORE_PERMISSIONS_TABLE	6

#define MAPISTORE_FOLDER        1
#define	MAPISTORE_MESSAGE	2
#define	MAPISTORE_ATTACHMENT	3
#define	MAPISTORE_TABLE 	4

#define	MAPISTORE_SOFT_DELETE		1
#define	MAPISTORE_PERMANENT_DELETE	2

struct mapistore_message {
	/* message props */
	char					*subject_prefix;
	char					*normalized_subject;

	/* recipients */
	struct SPropTagArray			*columns;
	uint32_t				recipients_count;
	struct mapistore_message_recipient	**recipients;
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

enum table_query_type {
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
	void				*oc_ctx;
};

struct tdb_wrap;

/* notes:
   openfolder takes the folderid alone as argument
   openmessage takes the message id and its parent folderid as arguments  */

struct mapistore_backend {
	/** backend operations */
	struct {
		const char	*name;
		const char	*description;
		const char	*namespace;

		int		(*init)(void);
		int		(*create_context)(TALLOC_CTX *, struct mapistore_connection_info *, struct tdb_wrap *, const char *, void **);
	} backend;

	/** context operations */
	struct {
		int		(*get_path)(void *, TALLOC_CTX *, uint64_t, char **);
		int		(*get_root_folder)(void *, TALLOC_CTX *, uint64_t, void **);
	} context;

        /** oxcfold operations */
        struct {
		int		(*open_folder)(void *, TALLOC_CTX *, uint64_t, void **);
		int		(*create_folder)(void *, TALLOC_CTX *, uint64_t, struct SRow *, void **);
		int		(*delete_folder)(void *, uint64_t);
		int		(*open_message)(void *, TALLOC_CTX *, uint64_t, void **, struct mapistore_message **);
		int		(*create_message)(void *, TALLOC_CTX *, uint64_t, uint8_t, void **);
		int		(*delete_message)(void *, uint64_t, uint8_t flags);
	        int		(*move_copy_messages)(void *, void *, uint32_t, uint64_t *, uint64_t *, struct Binary_r **, uint8_t);
		int		(*get_deleted_fmids)(void *, TALLOC_CTX *, uint8_t, uint64_t, struct I8Array_r **, uint64_t *);
		int		(*get_child_count)(void *, uint8_t, uint32_t *);

		/* constructor: open_folder */
                int		(*open_table)(void *, TALLOC_CTX *, uint8_t, uint32_t, void **, uint32_t *);
        } folder;

        /** oxcmsg operations */
        struct {
		int		(*modify_recipients)(void *, struct SPropTagArray *, struct ModifyRecipientRow *, uint16_t);
		int		(*save)(void *);
		int		(*submit)(void *, enum SubmitFlags);
                int		(*open_attachment)(void *, TALLOC_CTX *, uint32_t, void **);
                int		(*create_attachment)(void *, TALLOC_CTX *, void **, uint32_t *);
                int		(*get_attachment_table)(void *, TALLOC_CTX *, void **, uint32_t *);

		/* attachments */
                int		(*open_embedded_message)(void *, TALLOC_CTX *, void **, uint64_t *, struct mapistore_message **);
        } message;

        /** oxctabl operations */
        struct {
                int		(*get_available_properties)(void *, TALLOC_CTX *, struct SPropTagArray **);
                int		(*set_columns)(void *, uint16_t, enum MAPITAGS *);
                int		(*set_restrictions)(void *, struct mapi_SRestriction *, uint8_t *);
                int		(*set_sort_order)(void *, struct SSortOrderSet *, uint8_t *);
                int		(*get_row)(void *, TALLOC_CTX *, enum table_query_type, uint32_t, struct mapistore_property_data **);
                int		(*get_row_count)(void *, enum table_query_type, uint32_t *);
		int		(*handle_destructor)(void *, uint32_t);
        } table;

        /** oxcprpt operations */
        struct {
                int		(*get_available_properties)(void *, TALLOC_CTX *, struct SPropTagArray **);
                int		(*get_properties)(void *, TALLOC_CTX *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
                int		(*set_properties)(void *, struct SRow *);
        } properties;

	/** manager operations */
	struct {
		int		(*generate_uri)(TALLOC_CTX *, const char *, const char *, const char *, const char *, char **);
	} manager;
};

struct indexing_context_list;

struct backend_context {
	const struct mapistore_backend	*backend;
	void				*backend_object;
	void				*root_folder_object;
	struct indexing_context_list	*indexing;
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
	struct mapistore_subscription_list	*subscriptions;
	struct mapistore_notification_list	*notifications;
	struct tdb_wrap				*replica_mapping_ctx;
	void					*nprops_ctx;
	struct mapistore_connection_info	*conn_info;
	mqd_t					mq_users;
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

struct mapistore_context *mapistore_init(TALLOC_CTX *, const char *);
int mapistore_release(struct mapistore_context *);
int mapistore_set_connection_info(struct mapistore_context *, void *, const char *);
int mapistore_add_context(struct mapistore_context *, const char *, const char *, uint64_t, uint32_t *, void **);
int mapistore_add_context_ref_count(struct mapistore_context *, uint32_t);
int mapistore_del_context(struct mapistore_context *, uint32_t);
int mapistore_search_context_by_uri(struct mapistore_context *, const char *, uint32_t *, void **);
const char *mapistore_errstr(int);

int mapistore_folder_open_folder(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, void **);
int mapistore_folder_create_folder(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, struct SRow *, void **);
int mapistore_folder_delete_folder(struct mapistore_context *, uint32_t, void *, uint64_t, uint8_t);
int mapistore_folder_open_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, void **, struct mapistore_message **);
int mapistore_folder_create_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t, uint8_t, void **);
int mapistore_folder_delete_message(struct mapistore_context *, uint32_t, void *, uint64_t, uint8_t);
int mapistore_folder_move_copy_messages(struct mapistore_context *, uint32_t, void *, void *, uint32_t, uint64_t *, uint64_t *, struct Binary_r **, uint8_t);
int mapistore_folder_get_deleted_fmids(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint8_t, uint64_t, struct I8Array_r **, uint64_t *);
int mapistore_folder_get_folder_count(struct mapistore_context *, uint32_t, void *, uint32_t *);
int mapistore_folder_get_message_count(struct mapistore_context *, uint32_t, void *, uint8_t, uint32_t *);
int mapistore_folder_get_child_fids(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint64_t **, uint32_t *);
int mapistore_folder_get_child_fid_by_name(struct mapistore_context *, uint32_t, void *, const char *, uint64_t *);
int mapistore_folder_open_table(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint8_t, uint32_t, void **, uint32_t *);

int mapistore_message_modify_recipients(struct mapistore_context *, uint32_t, void *, struct SPropTagArray *, struct ModifyRecipientRow *, uint16_t);
int mapistore_message_save(struct mapistore_context *, uint32_t, void *);
int mapistore_message_submit(struct mapistore_context *, uint32_t, void *, enum SubmitFlags);
int mapistore_message_open_attachment(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint32_t, void **);
int mapistore_message_create_attachment(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, uint32_t *);
int mapistore_message_get_attachment_table(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, uint32_t *);
int mapistore_message_attachment_open_embedded_message(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, void **, uint64_t *, struct mapistore_message **msg);

int mapistore_table_get_available_properties(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, struct SPropTagArray **);
int mapistore_table_set_columns(struct mapistore_context *, uint32_t, void *, uint16_t, enum MAPITAGS *);
int mapistore_table_set_restrictions(struct mapistore_context *, uint32_t, void *, struct mapi_SRestriction *, uint8_t *);
int mapistore_table_set_sort_order(struct mapistore_context *, uint32_t, void *, struct SSortOrderSet *, uint8_t *);
int mapistore_table_get_row(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, enum table_query_type, uint32_t, struct mapistore_property_data **);
int mapistore_table_get_row_count(struct mapistore_context *, uint32_t, void *, enum table_query_type, uint32_t *);
int mapistore_table_handle_destructor(struct mapistore_context *, uint32_t, void *, uint32_t);

int mapistore_properties_get_available_properties(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, struct SPropTagArray **);
int mapistore_properties_get_properties(struct mapistore_context *, uint32_t, void *, TALLOC_CTX *, uint16_t, enum MAPITAGS *, struct mapistore_property_data *);
int mapistore_properties_set_properties(struct mapistore_context *, uint32_t, void *, struct SRow *);

/* definitions from mapistore_processing.c */
int mapistore_set_mapping_path(const char *);

/* definitions from mapistore_backend.c */
extern int	mapistore_backend_register(const void *);
const char	*mapistore_backend_get_installdir(void);
init_backend_fn	*mapistore_backend_load(TALLOC_CTX *, const char *);
struct backend_context *mapistore_backend_lookup(struct backend_context_list *, uint32_t);
struct backend_context *mapistore_backend_lookup_by_uri(struct backend_context_list *, const char *);
struct backend_context *mapistore_backend_lookup_by_name(TALLOC_CTX *, const char *);
bool		mapistore_backend_run_init(init_backend_fn *);

/* definitions from mapistore_indexing.c */
int mapistore_indexing_record_add_fid(struct mapistore_context *, uint32_t, uint64_t);
int mapistore_indexing_record_del_fid(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
int mapistore_indexing_record_add_mid(struct mapistore_context *, uint32_t, uint64_t);
int mapistore_indexing_record_del_mid(struct mapistore_context *, uint32_t, uint64_t, uint8_t);
int mapistore_indexing_record_get_uri(struct mapistore_context *, const char *, TALLOC_CTX *, uint64_t, char **, bool *);
int mapistore_indexing_record_get_fmid(struct mapistore_context *, const char *, const char *, bool, uint64_t *, bool *);

/* definitions from mapistore_replica_mapping.c */
_PUBLIC_ int mapistore_replica_mapping_add(struct mapistore_context *, const char *);
_PUBLIC_ int mapistore_replica_mapping_guid_to_replid(struct mapistore_context *, const struct GUID *, uint16_t *);
_PUBLIC_ int mapistore_replica_mapping_replid_to_guid(struct mapistore_context *, uint16_t, struct GUID *);

/* definitions from mapistore_namedprops.c */
int mapistore_namedprops_get_mapped_id(void *ldb_ctx, struct MAPINAMEID, uint16_t *);
int mapistore_namedprops_get_nameid(void *, uint16_t, struct MAPINAMEID **);

/* definitions from mapistore_mgmt.c */
int mapistore_mgmt_backend_register_user(struct mapistore_connection_info *, const char *, const char *);
int mapistore_mgmt_backend_unregister_user(struct mapistore_connection_info *, const char *, const char *);

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
};

struct mapistore_subscription *mapistore_new_subscription(TALLOC_CTX *, uint32_t, uint16_t, void *);

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
	MAPISTORE_OBJECT_MOVED = 5
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

struct mapistore_subscription_list *mapistore_find_matching_subscriptions(struct mapistore_context *, struct mapistore_notification *);
void mapistore_push_notification(struct mapistore_context *, uint8_t, enum mapistore_notification_type, void *);

__END_DECLS

#endif	/* ! __MAPISTORE_H */
