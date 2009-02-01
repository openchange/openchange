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
	struct loadparm_context	*lp_ctx;
	void			*conf_ctx;
	void			*users_ctx;
	TALLOC_CTX		*mem_ctx;
};


struct exchange_emsmdb_session {
	uint32_t			pullTimeStamp;
	struct mpm_session		*session;
	struct exchange_emsmdb_session	*prev;
	struct exchange_emsmdb_session	*next;
};


#define	EMSMDB_PCMSPOLLMAX	60000
#define	EMSMDB_PCRETRY		6
#define	EMSMDB_PCRETRYDELAY	10000

__BEGIN_DECLS

NTSTATUS	samba_init_module(void);

/* definitions from emsmdbp.c */
struct emsmdbp_context	*emsmdbp_init(struct loadparm_context *);
bool			emsmdbp_destructor(void *);
bool			emsmdbp_verify_user(struct dcesrv_call_state *, struct emsmdbp_context *);
bool			emsmdbp_verify_userdn(struct dcesrv_call_state *, struct emsmdbp_context *, const char *, struct ldb_message **);

__END_DECLS

#endif	/* __DCESRV_EXCHANGE_EMSMDB_H */
