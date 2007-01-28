/* 
   OpenChange MAPI implementation testsuite

   fetch mail from an Exchange server

   Copyright (C) Julien Kerihuel 2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "openchange.h"
#include <torture/torture.h>
#include "ndr_exchange.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/include/mapi_proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
				struct dcerpc_pipe **p, 
				const struct dcerpc_interface_table *table);

BOOL torture_rpc_mapi_fetchmail(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct emsmdb_context	*emsmdb;

	mem_ctx = talloc_init("torture_rpc_mapi_fetchmail");

	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_emsmdb);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);

		return False;
	}

	emsmdb = emsmdb_connect(mem_ctx, p, cmdline_credentials);
	if (!emsmdb) {
		return False;
	}
	
	emsmdb->mem_ctx = mem_ctx;

	OpenMsgStore(emsmdb, 0, NULL, "/o=OpenChange Organization/ou=First Administrative Group/cn=Recipients/cn=Administrator");

	talloc_free(mem_ctx);
	
	return (ret);
}
