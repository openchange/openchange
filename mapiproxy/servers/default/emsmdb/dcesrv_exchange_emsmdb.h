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
	struct loadparm_context		*lp_ctx;
	void				*oc_ctx;
	void				*conf_ctx;
	void				*users_ctx;
	TALLOC_CTX			*mem_ctx;
	struct mapi_handles_context	*handles_ctx;
};


struct exchange_emsmdb_session {
	uint32_t			pullTimeStamp;
	struct mpm_session		*session;
	struct exchange_emsmdb_session	*prev;
	struct exchange_emsmdb_session	*next;
};


struct emsmdbp_object_mailbox {
	char				*owner_Name;
	char				*owner_EssDN;
	char				*szUserDN;
};


struct emsmdbp_object_folder {
	uint64_t			folderID;
	bool				IsSystemFolder;
	int				systemfolder;
	/* pointer to the mapistore context goes here */
};


#define	EMSMDB_PCMSPOLLMAX		60000
#define	EMSMDB_PCRETRY			6
#define	EMSMDB_PCRETRYDELAY		10000

#define	EMSMDBP_NON_IPM_SUBTREE		0x1
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

/* definitions from emsmdbp.c */
struct emsmdbp_context	*emsmdbp_init(struct loadparm_context *, void *);
void			*emsmdbp_openchange_ldb_init(struct loadparm_context *);
bool			emsmdbp_destructor(void *);
bool			emsmdbp_verify_user(struct dcesrv_call_state *, struct emsmdbp_context *);
bool			emsmdbp_verify_userdn(struct dcesrv_call_state *, struct emsmdbp_context *, const char *, struct ldb_message **);

/* definitions from emsmdbp_object.c */
struct emsmdbp_object_mailbox *emsmdbp_object_mailbox_init(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *);
struct emsmdbp_object_folder *emsmdbp_object_folder_init(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct mapi_handles *);

/* definitions from oxcfold.c */
enum MAPISTATUS EcDoRpc_RopOpenFolder(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcnotif.c */
enum MAPISTATUS EcDoRpc_RopRegisterNotification(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcprpt.c */
enum MAPISTATUS EcDoRpc_RopGetPropertiesSpecific(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);

/* definitions from oxcstor.c */
enum MAPISTATUS	EcDoRpc_RopLogon(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, struct EcDoRpc_MAPI_REPL *, uint32_t *, uint16_t *);
enum MAPISTATUS	EcDoRpc_RopRelease(TALLOC_CTX *, struct emsmdbp_context *, struct EcDoRpc_MAPI_REQ *, uint32_t *, uint16_t *);

__END_DECLS

#endif	/* __DCESRV_EXCHANGE_EMSMDB_H */
