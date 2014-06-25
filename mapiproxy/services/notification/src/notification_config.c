#include <libconfig.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <talloc.h>

#include "notification.h"

/**
 * Read config file
 */
void read_config(struct context *ctx, const char *config_file)
{
	const char	*sval;
	int		ival;
	config_t	cfg;

	config_init(&cfg);
	if (!config_read_file(&cfg, config_file)) {
		config_destroy(&cfg);
		errx(EXIT_FAILURE, "Failed to read config: %s:%d - %s",
		     config_error_file(&cfg),
		     config_error_line(&cfg),
		     config_error_text(&cfg));
	}

	if (config_lookup_string(&cfg, "broker_host", &sval)) {
		ctx->broker_host = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		errx(EXIT_FAILURE, "Missing broker_host in config file %s", config_file);
	}

	if (config_lookup_int(&cfg, "broker_port", &ival)) {
		ctx->broker_port = ival;
	} else {
		errx(EXIT_FAILURE, "Missing broker_port in config file %s", config_file);
	}

	if (config_lookup_string(&cfg, "broker_user", &sval)) {
		ctx->broker_user = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		errx(EXIT_FAILURE, "Missing broker_user in config file %s", config_file);
	}

	if (config_lookup_string(&cfg, "broker_pass", &sval)) {
		ctx->broker_pass = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		errx(EXIT_FAILURE, "Missing broker_pass in config file %s", config_file);
	}

	if (config_lookup_string(&cfg, "broker_vhost", &sval)) {
		ctx->broker_vhost = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		ctx->broker_vhost = talloc_strdup(ctx->mem_ctx, "/");
	}

	if (config_lookup_string(&cfg, "broker_exchange", &sval)) {
		ctx->broker_exchange = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		errx(EXIT_FAILURE, "Missing broker_exchange in config file %s", config_file);
	}

	if (config_lookup_string(&cfg, "broker_new_mail_queue", &sval)) {
		ctx->broker_new_mail_queue = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		errx(EXIT_FAILURE, "Missing broker_new_mail_queue in config file %s", config_file);
	}

	if (config_lookup_string(&cfg, "broker_new_mail_routing_key", &sval)) {
		ctx->broker_new_mail_routing_key = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		errx(EXIT_FAILURE, "Missing broker_new_mail_routing_key in config file %s", config_file);
	}

	if (config_lookup_string(&cfg, "broker_consumer_tag", &sval)) {
		ctx->broker_new_mail_consumer_tag = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		ctx->broker_new_mail_consumer_tag = talloc_asprintf(ctx->mem_ctx,
								    "%s-consumer",
								    ctx->broker_new_mail_queue);
	}

	if (config_lookup_string(&cfg, "mapistore_backends_path", &sval)) {
		ctx->mapistore_backends_path = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		/* NULL for default location */
		ctx->mapistore_backends_path = NULL;
	}

	if (config_lookup_string(&cfg, "mapistore_backend", &sval)) {
		ctx->mapistore_backend = talloc_strdup(ctx->mem_ctx, sval);
	} else {
		ctx->mapistore_backend = talloc_strdup(ctx->mem_ctx, "SOGo");
	}

	config_destroy(&cfg);
}
