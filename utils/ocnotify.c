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

struct ocnotify_private {
	struct mapistore_context	mstore_ctx;
	const char			*username;
	bool				flush;
};

static void ocnotify_flush(struct ocnotify_private ocnotify, const char *host)
{
	enum mapistore_error	retval;

	if (ocnotify.flush) {
		retval = mapistore_notification_resolver_delete(&ocnotify.mstore_ctx, ocnotify.username, host);
		if (retval == MAPISTORE_SUCCESS) {
			oc_log(0, "* [OK]   %s resolver entry for user %s deleted", host, ocnotify.username);
		} else {
			oc_log(0, "* [ERR]  %s resolver entry for user %s not deleted", host, ocnotify.username);
		}
	}
}

static void ocnotify_newmail(TALLOC_CTX *mem_ctx, struct ocnotify_private ocnotify,
			     uint32_t count, const char **hosts, const char *data)
{
	enum mapistore_error	retval;
	int			sock;
	int			endpoint;
	int			i;
	int			bytes;
	int			timeout;
	int			rc;
	uint8_t			*blob = NULL;
	size_t			msglen;

	retval = mapistore_notification_payload_newmail(mem_ctx, (char *)data, &blob, &msglen);
	if (retval != MAPISTORE_SUCCESS) {
		oc_log(OC_LOG_ERROR, "unable to generate newmail payload");
		exit (1);
	}

	sock = nn_socket(AF_SP, NN_PUSH);
	if (sock < 0) {
		oc_log(OC_LOG_FATAL, "unable to create socket");
		exit (1);
	}

	timeout = 200;
	rc = nn_setsockopt(sock, NN_SOL_SOCKET, NN_SNDTIMEO, &timeout, sizeof(timeout));
	if (rc < 0) {
		oc_log(OC_LOG_WARNING, "unable to set timeout on socket send");
	}

	for (i = 0; i < count; i++) {
		endpoint = nn_connect(sock, hosts[i]);
		if (endpoint < 0) {
			oc_log(OC_LOG_ERROR, "unable to connect to %s", hosts[i]);
			ocnotify_flush(ocnotify, hosts[i]);
			continue;
		}
		bytes = nn_send(sock, (char *)blob, msglen, 0);
		if (bytes != msglen) {
			oc_log(OC_LOG_WARNING, "Error sending msg: %d sent but %d expected",
			       bytes, msglen);
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
	struct ocnotify_private			ocnotify;
	struct mapistore_notification_context	*ctx;
	const char				*opt_server = NULL;
	const char				*opt_newmail = NULL;
	bool					opt_list_server = false;
	bool					ret;
	int					verbosity = 0;
	char					*debuglevel = NULL;
	uint32_t				i = 0;
	uint32_t				count = 0;
	const char				**hosts = NULL;

	enum { OPT_USERNAME=1000, OPT_SERVER, OPT_SERVER_LIST, OPT_FLUSH, OPT_NEWMAIL, OPT_VERBOSE };

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "username", 'U', POPT_ARG_STRING, NULL, OPT_USERNAME, "set the username", NULL },
		{ "server", 'H', POPT_ARG_STRING, NULL, OPT_SERVER, "set the resolver address", NULL },
		{ "list", 0, POPT_ARG_NONE, NULL, OPT_SERVER_LIST, "list notification service instances", NULL },
		{ "flush", 0, POPT_ARG_NONE, NULL, OPT_FLUSH, "flush notification cache for the user", NULL },
		{ "newmail", 'n', POPT_ARG_STRING, NULL, OPT_NEWMAIL, "send newmail notification and specify .eml", NULL },
		{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, "Add one or more -v to increase verbosity", NULL },
		{ NULL, 0, 0, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) return 1;

	lp_ctx = loadparm_init_global(true);
	if (!lp_ctx) return 1;

	oc_log_init_stdout();

	memset(&ocnotify, 0, sizeof (struct ocnotify_private));

	pc = poptGetContext("ocnotify", argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_USERNAME:
			ocnotify.username = poptGetOptArg(pc);
			break;
		case OPT_SERVER:
			opt_server = poptGetOptArg(pc);
			break;
		case OPT_SERVER_LIST:
			opt_list_server = true;
			break;
		case OPT_FLUSH:
			ocnotify.flush = true;
			break;
		case OPT_NEWMAIL:
			opt_newmail = poptGetOptArg(pc);
			break;
		case OPT_VERBOSE:
			verbosity += 1;
			break;
		}
	}

	if (!ocnotify.username) {
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
	ocnotify.mstore_ctx.notification_ctx = ctx;

	/* Check if the user is registered */
	retval = mapistore_notification_resolver_exist(&ocnotify.mstore_ctx, ocnotify.username);
	if (retval) {
		oc_log(OC_LOG_ERROR, "[ERR] resolver session: '%s'", mapistore_errstr(retval));
		exit(1);
	}

	/* Retrieve server instances */
	retval = mapistore_notification_resolver_get(mem_ctx, &ocnotify.mstore_ctx, ocnotify.username,
						     &count, &hosts);
	if (retval) {
		oc_log(OC_LOG_ERROR, "[ERR] resolver record: '%s'", mapistore_errstr(retval));
		exit (1);
	}

	if (opt_list_server) {
		oc_log(0, "%d servers found for '%s'\n", count, ocnotify.username);
		for (i = 0; i < count; i++) {
			oc_log(0, "\t* %s\n", hosts[i]);
		}
	}

	/* Send mail notification */
	if (opt_newmail) {
		ocnotify_newmail(mem_ctx, ocnotify, count, hosts, opt_newmail);
	}

	/* Flush invalid data */
	if (ocnotify.flush) {
		for (i = 0; i < count; i++) {
			retval = mapistore_notification_resolver_delete(&ocnotify.mstore_ctx, ocnotify.username, hosts[i]);
		}
	}

	talloc_free(ctx);

	poptFreeContext(pc);
	talloc_free(mem_ctx);
	return 0;
}
