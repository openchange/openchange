/*
   OpenChange MAPI implementation

   Create an Exchange user

   Copyright (C) Julien Kerihuel 2006-2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/torture.h>
#include <torture/torture_proto.h>
#include <torture/mapi_torture.h>
#include <samba/popt.h>
#include <ldb.h>
#include <ldb_errors.h>
#include <db_wrap.h>
#include <ldap.h>
#include <core/nterr.h>

struct tce_async_context {
	int found;
};

static int tce_search_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
        struct tce_async_context *actx = talloc_get_type(context, struct tce_async_context);
	int ret;

        switch (ares->type) {

        case LDB_REPLY_ENTRY:
		if (ldb_msg_find_element(ares->message, "msExchMailboxGuid") != NULL) {
			actx->found = 1;
		}
                break;

        case LDB_REPLY_DONE:
                ret = 0;
                break;

        default:
                fprintf(stderr, "unknown Reply Type ignore it\n");
                return LDB_ERR_OTHER;
        }

        if (talloc_free(ares) == -1) {
                fprintf(stderr, "talloc_free failed\n");
                return LDB_ERR_OPERATIONS_ERROR;
        }

        return LDB_SUCCESS;
}

NTSTATUS torture_exchange_createuser(TALLOC_CTX *mem_ctx, const char *username,
				     const struct dom_sid *dom_sid)
{
	enum MAPISTATUS		retval;
	struct mapi_profile	*profile;
	struct ldb_context	*remote_ldb;
	struct ldb_request	*req;
	struct ldb_message	*msg;
	struct ldb_message	**res;
	struct tce_async_context *tce_ctx;
	char			*remote_ldb_url;
	const char * const	dom_attrs[] = { "objectSid", NULL };
	int			ret;
	uint32_t		count;
	char			**values;

	profile = global_mapi_ctx->session->profile;


	/* open LDAP connection */
	remote_ldb_url = talloc_asprintf(mem_ctx, "ldap://%s", profile->server);
	remote_ldb = ldb_wrap_connect(mem_ctx, remote_ldb_url, NULL,
				      cmdline_credentials, 0, NULL);
	if (!remote_ldb) return NT_STATUS_UNSUCCESSFUL;

	/* search the user's record using the user dom_sid */
	ret = gendb_search(remote_ldb, mem_ctx, NULL, &res,
			   dom_attrs, "(objectSid=%s)",
			   ldap_encode_ndr_dom_sid(mem_ctx, dom_sid));
	if (ret == -1) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}
	if (ret == 0) {
		return NT_STATUS_NO_SUCH_USER;
	}

	/* Prepare a new message for modify */
	msg = ldb_msg_new(mem_ctx);
	if (!msg) {
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = res[0]->dn;

	{
		int rtn;
		const char *exch_attrs[7];

		exch_attrs[0] = talloc_strdup(mem_ctx, username);
		rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "givenName", exch_attrs[0]);
		if (rtn == -1) return NT_STATUS_NO_MEMORY;

		exch_attrs[1] = talloc_asprintf(mem_ctx, "513");
		rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "userAccountControl", exch_attrs[1]);
		if (rtn == -1) return NT_STATUS_NO_MEMORY;

		{
			int	i;
			char	*realm = NULL;
			
			retval = GetProfileAttr(profile, "ProxyAddress", &count, &values);
			if (retval != MAPI_E_SUCCESS) return NT_STATUS_UNSUCCESSFUL;
			for (i = 0; i < count; i++) {
				if (values[i] && !strncasecmp("smtp", values[i], 4)) {
					realm = strchr(values[i], '@');
					realm += 1;
				}
			}
			if (!realm) return NT_STATUS_UNSUCCESSFUL;

			exch_attrs[2] = talloc_asprintf(mem_ctx, "%s@%s", username, realm);
			rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "mail", exch_attrs[2]);
			if (rtn == -1) return NT_STATUS_NO_MEMORY;
		}

		exch_attrs[3] = talloc_strdup(mem_ctx, username);
		rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "mailNickname", exch_attrs[3]);
		if (rtn == -1) return NT_STATUS_NO_MEMORY;

		exch_attrs[4] = talloc_asprintf(mem_ctx, "TRUE");
		rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "mDBUseDefaults", exch_attrs[4]);
		if (rtn == -1) return NT_STATUS_NO_MEMORY;

		{
			char	*org;
			
			org =  talloc_strndup(mem_ctx, profile->mailbox, 
					      strlen(profile->mailbox) - strlen(profile->username));			
			exch_attrs[5] = talloc_asprintf(mem_ctx, "%s%s", org, username);
			talloc_free(org);
			rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "legacyExchangeDN", exch_attrs[5]);
			if (rtn == -1) return NT_STATUS_NO_MEMORY;
		}

		exch_attrs[6] = talloc_strdup(mem_ctx, profile->homemdb);
		rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "msExchHomeServerName", exch_attrs[6]);
		if (rtn == -1) return NT_STATUS_NO_MEMORY;

		/* Prior we call ldb_modify, set up async ldb request on
		 * msExchMailboxGuid
		 */

		req = talloc_zero(mem_ctx, struct ldb_request);
		req->operation = LDB_SEARCH;
		req->op.search.base = res[0]->dn;
		req->op.search.scope = LDB_SCOPE_BASE;
		req->op.search.tree = ldb_parse_tree(remote_ldb, "(objectclass=*)");
		req->op.search.attrs = NULL;
		ldb_request_add_control(req, LDB_CONTROL_NOTIFICATION_OID, false, NULL);

		tce_ctx = talloc_zero(mem_ctx, struct tce_async_context);
		req->context = tce_ctx;
		req->callback = &tce_search_callback;
		ldb_set_timeout(mem_ctx, req, 60);

		rtn = ldb_request(remote_ldb, req);
		if (rtn != LDB_SUCCESS) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		DEBUG(0, ("async ldb request on msExchMailboxGuid sent\n"));

		/* We modify the user record with Exchange attributes */
		rtn = samdb_modify(remote_ldb, mem_ctx, msg);
		if (rtn != 0) return NT_STATUS_INTERNAL_DB_CORRUPTION;
		DEBUG(0, ("Extending AD user record with Exchange attributes\n"));
	}
	{
		int rtn;

		DEBUG(0, ("Waiting for Exchange mailbox creation\n"));
		while ((tce_ctx->found == 0) && (req->handle->state != LDB_ASYNC_DONE)) {
			rtn = ldb_wait(req->handle, LDB_WAIT_NONE);
			if (rtn != LDB_SUCCESS) {
				printf("rtn = %d (loop - unsuccessful)\n", rtn);
				return NT_STATUS_UNSUCCESSFUL;
			}
		}
		if (!tce_ctx->found) { /* timeout */
			printf("Timeout\n");
			return NT_STATUS_UNSUCCESSFUL; 
		}
		DEBUG(0, ("User mailbox generated\n"));
	}
	
	/* replace UserAccountControl attr in the user record */
	talloc_free(msg);
	msg = ldb_msg_new(mem_ctx);
	if (!msg) return NT_STATUS_NO_MEMORY;
	msg->dn = res[0]->dn;

	{
		const char *UserAccountControl;
		int rtn;

		UserAccountControl = talloc_asprintf(mem_ctx, "66048");
		rtn = samdb_msg_add_string(remote_ldb, mem_ctx, msg, "UserAccountControl", UserAccountControl);
		if (rtn == -1) return NT_STATUS_NO_MEMORY;

		rtn = samdb_replace(remote_ldb, mem_ctx, msg);
		if (rtn != 0) return NT_STATUS_INTERNAL_DB_CORRUPTION;
		DEBUG(0, ("ACB flags reset: password never expires\n"));
	}

	return NT_STATUS_OK;
}
