/*
   OpenChange MAPI implementation.

   Copyright (C) Jelmer Vernooij 2005.
   Copyright (C) Julien Kerihuel 2008.

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

#ifndef __EMSMDB_H__
#define	__EMSMDB_H__

struct emsmdb_info {
	char			*szDisplayName;
	char			*szDNPrefix;
	uint32_t		pcmsPollsMax;
	uint32_t		pcRetry;
	uint32_t		pcmsRetryDelay;
	uint32_t		picxr;
	uint16_t		rgwServerVersion[3];
};

struct emsmdb_context {
	struct dcerpc_pipe     	*rpc_connection;
	struct policy_handle   	handle;
	struct nspi_context    	*nspi;
	struct cli_credentials	*cred;
	TALLOC_CTX	       	*mem_ctx;
	struct EcDoRpc_MAPI_REQ	**cache_requests;
	uint32_t	       	cache_size;
	uint8_t			cache_count;
	uint16_t	       	prop_count;
	enum MAPITAGS	       	*properties;
	uint16_t     	       	max_data;
	bool		       	setup;
	struct emsmdb_info	info;
};

#define	MAILBOX_PATH	"/o=%s/ou=%s/cn=Recipients/cn=%s"

#endif /* __EMSMDB_H__ */
