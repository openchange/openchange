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

#include "utils/mapitest/mapitest.h"
#include "utils/openchange-tools.h"
#include "utils/dlinklist.h"

#include <popt.h>
#include <param.h>

#include "config.h"

/**
	\file
	Core of %mapitest implementation
*/

/**
   Initialize %mapitest structure
 */
static void mapitest_init(TALLOC_CTX *mem_ctx, struct mapitest *mt)
{
	mt->mem_ctx = mem_ctx;
	mt->stream = NULL;
	memset(&mt->info, 0, sizeof (mt->info));
	mt->session = NULL;

	mt->session = NULL;
	mt->mapi_all = true;
	mt->confidential = false;
	mt->no_server = false;
	mt->color = false;
	mt->online = false;
	mt->mapi_suite = false;
	mt->cmdline_calls = NULL;
	mt->cmdline_suite = NULL;
	mt->subunit_output = false;
}

/**
  Initialize %mapitest output stream

  \param mt pointer to mapitest context
  \param filename filename to write to (can be null, for output to stdout)
*/
static void mapitest_init_stream(struct mapitest *mt, const char *filename)
{
	if (filename == NULL) {
		mt->stream = fdopen(STDOUT_FILENO, "a");
	} else {
		mt->stream = fopen(filename, "w+");
	}

	if (mt->stream == NULL) {
		err(errno, "fdopen/fopen");
	}
}

/**
  Clean up %mapitest output stream

  \param mt pointer to mapitest context
*/
static void mapitest_cleanup_stream(struct mapitest *mt)
{
	fclose(mt->stream);
}


static bool mapitest_get_testnames(TALLOC_CTX *mem_ctx, struct mapitest *mt,
				   const char *parameter)
{
	struct mapitest_unit	*el = NULL;
	char			*temptok = NULL;

	if ((temptok = strtok((char *)parameter, ";")) == NULL) {
		fprintf(stderr, "Invalid testname list [;]\n");
		return false;
	}

	el = talloc_zero(mem_ctx, struct mapitest_unit);
	el->name = talloc_strdup(mem_ctx, temptok);
	DLIST_ADD(mt->cmdline_calls, el);

	while ((temptok = strtok(NULL, ";")) != NULL) {
		el = talloc_zero(mem_ctx, struct mapitest_unit);
		el->name = talloc_strdup(mem_ctx, temptok);
		DLIST_ADD_END(mt->cmdline_calls, el, struct mapitest_unit *);
	}

	return true;
}


static void mapitest_list(struct mapitest *mt, const char *name)
{
	struct mapitest_suite		*sel;
	struct mapitest_test		*el;

	/* List all tests */
	if (!name) {
		for (sel = mt->mapi_suite; sel; sel = sel->next) {
			printf("[*] Suite %s\n", sel->name);
			printf("===================================\n");
			printf("    * %-15s %s\n", "Name:", sel->name);
			printf("    * %-15s %5s\n", "Description:", sel->description);
			printf("    * Running Tests:\n");
			for (el = sel->tests; el; el = el->next) {
				printf("\t    - %-35s: %-10s\n", el->name, el->description);
			}
			printf("\n\n");
		}
	}
}


/**
 * Retrieve server specific information
 */
static bool mapitest_get_server_info(struct mapitest *mt,
				     char *opt_profname,
				     const char *password,
				     bool opt_dumpdata,
				     const char *opt_debug)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct emsmdb_info	*info = NULL;
	struct mapi_session	*session = NULL;

	/* if the user explicitly asked for just the no-server tests 
	to be run, then we're done here */
	if (mt->no_server == true) return 0;

	mem_ctx = talloc_named(NULL, 0, "mapitest_get_server_info");

	/* if no profile was specified, get the default */
	if (!opt_profname) {
		retval = GetDefaultProfile(mt->mapi_ctx, &opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			return false;
		}
	}
		

	/* debug options */
	SetMAPIDumpData(mt->mapi_ctx, opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(mt->mapi_ctx, atoi(opt_debug));
	}

	retval = MapiLogonEx(mt->mapi_ctx, &session, opt_profname, password);
	MAPIFreeBuffer(opt_profname);
	talloc_free(mem_ctx);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", retval);
		return false;
	}
	mt->session = session;
	mt->profile = session->profile;

	info = emsmdb_get_info(session);
	memcpy(&mt->info, info, sizeof (struct emsmdb_info));

	/* extract org and org_unit from info.mailbox */
	mt->org = x500_get_dn_element(mt->mem_ctx, info->szDNPrefix, "/o=");
	mt->org_unit = x500_get_dn_element(mt->mem_ctx, info->szDNPrefix, "/ou=");
	
	return true;
}



/**
 *  main program
 */
int main(int argc, const char *argv[])
{
	enum MAPISTATUS		retval;
	int32_t			num_tests_failed;
	TALLOC_CTX		*mem_ctx;
	struct mapitest		mt;
	poptContext		pc;
	int			opt;
	bool			ret;
	bool			opt_dumpdata = false;
	const char     		*opt_debug = NULL;
	const char		*opt_profdb = NULL;
	char			*opt_profname = NULL;
	const char		*opt_password = NULL;
	const char		*opt_outfile = NULL;
	char			*prof_tmp = NULL;
	bool			opt_leak_report = false;
	bool			opt_leak_report_full = false;

	enum { OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD,
	       OPT_CONFIDENTIAL, OPT_OUTFILE, OPT_MAPI_CALLS,
	       OPT_NO_SERVER, OPT_LIST_ALL, OPT_DUMP_DATA,
	       OPT_DEBUG, OPT_COLOR, OPT_SUBUNIT, OPT_LEAK_REPORT,
	       OPT_LEAK_REPORT_FULL };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "database",        'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB,       "set the profile database", NULL },
		{ "profile",         'p', POPT_ARG_STRING, NULL, OPT_PROFILE,          "set the profile name", NULL },
		{ "password",        'p', POPT_ARG_STRING, NULL, OPT_PASSWORD,         "set the profile or account password", NULL },
		{ "confidential",      0, POPT_ARG_NONE,   NULL, OPT_CONFIDENTIAL,     "remove any sensitive data from the report", NULL },
		{ "color",             0, POPT_ARG_NONE,   NULL, OPT_COLOR,            "color MAPI retval", NULL },
#if HAVE_SUBUNIT
		{ "subunit",           0, POPT_ARG_NONE,   NULL, OPT_SUBUNIT,          "output in subunit protocol format", NULL },
#endif
		{ "outfile",         'o', POPT_ARG_STRING, NULL, OPT_OUTFILE,          "set the report output file", NULL },
		{ "mapi-calls",        0, POPT_ARG_STRING, NULL, OPT_MAPI_CALLS,       "test custom ExchangeRPC tests", NULL },
		{ "list-all",          0, POPT_ARG_NONE,   NULL, OPT_LIST_ALL,         "list suite and tests - names and description", NULL },
		{ "no-server",         0, POPT_ARG_NONE,   NULL, OPT_NO_SERVER,        "only run tests that do not require server connection", NULL },
		{ "dump-data",         0, POPT_ARG_NONE,   NULL, OPT_DUMP_DATA,        "dump the hex data", NULL },
		{ "debuglevel",      'd', POPT_ARG_STRING, NULL, OPT_DEBUG,            "set debug level", NULL },
		{ "leak-report",       0, POPT_ARG_NONE,   NULL, OPT_LEAK_REPORT,      "enable talloc leak reporting on exit", NULL },
		{ "leak-report-full",  0, POPT_ARG_NONE,   NULL, OPT_LEAK_REPORT_FULL, "enable full talloc leak reporting on exit", NULL },
		POPT_OPENCHANGE_VERSION
		{ NULL, 0, 0, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "mapitest");
	mapitest_init(mem_ctx, &mt);
	mapitest_register_modules(&mt);

	pc = poptGetContext("mapitest", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DUMP_DATA:
			opt_dumpdata = true;
			break;
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			prof_tmp = poptGetOptArg(pc);
			opt_profname = talloc_strdup(mem_ctx, prof_tmp);
			free(prof_tmp);
			prof_tmp = NULL;
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_CONFIDENTIAL:
			mt.confidential = true;
			break;
		case OPT_OUTFILE:
			opt_outfile = poptGetOptArg(pc);
			break;
		case OPT_MAPI_CALLS:
			ret = mapitest_get_testnames(mem_ctx, &mt, poptGetOptArg(pc));
			if (ret == false) exit (-1);
			mt.mapi_all = false;
			break;
		case OPT_NO_SERVER:
			mt.no_server = true;
			break;
		case OPT_COLOR:
			mt.color = true;
			break;
		case OPT_SUBUNIT:
			mt.subunit_output = true;
			break;
		case OPT_LIST_ALL:
			mapitest_list(&mt, NULL);
			talloc_free(mem_ctx);
			poptFreeContext(pc);
			return 0;
			break;
		case OPT_LEAK_REPORT:
			opt_leak_report = true;
			talloc_enable_leak_report();
			break;
		case OPT_LEAK_REPORT_FULL:
			opt_leak_report_full = true;
			talloc_enable_leak_report_full();
			break;
		}
	}

	poptFreeContext(pc);

	/* Sanity check */
	if (mt.cmdline_calls && (mt.mapi_all == true)) {
		fprintf(stderr, "mapi-calls and mapi-all can't be set at the same time\n");
		return -1;
	}

	/* Initialize MAPI subsystem */
	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	retval = MAPIInitialize(&(mt.mapi_ctx), opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		return -2;
	}

	mapitest_init_stream(&mt, opt_outfile);
	
	mt.online = mapitest_get_server_info(&mt, opt_profname, opt_password,
					     opt_dumpdata, opt_debug);

	mapitest_print_headers(&mt);

	/* Do not run any tests if we couldn't find a profile or if
	 * server is offline and connection to server was implicitly
	 * specified */
	if (!opt_profname && mt.online == false && mt.no_server == false) {
		fprintf(stderr, "No MAPI profile found for online tests\n");
		return -2;
	}

	/* Run custom tests */
	if (mt.cmdline_calls) {
		struct mapitest_unit	*el;
		
		for (el = mt.cmdline_calls; el; el = el->next) {
			printf("[*] %s\n", el->name);
			mapitest_run_test(&mt, el->name);
		}
	} else {
		mapitest_run_all(&mt);
	}

	num_tests_failed = mapitest_stat_dump(&mt);

	mapitest_cleanup_stream(&mt);

	/* Uninitialize and free memory */
	MAPIUninitialize(mt.mapi_ctx);
	talloc_free(mt.mem_ctx);

	if (opt_leak_report) {
		talloc_report(NULL, stdout);
	}

	if (opt_leak_report_full) {
		talloc_report_full(NULL, stdout);
	}

	return num_tests_failed;
}
