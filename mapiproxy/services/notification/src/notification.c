#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <syslog.h>
#include <unistd.h>
#include <stdbool.h>
#include <popt.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <bsd/libutil.h>
#include <talloc.h>
#include <err.h>

#include "notification.h"

#include <param.h>
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"

volatile sig_atomic_t abort_flag = 0;

static bool notification_init(TALLOC_CTX *mem_ctx, struct context *ctx)
{
	enum MAPISTATUS retval;

	/* Initialize configuration */
	ctx->lp_ctx = loadparm_init(mem_ctx);
	if (ctx->lp_ctx == NULL) {
		errx(EXIT_FAILURE, "Failed to initialize configuration");
		return false;
	}
	lpcfg_load_default(ctx->lp_ctx);

	/* Initialize mapistore */
	ctx->mstore_ctx = mapistore_init(mem_ctx, ctx->lp_ctx, ctx->mapistore_backends_path);
	if (ctx->mstore_ctx == NULL) {
		errx(EXIT_FAILURE, "Failed to initialize mapistore");
		return false;
	}

	/* Initialize openchangedb context */
	retval = openchangedb_initialize(mem_ctx, ctx->lp_ctx, &ctx->ocdb_ctx);
	if (retval != MAPI_E_SUCCESS || ctx->ocdb_ctx == NULL) {
		errx(EXIT_FAILURE, "Failed to initialize openchange db");
		return false;
	}

	ctx->mstore_ctx->conn_info = talloc_zero(ctx->mstore_ctx, struct mapistore_connection_info);
	ctx->mstore_ctx->conn_info->mstore_ctx = ctx->mstore_ctx;
	ctx->mstore_ctx->conn_info->sam_ctx = NULL;
	ctx->mstore_ctx->conn_info->oc_ctx = ctx->ocdb_ctx;
	ctx->mstore_ctx->conn_info->username = NULL;

	return true;
}

static void abort_handler(int sig)
{
	abort_flag = 1;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX	*mem_ctx;
	const char	binary_name[] = "openchange-notification-service";
	char		*pidfile;
	bool		opt_daemon = false;
	bool		opt_debug = false;
	char		*opt_config = NULL;
	int		opt;
	poptContext	pc;
	struct context	*ctx;
	struct pidfh	*pfh;
	pid_t		otherpid;
	enum {
		OPT_DAEMON = 1000,
		OPT_DEBUG,
		OPT_CONFIG,
	};
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"daemon", 'D', POPT_ARG_NONE, NULL, OPT_DAEMON,
			"Become a daemon", NULL },
		{"debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG,
			"Debug mode", NULL },
		{"config", 'c', POPT_ARG_STRING, NULL, OPT_CONFIG,
			"Config file path", NULL },
		{ NULL },
	};

	/* Alloc context */
	mem_ctx = talloc_named(NULL, 0, binary_name);
	if (mem_ctx == NULL) {
		errx(EXIT_FAILURE, "No memory");
	}
	ctx = talloc(mem_ctx, struct context);
	if (ctx == NULL) {
		errx(EXIT_FAILURE, "No memory");
	}
	ctx->mem_ctx = mem_ctx;

	/* Parse command line arguments */
	pc = poptGetContext(binary_name, argc, argv, long_options, 0);
	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_DAEMON:
			opt_daemon = true;
			break;
		case OPT_DEBUG:
			opt_debug = true;
			break;
		case OPT_CONFIG:
			opt_config = poptGetOptArg(pc);
			break;
		default:
			fprintf(stderr, "Invalid option %s: %s\n", poptBadOption(pc, 0), poptStrerror(opt));
			poptPrintUsage(pc, stderr, 0);
			talloc_free(mem_ctx);
			exit(EXIT_FAILURE);
		}
	}
	poptFreeContext(pc);

	/* Setup logging and open log */
	setlogmask(LOG_UPTO(opt_debug ? LOG_DEBUG : LOG_INFO));
	openlog(binary_name, LOG_PID | (opt_daemon ? 0 : LOG_PERROR), LOG_DAEMON);

	/* Read config */
	if (opt_config != NULL) {
		read_config(ctx, opt_config);
	} else {
		read_config(ctx, DEFAULT_CONFIG_FILE);
	}

	/* Check daemon not already running */
	pidfile = talloc_asprintf(mem_ctx, "/run/%s.pid", binary_name);
	if (pidfile == NULL) {
		closelog();
		talloc_free(mem_ctx);
		errx(EXIT_FAILURE, "No memory");
	}
	pfh = pidfile_open(pidfile, 0644, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			closelog();
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.", (intmax_t) otherpid);
		}
		/* If we cannot create pidfile from other reasons, only warn. */
		warn("Cannot open or create pidfile %s", pidfile);
	}
	talloc_free(pidfile);

	/* Set file mask */
	umask(0);

	/* TODO chdir */

	/* Become daemon */
	if (opt_daemon) {
		if (daemon(0, 0) < 0) {
			closelog();
			talloc_free(mem_ctx);
			pidfile_remove(pfh);
			err(EXIT_FAILURE, "Failed to daemonize");
		}
	}

	/* Setup signals */
	signal(SIGHUP, opt_daemon ? SIG_IGN : abort_handler);
	signal(SIGINT, abort_handler);
	signal(SIGTERM, abort_handler);

	/* Write pid to file */
	pidfile_write(pfh);

	/* Initialize */
	if (!notification_init(mem_ctx, ctx)) {
		closelog();
		talloc_free(mem_ctx);
		pidfile_remove(pfh);
		err(EXIT_FAILURE, "Failed to initialize");
	}

	/* Do work */
	while (!abort_flag) {
		if (!broker_is_alive(ctx)) {
			if (!broker_connect(ctx)) {
				usleep(500000);
				continue;
			}
			if (!broker_declare(ctx)) {
				usleep(500000);
				continue;
			}
			if (!broker_start_consumer(ctx)) {
				usleep(500000);
				continue;
			}
		}
		broker_consume(ctx);
	}

	/* Disconnect from broker */
	broker_disconnect(ctx);

	/* Close logs */
	closelog();

	/* Unlink pidfile */
	pidfile_remove(pfh);

	/* Free memory */
	talloc_free(mem_ctx);

	exit(EXIT_SUCCESS);
}

