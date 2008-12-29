/*
   OpenChange Exchange Administration library.

   Copyright (C) Julien Kerihuel 2007.

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

/* OpenChange includes */
#include <libmapi/libmapi.h>
#include <libmapiadmin/proto.h>

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

/*
 * prototypes from Samba4
 */

__BEGIN_DECLS
struct ldb_dn *samdb_search_dn(struct ldb_context *, TALLOC_CTX *, struct ldb_dn *, const char *, ...) _PRINTF_ATTRIBUTE(4,5);
int samdb_msg_add_string(struct ldb_context *, TALLOC_CTX *,
			 struct ldb_message *, const char *, const char *);
int samdb_replace(struct ldb_context *, TALLOC_CTX *, struct ldb_message *);
struct dom_sid *dom_sid_add_rid(TALLOC_CTX *, const struct dom_sid *, uint32_t);
bool encode_pw_buffer(uint8_t buffer[516], const char *, int);
void arcfour_crypt_blob(uint8_t *, int, const DATA_BLOB *);
__END_DECLS

#define	DEFAULT_PROFDB_PATH	"%s/.openchange/profiles.ldb"
#define	MAPIADMIN_DEBUG_STR	"[%s:%d]: %s %s\n", __FUNCTION__, __LINE__

#endif /* __LIBMAPIADMIN_H__ */
