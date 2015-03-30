/*
   mapistore notification client helper

   OpenChange Project

   Copyright (C) Julien Kerihuel 2015

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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <talloc.h>
#include <param.h>
#include <popt.h>

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

static void ocnotify_newmail(TALLOC_CTX *mem_ctx, uint32_t count,
			     const char **hosts, const char *data)
{
	int	sock;
	int	endpoint;
	char	*msg = NULL;
	int	i;
	int	bytes;
	size_t	msglen;

	if (data) {
		msg = talloc_asprintf(mem_ctx, "newmail:%s", data);
	} else {
		msg = talloc_strdup(mem_ctx, "newmail:<fake>");
	}
	if (!msg) {
		oc_log(OC_LOG_FATAL, "unable to allocate memory");
		exit (1);
	}

	sock = nn_socket(AF_SP, NN_PUSH);
	if (sock < 0) {
		oc_log(OC_LOG_FATAL, "unable to create socket");
		exit (1);
	}

	for (i = 0; i < count; i++) {
		endpoint = nn_connect(sock, hosts[i]);
		if (endpoint < 0) {
			oc_log(OC_LOG_ERROR, "unable to connect to %s", hosts[i]);
		}
		msglen = strlen(msg) + 1;
		bytes = nn_send(sock, msg, msglen, 0);
		if (bytes != msglen) {
			oc_log(OC_LOG_WARNING, "Error sending msg '%s': %d sent but %d expected",
			       msg, bytes, msglen);
		}
		oc_log(OC_LOG_INFO, "message sent to %s", hosts[i]);
		nn_shutdown(sock, endpoint);
	}
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX				*mem_ctx;
	enum mapistore_error			retval;
	poptContext				pc;
	int					opt;
	struct loadparm_context			*lp_ctx;
	struct mapistore_context		mstore_ctx;
	struct mapistore_notification_context	*ctx;
	const char				*opt_username = NULL;
	const char				*opt_server = NULL;
	bool					opt_newmail = false;
	bool					opt_list_server = false;
	bool					ret;
	const char				*opt_data = NULL;
	int					verbosity = 0;
	char					*debuglevel = NULL;
	uint32_t				i = 0;
	uint32_t				count = 0;
	const char				**hosts = NULL;

	enum { OPT_USERNAME=1000, OPT_SERVER, OPT_DATA, OPT_SERVER_LIST, OPT_NEWMAIL, OPT_VERBOSE };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "username", 'U', POPT_ARG_STRING, NULL, OPT_USERNAME, "set the username", NULL },
		{ "server", 'H', POPT_ARG_STRING, NULL, OPT_SERVER, "set the resolver address", NULL },
		{ "list", 0, POPT_ARG_NONE, NULL, OPT_SERVER_LIST, "list notification service instances", NULL },
		{ "newmail", 'n', POPT_ARG_NONE, NULL, OPT_NEWMAIL, "send newmail notification", NULL },
		{ "data", 'D', POPT_ARG_STRING, NULL, OPT_DATA, "user-specified data to send", NULL },
		{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, "Add one or more -v to increase verbosity", NULL },
		{ NULL, 0, 0, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) return 1;

	lp_ctx = loadparm_init_global(true);
	if (!lp_ctx) return 1;

	oc_log_init_stdout();

	pc = poptGetContext("ocnotify", argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_USERNAME:
			opt_username = poptGetOptArg(pc);
			break;
		case OPT_SERVER:
			opt_server = poptGetOptArg(pc);
			break;
		case OPT_SERVER_LIST:
			opt_list_server = true;
			break;
		case OPT_DATA:
			opt_data = poptGetOptArg(pc);
			break;
		case OPT_NEWMAIL:
			opt_newmail = true;
			break;
		case OPT_VERBOSE:
			verbosity += 1;
			break;
		}
	}

	if (!opt_username) {
		fprintf(stderr, "[ERR] username not specified\n");
		exit (1);
	}

	debuglevel = talloc_asprintf(mem_ctx, "%u", verbosity);
	ret = lpcfg_set_cmdline(lp_ctx, "log level", debuglevel);
	if (ret == false) {
		oc_log(OC_LOG_FATAL, "unable to set log level");
		exit (1);
	}
	talloc_free(debuglevel);

	if (opt_server) {
		ret = lpcfg_set_cmdline(lp_ctx, "mapistore:notification_cache", opt_server);
		if (ret == false) {
			oc_log(OC_LOG_FATAL, "unable to set mapistore:notification_cache");
			exit (1);
		}
	}

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	if (retval != MAPISTORE_SUCCESS) {
		oc_log(OC_LOG_FATAL, "[ERR] unable to initialize mapistore notification");
		exit(1);
	}
	mstore_ctx.notification_ctx = ctx;

	/* Check if the user is registered */
	retval = mapistore_notification_resolver_exist(&mstore_ctx, opt_username);
	if (retval) {
		oc_log(OC_LOG_ERROR, "[ERR] resolver session: '%s'", mapistore_errstr(retval));
		exit(1);
	}

	/* Retrieve server instances */
	retval = mapistore_notification_resolver_get(mem_ctx, &mstore_ctx, opt_username,
						     &count, &hosts);
	if (retval) {
		oc_log(OC_LOG_ERROR, "[ERR] resolver record: '%s'", mapistore_errstr(retval));
		exit (1);
	}

	if (opt_list_server) {
		oc_log(0, "%d servers found for '%s'\n", count, opt_username);
		for (i = 0; i < count; i++) {
			oc_log(0, "\t* %s\n", hosts[i]);
		}
	}

	/* Send mail notification */
	if (opt_newmail) {
		ocnotify_newmail(mem_ctx, count, hosts, opt_data);
	}

	talloc_free(ctx);

	poptFreeContext(pc);
	talloc_free(mem_ctx);
	return 0;
}
