/*
   OpenChange Storage Abstraction Layer library test tool

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009-2014

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

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include <talloc.h>
#include <core/ntstatus.h>
#include <popt.h>
#include <param.h>
#include <util/debug.h>

/**
   \file mapistore_test.c

   \brief Test mapistore implementation
 */


int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	int				retval;
	struct mapistore_context	*mstore_ctx;
	struct loadparm_context		*lp_ctx;
	void				*openchangedb_ctx;
	poptContext			pc;
	int				opt;
	const char			*opt_debug = NULL;
	const char			*opt_uri = NULL;
	uint32_t			context_id = 0;
	void				*folder_object;
	void				*child_folder_object;

	enum { OPT_DEBUG=1000, OPT_URI };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "debuglevel",	'd', POPT_ARG_STRING, NULL, OPT_DEBUG,	"set the debug level", NULL },
		{ "uri", 'U', POPT_ARG_STRING, NULL, OPT_URI, "set the backend URI", NULL},
		{ NULL, 0, 0, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "mapistore_test");
	lp_ctx = loadparm_init_global(true);
	setup_logging(NULL, DEBUG_STDOUT);
	
	pc = poptGetContext("mapistore_test", argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_URI:
			opt_uri = poptGetOptArg(pc);
			break;
		}
	}

	poptFreeContext(pc);

	if (!opt_uri) {
		DEBUG(0, ("Usage: mapistore_test -U <opt_uri> [OPTION]\n"));
		exit(0);
	}

	if (opt_debug) {
		lpcfg_set_cmdline(lp_ctx, "log level", opt_debug);
	}
	
	retval = mapistore_set_mapping_path("/tmp");
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("%s\n", mapistore_errstr(retval)));
		exit (1);
	}

	mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
	if (!mstore_ctx) {
		exit (1);
	}

	openchangedb_ctx = mapiproxy_server_openchangedb_init(lp_ctx);

	retval = mapistore_set_connection_info(mstore_ctx, openchangedb_ctx, "openchange");

	retval = mapistore_add_context(mstore_ctx, "openchange", opt_uri, -1, &context_id, &folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("%s\n", mapistore_errstr(retval)));
		exit (1);
	}


	{
		char			*mapistore_URI = NULL;
		struct backend_context	*backend_ctx;

		backend_ctx = mapistore_backend_lookup(mstore_ctx->context_list, context_id);
		if (!backend_ctx || !backend_ctx->indexing) {
			DEBUG(0, ("Invalid backend_ctx\n"));
			exit (1);
		}
		retval = mapistore_backend_get_path(NULL, backend_ctx, 0xdeadbeef, &mapistore_URI);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_backend_get_path: %s\n", mapistore_errstr(retval)));
			exit (1);
		}

		retval = mapistore_backend_get_path(NULL, backend_ctx, 0xbeefdead, &mapistore_URI);
		if (retval == MAPISTORE_SUCCESS) {
			DEBUG(0, ("mapistore_backend_get_path: error expected!\n"));
			exit (1);
		}
	}

	retval = mapistore_folder_open_folder(mstore_ctx, context_id, folder_object, NULL, 0x1,
					      &child_folder_object);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("mapistore_folder_open_folder: %s\n", mapistore_errstr(retval)));
		exit (1);
	}

	retval = mapistore_del_context(mstore_ctx, context_id);

	retval = mapistore_release(mstore_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("%s\n", mapistore_errstr(retval)));
		exit (1);
	}

	return 0;
}
