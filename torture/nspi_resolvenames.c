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

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

/* FIXME: Should be part of Samba's data: */
NTSTATUS torture_rpc_connection(TALLOC_CTX *parent_ctx, 
                                struct dcerpc_pipe **p, 
                                const struct dcerpc_interface_table *table);


BOOL torture_rpc_nspi_resolvenames(struct torture_context *torture)
{
	NTSTATUS                status;
	MAPISTATUS		retval;
	struct dcerpc_pipe      *p;
	TALLOC_CTX              *mem_ctx;
	struct mapi_session	*session;
	BOOL                    ret = True;
	struct SPropTagArray    *SPropTagArray;
	struct SRowSet		*rowset = NULL;
	struct FlagList		*flaglist = NULL;
	const char		*profdb;
	const char		*profname;
	const char *username = lp_parm_string(-1, "exchange", "resolvename");
	char *tmp;
	char **usernames;
	int j;

	mem_ctx = talloc_init("torture_rpc_nspi_resolvenames");

	if (!username) {
		DEBUG(0,("Specify the usernames to resolve with exchange:resolvename\n"));
		talloc_free(mem_ctx);
		return False;
	}

	status = torture_rpc_connection(mem_ctx, &p, &dcerpc_table_exchange_nsp);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return False;
	}

	/* init mapi */
	profdb = lp_parm_string(-1, "mapi", "profile_store");
	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	/* profile name */
	profname = lp_parm_string(-1, "mapi", "profile");
	if (profname == 0) {
		DEBUG(0, ("Please specify a valid profile name\n"));
		return False;
	}

	retval = MapiLogonEx(&session, profname);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0xd,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME,
					  PR_ADDRTYPE,
					  PR_GIVEN_NAME,
					  PR_SMTP_ADDRESS,
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

	usernames = talloc_array(mem_ctx, char *, 2);
	usernames[0] = strdup(tmp);

	for (j = 1; (tmp = strtok(NULL, ",")) != NULL; j++) {
		     usernames = talloc_realloc(mem_ctx, usernames, char *, j+2);
		     usernames[j] = strdup(tmp);
	}
	usernames[j] = 0;

	retval = ResolveNames((const char **)usernames, SPropTagArray, &rowset, &flaglist, 0);
	mapi_errstr("ResolveNames", GetLastError());
	if (retval != MAPI_E_SUCCESS) return False;

	mapidump_Recipients((const char **)usernames, rowset, flaglist);

	retval = MAPIFreeBuffer(rowset);
	mapi_errstr("MAPIFreeBuffer: rowset", GetLastError());
	
	retval = MAPIFreeBuffer(flaglist);
	mapi_errstr("MAPIFreeBuffer: flaglist", GetLastError());
	
	MAPIUninitialize();

	talloc_free(mem_ctx);

	return ret;
}
