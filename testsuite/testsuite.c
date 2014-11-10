/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2014.

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

#include "testsuite.h"
#include "libmapi/version.h"

#include <popt.h>
#include <talloc.h>
#include <param.h>

/* options from command line */
struct loadparm_context *cmdline_lp_ctx = NULL;

static void popt_openchange_version_callback(poptContext con,
                                             enum poptCallbackReason reason,
                                             const struct poptOption *opt,
                                             const char *arg,
                                             const void *data)
{
        switch (opt->val) {
        case 'V':
                printf("Version %s\n", OPENCHANGE_VERSION_STRING);
                exit (0);
        }
}


enum {OPT_OPTION=1, OPT_DEBUG_LEVEL, OPT_LEAK_REPORT,OPT_LEAK_REPORT_FULL};

struct poptOption popt_openchange_version[] = {
        { NULL, '\0', POPT_ARG_CALLBACK, (void *)popt_openchange_version_callback, '\0', NULL, NULL },
        { "version", 'V', POPT_ARG_NONE, NULL, 'V', "Print version ", NULL },
        POPT_TABLEEND
};

struct poptOption popt_openchange_testsuite_options[] = {
	POPT_AUTOHELP
	{ "debuglevel",	    'd', POPT_ARG_STRING, NULL, OPT_DEBUG_LEVEL, "Set debug level", "DEBUGLEVEL" },
	{ "option",           0, POPT_ARG_STRING, NULL, OPT_OPTION, "Set smb.conf option from command line", "name=value" },
	{ "leak-report",      0, POPT_ARG_NONE, NULL, OPT_LEAK_REPORT, "enable talloc leak reporting on exit", NULL },
	{ "leak-report-full", 0, POPT_ARG_NONE, NULL, OPT_LEAK_REPORT_FULL, "enable full talloc leak reporting on exit", NULL },
	{ NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_openchange_version, 0, "Common openchange options:", NULL },
	{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
};


int main(int argc, const char *argv[])
{
	poptContext	pc;
	int		opt;
	const char	*arg;
	SRunner		*sr;
	int		nf;

	cmdline_lp_ctx = loadparm_init_global(false);

	pc = poptGetContext(NULL, argc, argv, popt_openchange_testsuite_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DEBUG_LEVEL:
			arg = poptGetOptArg(pc);
			lpcfg_set_cmdline(cmdline_lp_ctx, "log level", arg);
			break;
		case OPT_OPTION:
			lpcfg_set_option(cmdline_lp_ctx, poptGetOptArg(pc));
			break;
		case OPT_LEAK_REPORT:
			talloc_enable_leak_report();
			break;
		case OPT_LEAK_REPORT_FULL:
			talloc_enable_leak_report_full();
			break;
		default:
			poptPrintUsage(pc, stderr, 0);
			return EXIT_FAILURE;
		}
	}
	poptFreeContext(pc);

	if (lpcfg_configfile(cmdline_lp_ctx) == NULL) {
		lpcfg_load_default(cmdline_lp_ctx);
	}

	sr = srunner_create(suite_create("OpenChange unit testing"));

	/* libmapi */
	srunner_add_suite(sr, libmapi_property_suite());
	/* libmapiproxy */
	srunner_add_suite(sr, mapiproxy_openchangedb_mysql_suite());
	srunner_add_suite(sr, mapiproxy_openchangedb_ldb_suite());
	srunner_add_suite(sr, mapiproxy_openchangedb_multitenancy_mysql_suite());
	srunner_add_suite(sr, mapiproxy_openchangedb_logger_suite());
	/* libmapistore */
	srunner_add_suite(sr, mapistore_namedprops_suite());
	srunner_add_suite(sr, mapistore_namedprops_mysql_suite());
	srunner_add_suite(sr, mapistore_namedprops_tdb_suite());
	srunner_add_suite(sr, mapistore_indexing_mysql_suite());
	srunner_add_suite(sr, mapistore_indexing_tdb_suite());
	/* mapiproxy */
	srunner_add_suite(sr, mapiproxy_util_mysql_suite());

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
