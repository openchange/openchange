/*
   Asynchronous EMSMDB server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2015
   Copyright (C) Carlos Pérez-Aradros Herce 2015

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

#ifndef	DCESRV_ASYNCEMSMDB_H
#define	DCESRV_ASYNCEMSMDB_H

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "gen_ndr/asyncemsmdb.h"
#include "gen_ndr/ndr_asyncemsmdb.h"

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/gen_ndr/mapistore_notification.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>


struct exchange_asyncemsmdb_session {
	TALLOC_CTX				*mem_ctx;
	char					*cn;
	char					*bind_addr;
	struct dcesrv_call_state		*dce_call;
	struct EcDoAsyncWaitEx			*r;
	struct mapistore_context		*mstore_ctx;
	char					*username;
	char					*emsmdb_session_str;
	struct GUID				emsmdb_uuid;
	struct tevent_fd			*fd_event;
	int					sock;
	int					fd;
	int					lock;
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

#define	DCESRV_INTERFACE_ASYNCEMSMDB_BIND	dcerpc_server_asyncemsmdb_bind
#define	DCESRV_INTERFACE_ASYNCEMSMDB_UNBIND	dcerpc_server_asyncemsmdb_unbind

__BEGIN_DECLS

NTSTATUS dcerpc_server_asyncemsmdb_init(void);
NTSTATUS ndr_table_register(const struct ndr_interface_table *);
NTSTATUS samba_init_module(void);


/* Expose samdb_connect prototype */
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *, unsigned int);
struct ldb_context *samdb_connect_url(TALLOC_CTX *, struct tevent_context *, struct loadparm_context *, struct auth_session_info *, unsigned int, const char *);
void tevent_loop_allow_nesting(struct tevent_context *);

__END_DECLS

#define	ASYNCEMSMDB_FALLBACK_ADDR	"127.0.0.1"
#define	ASYNCEMSMDB_INBOX_SYSTEMIDX	13

#define	ASYNCEMSMDB_SPACE		' '
#define	ASYNCEMSMDB_SOGO_SPACE		"_SP_"
#define	ASYNCEMSMDB_SOGO_SPACE_LEN	4

#define	ASYNCEMSMDB_UNDERSCORE		'_'
#define	ASYNCEMSMDB_SOGO_UNDERSCORE	"_U_"
#define	ASYNCEMSMDB_SOGO_UNDERSCORE_LEN	3

#define	ASYNCEMSMDB_AT			'@'
#define	ASYNCEMSMDB_SOGO_AT		"_A_"
#define	ASYNCEMSMDB_SOGO_AT_LEN		3

#endif /* !DCESRV_ASYNCEMSMDB_H */
