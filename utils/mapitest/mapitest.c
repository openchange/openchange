/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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
#include <samba/popt.h>
#include <param.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>

/**
 * Initialize mapitest structure
 */
static void mapitest_init(TALLOC_CTX *mem_ctx, struct mapitest *mt)
{
	mt->mem_ctx = mem_ctx;
	mt->stream = NULL;
	memset(&mt->info, 0, sizeof (mt->info));

	mt->confidential = false;
	mt->mapi_all = false;
	mt->mapi_calls = false;
}


/**
 * Set the stream
 */
static void mapitest_init_stream(struct mapitest *mt, const char *filename)
{
	if (filename == NULL) {
		mt->stream = fdopen(STDOUT_FILENO, "r+");
	} else {
		mt->stream = fopen(filename, "w+");
	}

	if (mt->stream == NULL) {
		err(errno, "fdopen/fopen");
	}
}


/**
 * Retrieve server specific information
 */
static int mapitest_get_server_info(struct mapitest *mt,
				    const char *profdb,
				    const char *profname,
				    const char *password)
{
	enum MAPISTATUS		retval;
	struct emsmdb_info	*info = NULL;
	struct mapi_session	*session = NULL;

	if (!profdb) {
		profdb = talloc_asprintf(mt->mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}
	retval = MAPIInitialize(profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		return -1;
	}

	if (!profname) {
		retval = GetDefaultProfile(&profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			return -1;
		}
	}

	retval = MapiLogonEx(&session, profname, password);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", retval);
		return -1;
	}

	info = emsmdb_get_info();
	memcpy(&mt->info, info, sizeof (struct emsmdb_info));

	/* extract org and org_unit from info.mailbox */
	mt->org = x500_get_dn_element(mt->mem_ctx, info->mailbox, "/o=");
	mt->org_unit = x500_get_dn_element(mt->mem_ctx, info->mailbox, "/ou=");
	
	return 0;
}

/**
 * Program entry point
 */
int main(int argc, const char *argv[])
{
	TALLOC_CTX	*mem_ctx;
	struct mapitest	mt;
	poptContext	pc;
	int		opt;
	const char	*opt_profdb = NULL;
	const char	*opt_profname = NULL;
	const char	*opt_password = NULL;
	const char	*opt_outfile = NULL;

	enum { OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_USERNAME, OPT_PASSWORD,
	       OPT_CONFIDENTIAL, OPT_OUTFILE, OPT_MAPI_ALL, OPT_MAPI_CALLS,
	       OPT_MAPIADMIN_ALL };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name"},
		{"username", 'u', POPT_ARG_STRING, NULL, OPT_USERNAME, "set the account username"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile or account password"},
		{"confidential", 0, POPT_ARG_NONE, NULL, OPT_CONFIDENTIAL, "remove any sensitive data from the report"},
		{"outfile", 'o', POPT_ARG_STRING, NULL, OPT_OUTFILE, "set oct results output file"},
		{"mapi-all", 0, POPT_ARG_NONE, NULL, OPT_MAPI_ALL, "full test of the mapi implementation"},
		{"mapi-calls", 0, POPT_ARG_NONE, NULL, OPT_MAPI_CALLS, "test mapi atomic calls"},
		{"mapiadmin-all", 0, POPT_ARG_NONE, NULL, OPT_MAPIADMIN_ALL, "full test of the mapiadmin implementation"},
		{ NULL }
	};

	mem_ctx = talloc_init("mapitest");
	mapitest_init(mem_ctx, &mt);

	pc = poptGetContext("oct", argc, argv, long_options, 0);
	
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = poptGetOptArg(pc);
			break;
		case OPT_CONFIDENTIAL:
			mt.confidential = true;
			break;
		case OPT_OUTFILE:
			opt_outfile = poptGetOptArg(pc);
			break;
		case OPT_MAPI_ALL:
			break;
		case OPT_MAPI_CALLS:
			mt.mapi_calls = true;
			break;
		case OPT_MAPIADMIN_ALL:
			break;
		case OPT_USERNAME:
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		}
	}

	mapitest_init_stream(&mt, opt_outfile);
	mapitest_get_server_info(&mt, opt_profdb, opt_profname, opt_password);

	/* print mapitest report headers */
	mapitest_print_headers_start(&mt);
	mapitest_print_headers_info(&mt);
	mapitest_print_server_info(&mt);
	mapitest_print_options(&mt);
	mapitest_print_headers_end(&mt);

	/* test atomic mapi calls */
	if (mt.mapi_calls == true) {
		mapitest_calls(&mt);
	}

	if (opt_outfile) fclose(mt.stream);

	MAPIUninitialize();

	return 0;
}
