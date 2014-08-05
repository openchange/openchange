/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2009.
   Copyright (C) Fabien Le Mentec 2007.

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

#ifndef __MAPI_PROFILE_H
#define __MAPI_PROFILE_H


#include <talloc.h>


/* forward decls */
struct cli_credentials;
struct ldb_context;


/* mapi profile
 */
struct mapi_profile
{
	struct mapi_context	*mapi_ctx;
	struct cli_credentials	*credentials;
	char		*profname;
	const char	*org;
	const char	*ou;
	const char	*username;
	const char	*password;
	const char	*mailbox;
	const char	*workstation;
	const char	*homemdb;
	const char	*domain;
	const char	*realm;
	const char	*server;
	const char	*localaddr;
	const char	*server_name;
	bool		seal;
	uint32_t	codepage;
	uint32_t	language;
	uint32_t	method;
	uint32_t	exchange_version;
	const char	*kerberos;
	bool		roh;
	bool		roh_tls;
	const char	*roh_rpc_proxy_server;
	uint32_t	roh_rpc_proxy_port;
};

typedef int (*mapi_profile_callback_t)(struct PropertyRowSet_r *, const void *);

#define	OC_PROFILE_NOPASSWORD	1

#endif /* ! __MAPI_PROFILE_H__ */
