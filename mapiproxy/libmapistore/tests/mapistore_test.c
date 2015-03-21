/*
   OpenChange Storage Abstraction Layer library test tool

   OpenChange Project

   Copyright (C) Julien Kerihuel 2009

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
#include <talloc.h>
#include <popt.h>
#include <param.h>

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
	poptContext			pc;
	int				opt;
	const char			*opt_debug = NULL;
	uint32_t			context_id = 0;
	uint32_t			context_id2 = 0;
	uint32_t			context_id3 = 0;
	void				*root_folder;

	enum { OPT_DEBUG=1000 };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "debuglevel",	'd', POPT_ARG_STRING, NULL, OPT_DEBUG,	"set the debug level", NULL },
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
		}
	}

	poptFreeContext(pc);

	if (opt_debug) {
		lpcfg_set_cmdline(lp_ctx, "log level", opt_debug);
	}
	
	retval = mapistore_set_mapping_path("/tmp");
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "%s\n", mapistore_errstr(retval));
		exit (1);
	}

	mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
	if (!mstore_ctx) {
		exit (1);
	}

	retval = mapistore_add_context(mstore_ctx, "openchange", "sqlite:///tmp/test.db", -1, &context_id, &root_folder);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "%s\n", mapistore_errstr(retval));
		exit (1);
	}

	retval = mapistore_add_context(mstore_ctx, "openchange", "sqlite:///tmp/test2.db", -1, &context_id2, &root_folder);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "%s\n", mapistore_errstr(retval));
		exit (1);
	}

	OC_DEBUG(0, "Context ID: [1] = %d and [2] = %d\n", context_id, context_id2);

	retval = mapistore_add_context(mstore_ctx, "openchange", "fsocpf:///tmp/fsocpf", -1, &context_id3, &root_folder);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "%s\n", mapistore_errstr(retval));
		exit (1);
	}

	retval = mapistore_del_context(mstore_ctx, context_id);
	retval = mapistore_del_context(mstore_ctx, context_id2);
	retval = mapistore_del_context(mstore_ctx, context_id3);

	retval = mapistore_release(mstore_ctx);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "%s\n", mapistore_errstr(retval));
		exit(1);
	}

	return 0;
}
