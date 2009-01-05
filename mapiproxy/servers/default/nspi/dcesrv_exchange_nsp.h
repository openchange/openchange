/*
   MAPI Proxy - Exchange NSPI Server

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

#ifndef	__DCESRV_EXCHANGE_NSP_H
#define	__DCESRV_EXCHANGE_NSP_H

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>
#include <util/debug.h>

#ifndef	__BEGIN_DECLS
#ifdef	__cplusplus
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

struct emsabp_context {
	void		*conf_ctx;
	void		*users_ctx;
	void		*ldb_ctx;
	TALLOC_CTX	*mem_ctx;
};


struct exchange_nsp_session {
	struct mpm_session		*session;
	struct exchange_nsp_session	*prev;
	struct exchange_nsp_session	*next;
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



__BEGIN_DECLS

NTSTATUS	samba_init_module(void);

/* definitions from emsabp.c */
struct emsabp_context	*emsabp_init(struct loadparm_context *);
bool			emsabp_destructor(void *);
bool			emsabp_verify_user(struct dcesrv_call_state *, struct emsabp_context *);
bool			emsabp_verify_codepage(struct loadparm_context *, struct emsabp_context *, uint32_t);
bool			emsabp_verify_lcid(struct loadparm_context *, struct emsabp_context *, uint32_t);
struct GUID		*emsabp_get_server_GUID(struct loadparm_context *, struct emsabp_context *);

__END_DECLS

#endif /* __DCESRV_EXCHANGE_NSP_H */
