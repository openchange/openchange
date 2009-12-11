/*
   MAPI Proxy - Exchange EMSMDB Server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <mapiproxy/libmapiproxy/libmapiproxy.h>
#include <mapiproxy/libmapistore/mapistore.h>
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
	struct exchange_emsmdb_session	*prev;
	struct exchange_emsmdb_session	*next;
};

enum emsmdbp_object_type {
	EMSMDBP_OBJECT_UNDEF		= 0x0,
	EMSMDBP_OBJECT_MAILBOX		= 0x1,
	EMSMDBP_OBJECT_FOLDER		= 0x2,
	EMSMDBP_OBJECT_MESSAGE		= 0x3,
	EMSMDBP_OBJECT_TABLE		= 0x4
};

struct emsmdbp_object_mailbox {
	uint64_t			folderID;
	char				*owner_Name;
	char				*owner_EssDN;
	char				*szUserDN;
};


struct emsmdbp_object_folder {
	uint64_t			folderID;
	bool				IsSystemFolder;
	int				systemfolder;
	uint32_t			contextID;
};


struct emsmdbp_object_table {
	uint64_t			folderID;
	bool				IsSystemTable;
	uint16_t			prop_count;
	uint32_t			*properties;
	uint32_t			numerator;
	uint32_t			denominator;
};

union emsmdbp_objects {
	struct emsmdbp_object_mailbox	*mailbox;
	struct emsmdbp_object_folder	*folder;
	struct emsmdbp_object_table	*table;
};

struct emsmdbp_object {
	enum emsmdbp_object_type	type;
	union emsmdbp_objects		object;
	struct mapistore_context	*mstore_ctx;
	void				*private_data;
};


#define	EMSMDB_PCMSPOLLMAX		60000
#define	EMSMDB_PCRETRY			6
#define	EMSMDB_PCRETRYDELAY		10000

#define	EMSMDBP_MAILBOX_ROOT		0x1
#define	EMSMDBP_DEFERRED_ACTIONS	0x2
#define	EMSMDBP_SPOOLER_QUEUE		0x3
#define	EMSMDBP_TOP_INFORMATION_STORE	0x4
#define	EMSMDBP_INBOX			0x5
#define	EMSMDBP_OUTBOX			0x6
#define	EMSMDBP_SENT_ITEMS		0x7
#define	EMSMDBP_DELETED_ITEMS		0x8
#define	EMSMDBP_COMMON_VIEWS		0x9
#define	EMSMDBP_SCHEDULE		0xA
#define	EMSMDBP_SEARCH			0xB
#define	EMSMDBP_VIEWS			0xC
#define	EMSMDBP_SHORTCUTS		0xD

__BEGIN_DECLS

NTSTATUS	samba_init_module(void);
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *);

/* definitions from emsmdbp.c */
struct emsmdbp_context	*emsmdbp_init(struct loadparm_context *, void *);
void			*emsmdbp_openchange_ldb_init(struct loadparm_context *);
bool			emsmdbp_destructor(void *);
bool			emsmdbp_verify_user(struct dcesrv_call_state *, struct emsmdbp_context *);
bool			emsmdbp_verify_userdn(struct dcesrv_call_state *, struct emsmdbp_context *, const char *, struct ldb_message **);

/* definitions from emsmdbp_object.c */
struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *, struct emsmdbp_context *);
struct emsmdbp_object *emsmdbp_object_mailbox_init(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *);
struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct mapi_handles *);
struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *, struct emsmdbp_context *, struct mapi_handles *);

/* definitions from oxcfold.c */
enum MAPISTATUS EcDoRpc_RopOpenFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetHierarchyTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetContentsTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcmsg.c */
enum MAPISTATUS EcDoRpc_RopCreateMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSaveChangesMessage(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcnotif.c */
enum MAPISTATUS EcDoRpc_RopRegisterNotification(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcprpt.c */
enum MAPISTATUS EcDoRpc_RopGetPropertiesSpecific(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSetProperties(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopGetPropertyIdsFromNames(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcstor.c */
enum MAPISTATUS	EcDoRpc_RopLogon(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopRelease(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopGetReceiveFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definition from oxctabl.c */
enum MAPISTATUS EcDoRpc_RopSetColumns(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSortTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopRestrict(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopQueryRows(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopQueryPosition(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopSeekRow(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS EcDoRpc_RopFindRow(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxorule.c */
enum MAPISTATUS EcDoRpc_RopGetRulesTable(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

__END_DECLS

#endif	/* __DCESRV_EXCHANGE_EMSMDB_H */
