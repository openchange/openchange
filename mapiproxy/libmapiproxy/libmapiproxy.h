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

struct openchangedb_table {
	uint64_t			folderID;
	uint8_t				table_type;
	struct SSortOrderSet		*lpSortCriteria;
	struct mapi_SRestriction	*restrictions;
	struct ldb_result		*res;
};

enum openchangedb_message_status {
	OPENCHANGEDB_MESSAGE_CREATE	= 0x1,
	OPENCHANGEDB_MESSAGE_OPEN	= 0x2
};

struct openchangedb_message {
	enum openchangedb_message_status	status;
	uint64_t				messageID;
	uint64_t				folderID;
	void					*ldb_ctx;
	struct ldb_message			*msg;
	struct ldb_result			*res;
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


#define	OPENCHANGE_LDB_NAME	"openchange.ldb"

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
void *mapiproxy_server_openchange_ldb_init(struct loadparm_context *);

/* definitions from dcesrv_mapiproxy_session. c */
struct mpm_session *mpm_session_new(TALLOC_CTX *, struct server_id, uint32_t);
struct mpm_session *mpm_session_init(TALLOC_CTX *, struct dcesrv_call_state *);
bool mpm_session_increment_ref_count(struct mpm_session *);
bool mpm_session_set_destructor(struct mpm_session *, bool (*destructor)(void *));
bool mpm_session_set_private_data(struct mpm_session *, void *);
bool mpm_session_release(struct mpm_session *);
bool mpm_session_cmp_sub(struct mpm_session *, struct server_id, uint32_t);
bool mpm_session_cmp(struct mpm_session *, struct dcesrv_call_state *);

/* definitions from openchangedb.c */
enum MAPISTATUS openchangedb_get_new_folderID(struct ldb_context *, uint64_t *);
enum MAPISTATUS openchangedb_get_new_folderIDs(struct ldb_context *, TALLOC_CTX *, uint64_t, struct UI8Array_r **);
enum MAPISTATUS openchangedb_get_new_changeNumber(struct ldb_context *, uint64_t *);
enum MAPISTATUS openchangedb_get_new_changeNumbers(struct ldb_context *, TALLOC_CTX *, uint64_t, struct UI8Array_r **);
enum MAPISTATUS openchangedb_get_next_changeNumber(struct ldb_context *, uint64_t *);
enum MAPISTATUS openchangedb_reserve_fmid_range(struct ldb_context *, uint64_t, uint64_t *);
enum MAPISTATUS openchangedb_get_SystemFolderID(struct ldb_context *, const char *, uint32_t, uint64_t *);
enum MAPISTATUS openchangedb_get_PublicFolderID(struct ldb_context *, uint32_t, uint64_t *);
enum MAPISTATUS openchangedb_get_distinguishedName(TALLOC_CTX *, struct ldb_context *, uint64_t, char **);
enum MAPISTATUS openchangedb_get_mailboxDN(TALLOC_CTX *, struct ldb_context *, uint64_t, char **);
enum MAPISTATUS	openchangedb_get_MailboxGuid(struct ldb_context *, const char *, struct GUID *);
enum MAPISTATUS	openchangedb_get_MailboxReplica(struct ldb_context *, const char *, uint16_t *, struct GUID *);
enum MAPISTATUS openchangedb_get_PublicFolderReplica(struct ldb_context *, uint16_t *, struct GUID *);
enum MAPISTATUS openchangedb_get_parent_fid(struct ldb_context *, uint64_t, uint64_t *, bool);
enum MAPISTATUS openchangedb_get_MAPIStoreURIs(struct ldb_context *, const char *, TALLOC_CTX *, struct StringArrayW_r **);
enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *, struct ldb_context *, uint64_t, char **, bool);
enum MAPISTATUS openchangedb_set_mapistoreURI(struct ldb_context *, uint64_t, const char *, bool);
enum MAPISTATUS openchangedb_get_fid(struct ldb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_get_ReceiveFolder(TALLOC_CTX *, struct ldb_context *, const char *, const char *, uint64_t *, const char **);
enum MAPISTATUS openchangedb_get_TransportFolder(struct ldb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_lookup_folder_property(struct ldb_context *, uint32_t, uint64_t);
enum MAPISTATUS openchangedb_set_folder_properties(struct ldb_context *, uint64_t, struct SRow *);
char *openchangedb_set_folder_property_data(TALLOC_CTX *, struct SPropValue *);
enum MAPISTATUS openchangedb_get_folder_property(TALLOC_CTX *, struct ldb_context *, uint32_t, uint64_t, void **);
enum MAPISTATUS openchangedb_get_folder_count(struct ldb_context *, uint64_t, uint32_t *);
enum MAPISTATUS openchangedb_get_message_count(struct ldb_context *, uint64_t, uint32_t *, bool);
enum MAPISTATUS openchangedb_get_system_idx(struct ldb_context *, uint64_t, int *);
enum MAPISTATUS openchangedb_get_table_property(TALLOC_CTX *, struct ldb_context *, const char *, uint32_t, uint32_t, void **);
enum MAPISTATUS openchangedb_get_fid_by_name(struct ldb_context *, uint64_t, const char*, uint64_t *);
enum MAPISTATUS openchangedb_get_mid_by_subject(struct ldb_context *, uint64_t, const char *, bool, uint64_t *);
enum MAPISTATUS openchangedb_set_ReceiveFolder(struct ldb_context *, const char *, const char *, uint64_t);
enum MAPISTATUS openchangedb_create_mailbox(struct ldb_context *, const char *, int, uint64_t *);
enum MAPISTATUS openchangedb_create_folder(struct ldb_context *, uint64_t, uint64_t, uint64_t, const char *, int);
enum MAPISTATUS openchangedb_delete_folder(struct ldb_context *, uint64_t);
enum MAPISTATUS openchangedb_get_fid_from_partial_uri(struct ldb_context *, const char *, uint64_t *);
enum MAPISTATUS openchangedb_get_users_from_partial_uri(TALLOC_CTX *, struct ldb_context *, const char *, uint32_t *, char ***, char ***);
void *openchangedb_get_special_property(TALLOC_CTX *, struct ldb_context *, struct ldb_result *, uint32_t, const char *);
void *openchangedb_get_property_data(TALLOC_CTX *, struct ldb_result *, uint32_t, uint32_t, const char *);
void *openchangedb_get_property_data_message(TALLOC_CTX *, struct ldb_message *, uint32_t, const char *);

/* definitions from openchangedb_table.c */
enum MAPISTATUS openchangedb_table_init(TALLOC_CTX *, uint8_t, uint64_t, void **);
enum MAPISTATUS openchangedb_table_set_sort_order(void *, struct SSortOrderSet *);
enum MAPISTATUS openchangedb_table_set_restrictions(void *, struct mapi_SRestriction *);
enum MAPISTATUS openchangedb_table_get_property(TALLOC_CTX *, void *, struct ldb_context *, enum MAPITAGS, uint32_t, bool live_filtered, void **);

/* definitions from openchangedb_message.c */
enum MAPISTATUS openchangedb_message_open(TALLOC_CTX *, struct ldb_context *, uint64_t, uint64_t, void **, void **);
enum MAPISTATUS openchangedb_message_create(TALLOC_CTX *, struct ldb_context *, uint64_t, uint64_t, bool, void **);
enum MAPISTATUS openchangedb_message_save(void *, uint8_t);
enum MAPISTATUS openchangedb_message_get_property(TALLOC_CTX *, void *, uint32_t, void **);
enum MAPISTATUS openchangedb_message_set_properties(TALLOC_CTX *, void *, struct SRow *);

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
