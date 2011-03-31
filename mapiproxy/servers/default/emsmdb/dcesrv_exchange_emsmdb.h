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
#include <util/debug.h>
#include <time.h>

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
	char				*szUserDN;
	char				*szDisplayName;
	uint32_t			userLanguage;
	char				*username;
	struct loadparm_context		*lp_ctx;
	void				*oc_ctx;
	struct ldb_context		*samdb_ctx;
	struct mapistore_context	*mstore_ctx;
	struct mapi_handles_context	*handles_ctx;

	TALLOC_CTX			*mem_ctx;
};

struct exchange_emsmdb_session {
	uint32_t			pullTimeStamp;
	struct mpm_session		*session;
        struct GUID                     uuid;
	struct exchange_emsmdb_session	*prev;
	struct exchange_emsmdb_session	*next;
};

enum emsmdbp_object_type {
	EMSMDBP_OBJECT_UNDEF		= 0x0,
	EMSMDBP_OBJECT_MAILBOX		= 0x1,
	EMSMDBP_OBJECT_FOLDER		= 0x2,
	EMSMDBP_OBJECT_MESSAGE		= 0x3,
	EMSMDBP_OBJECT_TABLE		= 0x4,
	EMSMDBP_OBJECT_STREAM		= 0x5,
	EMSMDBP_OBJECT_ATTACHMENT	= 0x6,
        EMSMDBP_OBJECT_SUBSCRIPTION     = 0x7
};

struct emsmdbp_object_mailbox {
	uint64_t			folderID;
	char				*owner_Name;
	char				*owner_EssDN;
	char				*szUserDN;
	bool				mailboxstore;
};


struct emsmdbp_object_folder {
	uint64_t			folderID;
	uint32_t			contextID;
	bool				mapistore;
	bool				mapistore_root; /* root mapistore container or not */
	bool				mailboxstore;
};

struct emsmdbp_object_message {
	uint64_t			folderID;
	uint64_t			messageID;
	uint32_t			contextID;
	bool				mapistore;
};

struct emsmdbp_object_table {
	uint64_t			folderID;
	uint8_t				ulType;
	uint32_t			contextID;
	bool				IsSystemTable;
	uint16_t			prop_count;
	uint32_t			*properties;
	uint32_t			numerator;
	uint32_t			denominator;
	bool				mapistore;
        struct mapistore_subscription_list   *subscription_list;
};

struct emsmdbp_object_stream {
	uint32_t			property;
	uint32_t			contextID;
	uint64_t			objectID;
	uint8_t				objectType;
	uint32_t			flags;
	int				fd;
	bool				mapistore;
        bool                            parent_poc_api;
        void                            *parent_poc_backend_object;
};

struct emsmdbp_object_attachment {
	uint64_t			messageID;
	uint32_t			attachmentID;
	uint32_t			contextID;
	bool				mapistore;
};

struct emsmdbp_object_subscription {
	uint32_t			handle;
	uint32_t			contextID;
        struct mapistore_subscription_list *subscription_list;
	bool				mapistore;
};

union emsmdbp_objects {
	struct emsmdbp_object_mailbox	*mailbox;
	struct emsmdbp_object_folder	*folder;
	struct emsmdbp_object_message	*message;
	struct emsmdbp_object_table	*table;
	struct emsmdbp_object_stream	*stream;
	struct emsmdbp_object_attachment *attachment;
	struct emsmdbp_object_subscription *subscription;
};

struct emsmdbp_object {
	enum emsmdbp_object_type	type;
	union emsmdbp_objects		object;
	struct mapistore_context	*mstore_ctx;
	void				*private_data;
        bool                            poc_api;
        void                            *poc_backend_object; /* private_data ? */
};


#define	EMSMDB_PCMSPOLLMAX		60000
#define	EMSMDB_PCRETRY			6
#define	EMSMDB_PCRETRYDELAY		10000

#define	EMSMDBP_MAILBOX_ROOT		0x1
#define	EMSMDBP_DEFERRED_ACTIONS	0x2
#define	EMSMDBP_SPOOLER_QUEUE		0x3
#define	EMSMDBP_TODO_SEARCH		0x4
#define	EMSMDBP_TOP_INFORMATION_STORE	0x5
#define	EMSMDBP_INBOX			0x6
#define	EMSMDBP_OUTBOX			0x7
#define	EMSMDBP_SENT_ITEMS		0x8
#define	EMSMDBP_DELETED_ITEMS		0x9
#define	EMSMDBP_COMMON_VIEWS		0xA
#define	EMSMDBP_SCHEDULE		0xB
#define	EMSMDBP_SEARCH			0xC
#define	EMSMDBP_VIEWS			0xD
#define	EMSMDBP_SHORTCUTS		0xE

#define EMSMDBP_PF_ROOT			0x0
#define EMSMDBP_PF_IPMSUBTREE		0x1
#define EMSMDBP_PF_NONIPMSUBTREE	0x2
#define EMSMDBP_PF_EFORMSREGISTRY	0x3
#define EMSMDBP_PF_FREEBUSY		0x4
#define EMSMDBP_PF_OAB			0x5
#define EMSMDBP_PF_LOCALEFORMS		0x6
#define EMSMDBP_PF_LOCALFREEBUSY	0x7
#define EMSMDBP_PF_LOCALOAB		0x8


#define	EMSMDBP_TABLE_FOLDER_TYPE	0x1
#define	EMSMDBP_TABLE_MESSAGE_TYPE	0x2
#define	EMSMDBP_TABLE_FAI_TYPE		0x3
#define	EMSMDBP_TABLE_RULE_TYPE		0x4
#define	EMSMDBP_TABLE_ATTACHMENT_TYPE	0x5

__BEGIN_DECLS

NTSTATUS	samba_init_module(void);
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *);

/* definitions from emsmdbp.c */
struct emsmdbp_context	*emsmdbp_init(struct loadparm_context *, const char *, void *);
void			*emsmdbp_openchange_ldb_init(struct loadparm_context *);
bool			emsmdbp_destructor(void *);
bool			emsmdbp_verify_user(struct dcesrv_call_state *, struct emsmdbp_context *);
bool			emsmdbp_verify_userdn(struct dcesrv_call_state *, struct emsmdbp_context *, const char *, struct ldb_message **);
enum MAPISTATUS		emsmdbp_resolve_recipient(TALLOC_CTX *, struct emsmdbp_context *, char *, struct mapi_SPropTagArray *, struct RecipientRow *);

/* definitions from emsmdbp_object.c */
const char	      *emsmdbp_getstr_type(struct emsmdbp_object *);
bool		      emsmdbp_is_mapistore(struct mapi_handles *);
bool		      emsmdbp_is_mailboxstore(struct mapi_handles *);
uint32_t	      emsmdbp_get_contextID(struct mapi_handles *);
struct mapi_handles   *emsmdbp_object_get_folder_handle_by_fid(struct mapi_handles_context *, uint64_t);
struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *, struct emsmdbp_context *);
struct emsmdbp_object *emsmdbp_object_mailbox_init(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, bool);
struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, struct mapi_handles *);
struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *, struct emsmdbp_context *, struct mapi_handles *);
struct emsmdbp_object *emsmdbp_object_message_init(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, struct mapi_handles *);
struct emsmdbp_object *emsmdbp_object_stream_init(TALLOC_CTX *, struct emsmdbp_context *, uint32_t, enum OpenStream_OpenModeFlags, struct mapi_handles *);
struct emsmdbp_object *emsmdbp_object_attachment_init(TALLOC_CTX *, struct emsmdbp_context *, uint64_t, struct mapi_handles *);
struct emsmdbp_object *emsmdbp_object_subscription_init(TALLOC_CTX *, struct emsmdbp_context *, struct mapi_handles *);

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
enum MAPISTATUS EcDoRpc_RopSetProperties(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopDeleteProperties(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopOpenStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopReadStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopWriteStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetStreamSize(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSeekStream(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetStreamSize(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
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
enum MAPISTATUS	EcDoRpc_RopReadPerUserInformation(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

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
enum MAPISTATUS	EcDoRpc_RopGetTransportFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopOptionsData(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxorule.c */
enum MAPISTATUS EcDoRpc_RopGetRulesTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopModifyRules(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcperm.c */
enum MAPISTATUS EcDoRpc_RopGetPermissionsTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcfxics.c */
enum MAPISTATUS EcDoRpc_RopFastTransferSourceCopyTo(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopFastTransferSourceGetBuffer(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

__END_DECLS

#endif	/* __DCESRV_EXCHANGE_EMSMDB_H */
