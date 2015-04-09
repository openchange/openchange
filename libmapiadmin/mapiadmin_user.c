/*
   OpenChange Exchange Administration library.

   Based on the work by Andrew Tridgell, 2004

   Original source code available in SAMBA_4_0:
   source/torture/rpc/testjoin.c

   Copyright (C) Julien Kerihuel 2007-2010.

   SAMR related code

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

#include "libmapiadmin/libmapiadmin.h"

#include <param.h>
#include <credentials.h>
#include <ldb_errors.h>
#include <ldb_wrap.h>
#include <ldap_ndr.h>

#include <gen_ndr/ndr_samr.h>
#include <gen_ndr/ndr_samr_c.h>

#include <time.h>

/**
	\file
	User management functions for mapiadmin
*/

/**
 * open connection so SAMR + Join Domain
 * common code needed when adding or removing users
 */
static enum MAPISTATUS mapiadmin_samr_connect(struct mapiadmin_ctx *mapiadmin_ctx,
					      TALLOC_CTX *mem_ctx)
{
	NTSTATUS			status;
	struct tevent_context		*ev;
	struct mapi_context		*mapi_ctx;
	struct mapi_profile		*profile;
	struct samr_Connect		c;
	struct samr_OpenDomain		o;
	struct samr_LookupDomain	l;
	struct policy_handle		handle;
	struct policy_handle		domain_handle;
	struct lsa_String		name;

	MAPI_RETVAL_IF(!mapiadmin_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->session, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->session->profile, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->session->profile->credentials, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->username, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = mapiadmin_ctx->session->mapi_ctx;
	MAPI_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	profile = mapiadmin_ctx->session->profile;
	
	mapiadmin_ctx->user_ctx = talloc_zero(mem_ctx, struct test_join);
	MAPI_RETVAL_IF(!mapiadmin_ctx->user_ctx, MAPI_E_NOT_ENOUGH_RESOURCES ,NULL);

	OC_DEBUG(3, "Connecting to SAMR");

	ev = tevent_context_init(mem_ctx);

	status = dcerpc_pipe_connect(mapiadmin_ctx->user_ctx,
				     &mapiadmin_ctx->user_ctx->p,
				     mapiadmin_ctx->dc_binding ? 
				     mapiadmin_ctx->dc_binding : 
				     mapiadmin_ctx->binding,
				     &ndr_table_samr,
				     profile->credentials, ev, mapi_ctx->lp_ctx);
					     
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, NULL);	

	profile = mapiadmin_ctx->session->profile;

	c.in.system_name = NULL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.out.connect_handle = &handle;

	status = dcerpc_samr_Connect_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		if (NT_STATUS_EQUAL(status, NT_STATUS_NET_WRITE_FAULT)) {
			errstr = dcerpc_errstr(mapiadmin_ctx->user_ctx, mapiadmin_ctx->user_ctx->p->last_fault_code);
		}
		OC_DEBUG(3, "samr_Connect failed - %s", errstr);
		return MAPI_E_CALL_FAILED;
	}

	OC_DEBUG(3, "Opening domain %s", profile->domain);

	name.string = profile->domain;
	l.in.connect_handle = &handle;
	l.in.domain_name = &name;

	l.out.sid = talloc(mem_ctx, struct dom_sid2 *);
	talloc_steal(mapiadmin_ctx->user_ctx, l.out.sid);

	status = dcerpc_samr_LookupDomain_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &l);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "LookupDomain failed - %s", nt_errstr(status));
		return MAPI_E_CALL_FAILED;
	}

	mapiadmin_ctx->user_ctx->dom_sid = *l.out.sid;
	mapiadmin_ctx->user_ctx->dom_netbios_name = talloc_strdup(mapiadmin_ctx->user_ctx, profile->domain);
	if (!mapiadmin_ctx->user_ctx->dom_netbios_name) return MAPI_E_CALL_FAILED;

	o.in.connect_handle = &handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = *l.out.sid;
	o.out.domain_handle = &domain_handle;

	status = dcerpc_samr_OpenDomain_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &o);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "OpenDomain failed - %s", nt_errstr(status));
		return MAPI_E_CALL_FAILED;
	}

	mapiadmin_ctx->handle = talloc_memdup(mem_ctx, &domain_handle, sizeof (struct policy_handle));

	errno = 0;
	return MAPI_E_SUCCESS;
}


struct tce_async_context {
	int	found;
};

static int tce_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct tce_async_context	*actx = talloc_get_type(req->context, struct tce_async_context);

	switch (ares->type) {

	case LDB_REPLY_ENTRY:
		if (ldb_msg_find_element(ares->message, "msExchMailboxGuid") != NULL) {
			OC_DEBUG(3, "msExchMailboxGuid found!");
			actx->found = 1;
			talloc_free(ares);
			return ldb_request_done(req, LDB_SUCCESS);
		}
		break;
	case LDB_REPLY_DONE:
		break;
	default:
		OC_DEBUG(3, "unknown Reply Type ignore it");
		talloc_free(ares);
		return LDB_ERR_OTHER;
	}

	if (talloc_free(ares) == -1) {
		OC_DEBUG(3, "talloc_free failed");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

/**
 * Extend user attributes to be Exchange user
 */
_PUBLIC_ enum MAPISTATUS mapiadmin_user_extend(struct mapiadmin_ctx *mapiadmin_ctx)
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct tevent_context		*ev = NULL;
	struct mapi_context		*mapi_ctx;
	struct mapi_profile		*profile;
	struct ldb_context		*remote_ldb;
	struct ldb_request		*req;
	struct ldb_message		*msg;
	struct ldb_result		*res;
	struct ldb_control		**controls;
	const char			*control_strings[2] = { "notification:0", NULL };
	struct tce_async_context	*tce_ctx;
	const struct dom_sid		*dom_sid;
	char				*remote_ldb_url;
	const char * const		dom_attrs[] = { "*", NULL };
	int				ret;
	uint32_t			count;
	char				**values;
	const char			*exch_attrs[7];
	uint32_t			i;
	char				*realm = NULL;
	char				*org = NULL;
	const char			*UserAccountControl;
	struct ldb_dn			*account_dn;

	/* Sanity checks */
	MAPI_RETVAL_IF(!mapiadmin_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->session, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->session->profile, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->session->profile->credentials, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->user_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_ctx = mapiadmin_ctx->session->mapi_ctx;
	MAPI_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	profile = mapiadmin_ctx->session->profile;
	dom_sid = mapiadmin_ctx->user_ctx->user_sid;

	/* initialize memory context */
	mem_ctx = talloc_named((TALLOC_CTX *)mapiadmin_ctx, 0, "mapiadmin_user_extend");

	/* open LDAP connection */
	ev = tevent_context_init(mem_ctx);
	remote_ldb_url = talloc_asprintf(mem_ctx, "ldap://%s", profile->server);
	MAPI_RETVAL_IF(!remote_ldb_url, MAPI_E_CORRUPT_DATA, mem_ctx);
	remote_ldb = ldb_wrap_connect(mem_ctx, ev, mapi_ctx->lp_ctx, remote_ldb_url, 
				      NULL, mapiadmin_ctx->session->profile->credentials, 0);
	MAPI_RETVAL_IF(!remote_ldb, ecRpcFailed, mem_ctx);

	/* Search the user_dn */
	account_dn = samdb_search_dn(remote_ldb, mem_ctx, NULL, 
				     "(&(objectSid=%s)(objectClass=user))", 
				     ldap_encode_ndr_dom_sid(mem_ctx, dom_sid));

	ret = ldb_search(remote_ldb, mem_ctx, &res, account_dn, LDB_SCOPE_SUBTREE, dom_attrs, "(objectSid=%s)",
			 ldap_encode_ndr_dom_sid(mem_ctx, dom_sid));
	MAPI_RETVAL_IF(ret != LDB_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);
	MAPI_RETVAL_IF(res->count != 1, MAPI_E_NOT_FOUND, mem_ctx);

	/* Prepare a new message for modify */
	msg = ldb_msg_new(mem_ctx);
	MAPI_RETVAL_IF(!msg, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	msg->dn = res->msgs[0]->dn;

	/* message: givenName */
	exch_attrs[0] = talloc_strdup(mem_ctx, mapiadmin_ctx->username);
	ret = ldb_msg_add_string(msg, "givenName", exch_attrs[0]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* message: userAccountControl */
	exch_attrs[1] = talloc_asprintf(mem_ctx, "513");
	ret = ldb_msg_add_string(msg, "userAccountControl", exch_attrs[1]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);
	msg->elements[1].flags = LDB_FLAG_MOD_REPLACE;

	/* message: mail */
	retval = GetProfileAttr(profile, "ProxyAddress", &count, &values);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	for (i = 0; i < count; i++) {
		if (values[i] && !strncasecmp("smtp", values[i], 4)) {
			realm = strchr(values[i], '@');
			realm += 1;
		}
	}
	MAPI_RETVAL_IF(!realm, MAPI_E_NOT_FOUND, mem_ctx);

	exch_attrs[2] = talloc_asprintf(mem_ctx, "%s@%s", mapiadmin_ctx->username, realm);
	ret = ldb_msg_add_string(msg, "mail", exch_attrs[2]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* message: mailNickname */
	exch_attrs[3] = talloc_strdup(mem_ctx, mapiadmin_ctx->username);
	ret = ldb_msg_add_string(msg, "mailNickname", exch_attrs[3]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* message: mDBUseDefaults */
	exch_attrs[4] = talloc_asprintf(mem_ctx, "TRUE");
	ret = ldb_msg_add_string(msg, "mDBUseDefaults", exch_attrs[4]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* message: legacyExchangeDN */
	org = talloc_strndup(mem_ctx, profile->mailbox,
			     strlen(profile->mailbox) - strlen(profile->username));
	exch_attrs[5] = talloc_asprintf(mem_ctx, "%s%s", org, mapiadmin_ctx->username);
	talloc_free(org);
	ret = ldb_msg_add_string(msg, "legacyExchangeDN", exch_attrs[5]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* message: msExchHomeServerName */
	exch_attrs[6] = talloc_strdup(mem_ctx, profile->homemdb);
	ret = ldb_msg_add_string(msg, "msExchHomeServerName", exch_attrs[6]);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);

	/* Prior we call ldb_modify, set up async ldb request on
	 * msExchMailboxGuid 
	 */
	req = talloc_zero(mem_ctx, struct ldb_request);
	tce_ctx = talloc_zero(mem_ctx, struct tce_async_context);
	controls = ldb_parse_control_strings(remote_ldb, mem_ctx, control_strings);

	ret = ldb_build_search_req(&req, remote_ldb, mem_ctx, 
				   msg->dn,
				   LDB_SCOPE_BASE,
				   "(objectclass=*)",
				   NULL,
				   controls, 
				   (void *)tce_ctx, 
				   tce_search_callback, 
				   NULL);
	OC_DEBUG(3, "ldb_build_search_req: %s", ldb_strerror(ret));
	MAPI_RETVAL_IF((ret != LDB_SUCCESS), MAPI_E_CALL_FAILED, mem_ctx);

	ldb_set_timeout(mem_ctx, req, 60);

	ret = ldb_request(remote_ldb, req);
	OC_DEBUG(3, "ldb_request: %s", ldb_strerror(ret));
	MAPI_RETVAL_IF((ret != LDB_SUCCESS), MAPI_E_CALL_FAILED, mem_ctx);

	ret = ldb_modify(remote_ldb, msg);
	OC_DEBUG(3, "ldb_modify: %s", ldb_strerror(ret));
	MAPI_RETVAL_IF((ret != LDB_SUCCESS), MAPI_E_CORRUPT_DATA, mem_ctx);

	/* async search */
	ret = ldb_wait(req->handle, LDB_WAIT_ALL);
	OC_DEBUG(3, "ldb_wait: %s", ldb_strerror(ret));
	MAPI_RETVAL_IF((ret != LDB_SUCCESS), MAPI_E_CALL_FAILED, mem_ctx);
	MAPI_RETVAL_IF(!tce_ctx->found, MAPI_E_CALL_FAILED, mem_ctx);
	
	/* When successful replace UserAccountControl attr in the user
	 * record 
	 */
	talloc_free(msg);
	msg = ldb_msg_new(mem_ctx);
	MAPI_RETVAL_IF(!msg, MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);
	msg->dn = res->msgs[0]->dn;

	UserAccountControl = talloc_asprintf(mem_ctx, "66048");
	ret = ldb_msg_add_string(msg, "UserAccountControl", UserAccountControl);
	MAPI_RETVAL_IF((ret == -1), MAPI_E_NOT_ENOUGH_RESOURCES, mem_ctx);
	msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;

	ret = ldb_modify(remote_ldb, msg);
	OC_DEBUG(3, "ldb_modify: %s", ldb_strerror(ret));
	MAPI_RETVAL_IF((ret != LDB_SUCCESS), MAPI_E_CORRUPT_DATA, mem_ctx);

	/* reset errno before leaving */
	errno = 0;
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

/**
 * Add a user to Active Directory 
 */
_PUBLIC_ enum MAPISTATUS mapiadmin_user_add(struct mapiadmin_ctx *mapiadmin_ctx)
{
	TALLOC_CTX			*mem_ctx;
	NTSTATUS			status;
	enum MAPISTATUS			retval;
	struct mapi_context		*mapi_ctx;
	struct samr_CreateUser2		r;
	struct samr_GetUserPwInfo	pwp;
	struct samr_SetUserInfo		s;
	union samr_UserInfo		u;
	uint32_t			access_granted;
	uint32_t			rid;
	DATA_BLOB			session_key;
	struct lsa_String		name;
	int				policy_min_pw_len = 0;

	mem_ctx = talloc_named(NULL, 0, "mapiadmin_user_add");

	retval = mapiadmin_samr_connect(mapiadmin_ctx, mem_ctx);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	OC_DEBUG(3, "Creating account %s", mapiadmin_ctx->username);

	mapi_ctx = mapiadmin_ctx->session->mapi_ctx;
	MAPI_RETVAL_IF(!mapi_ctx, MAPI_E_NOT_INITIALIZED, mem_ctx);

again:
	name.string = mapiadmin_ctx->username;
	r.in.domain_handle = mapiadmin_ctx->handle;
	r.in.account_name = &name;
	r.in.acct_flags = ACB_NORMAL;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle = &mapiadmin_ctx->user_ctx->user_handle;
	r.out.access_granted = &access_granted;
	r.out.rid = &rid;

	status = dcerpc_samr_CreateUser2_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &r);

	if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
		mapiadmin_user_del(mapiadmin_ctx);
		if (NT_STATUS_IS_OK(status)) {
			goto again;
		} else {
		        MAPI_RETVAL_IF(1,MAPI_E_CALL_FAILED,mem_ctx);
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "CreateUser2 failed - %s", nt_errstr(status));
	        MAPI_RETVAL_IF(1,MAPI_E_CALL_FAILED,mem_ctx);
	}

	mapiadmin_ctx->user_ctx->user_sid = dom_sid_add_rid(mapiadmin_ctx->user_ctx, mapiadmin_ctx->user_ctx->dom_sid, rid);

	pwp.in.user_handle = &mapiadmin_ctx->user_ctx->user_handle;
	pwp.out.info = talloc_zero(mem_ctx, struct samr_PwInfo);

	status = dcerpc_samr_GetUserPwInfo_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &pwp);
	if (NT_STATUS_IS_OK(status)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	} else {
		OC_DEBUG(3, "GetUserPwInfo failed - %s", nt_errstr(status));
		MAPI_RETVAL_IF(1,MAPI_E_CALL_FAILED,mem_ctx);
	}

	if (!mapiadmin_ctx->password) {
		mapiadmin_ctx->password = generate_random_str(mapiadmin_ctx->user_ctx, MAX(8, policy_min_pw_len));
	}

	OC_DEBUG(3, "Setting account password '%s'", mapiadmin_ctx->password);

	ZERO_STRUCT(u);
	s.in.user_handle = &mapiadmin_ctx->user_ctx->user_handle;
	s.in.info = &u;
	s.in.level = 24;

	encode_pw_buffer(u.info24.password.data, mapiadmin_ctx->password, STR_UNICODE);
	u.info24.password_expired = 0;

	status = dcerpc_fetch_session_key(mapiadmin_ctx->user_ctx->p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "SetUserInfo level %d - no session key - %s",
			  s.in.level, nt_errstr(status));
		mapiadmin_user_del(mapiadmin_ctx);
		MAPI_RETVAL_IF(1,MAPI_E_CALL_FAILED,mem_ctx);
	}

	arcfour_crypt_blob(u.info24.password.data, 516, &session_key);

	status = dcerpc_samr_SetUserInfo_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &s);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "SetUserInfo failed - %s", nt_errstr(status));
		if (NT_STATUS_EQUAL(status, NT_STATUS_PASSWORD_RESTRICTION)) {
		        MAPI_RETVAL_IF(1, MAPI_E_BAD_VALUE, mem_ctx);
		} else {
		        MAPI_RETVAL_IF(1, MAPI_E_CALL_FAILED, mem_ctx);
		}
	}

	ZERO_STRUCT(u);
	s.in.user_handle = &mapiadmin_ctx->user_ctx->user_handle;
	s.in.info = &u;
	s.in.level = 21;

	u.info21.acct_flags = ACB_NORMAL | ACB_PWNOEXP;
	u.info21.fields_present = SAMR_FIELD_ACCT_FLAGS | SAMR_FIELD_DESCRIPTION | SAMR_FIELD_COMMENT | SAMR_FIELD_FULL_NAME;

	u.info21.comment.string = talloc_asprintf(mapiadmin_ctx->user_ctx, 
						  mapiadmin_ctx->comment ? 
						  mapiadmin_ctx->comment :
						  "Created by OpenChange: %s", 
						  timestring(mapiadmin_ctx->user_ctx, time(NULL)));
	
	u.info21.full_name.string = talloc_asprintf(mapiadmin_ctx->user_ctx, 
						    mapiadmin_ctx->fullname ?
						    mapiadmin_ctx->fullname :
						    "Account for OpenChange: %s", 
						    timestring(mapiadmin_ctx->user_ctx, time(NULL)));
	
	u.info21.description.string = talloc_asprintf(mapiadmin_ctx->user_ctx, 
						      mapiadmin_ctx->description ?
						      mapiadmin_ctx->description :
						      "OpenChange account created by host %s: %s", 
					 lpcfg_netbios_name(mapi_ctx->lp_ctx), 
					 timestring(mapiadmin_ctx->user_ctx, time(NULL)));

	OC_DEBUG(3, "Resetting ACB flags, force pw change time");

	status = dcerpc_samr_SetUserInfo_r(mapiadmin_ctx->user_ctx->p->binding_handle, mapiadmin_ctx->user_ctx, &s);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "SetUserInfo failed - %s", nt_errstr(status));
		MAPI_RETVAL_IF(1, MAPI_E_CALL_FAILED, mem_ctx);
	}
	retval = mapiadmin_user_extend(mapiadmin_ctx);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(3, "mapiadmin_user_extend: 0x%x", GetLastError());
	        mapiadmin_user_del(mapiadmin_ctx);
		MAPI_RETVAL_IF(1, MAPI_E_CALL_FAILED,mem_ctx);
	}

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

/**
 * Delete a user from Active Directory 
 */
_PUBLIC_ enum MAPISTATUS mapiadmin_user_del(struct mapiadmin_ctx *mapiadmin_ctx)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	NTSTATUS		status;
	struct samr_DeleteUser	d;
	struct policy_handle	user_handle;
	uint32_t		rid;
	struct samr_LookupNames	n;
	struct lsa_String	sname;
	struct samr_OpenUser	r;

	MAPI_RETVAL_IF(!mapiadmin_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!mapiadmin_ctx->username, MAPI_E_NOT_INITIALIZED, NULL);

	mem_ctx = talloc_named(NULL, 0, "mapiadmin_user_del");

 	/* Initiate SAMR connection if not already done */
	if (!mapiadmin_ctx->user_ctx) {
		retval = mapiadmin_samr_connect(mapiadmin_ctx, mem_ctx);
		MAPI_RETVAL_IF(retval, GetLastError(), mem_ctx);		
	}

	sname.string = mapiadmin_ctx->username;

	n.in.domain_handle = mapiadmin_ctx->handle;
	n.in.num_names = 1;
	n.in.names = &sname;

	n.out.rids = talloc_zero(mem_ctx, struct samr_Ids);
	n.out.types = talloc_zero(mem_ctx, struct samr_Ids);

	status = dcerpc_samr_LookupNames_r(mapiadmin_ctx->user_ctx->p->binding_handle, mem_ctx, &n);
	if (NT_STATUS_IS_OK(status)) {
		rid = n.out.rids->ids[0];
	} else {
		talloc_free(mem_ctx);
		return MAPI_E_NOT_FOUND;
	}

	r.in.domain_handle = mapiadmin_ctx->handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	status = dcerpc_samr_OpenUser_r(mapiadmin_ctx->user_ctx->p->binding_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(3, "OpenUser(%s) failed - %s", mapiadmin_ctx->username, nt_errstr(status));
		MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_NOT_FOUND, mem_ctx);
	}

	d.in.user_handle = &user_handle;
	d.out.user_handle = &user_handle;
	status = dcerpc_samr_DeleteUser_r(mapiadmin_ctx->user_ctx->p->binding_handle, mem_ctx, &d);
	MAPI_RETVAL_IF(!NT_STATUS_IS_OK(status), MAPI_E_CALL_FAILED, mem_ctx);

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

_PUBLIC_ enum MAPISTATUS mapiadmin_user_mod(struct mapiadmin_ctx *mapiadmin)
{
	return MAPI_E_NO_SUPPORT;
}
