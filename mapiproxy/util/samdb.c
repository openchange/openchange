/*
   SamDB util functions

   OpenChange Project

   Copyright (C) Carlos Pérez-Aradros Herce 2015

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

#include "samdb.h"

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/fault_util.h"

/* Expose samdb_connect prototype */
struct ldb_context *samdb_connect(TALLOC_CTX *, struct tevent_context *,
				  struct loadparm_context *,
				  struct auth_session_info *,
				  unsigned int);

struct ldb_context *samdb_connect_url(TALLOC_CTX *, struct tevent_context *,
				      struct loadparm_context *,
				      struct auth_session_info *,
				      unsigned int,
				      const char *);
void tevent_loop_allow_nesting(struct tevent_context *);
struct loadparm_context;

/*
  Private struct declarations to assign timeout when samdb_context is a
  ldap connection (these struct are either private or not published on samba
  package, source4/libcli/ldap/ldap_client.h).
*/
struct ldb_context {
	struct ldb_module *modules;
};

struct ldb_module {
	struct ldb_module *prev, *next;
	struct ldb_context *ldb;
	struct ildb_private *private_data;
};

struct ildb_private {
	struct ldap_connection *ldap;
};

struct ldap_connection {
	struct {
		struct tstream_context *raw;
		struct tstream_context *tls;
		struct tstream_context *sasl;
		struct tstream_context *active;

		struct tevent_queue *send_queue;
		struct tevent_req *recv_subreq;
	} sockets;

	struct loadparm_context *lp_ctx;

	char *host;
	uint16_t port;
	bool ldaps;

	const char *auth_dn;
	const char *simple_pw;

	struct {
		char *url;
		int max_retries;
		int retries;
		time_t previous;
	} reconnect;

	struct {
		enum { LDAP_BIND_SIMPLE, LDAP_BIND_SASL } type;
		void *creds;
	} bind;

	/* next message id to assign */
	unsigned next_messageid;

	/* Outstanding LDAP requests that have not yet been replied to */
	struct ldap_request *pending;

	/* Let's support SASL */
	struct gensec_security *gensec;

	/* the default timeout for messages */
	int timeout;
};
/* End of private declaration to assign timeout to ldap connection */


/* Loadparm context */
static struct loadparm_context *lp_ctx = NULL;


/**
   \details Initialize ldb_context to samdb, creates one for all emsmdbp
   contexts

   \param lp_ctx pointer to the loadparm context
 */
struct ldb_context *samdb_init(TALLOC_CTX *mem_ctx)
{
	struct tevent_context		*ev;
	const char			*samdb_url;
	static struct ldb_context	*samdb_ctx;

	if (!lp_ctx) lp_ctx = loadparm_init_global(true);

	ev = tevent_context_init(mem_ctx);
	if (!ev) {
		return NULL;
	}
	tevent_loop_allow_nesting(ev);

	/* Retrieve samdb url (local or external) */
	samdb_url = lpcfg_parm_string(lp_ctx, NULL, "dcerpc_mapiproxy", "samdb_url");

	OC_DEBUG(5, "Connecting to sam db");
	if (!samdb_url) {
		samdb_ctx = samdb_connect(mem_ctx, ev, lp_ctx, system_session(lp_ctx), 0);
	} else {
		samdb_ctx = samdb_connect_url(mem_ctx, ev, lp_ctx, system_session(lp_ctx),
					      LDB_FLG_RECONNECT, samdb_url);

		if (samdb_ctx) samdb_ctx->modules->private_data->ldap->timeout = 10;
	}

	return samdb_ctx;
}


struct ldb_context *samdb_reconnect(struct ldb_context *samdb_ctx)
{
	struct ldb_context *res;
	TALLOC_CTX *mem_ctx = talloc_parent(samdb_ctx);
	res = samdb_init(mem_ctx);

	/* only free previous ldb context after a successful reconnect */
	if (res) talloc_free(samdb_ctx);

	return res;
}

int safe_ldb_search(struct ldb_context **ldb_ptr, TALLOC_CTX *mem_ctx,
		    struct ldb_result **result, struct ldb_dn *base,
		    enum ldb_scope scope, const char * const *attrs,
		    const char *exp_fmt, ...)
{
	va_list ap;
	int tries = 0;
	int ret;
	char *formatted = NULL;
	struct ldb_context *new_ldb = NULL;

	if (ldb_ptr == NULL) {
		OC_DEBUG(0, "safe_ldb_search got wrong ldb_context");
		return -1;
	}

	if (exp_fmt) {
		va_start(ap, exp_fmt);
		formatted = talloc_vasprintf(mem_ctx, exp_fmt, ap);
		va_end(ap);

		if (!formatted) return LDB_ERR_OPERATIONS_ERROR;
	}


	ret = ldb_search(*ldb_ptr, mem_ctx, result, base, scope, attrs, formatted);
	if (ret != LDB_ERR_OPERATIONS_ERROR) {
		return ret;
	}

	/* Something failed, try reconnecting */
	do {
		OC_DEBUG(3, "sam db connection lost, reconnecting...");
		new_ldb = samdb_init(talloc_parent(*ldb_ptr));

		if (new_ldb) {
			ret = ldb_search(new_ldb, mem_ctx, result, base, scope, attrs, formatted);
			if (ret != LDB_ERR_OPERATIONS_ERROR) {
				/* reconnect worked, replace ldb_context with the new one */
				talloc_free(*ldb_ptr);
				*ldb_ptr = new_ldb;
				break;
			}
			else {
				/* reconnect didn't help, forget and retry */
				TALLOC_FREE(new_ldb);
			}
		}

		tries++;
	} while (tries < 3);

	return ret;
}
