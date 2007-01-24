/* 
   OpenChange NSPI Profile implementation

   create a MAPI profile

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
#include <torture/torture.h>
#include "ndr_exchange.h"
#include "libmapi/include/nspi.h"
#include "libmapi/include/mapi_proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
				struct dcerpc_pipe **p, 
				const struct dcerpc_interface_table *table);



BOOL torture_rpc_nspi_profile(struct torture_context *torture)
{
	NTSTATUS		status;
	struct dcerpc_pipe	*p;
	TALLOC_CTX		*mem_ctx;
	BOOL			ret = True;
	struct nspi_context	*nspi;
	struct SPropTagArray	*SPropTagArray;

	mem_ctx = talloc_init("torture_rpc_nspi_profile");
	
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

	ret &= nspi_GetHierarchyInfo(nspi);

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0xc,
					  PR_DISPLAY_NAME,
					  PR_OFFICE_TELEPHONE_NUMBER,
					  PR_OFFICE_LOCATION,
					  PR_TITLE,
					  PR_COMPANY_NAME,
					  PR_ACCOUNT,
					  PR_ADDRTYPE,
					  PR_ENTRYID,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_INSTANCE_KEY,
					  PR_EMAIL_ADDRESS);
	ret &= nspi_GetMatches(nspi, SPropTagArray);

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 9,
					  PR_ACCOUNT,
					  PR_EMS_AB_EXPIRATION_TIME,
					  PR_DISPLAY_NAME,
					  PR_EMAIL_ADDRESS,
					  PR_DISPLAY_TYPE,
					  PR_EMS_AB_HOME_MDB,
					  PR_ATTACH_NUM,
					  PR_PROFILE_HOME_SERVER_ADDRS,
					  PR_EMS_AB_PROXY_ADDRESSES
					  );
	ret &= nspi_QueryRows(nspi, SPropTagArray);

	ret &= nspi_DNToEph(nspi);

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0x1,
					  PR_EMS_AB_NETWORK_ADDRESS);
	ret &= nspi_GetProps(nspi, SPropTagArray);

	ret &= nspi_unbind(nspi);

	talloc_free(mem_ctx);

	return ret;
}
