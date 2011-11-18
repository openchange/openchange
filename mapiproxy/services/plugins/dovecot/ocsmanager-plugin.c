/*
   OpenChange Service Manager Plugin for Dovecot

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011.

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

/*
  Step 1. Compile the plugin
  gcc  -fPIC -shared -DHAVE_CONFIG_H `echo $DOVECOT_CFLAGS $LIBDOVECOT_INCLUDE $LIBDOVECOT_STORAGE_INCLUDE` ocsmanager-plugin.c -L/usr/lib/dovecot/modules/ -l15_notify_plugin -o ocsmanager_plugin.so

  Step 2: Install the plugin
  sudo cp ocsmanager_plugin.so /usr/lib/dovecot/modules/ocsmanager_plugin.so

  Step 3: Configure ocsmanager plugin options in dovecot.conf
  plugin {
  ocsmanager_backend = SOGo
  ocsmanager_newmail = /home/openchange/openchange/sogo-good/mapiproxy/services/client/newmail.py
  ocsmanager_config = /home/openchange/openchange/sogo-good/mapiproxy/services/client/ocsmanager.cfg
}

 Step 4: Restart dovecot
 */

#include "ocsmanager-plugin.h"

#define	OCSMANAGER_USER_CONTEXT(obj) \
	MODULE_CONTEXT(obj, ocsmanager_user_module)

enum ocsmanager_field {
	OCSMANAGER_FIELD_UID		= 0x1,
	OCSMANAGER_FIELD_BOX		= 0x2,
	OCSMANAGER_FIELD_MSGID		= 0x4,
	OCSMANAGER_FIELD_PSIZE		= 0x8,
	OCSMANAGER_FIELD_VSIZE		= 0x10,
	OCSMANAGER_FIELD_FLAGS		= 0x20,
	OCSMANAGER_FIELD_FROM		= 0x40,
	OCSMANAGER_FIELD_SUBJECT	= 0x80
};

#define	OCSMANAGER_DEFAULT_FIELDS \
	(OCSMANAGER_FIELD_UID | OCSMANAGER_FIELD_BOX | \
	 OCSMANAGER_FIELD_MSGID | OCSMANAGER_FIELD_PSIZE)

enum ocsmanager_event {
	OCSMANAGER_EVENT_DELETE		= 0x1,
	OCSMANAGER_EVENT_UNDELETE	= 0x2,
	OCSMANAGER_EVENT_EXPUNGE	= 0x4,
	OCSMANAGER_EVENT_SAVE		= 0x8,
	OCSMANAGER_EVENT_COPY		= 0x10,
	OCSMANAGER_EVENT_MAILBOX_CREATE	= 0x20,
	OCSMANAGER_EVENT_MAILBOX_DELETE	= 0x40,
	OCSMANAGER_EVENT_MAILBOX_RENAME	= 0x80,
	OCSMANAGER_EVENT_FLAG_CHANGE	= 0x100
};

#define	OCSMANAGER_DEFAULT_EVENTS	(OCSMANAGER_EVENT_SAVE)

struct ocsmanager_user {
	union mail_user_module_context	module_ctx;
	enum ocsmanager_field		fields;
	enum ocsmanager_event		events;
	char				*username;
	const char			*backend;
	const char			*bin;
	const char			*config;
};

struct ocsmanager_message {
	struct ocsmanager_message	*prev;
	struct ocsmanager_message	*next;
	enum ocsmanager_event		event;
	bool				ignore;
	uint32_t			uid;
	char				*destination_folder;
	char				*username;
	const char			*backend;
	char				*bin;
	char				*config;
};

struct ocsmanager_mail_txn_context {
	pool_t				pool;
	struct ocsmanager_message	*messages;
	struct ocsmanager_message	*messages_tail;
};

static MODULE_CONTEXT_DEFINE_INIT(ocsmanager_user_module,
				  &mail_user_module_register);

static void ocsmanager_mail_user_created(struct mail_user *user)
{
	struct ocsmanager_user	*ocsuser;
	const char		*str;

	i_debug("ocsmanager_mail_user_created");
	i_debug("username = %s", user->username);

	ocsuser = p_new(user->pool, struct ocsmanager_user, 1);
	MODULE_CONTEXT_SET(user, ocsmanager_user_module, ocsuser);

	ocsuser->fields = OCSMANAGER_DEFAULT_FIELDS;
	ocsuser->events = OCSMANAGER_DEFAULT_EVENTS;
	ocsuser->username = p_strdup(user->pool, user->username);
	str = mail_user_plugin_getenv(user, "ocsmanager_backend");
	if (!str) {
		i_fatal("Missing ocsmanager_backend parameter in dovecot.conf");
	}
	ocsuser->backend = str;

	str = mail_user_plugin_getenv(user, "ocsmanager_newmail");
	if (!str) {
		i_fatal("Missing ocsmanager_newmail parameter in dovecot.conf");
	}
	ocsuser->bin = str;
	
	str = mail_user_plugin_getenv(user, "ocsmanager_config");
	if (!str) {
		i_fatal("Missing ocsmanager_config parameter in dovecot.conf");
	}
	ocsuser->config = str;
}

static void ocsmanager_mail_save(void *txn, struct mail *mail)
{
	i_debug("ocsmanager_mail_save");
	i_debug("message UID = %d\n", mail->uid);
}

static void ocsmanager_mail_copy(void *txn, struct mail *src, struct mail *dst)
{
	struct ocsmanager_mail_txn_context	*ctx = (struct ocsmanager_mail_txn_context *) txn;
	struct ocsmanager_user			*mctx = OCSMANAGER_USER_CONTEXT(dst->box->storage->user);
	struct ocsmanager_message		*msg;
	int					i;

	if (strcmp(src->box->storage->name, "raw") == 0) {
		/* special case: lda/lmtp is saving a mail */
		msg = p_new(ctx->pool, struct ocsmanager_message, 1);
		msg->event = OCSMANAGER_EVENT_COPY;
		msg->ignore = FALSE;
		msg->username = p_strdup(ctx->pool, mctx->username);
		msg->backend = p_strdup(ctx->pool, mctx->backend);
		msg->destination_folder = p_strdup(ctx->pool, mailbox_get_name(dst->box));
		msg->bin = p_strdup(ctx->pool, mctx->bin);
		msg->config = p_strdup(ctx->pool, mctx->config);

		/* FIXME: Quick hack of the night */
		msg->username[0] = toupper(msg->username[0]);
		for (i = 0; i < strlen(msg->destination_folder); i++) {
			msg->destination_folder[i] = tolower(msg->destination_folder[i]);
		}

		DLLIST2_APPEND(&ctx->messages, &ctx->messages_tail, msg);		
	}
}

static void *ocsmanager_mail_transaction_begin(struct mailbox_transaction_context *t ATTR_UNUSED)
{
	pool_t					pool;
	struct ocsmanager_mail_txn_context	*ctx;

	pool = pool_alloconly_create("ocsmanager", 2048);
	ctx = p_new(pool, struct ocsmanager_mail_txn_context, 1);
	ctx->pool = pool;

	return ctx;
}

static void ocsmanager_mail_transaction_commit(void *txn, 
					       struct mail_transaction_commit_changes *changes)
{
	struct ocsmanager_mail_txn_context	*ctx = (struct ocsmanager_mail_txn_context *)txn;
	uint32_t				uid;
	struct ocsmanager_message		*msg;
	unsigned int				n = 0;
	struct seq_range_iter			iter;
	char					*command;

	seq_range_array_iter_init(&iter, &changes->saved_uids);
	for (msg = ctx->messages; msg != NULL; msg = msg->next) {
		if (msg->event == OCSMANAGER_EVENT_COPY) {
			if (!seq_range_array_iter_nth(&iter, n++, &uid)) uid = 0;
			msg->uid = uid;
			
			i_debug("# uid = %d", msg->uid);
			i_debug("# folder = %s", msg->destination_folder);
			i_debug("# username = %s", msg->username);
			i_debug("# backend = %s", msg->backend);

			/* FIXME: I'm ashamed but I'm tired */
			command = p_strdup_printf(ctx->pool, "python %s --config %s --backend %s --user %s --folder %s --msgid %d", msg->bin, msg->config, msg->backend, msg->username, msg->destination_folder, msg->uid);
			system(command);
		}
	}
	i_assert(!seq_range_array_iter_nth(&iter, n, &uid));

	pool_unref(&ctx->pool);
}

static void ocsmanager_mail_transaction_rollback(void *txn)
{
	struct ocsmanager_mail_txn_context	*ctx = (struct ocsmanager_mail_txn_context *) txn;

	pool_unref(&ctx->pool);
}

static const struct notify_vfuncs ocsmanager_vfuncs = {
	.mail_transaction_begin = ocsmanager_mail_transaction_begin,
	.mail_save = ocsmanager_mail_save,
	.mail_copy = ocsmanager_mail_copy,
	.mail_transaction_commit = ocsmanager_mail_transaction_commit,
	.mail_transaction_rollback = ocsmanager_mail_transaction_rollback,
};

static struct notify_context *ocsmanager_ctx;

static struct mail_storage_hooks ocsmanager_mail_storage_hooks = {
	.mail_user_created = ocsmanager_mail_user_created
};

void ocsmanager_plugin_init(struct module *module)
{
	i_debug("oscmanager_plugin_init");
	ocsmanager_ctx = notify_register(&ocsmanager_vfuncs);
	mail_storage_hooks_add(module, &ocsmanager_mail_storage_hooks);
}

void ocsmanager_plugin_deinit(void)
{  
	i_debug("oscmanager_plugin_deinit");
	mail_storage_hooks_remove(&ocsmanager_mail_storage_hooks);
	notify_unregister(ocsmanager_ctx);
}

const char *ocsmanager_plugin_dependencies[] = { "notify", NULL };
