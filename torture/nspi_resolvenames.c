/* 
   OpenChange MAPI implementation testsuite

   query the WAB and attempts to resolve the given names

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
#include "libmapi/include/nspi.h"
#include "libmapi/include/emsmdb.h"
#include "libmapi/include/mapi_proto.h"

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
                                struct dcerpc_pipe **p, 
                                const struct dcerpc_interface_table *table);


BOOL torture_rpc_nspi_resolvenames(struct torture_context *torture)
{
	NTSTATUS                status;
	struct dcerpc_pipe      *p;
	TALLOC_CTX              *mem_ctx;
	BOOL                    ret = True;
	struct nspi_context     *nspi;
	struct SPropTagArray    *SPropTagArray;
	const char *username = lp_parm_string(-1, "exchange", "resolvename");
	char *tmp;
	char **usernames;
	int j;

	mem_ctx = talloc_init("torture_rpc_check_recipients");

	if (!username) {
		DEBUG(0,("Sepcify the username to resolve with exchange:resolvename\n"));
		talloc_free(mem_ctx);
		return False;
	}

	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_nsp);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return False;
	}

	nspi = nspi_bind(mem_ctx, p, cmdline_credentials);
	if (!nspi) {
		talloc_free(mem_ctx);
		return False;
	}

	nspi->mem_ctx = mem_ctx;

	SPropTagArray = set_SPropTagArray(nspi->mem_ctx, 0xb,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME,
					  PR_ADDRTYPE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_EMAIL_ADDRESS,
					  PR_SEND_INTERNET_ENCODING,
					  PR_SEND_RICH_INFO,
					  PR_SEARCH_KEY,
					  PR_TRANSMITTABLE_DISPLAY_NAME,
					  PR_7BIT_DISPLAY_NAME);

	if ((tmp = strtok((char *)username, ",")) == NULL){
		DEBUG(2, ("Invalid usernames string format\n"));
		exit (1);
	}

	usernames = talloc_array(nspi->mem_ctx, char *, 2);
	usernames[0] = strdup(tmp);

	for (j = 1; (tmp = strtok(NULL, ",")) != NULL; j++) {
		     usernames = talloc_realloc(nspi->mem_ctx, usernames, char *, j+2);
		     usernames[j] = strdup(tmp);
	}
	usernames[j] = 0;

	ret &= nspi_ResolveNames(nspi, j, (const char **)usernames, SPropTagArray);

	ret &= nspi_unbind(nspi);

	talloc_free(mem_ctx);

	return ret;
}
