/*
   List the system and special folders for the user mailbox

   OpenChange Project

   Copyright (C) 2014 Kamen Mazdrashki <kmazdrashki@zentyal.com>

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

#include "../mapiproxy/libmapistore/mapistore.h"
#include "../mapiproxy/libmapistore/mapistore_errors.h"
#include "../mapiproxy/libmapiproxy/libmapiproxy.h"
#include <talloc.h>
#include <popt.h>
#include <param.h>
#include <util/debug.h>


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

struct poptOption popt_openchange_version[] = {
        { NULL, '\0', POPT_ARG_CALLBACK, (void *)popt_openchange_version_callback, '\0', NULL, NULL },
        { "version", 'V', POPT_ARG_NONE, NULL, 'V', "Print version ", NULL },
        POPT_TABLEEND
};

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	struct openchangedb_context 	*oc_ctx = NULL;
	struct mapistore_context	*mstore_ctx = NULL;
	struct mapistore_contexts_list  *context;
	struct mapistore_contexts_list  *contexts_list = NULL;
	enum mapistore_error		ret;
	enum MAPISTATUS			retval;
	poptContext			pc;
	int				opt;
	const char			*opt_debug = NULL;
	const char			*opt_username = NULL;

	enum {
		OPT_DEBUG = 1000,
		OPT_USERNAME
	};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "debuglevel",	'd', POPT_ARG_STRING, NULL, OPT_DEBUG,	"set the debug level", NULL },
		{ "username",	'u', POPT_ARG_STRING, NULL, OPT_USERNAME, "mapistore user to enum contexts for", NULL },
		{ NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_openchange_version, 0, "Common openchange options:", NULL },
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "mapistore_tool");

	pc = poptGetContext("mapistore_tool", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_USERNAME:
			opt_username = poptGetOptArg(pc);
			break;
		}
	}

	/**
	 * Sanity checks
	 */

	if (!opt_username) {
		poptPrintUsage(pc, stderr, 0);
		return 1;
	}

	/* Initialize configuration */
	lp_ctx = loadparm_init(mem_ctx);
	if (opt_debug) {
		lpcfg_set_cmdline(lp_ctx, "log level", opt_debug);
	}
	lpcfg_load_default(lp_ctx);

	retval = openchangedb_initialize(mem_ctx, lp_ctx, &oc_ctx);

	/* Initialize mapistore */
	mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
	if (mstore_ctx == NULL) {
		perror("Failed to initialize mapistore\n");
		talloc_free(mem_ctx);
		return 1;
	}

//	mapistore_set_connection_info(mstore_ctx, NULL, oc_ctx, opt_username);
	mstore_ctx->conn_info = talloc_zero(mstore_ctx, struct mapistore_connection_info);
	mstore_ctx->conn_info->oc_ctx = talloc_reference(mstore_ctx->conn_info, oc_ctx);
	mstore_ctx->conn_info->username = talloc_strdup(mem_ctx, opt_username);

	ret = mapistore_list_contexts_for_user(mstore_ctx, opt_username, mem_ctx, &contexts_list);
	context = contexts_list;
	while (context) {
		printf("{\n");
		printf("  name: '%s',\n", context->name);
		printf("  url:  '%s',\n", context->url);
		printf("  role: '%d',\n", context->role);
		printf("  main_folder:  '%d'\n", context->main_folder);
		printf("}\n");
		context = context->next;
	}

	ret = mapistore_release(mstore_ctx);

	talloc_free(mem_ctx);

	return 0;
}
