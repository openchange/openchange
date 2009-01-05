/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008-2009

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

#include <dcerpc_server.h>
#include <talloc.h>
#include <tevent.h>
#include <libmapi/dlinklist.h>

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
	bool				(*destructor)(void *);
	void				*private_data;
};


#define	NTLM_AUTH_IS_OK(dce_call) \
(dce_call->conn->auth_state.session_info->server_info->authenticated == true)

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


/* definitions from dcesrv_mapiproxy_session. c */
struct mpm_session *mpm_session_new(TALLOC_CTX *, struct server_id, uint32_t);
struct mpm_session *mpm_session_init(TALLOC_CTX *, struct dcesrv_call_state *);
bool mpm_session_set_destructor(struct mpm_session *, bool (*destructor)(void *));
bool mpm_session_set_private_data(struct mpm_session *, void *);
bool mpm_session_release(struct mpm_session *);
bool mpm_session_cmp_sub(struct mpm_session *, struct server_id, uint32_t);
bool mpm_session_cmp(struct mpm_session *, struct dcesrv_call_state *);

__END_DECLS

#endif /* ! __LIBMAPIPROXY_H__ */
