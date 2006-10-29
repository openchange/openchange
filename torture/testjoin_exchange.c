/* 
   OpenChange NSPI implementation

   Create an Exchange user

   Copyright (C) Julien Kerihuel 2006
   
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

#include "openchange.h"

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

NTSTATUS	torture_create_exchangeuser(TALLOC_CTX *mem_ctx, const struct dom_sid *sid)
{
	NTSTATUS		status;
	TALLOC_CTX		*tmp_ctx = (TALLOC_CTX *) NULL;
	struct ldb_context	*remote_ldb;
	struct ldb_message	**res;
	struct ldb_message	*msg;
	struct tce_async_context *tce_ctx = talloc_zero(tmp_ctx, struct tce_async_context);
	char			*remote_ldb_url;
	struct dcerpc_binding	*b;
	const char		*dc_binding = lp_parm_string(-1, "torture", "dc_binding");
	const char		*binding = lp_parm_string(-1, "torture", "binding");
	const char * const	dom_attrs[] = { "objectSid", NULL};
	int			ret;

/* 	struct ldb_request req; */
/* 	struct ldb_control **ctrl = talloc_array(tmp_ctx, struct ldb_control *, 2); */

	struct ldb_control **ctrl;
	struct ldb_paged_control *control;
	struct ldb_request *req;
	struct tce_async_context *actx = talloc_zero(tmp_ctx, struct tce_async_context);
/* 	struct test_schema_ctx *actx; */

	tmp_ctx = talloc_named(mem_ctx, 0, "torture_create_exchangeuser temp context");
	if (!tmp_ctx) {
		return  NT_STATUS_NO_MEMORY;
	}

	/* parse the binding string so we can retrieve the host */
	if (dc_binding) {
		status = dcerpc_parse_binding(tmp_ctx, dc_binding, &b);
	} else {
		status = dcerpc_parse_binding(tmp_ctx, binding, &b);
	}

	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to parse dcerpc binding\n");
	}

	/* open with LDAP */
	remote_ldb_url = talloc_asprintf(tmp_ctx, "ldap://%s", b->host);
	if (!remote_ldb_url) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	remote_ldb = ldb_wrap_connect(tmp_ctx, remote_ldb_url, NULL,
				      cmdline_credentials, 0, NULL);
	if (!remote_ldb) {
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* search the user's record using the user dom_sid */
	ret = gendb_search(remote_ldb, tmp_ctx, NULL, &res, 
			   dom_attrs, "(objectSid=%s)", 
			   ldap_encode_ndr_dom_sid(tmp_ctx, sid));
	if (ret != 1) {
	  return NT_STATUS_NO_SUCH_USER;
	}

	/* Prepare a new message, for the modify */
	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
	  talloc_free(tmp_ctx);
	  return NT_STATUS_NO_MEMORY;
	}
	msg->dn = res[0]->dn;

	{
		int rtn;
		const char *exchange_attributes[7];

		exchange_attributes[0] = talloc_asprintf(tmp_ctx, "OpenChange");
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "givenName", exchange_attributes[0]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		exchange_attributes[1] = talloc_asprintf(tmp_ctx, "513");
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "userAccountControl", exchange_attributes[1]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		
		exchange_attributes[2] = talloc_asprintf(tmp_ctx, "%s@openchange.info", TEST_USER_NAME);
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "mail", exchange_attributes[2]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		exchange_attributes[3] = talloc_asprintf(tmp_ctx, "%s", TEST_USER_NAME);
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "mailNickname", exchange_attributes[3]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		exchange_attributes[4] = talloc_asprintf(tmp_ctx, "TRUE");
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "mDBUseDefaults", exchange_attributes[4]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		exchange_attributes[5] = talloc_asprintf(tmp_ctx, "/o=OpenChange Organization/ou=first administrative group/cn=Recipients/cn=%s", 
							 TEST_USER_NAME);
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "legacyExchangeDN", exchange_attributes[5]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		exchange_attributes[6] = talloc_asprintf(tmp_ctx, "CN=Mailbox Store (EXCHANGE),CN=First Storage Group,CN=InformationStore,CN=EXCHANGE,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=OpenChange Organization,CN=Microsoft Exchange,CN=Services,CN=Configuration,DC=openchange,DC=info");
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "homeMDB", exchange_attributes[6]);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		/* Prior we call ldb_modify, we set up the async ldb request on msExchMailboxGuid */
/* 		ctrl[1] = NULL; */
/* 		ctrl[0] = talloc(ctrl, struct ldb_control); */
/* 		ctrl[0]->oid = LDB_CONTROL_NOTIFICATION_OID; */
/* 		ctrl[0]->critical = 1; */
/* 		ctrl[0]->data = NULL; */

/* 		req.operation = LDB_ASYNC_SEARCH; */
/* 		req.op.search.base = res[0]->dn; */
/* 		req.op.search.scope = LDB_SCOPE_BASE; */
/* 		req.op.search.tree = ldb_parse_tree(remote_ldb, "(objectclass=*)"); */
/* 		req.op.search.attrs = NULL; */
/* 		req.controls = ctrl; */
/* 		req.async.context = tce_ctx; */
/* 		req.async.callback = &tce_search_callback; */
/* 		req.async.timeout = 60; /\* 60 sec. timeout FIX: bug in timeout callback *\/ */

	req = talloc(tmp_ctx, struct ldb_request);
/* 	actx = talloc(req, struct test_schema_ctx); */
		
	ctrl = talloc_array(req, struct ldb_control *, 2);
	ctrl[1] = NULL;
	ctrl[0] = talloc(ctrl, struct ldb_control);
	ctrl[0]->oid = LDB_CONTROL_NOTIFICATION_OID;
	ctrl[0]->critical = True;
	ctrl[0]->data = NULL;
/* 	control = talloc(ctrl[0], struct ldb_paged_control); */
/* 	control->size = 1000; */
/* 	control->cookie = NULL; */
/* 	control->cookie_len = 0; */
/* 	ctrl[0]->data = control; */

	req->operation = LDB_SEARCH;
	req->op.search.base = res[0]->dn;
	req->op.search.scope = LDB_SCOPE_BASE;
	req->op.search.tree = ldb_parse_tree(remote_ldb, "(objectclass=*)");
	if (req->op.search.tree == NULL) return -1;
	req->op.search.attrs = NULL;
	req->controls = ctrl;
	req->context = tce_ctx;
	req->callback = &tce_search_callback;
	ldb_set_timeout(tmp_ctx, req, 60);

/* 	actx->count		= 0; */
/* 	actx->ctrl		= control; */
/* 	actx->callback		= callback; */
/* 	actx->private_data	= private_data; */
/* again: */
/* 	actx->pending		= True; */

		rtn = ldb_request(remote_ldb, req);
		if (rtn!= LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return NT_STATUS_UNSUCCESSFUL;
		}
		DEBUG(0, ("Async ldb request on msExchMailboxGuid attribute sent\n"));

		/* We modify the user record with the Exchange attributes */
		rtn = samdb_modify(remote_ldb, tmp_ctx, msg);
		if (rtn != 0) {
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		DEBUG(0, ("Extending AD user record with Exchange attributes\n"));
	}
	
	/* We now wait for Exchange to modify the AD and auto-generate mailbox related attributes */

	{
		int rtn;

		DEBUG(0, ("Waiting for Exchange to create the mailbox and modify the AD user record\n"));
		while ((tce_ctx->found == 0) && (req->handle->state != LDB_ASYNC_DONE)) {
			rtn = ldb_wait(req->handle, LDB_WAIT_NONE);
/* 			rtn = ldb_async_wait(req->handle, LDB_WAIT_NONE); */
			if (rtn!= LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return NT_STATUS_UNSUCCESSFUL;
			}
		}
		
		if ( ! tce_ctx->found) { /* timed out */
			talloc_free(tmp_ctx);
			return NT_STATUS_UNSUCCESSFUL;
		}
		DEBUG(0, ("User mailbox and AD record generated\n"));
	}

	/* We now replace the UserAccountControl attr in the user record with*/
	talloc_free(msg);
	
	/* Prepare a new message, for the replace */
	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
	  talloc_free(tmp_ctx);
	  return NT_STATUS_NO_MEMORY;
	}
	msg->dn = res[0]->dn;
	
	{
		const char *UserAccountControl;
		int rtn;
		
		UserAccountControl = talloc_asprintf(tmp_ctx, "66048");
		rtn = samdb_msg_add_string(remote_ldb, tmp_ctx, msg, "UserAccountControl", UserAccountControl);
		if (rtn == -1) {
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		
		rtn = samdb_replace(remote_ldb, tmp_ctx, msg);
		if (rtn != 0) {
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		DEBUG(0, ("Resetting ACB flags, pw never expires\n"));
	}

	return NT_STATUS_OK;
}
