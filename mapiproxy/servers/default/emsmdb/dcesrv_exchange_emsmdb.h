/*
   MAPI Proxy - Exchange EMSMDB Server

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

#ifndef	__DCESRV_EXCHANGE_EMSMDB_H
#define	__DCESRV_EXCHANGE_EMSMDB_H

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include <ldb.h>
#include <ldb_errors.h>
#include <tevent.h>
#include <util/debug.h>
#include <time.h>
#include <inttypes.h>

#ifndef	__BEGIN_DECLS
#ifdef	__cplusplus
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

struct emsmdbp_context {
	char					*szUserDN;
	char					*szDisplayName;
	uint32_t				userLanguage;
	char					*username;
	struct loadparm_context			*lp_ctx;
	struct ldb_context			*oc_ctx;
	struct ldb_context			*samdb_ctx;
	struct mapistore_context		*mstore_ctx;
	struct mapi_handles_context		*handles_ctx;

	TALLOC_CTX				*mem_ctx;
};

struct exchange_emsmdb_session {
	uint32_t			pullTimeStamp;
	struct mpm_session		*session;
        struct GUID                     uuid;
	struct exchange_emsmdb_session	*prev;
	struct exchange_emsmdb_session	*next;
};

struct emsmdbp_stream {
	size_t			position;
	DATA_BLOB		buffer;
};

struct emsmdbp_syncconfigure_request {
	bool is_collector;
	bool contents_mode;

	bool unicode;
	bool use_cpid;
	bool recover_mode;
	bool force_unicode;

	bool no_deletions;
	bool no_soft_deletions;
	bool no_foreign_identifiers;

	bool request_eid;

	/* contents only */
	bool partial_item;
	bool fai;
	bool normal;
	bool ignored_specified_on_fai;
	bool best_body;
	bool ignore_no_longer_in_scope;
	bool read_state;
	bool progress;

	bool request_message_size;
	bool request_cn;
	bool order_by_delivery_time;

	DATA_BLOB restriction;
};

enum emsmdbp_object_type {
	EMSMDBP_OBJECT_UNDEF		= 0x0,
	EMSMDBP_OBJECT_MAILBOX		= 0x1,
	EMSMDBP_OBJECT_FOLDER		= 0x2,
	EMSMDBP_OBJECT_MESSAGE		= 0x3,
	EMSMDBP_OBJECT_TABLE		= 0x4,
	EMSMDBP_OBJECT_STREAM		= 0x5,
	EMSMDBP_OBJECT_ATTACHMENT	= 0x6,
        EMSMDBP_OBJECT_SUBSCRIPTION     = 0x7,
	EMSMDBP_OBJECT_FTCONTEXT	= 0x8, /* Fast Transfer */
	EMSMDBP_OBJECT_SYNCCONTEXT	= 0x9
};

struct emsmdbp_object_mailbox {
	uint64_t			folderID;
	char				*owner_Name;
	char				*owner_EssDN;
	char				*szUserDN;
	char				*owner_username;
	bool				mailboxstore;
};

struct emsmdbp_object_folder {
	uint64_t			folderID;
	uint32_t			contextID; /* requires mapistore_root == true, undefined otherwise */
	bool				mapistore_root; /* root mapistore container or not */
	struct SRow			*postponed_props; /* storage for properties set until PR_CONTAINER_CLASS_UNICODE is set */
};

struct emsmdbp_object_message {
	uint64_t				folderID;
	uint64_t				messageID;
	bool					read_write;
	struct mapistore_freebusy_properties	*fb_properties;
};

struct emsmdbp_object_table {
	enum mapistore_table_type		ulType;
	uint32_t				handle;
	bool					restricted;
	uint16_t				prop_count;
	enum MAPITAGS				*properties;
	uint32_t				numerator;
	uint32_t				denominator;
        struct mapistore_subscription_list	*subscription_list;
};

struct emsmdbp_object_stream {
	bool				read_write;
	bool				needs_commit;
	enum MAPITAGS			property;
	struct emsmdbp_stream		stream;
};

struct emsmdbp_stream_data {
	enum MAPITAGS			prop_tag;
	DATA_BLOB			data;
	struct emsmdbp_stream_data	*next;
	struct emsmdbp_stream_data	*prev;
};

struct emsmdbp_object_attachment {
	uint32_t			attachmentID;
};

struct emsmdbp_object_subscription {
	uint32_t				handle;
        struct mapistore_subscription_list	*subscription_list;
};

struct emsmdbp_object_synccontext {
	struct emsmdbp_syncconfigure_request	request;

	/* debugging */
	unsigned int sent_objects;
	unsigned int skipped_objects;
	unsigned int total_objects;
	struct timeval request_start;

	/* uploaded synchronization state */
	struct idset		*idset_given;
	struct idset		*cnset_seen;
	struct idset		*cnset_seen_fai;
	struct idset		*cnset_read;

	struct SPropTagArray	properties;
	struct SPropTagArray	fai_properties;

	/* sync state upload */
	enum StateProperty	state_property;
	struct emsmdbp_stream	state_stream;

	/* data download */
	uint16_t		sync_stage;
	void			*sync_data;
	uint16_t		steps;
	uint16_t		total_steps;

	/* download buffers */
	struct emsmdbp_stream	stream;
	uint32_t		*cutmarks;
	uint32_t		next_cutmark_idx;
};

struct emsmdbp_object_ftcontext {
	uint16_t		steps;
	uint16_t		total_steps;

	struct emsmdbp_stream	stream;
	uint32_t		*cutmarks;
	uint32_t		next_cutmark_idx;
};

union emsmdbp_objects {
	struct emsmdbp_object_mailbox	*mailbox;
	struct emsmdbp_object_folder	*folder;
	struct emsmdbp_object_message	*message;
	struct emsmdbp_object_table	*table;
	struct emsmdbp_object_stream	*stream;
	struct emsmdbp_object_attachment *attachment;
	struct emsmdbp_object_subscription *subscription;
	struct emsmdbp_object_synccontext *synccontext;
	struct emsmdbp_object_ftcontext *ftcontext;
};

struct emsmdbp_object {
	struct emsmdbp_object		*parent_object;
	enum emsmdbp_object_type	type;
	union emsmdbp_objects		object;
	struct emsmdbp_context		*emsmdbp_ctx;
        void                            *backend_object;  /* used with mapistore */
	struct emsmdbp_stream_data      *stream_data;
};

#define	EMSMDB_PCMSPOLLMAX		60000
#define	EMSMDB_PCRETRY			6
#define	EMSMDB_PCRETRYDELAY		10000

enum emsmdbp_mailbox_systemidx {
	EMSMDBP_MAILBOX_ROOT = 1,
	EMSMDBP_DEFERRED_ACTION,
	EMSMDBP_SPOOLER_QUEUE,
	EMSMDBP_COMMON_VIEWS,
	EMSMDBP_SCHEDULE,
	EMSMDBP_SEARCH,
	EMSMDBP_VIEWS,
	EMSMDBP_SHORTCUTS,

	/* search folders */
	EMSMDBP_REMINDERS,
	EMSMDBP_TODO,
	EMSMDBP_TRACKEDMAILPROCESSING,

	/* user-visible folders */
	EMSMDBP_TOP_INFORMATION_STORE,
	EMSMDBP_INBOX,
	EMSMDBP_OUTBOX,
	EMSMDBP_SENT_ITEMS,
	EMSMDBP_DELETED_ITEMS,

	EMSMDBP_MAX_MAILBOX_SYSTEMIDX
};

enum emsmdbp_pf_systemidx {
	EMSMDBP_PF_ROOT = 1,
	EMSMDBP_PF_IPMSUBTREE,
	EMSMDBP_PF_NONIPMSUBTREE,
	EMSMDBP_PF_EFORMSREGISTRY,
	EMSMDBP_PF_FREEBUSY,
	EMSMDBP_PF_OAB,
	EMSMDBP_PF_LOCALEFORMS,
	EMSMDBP_PF_LOCALFREEBUSY,
	EMSMDBP_PF_LOCALOAB,

	EMSMDBP_MAX_PF_SYSTEMIDX
};

struct emsmdbp_special_folder {
	enum mapistore_context_role	role;
	enum MAPITAGS			entryid_property;
	const char			*name;
};

__BEGIN_DECLS

NTSTATUS	samba_init_module(void);
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *, int);

/* definitions from emsmdbp.c */
struct emsmdbp_context	*emsmdbp_init(struct loadparm_context *, const char *, void *);
void			*emsmdbp_openchange_ldb_init(struct loadparm_context *);
bool			emsmdbp_destructor(void *);
bool			emsmdbp_verify_user(struct dcesrv_call_state *, struct emsmdbp_context *);
bool			emsmdbp_verify_userdn(struct dcesrv_call_state *, struct emsmdbp_context *, const char *, struct ldb_message **);
enum MAPISTATUS		emsmdbp_resolve_recipient(TALLOC_CTX *, struct emsmdbp_context *, char *, struct mapi_SPropTagArray *, struct RecipientRow *);

const struct GUID *const	MagicGUIDp;
int				emsmdbp_guid_to_replid(struct emsmdbp_context *, const char *username, const struct GUID *, uint16_t *);
int				emsmdbp_replid_to_guid(struct emsmdbp_context *, const char *username, const uint16_t, struct GUID *);
int				emsmdbp_source_key_from_fmid(TALLOC_CTX *, struct emsmdbp_context *, const char *username, uint64_t, struct Binary_r **);

/* definitions from emsmdbp_object.c */
const char	      *emsmdbp_getstr_type(struct emsmdbp_object *);
bool		      emsmdbp_is_mapistore(struct emsmdbp_object *);
bool		      emsmdbp_is_mailboxstore(struct emsmdbp_object *);
char                  *emsmdbp_get_owner(struct emsmdbp_object *object);

int		      emsmdbp_get_uri_from_fid(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, char **);
int		      emsmdbp_get_fid_from_uri(struct emsmdbp_context *, const char *, uint64_t *);
uint32_t	      emsmdbp_get_contextID(struct emsmdbp_object *);

/* definitions from emsmdbp_privisioning.c */
enum MAPISTATUS       emsmdbp_mailbox_provision(struct emsmdbp_context *, const char *);
enum MAPISTATUS       emsmdbp_mailbox_provision_public_freebusy(struct emsmdbp_context *, const char *);

/* With emsmdbp_object_create_folder and emsmdbp_object_open_folder, the parent object IS the direct parent */
enum mapistore_error  emsmdbp_object_get_fid_by_name(struct emsmdbp_context *, struct emsmdbp_object *, const char *, uint64_t *);
enum MAPISTATUS       emsmdbp_object_create_folder(struct emsmdbp_context *, struct emsmdbp_object *, TALLOC_CTX *, uint64_t, struct SRow *, struct emsmdbp_object **);
enum mapistore_error  emsmdbp_object_open_folder(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, uint64_t, struct emsmdbp_object **);
enum mapistore_error  emsmdbp_object_open_folder_by_fid(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, uint64_t, struct emsmdbp_object **);

struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *parent_object);
int emsmdbp_object_copy_properties(struct emsmdbp_context *, struct emsmdbp_object *, struct emsmdbp_object *, struct SPropTagArray *, bool);
struct emsmdbp_object *emsmdbp_object_mailbox_init(TALLOC_CTX *, struct emsmdbp_context *, const char *, bool);
struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, struct emsmdbp_object *);
int emsmdbp_folder_get_folder_count(struct emsmdbp_context *, struct emsmdbp_object *, uint32_t *);
enum mapistore_error emsmdbp_folder_delete(struct emsmdbp_context *, struct emsmdbp_object *, uint64_t, uint8_t);
enum mapistore_error emsmdbp_folder_move_folder(struct emsmdbp_context *, struct emsmdbp_object *, struct emsmdbp_object *, TALLOC_CTX *, const char *);
struct emsmdbp_object *emsmdbp_folder_open_table(TALLOC_CTX *, struct emsmdbp_object *, uint32_t, uint32_t);
struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *);
int emsmdbp_object_table_get_available_properties(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, struct SPropTagArray **);
void **emsmdbp_object_table_get_row_props(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, uint32_t, enum mapistore_query_type, enum MAPISTATUS **);
struct emsmdbp_object *emsmdbp_object_message_init(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, struct emsmdbp_object *);
enum mapistore_error emsmdbp_object_message_open(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, uint64_t, uint64_t, bool, struct emsmdbp_object **, struct mapistore_message **);
struct emsmdbp_object *emsmdbp_object_message_open_attachment_table(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *);
struct emsmdbp_object *emsmdbp_object_stream_init(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *);
int emsmdbp_object_stream_commit(struct emsmdbp_object *);
struct emsmdbp_object *emsmdbp_object_attachment_init(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, struct emsmdbp_object *);
struct emsmdbp_object *emsmdbp_object_subscription_init(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *);
int emsmdbp_object_get_available_properties(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, struct SPropTagArray **);
int emsmdbp_object_set_properties(struct emsmdbp_context *, struct emsmdbp_object *, struct SRow *);
void **emsmdbp_object_get_properties(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *, struct SPropTagArray *, enum MAPISTATUS **);
struct emsmdbp_object *emsmdbp_object_synccontext_init(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *);
struct emsmdbp_object *emsmdbp_object_ftcontext_init(TALLOC_CTX *, struct emsmdbp_context *, struct emsmdbp_object *);
struct emsmdbp_stream_data *emsmdbp_stream_data_from_value(TALLOC_CTX *, enum MAPITAGS, void *value, bool);
struct emsmdbp_stream_data *emsmdbp_object_get_stream_data(struct emsmdbp_object *, enum MAPITAGS);
DATA_BLOB emsmdbp_stream_read_buffer(struct emsmdbp_stream *, uint32_t);
void emsmdbp_stream_write_buffer(TALLOC_CTX *, struct emsmdbp_stream *, DATA_BLOB);
void emsmdbp_fill_table_row_blob(TALLOC_CTX *, struct emsmdbp_context *, DATA_BLOB *, uint16_t, enum MAPITAGS *, void **, enum MAPISTATUS *);
void emsmdbp_fill_row_blob(TALLOC_CTX *, struct emsmdbp_context *, uint8_t *, DATA_BLOB *,struct SPropTagArray *, void **, enum MAPISTATUS *, bool *);

/* definitions from oxcfold.c */
enum MAPISTATUS EcDoRpc_RopOpenFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetHierarchyTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetContentsTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopCreateFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopDeleteFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopDeleteMessages(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetSearchCriteria(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetSearchCriteria(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopEmptyFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopMoveCopyMessages(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopMoveFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopCopyFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);


/* definitions from oxcmsg.c */
enum MAPISTATUS EcDoRpc_RopOpenMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopCreateMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSaveChangesMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopRemoveAllRecipients(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopModifyRecipients(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopReloadCachedInformation(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetMessageReadFlag(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetAttachmentTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopOpenAttach(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopCreateAttach(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSaveChangesAttachment(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopOpenEmbeddedMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcnotif.c */
enum MAPISTATUS EcDoRpc_RopRegisterNotification(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcprpt.c */
enum MAPISTATUS EcDoRpc_RopGetPropertiesSpecific(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetPropertiesAll(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetPropertiesList(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetProperties(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopDeleteProperties(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopOpenStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopReadStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopWriteStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopCommitStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetStreamSize(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSeekStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetStreamSize(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetNamesFromIDs(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetPropertyIdsFromNames(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopDeletePropertiesNoReplicate(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopCopyTo(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcstor.c */
enum MAPISTATUS	EcDoRpc_RopLogon(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopRelease(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetReceiveFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopSetReceiveFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetPerUserLongTermIds(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetPerUserGuid(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetStoreState(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopReadPerUserInformation(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopLongTermIdFromId(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopIdFromLongTermId(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definition from oxctabl.c */
enum MAPISTATUS EcDoRpc_RopSetColumns(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSortTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopRestrict(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopQueryRows(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopQueryPosition(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSeekRow(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopFindRow(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopResetTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definition from oxomsg.c */
enum MAPISTATUS	EcDoRpc_RopSubmitMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopSetSpooler(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetAddressTypes(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopTransportSend(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetTransportFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopOptionsData(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxorule.c */
enum MAPISTATUS EcDoRpc_RopGetRulesTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopModifyRules(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcperm.c */
enum MAPISTATUS EcDoRpc_RopGetPermissionsTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopModifyPermissions(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcfxics.c */
enum MAPISTATUS EcDoRpc_RopFastTransferSourceCopyTo(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopFastTransferSourceGetBuffer(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncConfigure(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncImportMessageChange(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncImportHierarchyChange(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncImportDeletes(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamBegin(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamContinue(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncUploadStateStreamEnd(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncImportMessageMove(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncOpenCollector(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetLocalReplicaIds(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncImportReadStateChanges(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSyncGetTransferState(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetLocalReplicaMidsetDeleted(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

__END_DECLS

#endif	/* __DCESRV_EXCHANGE_EMSMDB_H */
