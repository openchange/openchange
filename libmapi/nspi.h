/*
 *  OpenChange NSPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __NSPI_H__
#define	__NSPI_H__

struct mapi_profile {
	uint32_t	*instance_key;
};

struct nspi_context {
	struct dcerpc_pipe	*rpc_connection;
	struct policy_handle	handle;
	TALLOC_CTX		*mem_ctx;
	struct cli_credentials	*cred;
	struct MAPI_SETTINGS	*settings;
	struct mapi_profile	*profile;
	struct SRowSet		*rowSet;
	char			*org;
	char			*org_unit;
};

#define	ORG		"/o="
#define	ORG_UNIT	"/ou="
#define	SERVER_DN	"/o=%s/ou=%s/cn=Configuration/cn=Servers/cn=%s"

#endif /* __NSPI_H__ */
