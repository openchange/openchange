/*
   OpenChange Exchange Administration library.

   Copyright (C) Julien Kerihuel 2007-2010.

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

#ifndef	__LIBMAPIADMIN_H__
#define	__LIBMAPIADMIN_H__

#define	_GNU_SOURCE	1

struct mapiadmin_ctx;

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* Samba4 includes */
#include <stdint.h>
#include <talloc.h>
#include <ldb.h>
#include <tevent.h>
#ifndef _PUBLIC_
#define _PUBLIC_
#endif
#include <util/debug.h>

/* OpenChange includes */
#include "libmapi/libmapi.h"

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)

#ifndef	__BEGIN_DECLS
#ifdef	__cplusplus
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

#ifndef	MAX
#define	MAX(p,q) (((p) >= (q)) ? (p) : (q))
#endif

/**
	\file
	Structures for MAPI admin functions
*/

struct test_join {
	struct dcerpc_pipe		*p;
	struct policy_handle		user_handle;
	struct libnet_JoinDomain	*libnet_r;
	struct dom_sid			*dom_sid;
	const char			*dom_netbios_name;
	const char			*dom_dns_name;
	struct dom_sid			*user_sid;
	struct GUID			user_guid;
	const char			*netbios_name;
};

/**
	MAPI admin function context
*/
struct mapiadmin_ctx
{
	struct mapi_session	*session;
	const char		*username;
	const char		*password;
	const char		*fullname;
	const char		*description;
	const char		*comment;
	struct test_join	*user_ctx;
	const char		*binding;
	const char		*dc_binding;
	struct policy_handle	*handle;
};


__BEGIN_DECLS

/* The following definitions come from samba4 framework */
struct ldb_dn *samdb_search_dn(struct ldb_context *, TALLOC_CTX *, struct ldb_dn *, const char *, ...) _PRINTF_ATTRIBUTE(4,5);
struct dom_sid *dom_sid_add_rid(TALLOC_CTX *, const struct dom_sid *, uint32_t);
bool encode_pw_buffer(uint8_t buffer[516], const char *, int);
void arcfour_crypt_blob(uint8_t *, int, const DATA_BLOB *);

/* The following public definitions come from libmapiadmin/mapiadmin.c */
struct mapiadmin_ctx *mapiadmin_init(struct mapi_session *);
enum MAPISTATUS mapiadmin_release(struct mapiadmin_ctx *);

/* The following public definitions come from libmapiadmin/mapiadmin_user.c */
enum MAPISTATUS mapiadmin_user_extend(struct mapiadmin_ctx *);
enum MAPISTATUS mapiadmin_user_add(struct mapiadmin_ctx *);
enum MAPISTATUS mapiadmin_user_del(struct mapiadmin_ctx *);
enum MAPISTATUS mapiadmin_user_mod(struct mapiadmin_ctx *);

__END_DECLS

#define	DEFAULT_PROFDB_PATH	"%s/.openchange/profiles.ldb"
#define	MAPIADMIN_DEBUG_STR	"[%s:%d]: %s %s\n", __FUNCTION__, __LINE__

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif /* __LIBMAPIADMIN_H__ */
