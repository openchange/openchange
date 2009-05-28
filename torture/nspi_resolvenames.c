/*
   OpenChange MAPI torture suite implementation.

   Query the WAB and attempts to resolve the given names

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

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>

bool torture_rpc_nspi_resolvenames(struct torture_context *torture)
{
	NTSTATUS                status;
	enum MAPISTATUS		retval;
	struct dcerpc_pipe      *p;
	TALLOC_CTX              *mem_ctx;
	struct mapi_session	*session = NULL;
	bool                    ret = true;
	struct SPropTagArray    *SPropTagArray;
	struct SRowSet		*rowset = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	const char		*profdb;
	char			*profname;
	const char		*username = lp_parm_string(torture->lp_ctx, NULL, "exchange", "resolvename");
	const char		*password = lp_parm_string(torture->lp_ctx, NULL, "mapi", "password");
	uint32_t		unicode = lp_parm_int(torture->lp_ctx, NULL, "mapi", "unicode", 0);
	char *tmp;
	char **usernames;
	int j;

	mem_ctx = talloc_named(NULL, 0, "torture_rpc_nspi_resolvenames");

	if (!username) {
		DEBUG(0,("Specify the usernames to resolve with exchange:resolvename\n"));
		talloc_free(mem_ctx);
		return false;
	}

	status = torture_rpc_connection(torture, &p, &ndr_table_exchange_nsp);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* init mapi */
	profdb = lp_parm_string(torture->lp_ctx, NULL, "mapi", "profile_store");
	if (!profdb) {
		profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB_PATH, getenv("HOME"));
		if (!profdb) {
			DEBUG(0, ("Specify a valie MAPI profile store\n"));
			return false;
		}
	}
	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	/* profile name */
	profname = talloc_strdup(mem_ctx, lp_parm_string(torture->lp_ctx, NULL, "mapi", "profile"));
	if (!profname) {
		retval = GetDefaultProfile(&profname);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("Please specify a valid profile name\n"));
			return false;
		}
	}
	
	retval = MapiLogonProvider(&session, profname, password, PROVIDER_ID_NSPI);
	talloc_free(profname);
	mapi_errstr("MapiLogonProvider", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

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

	retval = ResolveNames(session, (const char **)usernames, SPropTagArray, &rowset, &flaglist, unicode?MAPI_UNICODE:0);
	mapi_errstr("ResolveNames", GetLastError());
	if (retval != MAPI_E_SUCCESS) return false;

	mapidump_Recipients((const char **)usernames, rowset, flaglist);

	retval = MAPIFreeBuffer(rowset);
	mapi_errstr("MAPIFreeBuffer: rowset", GetLastError());
	
	retval = MAPIFreeBuffer(flaglist);
	mapi_errstr("MAPIFreeBuffer: flaglist", GetLastError());
	
	MAPIUninitialize();

	talloc_free(mem_ctx);

	return ret;
}
