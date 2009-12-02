/*
   OpenChange MAPI torture suite implementation.

   Create an Exchange user

   Copyright (C) Julien Kerihuel 2006 - 2007.

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
   Several functions are forked from Samba4 trunk for convenient
   purposes:
		- DeleteUser_byname
		- torture_join_user_sid
		- torture_create_testuser
		- torture_leave_domain
 */

#include <libmapi/libmapi.h>
#include <gen_ndr/ndr_exchange.h>
#include <param.h>
#include <credentials.h>
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <samba/popt.h>
#include <ldb.h>
#include <ldb_errors.h>
#include <ldb_wrap.h>
#include <ldap_ndr.h>
#include <util_ldb.h>
#include <gen_ndr/ndr_samr.h>
#include <gen_ndr/ndr_samr_c.h>
#include <time.h>
#include <core/error.h>
#include <tevent.h>

struct tce_async_context {
	int found;
};

static int tce_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
        struct tce_async_context *actx = talloc_get_type(req->context, struct tce_async_context);
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
	struct tevent_context	*ev = NULL;
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

	ev = tevent_context_init(talloc_autofree_context());

	/* open LDAP connection */
	remote_ldb_url = talloc_asprintf(mem_ctx, "ldap://%s", profile->server);
	remote_ldb = ldb_wrap_connect(mem_ctx, ev, global_mapi_ctx->lp_ctx, remote_ldb_url, 
								  NULL, cmdline_credentials, 0);
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
		req->callback = tce_search_callback;
		ldb_set_timeout(mem_ctx, req, 60);

		rtn = ldb_request(remote_ldb, req);
		if (rtn != LDB_SUCCESS) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		DEBUG(0, ("async ldb request on msExchMailboxGuid sent\n"));

		/* We modify the user record with Exchange attributes */
		rtn = ldb_modify(remote_ldb, msg);
		if (rtn != 0) return NT_STATUS_INTERNAL_DB_CORRUPTION;
		DEBUG(0, ("Extending AD user record with Exchange attributes\n"));
	}
	{
		int rtn;

		DEBUG(0, ("Waiting for Exchange mailbox creation\n"));
		rtn = ldb_wait(req->handle, LDB_WAIT_NONE);
		if (rtn != LDB_SUCCESS) {
			printf("rtn = %d (loop - unsuccessful)\n", rtn);
			return NT_STATUS_UNSUCCESSFUL;
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

static NTSTATUS DeleteUser_byname(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
				  struct policy_handle *handle, const char *name)
{
	NTSTATUS status;
	struct samr_DeleteUser d;
	struct policy_handle user_handle;
	uint32_t rid;
	struct samr_LookupNames n;
	struct lsa_String sname;
	struct samr_OpenUser r;

	sname.string = name;

	n.in.domain_handle = handle;
	n.in.num_names = 1;
	n.in.names = &sname;

	status = dcerpc_samr_LookupNames(p, mem_ctx, &n);
	if (NT_STATUS_IS_OK(status)) {
		rid = n.out.rids->ids[0];
	} else {
		return status;
	}

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	status = dcerpc_samr_OpenUser(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenUser(%s) failed - %s\n", name, nt_errstr(status));
		return status;
	}

	d.in.user_handle = &user_handle;
	d.out.user_handle = &user_handle;
	status = dcerpc_samr_DeleteUser(p, mem_ctx, &d);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

const struct dom_sid *torture_join_user_sid(struct test_join *join)
{
	return join->user_sid;
}

struct test_join *torture_create_testuser(struct torture_context *torture,
					  const char *username, 
					  const char *domain,
					  uint16_t acct_type,
					  const char **random_password)
{
	NTSTATUS status;
	struct samr_Connect c;
	struct samr_CreateUser2 r;
	struct samr_OpenDomain o;
	struct samr_LookupDomain l;
	struct samr_GetUserPwInfo pwp;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	struct policy_handle handle;
	struct policy_handle domain_handle;
	uint32_t access_granted;
	uint32_t rid;
	DATA_BLOB session_key;
	struct lsa_String name;
	
	int policy_min_pw_len = 0;
	struct test_join *join;
	char *random_pw;
	const char *dc_binding = lp_parm_string(torture->lp_ctx,
											NULL, "torture", "dc_binding");

	join = talloc(NULL, struct test_join);
	if (join == NULL) {
		return NULL;
	}

	ZERO_STRUCTP(join);

	printf("Connecting to SAMR\n");
	
	if (dc_binding) {
		status = dcerpc_pipe_connect(join,
					     &join->p,
					     dc_binding,
					     &ndr_table_samr,
					     cmdline_credentials, 
					     NULL, torture->lp_ctx);
					     
	} else {
		status = torture_rpc_connection(torture, 
						&join->p, 
						&ndr_table_samr);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}

	c.in.system_name = NULL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.out.connect_handle = &handle;

	status = dcerpc_samr_Connect(join->p, join, &c);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		if (NT_STATUS_EQUAL(status, NT_STATUS_NET_WRITE_FAULT)) {
			errstr = dcerpc_errstr(join, join->p->last_fault_code);
		}
		printf("samr_Connect failed - %s\n", errstr);
		return NULL;
	}

	printf("Opening domain %s\n", domain);

	name.string = domain;
	l.in.connect_handle = &handle;
	l.in.domain_name = &name;

	status = dcerpc_samr_LookupDomain(join->p, join, &l);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupDomain failed - %s\n", nt_errstr(status));
		goto failed;
	}

	talloc_steal(join, l.out.sid);
	join->dom_sid = *l.out.sid;
	join->dom_netbios_name = talloc_strdup(join, domain);
	if (!join->dom_netbios_name) goto failed;

	o.in.connect_handle = &handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = *l.out.sid;
	o.out.domain_handle = &domain_handle;

	status = dcerpc_samr_OpenDomain(join->p, join, &o);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		goto failed;
	}

	printf("Creating account %s\n", username);

again:
	name.string = username;
	r.in.domain_handle = &domain_handle;
	r.in.account_name = &name;
	r.in.acct_flags = acct_type;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle = &join->user_handle;
	r.out.access_granted = &access_granted;
	r.out.rid = &rid;

	status = dcerpc_samr_CreateUser2(join->p, join, &r);

	if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
		status = DeleteUser_byname(join->p, join, &domain_handle, name.string);
		if (NT_STATUS_IS_OK(status)) {
			goto again;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateUser2 failed - %s\n", nt_errstr(status));
		goto failed;
	}

	join->user_sid = dom_sid_add_rid(join, join->dom_sid, rid);

	pwp.in.user_handle = &join->user_handle;

	status = dcerpc_samr_GetUserPwInfo(join->p, join, &pwp);
	if (NT_STATUS_IS_OK(status)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}

	
	random_pw = generate_random_str(join, MAX(8, policy_min_pw_len));

	printf("Setting account password '%s'\n", random_pw);

	ZERO_STRUCT(u);
	s.in.user_handle = &join->user_handle;
	s.in.info = &u;
	s.in.level = 24;

	encode_pw_buffer(u.info24.password.data, random_pw, STR_UNICODE);
	u.info24.password_expired = 0;

	status = dcerpc_fetch_session_key(join->p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		torture_leave_domain(join);
		goto failed;
	}

	arcfour_crypt_blob(u.info24.password.data, 516, &session_key);

	status = dcerpc_samr_SetUserInfo(join->p, join, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(status));
		goto failed;
	}

	ZERO_STRUCT(u);
	s.in.user_handle = &join->user_handle;
	s.in.info = &u;
	s.in.level = 21;

	u.info21.acct_flags = acct_type | ACB_PWNOEXP;
	u.info21.fields_present = SAMR_FIELD_ACCT_FLAGS | SAMR_FIELD_DESCRIPTION | SAMR_FIELD_COMMENT | SAMR_FIELD_FULL_NAME;

	u.info21.comment.string = talloc_asprintf(join, 
						  "Tortured by Samba4: %s", 
						  timestring(join, time(NULL)));
	
	u.info21.full_name.string = talloc_asprintf(join, 
						    "Torture account for Samba4: %s", 
						    timestring(join, time(NULL)));
	
	u.info21.description.string = talloc_asprintf(join, 
					 "Samba4 torture account created by host %s: %s", 
					 lp_netbios_name(torture->lp_ctx), 
					 timestring(join, time(NULL)));

	printf("Resetting ACB flags, force pw change time\n");

	status = dcerpc_samr_SetUserInfo(join->p, join, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(status));
		goto failed;
	}

	if (random_password) {
		*random_password = random_pw;
	}

	return join;

failed:
	torture_leave_domain(join);
	return NULL;
}

/*
  leave the domain, deleting the machine acct
*/

_PUBLIC_ void torture_leave_domain(struct test_join *join)
{
	struct samr_DeleteUser d;
	NTSTATUS status;

	if (!join) {
		return;
	}
	d.in.user_handle = &join->user_handle;
	d.out.user_handle = &join->user_handle;
					
	/* Delete machine account */

	status = dcerpc_samr_DeleteUser(join->p, join, &d);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Delete of machine account failed\n");
	} else {
		printf("Delete of machine account was successful.\n");
	}

	talloc_free(join);
}
