/* 
   Unix NSPI implementation

   test suite for nspi rpc operations

   Copyright (C) Julien Kerihuel 2005
   
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
#include <torture.h>
#include "ndr_exchange.h"
#include "libmapi/IMAPISession.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/mapi_proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
				struct dcerpc_pipe **p, 
				const struct dcerpc_interface_table *table);



BOOL torture_rpc_nspi(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct nspi_context	*nspi;

	mem_ctx = talloc_init("torture_rpc_nspi");
	
	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_nsp);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);

		return False;
	}

	nspi = nspi_bind(mem_ctx, p, cmdline_credentials);
	if (!nspi) {
		return False;
	}

	nspi->mem_ctx = mem_ctx;

	ret &= nspi_unbind(nspi);

	talloc_free(mem_ctx);

	return ret;
}
