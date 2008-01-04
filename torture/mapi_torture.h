/*
   OpenChange MAPI torture suite implementation.

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

#ifndef __MAPI_TORTURE_H__
#define	__MAPI_TORTURE_H__

#include <ldb.h>

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS		extern "C" {
#define __END_DECLS		}
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

#ifndef	MAX
#define MAX(p,q) (((p) >= (q)) ? (p) : (q))
#endif

__BEGIN_DECLS

int samdb_msg_add_string(struct ldb_context *, TALLOC_CTX *, 
			 struct ldb_message *, const char *, const char *);
int samdb_replace(struct ldb_context *, TALLOC_CTX *, struct ldb_message *);
struct dom_sid *dom_sid_add_rid(TALLOC_CTX *,  const struct dom_sid *, uint32_t);
bool encode_pw_buffer(uint8_t buffer[516], const char *, int);
void arcfour_crypt_blob(uint8_t *, int, const DATA_BLOB *);

__END_DECLS

#define	DEFAULT_PROFDB_PATH	"%s/.openchange/profiles.ldb"

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


#endif /* __MAPI_TORTURE_H__ */
