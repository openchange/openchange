/*
   OpenChange NSPI implementation.

   Copyright (C) Julien Kerihuel 2005.

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

#ifndef __NSPI_H__
#define	__NSPI_H__

struct nspi_context {
	struct dcerpc_pipe	*rpc_connection;
	struct policy_handle	handle;
	TALLOC_CTX		*mem_ctx;
	struct cli_credentials	*cred;
	struct STAT		*pStat;
	struct mapi_profile	*profile;
	struct SRowSet		*rowSet;
	char			*org;
	char			*org_unit;
	char			*servername;
	uint32_t		version;
};

#define	ORG		"/o="
#define	ORG_UNIT	"/ou="
#define	SERVER_DN	"/o=%s/ou=%s/cn=Configuration/cn=Servers/cn=%s"
#define	SERVERNAME	"/cn=Servers/cn="
#define	RECIPIENT_DN	"/o=%s/ou=%s/cn=Recipients/cn=%s"

#endif /* __NSPI_H__ */
