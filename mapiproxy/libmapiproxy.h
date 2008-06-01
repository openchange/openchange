/*
   MAPI Proxy

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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
#include <libmapi/dlinklist.h>

struct mapiproxy_module {
	const char		*name;
	const char		*description;
	const char		*endpoint;
	NTSTATUS		(*init)(struct dcesrv_context *);
	NTSTATUS		(*push)(struct dcesrv_call_state *, TALLOC_CTX *, void *);
	NTSTATUS		(*ndr_pull)(struct dcesrv_call_state *, TALLOC_CTX *, struct ndr_pull *);
	NTSTATUS		(*pull)(struct dcesrv_call_state *, TALLOC_CTX *, void *);
	NTSTATUS		(*dispatch)(struct dcesrv_call_state *, TALLOC_CTX *, void *);
};

struct mapiproxy_module_list {
	const struct mapiproxy_module  	*module;
	struct mapiproxy_module_list	*prev;
	struct mapiproxy_module_list	*next;
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

NTSTATUS mapiproxy_module_register(const void *);
NTSTATUS mapiproxy_module_init(struct dcesrv_context *);

NTSTATUS mapiproxy_module_push(struct dcesrv_call_state *, TALLOC_CTX *, void *);
NTSTATUS mapiproxy_module_pull(struct dcesrv_call_state *, TALLOC_CTX *, void *);
NTSTATUS mapiproxy_module_ndr_pull(struct dcesrv_call_state *, TALLOC_CTX *, struct ndr_pull *);
NTSTATUS mapiproxy_module_dispatch(struct dcesrv_call_state *, TALLOC_CTX *, void *);

const struct mapiproxy_module *mapiproxy_module_byname(const char *);

__END_DECLS

#endif /* ! __LIBMAPIPROXY_H__ */
