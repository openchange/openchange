#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>
#include <syslog.h>
#include <amqp.h>
#include <json-c/json.h>

#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"

#include "notification.h"

#define EMSMDBP_INBOX 13

static void notification_publish(TALLOC_CTX *mem_ctx, const struct context *ctx,
				 const char *username, uint64_t mid, uint64_t fid,
				 const char *message_uri)
{
	int			ret;
	amqp_rpc_reply_t	r;
	json_object		*jobj;
	json_object		*juser;
	json_object		*jmid;
	json_object		*jfid;
	json_object		*juri;

	/* Build the body */
	jobj = json_object_new_object();
	juser = json_object_new_string(username);
	jmid = json_object_new_int64(mid);
	jfid = json_object_new_int64(fid);
	juri = json_object_new_string(message_uri);

	json_object_object_add(jobj, "user", juser);
	json_object_object_add(jobj, "mid", jmid);
	json_object_object_add(jobj, "fid", jfid);
	json_object_object_add(jobj, "uri", juri);

	/* Build the exchange name for user */
	char *user_exchange = talloc_asprintf(mem_ctx, "%s_notification", username);

	/* Declare the exchange */
	syslog(LOG_DEBUG, "Declaring user exchange '%s'", user_exchange);
	amqp_exchange_declare(ctx->broker_conn,
			      2,	/* Channel */
			      amqp_cstring_bytes(user_exchange),
			      amqp_cstring_bytes("fanout"),
			      0,	/* Passive */
			      0,	/* Durable */
			      amqp_empty_table);
	r = amqp_get_rpc_reply(ctx->broker_conn);
	if (r.reply_type != AMQP_RESPONSE_NORMAL) {
		char *err = broker_err(ctx->mem_ctx, r);
		syslog(LOG_ERR, "Failed to declare exchange: %s", err);
		talloc_free(err);

		/* Free memory */
		json_object_put(jobj);

		return;
	}

	/* Publish message to exchange */
	syslog(LOG_DEBUG, "Publishing notification to exchange '%s'", user_exchange);
	ret = amqp_basic_publish(ctx->broker_conn,
				 2,			/* Channel */
				 amqp_cstring_bytes(user_exchange),
				 amqp_empty_bytes,	/* Routing key */
				 0,			/* Mandatory */
				 0,			/* Inmediate */
				 NULL,			/* Properties */
				 amqp_cstring_bytes(json_object_to_json_string(jobj)));
	if (ret != AMQP_STATUS_OK) {
		syslog(LOG_ERR,	"Error publishing message: %s", amqp_error_string2(ret));

		/* Free memory */
		json_object_put(jobj);

		return;
	}

	/* Check for errors may happen on the broker */
	r = amqp_get_rpc_reply(ctx->broker_conn);
	if (r.reply_type != AMQP_RESPONSE_NORMAL) {
		char *err = broker_err(mem_ctx, r);
		syslog(LOG_ERR, "Error publishing message: %s", err);
		talloc_free(err);

		/* Free memory */
		json_object_put(jobj);

		return;
	}

	/* Free memory */
	json_object_put(jobj);
}

void notification_register_message(TALLOC_CTX *mem_ctx, const struct context *ctx,
				   const char *username, const char *folder, uint32_t message_id)
{
	int			ret;
	uint64_t		mid;
	uint64_t		fid;
	char			*folder_uri;
	char			*message_uri;
	enum MAPISTATUS		retval;
	struct indexing_context	*ictx;

	/* Fetch the INBOX mailbox ID */
	/* FIXME hardcoded inbox */
	retval = openchangedb_get_SystemFolderID(ctx->ocdb_ctx, username, EMSMDBP_INBOX, &fid);
	if (retval != MAPI_E_SUCCESS) {
		syslog(LOG_ERR, "Failed to get the mailbox ID: %s", mapi_get_errstr(retval));
		return;
	}

	/* Fetch the folder URI */
	retval = openchangedb_get_mapistoreURI(mem_ctx, ctx->ocdb_ctx, username, fid, &folder_uri, true);
	if (retval != MAPI_E_SUCCESS) {
		syslog(LOG_ERR, "Failed to get the folder URI: %s", mapi_get_errstr(retval));
		return;
	}

	/* Open connection to indexing database */
	ret = mapistore_indexing_add(ctx->mstore_ctx, username, &ictx);
	if (ret) {
		syslog(LOG_ERR, "Failed to create indexing connection");
		return;
	}

	/* Generate a new mid for the message */
	ret = mapistore_indexing_get_new_folderID_as_user(ctx->mstore_ctx, username, &mid);
	if (ret) {
		syslog(LOG_ERR, "Failed to get new message mid: %s", mapi_get_errstr(ret));
		return;
	}

	/* Build the full URI, appending the message ID to the folder ID */
	/* FIXME: Delegate this functionality to mapistore backend */
	message_uri = talloc_asprintf(mem_ctx, "%s%d.eml", folder_uri, message_id);
	if (message_uri == NULL) {
		syslog(LOG_ERR, "Failed to generate message URI");
		return;
	}

	/* Add the record to the indexing database */
	ret = ictx->add_fmid(ictx, username, mid, message_uri);
	if (ret) {
		syslog(LOG_ERR, "Failed to add mid");
		return;
	}

	syslog(LOG_DEBUG, "Message registered for user %s (mid=0x%.16"PRIx64
			  "(%"PRIu64"), fid=0x%.16"PRIx64"(%"PRIu64"), uri=%s)",
			  username, mid, mid, fid, fid, message_uri);

	/* Publish notification on the user specific queue */
	notification_publish(mem_ctx, ctx, username, mid, fid, message_uri);

	return;
}
