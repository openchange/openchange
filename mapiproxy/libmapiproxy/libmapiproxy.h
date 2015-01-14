/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008-2011

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

#ifndef	__LIBMAPIPROXY_H__
#define	__LIBMAPIPROXY_H__

#include <signal.h>
#include <dcerpc_server.h>
#include <talloc.h>
#include <tevent.h>
#include <tdb.h>
#include <ldb.h>
#include <ldb_errors.h>
#include <dlinklist.h>
#include <fcntl.h>
#include <errno.h>

#include <gen_ndr/server_id.h>
#include <gen_ndr/exchange.h>

struct mapiproxy {
	bool			norelay;
	bool			ahead;
};


enum mapiproxy_status {
	MAPIPROXY_DEFAULT	= 0x0,
	MAPIPROXY_CUSTOM	= 0x1
};


struct mapiproxy_module {
	enum mapiproxy_status	status;
	const char		*name;
	const char		*description;
	const char		*endpoint;
	NTSTATUS		(*init)(struct dcesrv_context *);
	NTSTATUS		(*push)(struct dcesrv_call_state *, TALLOC_CTX *, void *);
	NTSTATUS		(*ndr_pull)(struct dcesrv_call_state *, TALLOC_CTX *, struct ndr_pull *);
	NTSTATUS		(*pull)(struct dcesrv_call_state *, TALLOC_CTX *, void *);
	NTSTATUS		(*dispatch)(struct dcesrv_call_state *, TALLOC_CTX *, void *, struct mapiproxy *);
	NTSTATUS		(*unbind)(struct server_id, uint32_t);
};


struct mapiproxy_module_list {
	const struct mapiproxy_module  	*module;
	struct mapiproxy_module_list	*prev;
	struct mapiproxy_module_list	*next;
};


struct mpm_session {
	struct server_id		server_id;
	uint32_t			context_id;
	uint32_t			ref_count;
	bool				(*destructor)(void *);
	void				*private_data;
};


struct auth_serversupplied_info 
{
	struct dom_sid	*account_sid;
	struct dom_sid	*primary_group_sid;

	size_t		n_domain_groups;
	struct dom_sid	**domain_groups;

	DATA_BLOB	user_session_key;
	DATA_BLOB	lm_session_key;

	const char	*account_name;
	const char	*domain_name;

	const char	*full_name;
	const char	*logon_script;
	const char	*profile_path;
	const char	*home_directory;
	const char	*home_drive;
	const char	*logon_server;
	
	NTTIME		last_logon;
	NTTIME		last_logoff;
	NTTIME		acct_expiry;
	NTTIME		last_password_change;
	NTTIME		allow_password_change;
	NTTIME		force_password_change;

	uint16_t	logon_count;
	uint16_t	bad_password_count;

	uint32_t	acct_flags;

	bool		authenticated;
};


struct mapi_handles {
	uint32_t	       	handle;
	uint32_t		parent_handle;
	void		       	*private_data;
	struct mapi_handles	*prev;
	struct mapi_handles	*next;
};


struct mapi_handles_context {
	TDB_CONTEXT	       	*tdb_ctx;
	uint32_t		last_handle;
	struct mapi_handles    	*handles;
};


#define	MAPI_HANDLES_RESERVED	0xFFFFFFFF
#define	MAPI_HANDLES_ROOT	"root"
#define	MAPI_HANDLES_NULL	"null"


/**
   EMSABP server defines
 */
#define	EMSABP_TDB_NAME		"emsabp_tdb.tdb"

/**
   Represents the NSPI Protocol in Permanent Entry IDs.
 */
static const uint8_t GUID_NSPI[] = {
0xDC, 0xA7, 0x40, 0xC8, 0xC0, 0x42, 0x10, 0x1A, 0xB4, 0xB9,
0x08, 0x00, 0x2B, 0x2F, 0xE1, 0x82
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

/* definitions from dcesrv_mapiproxy_module.c */
NTSTATUS mapiproxy_module_register(const void *);
NTSTATUS mapiproxy_module_init(struct dcesrv_context *);

NTSTATUS mapiproxy_module_push(struct dcesrv_call_state *, TALLOC_CTX *, void *);
NTSTATUS mapiproxy_module_pull(struct dcesrv_call_state *, TALLOC_CTX *, void *);
NTSTATUS mapiproxy_module_ndr_pull(struct dcesrv_call_state *, TALLOC_CTX *, struct ndr_pull *);
NTSTATUS mapiproxy_module_dispatch(struct dcesrv_call_state *, TALLOC_CTX *, void *, struct mapiproxy *);
NTSTATUS mapiproxy_module_unbind(struct server_id, uint32_t);

const struct mapiproxy_module *mapiproxy_module_byname(const char *);

/* definitions from dcesrv_mapiproxy_server.c */
NTSTATUS mapiproxy_server_register(const void *);
NTSTATUS mapiproxy_server_init(struct dcesrv_context *);
NTSTATUS mapiproxy_server_unbind(struct server_id, uint32_t);
NTSTATUS mapiproxy_server_dispatch(struct dcesrv_call_state *, TALLOC_CTX *, void *, struct mapiproxy *);
bool mapiproxy_server_loaded(const char *);

const struct mapiproxy_module *mapiproxy_server_bystatus(const char *, enum mapiproxy_status);
const struct mapiproxy_module *mapiproxy_server_byname(const char *);

TDB_CONTEXT *mapiproxy_server_emsabp_tdb_init(struct loadparm_context *);
void *mapiproxy_server_openchangedb_init(struct loadparm_context *);

/* definitions from dcesrv_mapiproxy_session. c */
struct mpm_session *mpm_session_new(TALLOC_CTX *, struct server_id, uint32_t);
struct mpm_session *mpm_session_init(TALLOC_CTX *, struct dcesrv_call_state *);
bool mpm_session_increment_ref_count(struct mpm_session *);
bool mpm_session_set_destructor(struct mpm_session *, bool (*destructor)(void *));
bool mpm_session_set_private_data(struct mpm_session *, void *);
bool mpm_session_release(struct mpm_session *);
bool mpm_session_cmp_sub(struct mpm_session *, struct server_id, uint32_t);
bool mpm_session_cmp(struct mpm_session *, struct dcesrv_call_state *);

struct openchangedb_context;

/* definitions from openchangedb.c */
enum MAPISTATUS openchangedb_initialize(TALLOC_CTX *, struct loadparm_context *, struct openchangedb_context **oc_ctx);
enum MAPISTATUS openchangedb_get_new_changeNumber(struct openchangedb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_get_new_changeNumbers(struct openchangedb_context *, TALLOC_CTX *, const char *, uint64_t, struct UI8Array_r **);
enum MAPISTATUS openchangedb_get_next_changeNumber(struct openchangedb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_get_SystemFolderID(struct openchangedb_context *, const char *, uint32_t, uint64_t *);
enum MAPISTATUS openchangedb_get_SpecialFolderID(struct openchangedb_context *, const char *, uint32_t, uint64_t *);
enum MAPISTATUS openchangedb_get_PublicFolderID(struct openchangedb_context *, const char *, uint32_t, uint64_t *);
enum MAPISTATUS openchangedb_get_distinguishedName(TALLOC_CTX *, struct openchangedb_context *, uint64_t, char **);
enum MAPISTATUS	openchangedb_get_MailboxGuid(struct openchangedb_context *, const char *, struct GUID *);
enum MAPISTATUS	openchangedb_get_MailboxReplica(struct openchangedb_context *, const char *, uint16_t *, struct GUID *);
enum MAPISTATUS openchangedb_get_PublicFolderReplica(struct openchangedb_context *, const char *, uint16_t *, struct GUID *);
enum MAPISTATUS openchangedb_get_parent_fid(struct openchangedb_context *, const char *, uint64_t, uint64_t *, bool);
enum MAPISTATUS openchangedb_get_MAPIStoreURIs(struct openchangedb_context *, const char *, TALLOC_CTX *, struct StringArrayW_r **);
enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *, struct openchangedb_context *, const char *, uint64_t, char **, bool);
enum MAPISTATUS openchangedb_set_mapistoreURI(struct openchangedb_context *, const char *, uint64_t, const char *);
enum MAPISTATUS openchangedb_get_fid(struct openchangedb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_get_ReceiveFolder(TALLOC_CTX *, struct openchangedb_context *, const char *, const char *, uint64_t *, const char **);
enum MAPISTATUS openchangedb_get_ReceiveFolderTable(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t *, struct ReceiveFolder **);
enum MAPISTATUS openchangedb_get_TransportFolder(struct openchangedb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_lookup_folder_property(struct openchangedb_context *, uint32_t, uint64_t);
enum MAPISTATUS openchangedb_set_folder_properties(struct openchangedb_context *, const char *, uint64_t, struct SRow *);
char *          openchangedb_set_folder_property_data(TALLOC_CTX *, struct SPropValue *);
enum MAPISTATUS openchangedb_get_folder_property(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t, uint64_t, void **);
enum MAPISTATUS openchangedb_get_folder_count(struct openchangedb_context *, const char *, uint64_t, uint32_t *);
enum MAPISTATUS openchangedb_get_message_count(struct openchangedb_context *, const char *, uint64_t, uint32_t *, bool);
enum MAPISTATUS openchangedb_get_system_idx(struct openchangedb_context *, const char *, uint64_t, int *);
enum MAPISTATUS openchangedb_get_table_property(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t, uint32_t, void **);
enum MAPISTATUS openchangedb_get_fid_by_name(struct openchangedb_context *, const char *, uint64_t, const char*, uint64_t *);
enum MAPISTATUS openchangedb_get_mid_by_subject(struct openchangedb_context *, const char *, uint64_t, const char *, bool, uint64_t *);
enum MAPISTATUS openchangedb_set_ReceiveFolder(struct openchangedb_context *, const char *, const char *, uint64_t);
enum MAPISTATUS openchangedb_create_mailbox(struct openchangedb_context *, const char *, const char *, const char *, uint64_t, const char *);
enum MAPISTATUS openchangedb_create_folder(struct openchangedb_context *, const char *, uint64_t, uint64_t, uint64_t, const char *, int);
enum MAPISTATUS openchangedb_delete_folder(struct openchangedb_context *, const char *, uint64_t);
enum MAPISTATUS openchangedb_get_fid_from_partial_uri(struct openchangedb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_get_users_from_partial_uri(TALLOC_CTX *, struct openchangedb_context *, const char *, uint32_t *, char ***, char ***);
enum MAPISTATUS openchangedb_transaction_start(struct openchangedb_context *);
enum MAPISTATUS openchangedb_transaction_commit(struct openchangedb_context *);

enum MAPISTATUS openchangedb_get_new_public_folderID(struct openchangedb_context *, const char *, uint64_t *);
bool		openchangedb_is_public_folder_id(struct openchangedb_context *, uint64_t);

enum MAPISTATUS openchangedb_get_indexing_url(struct openchangedb_context *, const char *, const char **);
bool 		openchangedb_set_locale(struct openchangedb_context*, const char *, uint32_t);
const char **	openchangedb_get_folders_names(TALLOC_CTX *, struct openchangedb_context *, const char *, const char *);

/* definitions from openchangedb_table.c */
enum MAPISTATUS openchangedb_table_init(TALLOC_CTX *, struct openchangedb_context *, const char *, uint8_t, uint64_t, void **);
enum MAPISTATUS openchangedb_table_set_sort_order(struct openchangedb_context *, void *, struct SSortOrderSet *);
enum MAPISTATUS openchangedb_table_set_restrictions(struct openchangedb_context *, void *, struct mapi_SRestriction *);
enum MAPISTATUS openchangedb_table_get_property(TALLOC_CTX *, struct openchangedb_context *, void *, enum MAPITAGS, uint32_t, bool, void **);

/* definitions from openchangedb_message.c */
enum MAPISTATUS openchangedb_message_open(TALLOC_CTX *, struct openchangedb_context *, const char *, uint64_t, uint64_t, void **, void **);
enum MAPISTATUS openchangedb_message_create(TALLOC_CTX *, struct openchangedb_context *, const char *, uint64_t, uint64_t, bool, void **);
enum MAPISTATUS openchangedb_message_save(struct openchangedb_context *, void *, uint8_t);
enum MAPISTATUS openchangedb_message_get_property(TALLOC_CTX *, struct openchangedb_context *, void *, uint32_t, void **);
enum MAPISTATUS openchangedb_message_set_properties(TALLOC_CTX *, struct openchangedb_context *, void *, struct SRow *);

/* definitions from auto-generated openchangedb_property.c */
const char *openchangedb_property_get_attribute(uint32_t);

/* definitions from mapi_handles.c */
struct mapi_handles_context *mapi_handles_init(TALLOC_CTX *);
enum MAPISTATUS	mapi_handles_release(struct mapi_handles_context *);
enum MAPISTATUS mapi_handles_search(struct mapi_handles_context *, uint32_t, struct mapi_handles **);
enum MAPISTATUS mapi_handles_add(struct mapi_handles_context *, uint32_t, struct mapi_handles **);
enum MAPISTATUS mapi_handles_delete(struct mapi_handles_context *, uint32_t);
enum MAPISTATUS mapi_handles_get_private_data(struct mapi_handles *, void **);
enum MAPISTATUS mapi_handles_set_private_data(struct mapi_handles *, void *);
enum MAPISTATUS mapi_handles_get_systemfolder(struct mapi_handles *, int *);
enum MAPISTATUS mapi_handles_set_systemfolder(struct mapi_handles *, int);

/* definitions from entryid.c */
enum MAPISTATUS entryid_set_AB_EntryID(TALLOC_CTX *, const char *, struct SBinary_short *);
enum MAPISTATUS entryid_set_folder_EntryID(TALLOC_CTX *, struct GUID *, struct GUID *, uint16_t, uint64_t, struct Binary_r **);

/* definitions from modules.c */
typedef NTSTATUS (*openchange_plugin_init_fn) (void);
openchange_plugin_init_fn *load_openchange_plugins(TALLOC_CTX *mem_ctx, const char *path);

__END_DECLS

#endif /* ! __LIBMAPIPROXY_H__ */

