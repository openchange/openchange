/*
   OpenChange Unit Testing

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

#include "testsuite.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "mapiproxy/libmapistore/gen_ndr/mapistore_notification.h"
#include "mapiproxy/libmapistore/gen_ndr/ndr_mapistore_notification.h"

/* Global variables */
static struct GUID	gl_async_uuid;
static struct GUID	gl_uuid;
static const char	*gl_cn = "FooBar";
static const char	*gl_host1 = "tcp://host1:9005";
static const char	*gl_host2 = "tcp://host2:9006";
static const char	*gl_host3 = "tcp://host3:9007";
static const uint64_t	gl_FolderId = 0xf503000000000001;
static const uint64_t	gl_MessageId = 0xdeadbeef00000001;
static const uint32_t	gl_handle = 0x42;
static const uint16_t	gl_flags_newmail = sub_NewMail;
static const uint16_t	gl_flags_table = sub_TableModified;
static const uint16_t	gl_flags_wholestore = sub_WholeStore;
static enum MAPITAGS	gl_tags[] = { PidTagParentFolderId, PidTagSubject };
static const char	*gl_deliver_1 = "deliver1";
static const char	*gl_deliver_2 = "deliver2";

START_TEST(test_initialization) {
	TALLOC_CTX				*mem_ctx = NULL;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	bool					bret;

	mem_ctx = talloc_named(NULL, 0, "test_initialization");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	/* check sanity check compliance */
	retval = mapistore_notification_init(mem_ctx, NULL, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* check fallback behavior */
	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check invalid memcached server configuration settings */
	bret = lpcfg_set_cmdline(lp_ctx, "mapistore:notification_cache", "--SERVER=");
	ck_assert(bret == true);
	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_ERR_CONTEXT_FAILED);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(session_add) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;
	const char				*cn = NULL;

	/* Check sanity check compliance */
	retval = mapistore_notification_session_add(NULL, gl_uuid, gl_async_uuid, cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_session_add(&mstore_ctx, gl_uuid, gl_async_uuid, cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_session_add(&mstore_ctx, gl_uuid, gl_async_uuid, cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "session_add");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check when cn is NULL: ndr_push_error expected */
	mstore_ctx.notification_ctx = ctx;
	retval = mapistore_notification_session_add(&mstore_ctx, gl_uuid, gl_async_uuid, cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_DATA);

	/* register session */
	cn = gl_cn;
	retval = mapistore_notification_session_add(&mstore_ctx, gl_uuid, gl_async_uuid, cn);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check if session is registered */
	retval = mapistore_notification_session_exist(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* attempt to double register the session */
	retval = mapistore_notification_session_add(&mstore_ctx, gl_uuid, gl_async_uuid, cn);
	ck_assert_int_eq(retval, MAPISTORE_ERROR);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(session_exist) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_session_exist(NULL, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_session_exist(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_session_exist(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "session_exist");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Test non-existent key */
	retval = mapistore_notification_session_exist(&mstore_ctx, GUID_random());
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Test global async_uuid key */
	retval = mapistore_notification_session_exist(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(session_get) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;
	struct GUID				uuid;
	char					*cnp = NULL;

	/* Check sanity check compliance */
	retval = mapistore_notification_session_get(NULL, NULL, gl_async_uuid, &uuid, &cnp);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_session_get(NULL, &mstore_ctx, gl_async_uuid, NULL, &cnp);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_session_get(NULL, &mstore_ctx, gl_async_uuid, &uuid, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_session_get(NULL, &mstore_ctx, gl_async_uuid, &uuid, &cnp);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_session_get(NULL, &mstore_ctx, gl_async_uuid, &uuid, &cnp);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "session_get");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Retrieve non existent record */
	retval = mapistore_notification_session_get(mem_ctx, &mstore_ctx, GUID_random(), &uuid, &cnp);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Retrieve global async_uuid record data */
	retval = mapistore_notification_session_get(mem_ctx, &mstore_ctx, gl_async_uuid, &uuid, &cnp);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_str_eq(gl_cn, cnp);
	ck_assert(GUID_compare(&gl_async_uuid, &uuid));

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(session_delete) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_session_delete(NULL, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_session_delete(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_session_delete(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "session_delete");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to delete non existing key */
	retval = mapistore_notification_session_delete(&mstore_ctx, GUID_random());
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Try to delete global session */
	retval = mapistore_notification_session_delete(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* try to delete twice */
	retval = mapistore_notification_session_delete(&mstore_ctx, gl_async_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(resolver_add) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_resolver_add(NULL, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_resolver_add(&mstore_ctx, NULL, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "resolver_add");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check when cn has space */
	mstore_ctx.notification_ctx = ctx;
	retval = mapistore_notification_resolver_add(&mstore_ctx, "Foo Bar", gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_DATA);

	/* add record */
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check if record now exists */
	retval = mapistore_notification_resolver_exist(&mstore_ctx, gl_cn);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* try to register gl_host1 twice */
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_EXIST);

	/* add 2nd host to gl_cn record */
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host2);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* add 3rd host to gl_cn record */
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host3);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* try to add 2nd host again to gl_cn record */
	retval = mapistore_notification_resolver_add(&mstore_ctx, gl_cn, gl_host2);
	ck_assert_int_eq(retval, MAPISTORE_ERR_EXIST);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(resolver_exist) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_resolver_exist(NULL, gl_cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_resolver_exist(&mstore_ctx, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_resolver_exist(&mstore_ctx, gl_cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_resolver_exist(&mstore_ctx, gl_cn);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "resolver_exist");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	mstore_ctx.notification_ctx = ctx;

	/* Test gl_cn existence */
	retval = mapistore_notification_resolver_exist(&mstore_ctx, gl_cn);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Test non existent key */
	retval = mapistore_notification_resolver_exist(&mstore_ctx, "nonexistent");
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(resolver_get) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;
	uint32_t				count = 0;
	const char				**hosts;

	/* Check sanity check compliance */
	retval = mapistore_notification_resolver_get(NULL, NULL, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_resolver_get(NULL, &mstore_ctx, NULL, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_resolver_get(NULL, &mstore_ctx, gl_cn, NULL, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_resolver_get(NULL, &mstore_ctx, gl_cn, &count, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_resolver_get(NULL, &mstore_ctx, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_resolver_get(NULL, &mstore_ctx, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "resolver_exist");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	mstore_ctx.notification_ctx = ctx;

	/* Retrieve non existent record */
	retval = mapistore_notification_resolver_get(mem_ctx, &mstore_ctx, "nonexistent", &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Retrieve gl_cn record data */
	retval = mapistore_notification_resolver_get(mem_ctx, &mstore_ctx, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(count, 3);
	ck_assert_str_eq(hosts[0], gl_host1);
	ck_assert_str_eq(hosts[1], gl_host2);
	ck_assert_str_eq(hosts[2], gl_host3);
	talloc_free(hosts);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(resolver_delete) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;
	uint32_t				count = 0;
	const char				**hosts = NULL;

	/* Check sanity check compliance */
	retval = mapistore_notification_resolver_delete(NULL, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_resolver_delete(&mstore_ctx, NULL, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "session_delete");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to delete non existent key */
	retval = mapistore_notification_resolver_delete(&mstore_ctx, "nonexistent", gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Try to delete gl_host1 from gl_cn */
	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Try to delete gl_host1 twice */
	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, gl_host1);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Check that record now only have gl_host2 and gl_host3 */
	retval = mapistore_notification_resolver_get(mem_ctx, &mstore_ctx, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(count, 2);
	ck_assert_str_eq(hosts[0], gl_host2);
	ck_assert_str_eq(hosts[1], gl_host3);
	talloc_free(hosts);

	/* Try to delete gl_host2 */
	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, gl_host2);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_resolver_get(mem_ctx, &mstore_ctx, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(count, 1);
	ck_assert_str_eq(hosts[0], gl_host3);
	talloc_free(hosts);

	/* Try to delete gl_host3 */
	retval = mapistore_notification_resolver_delete(&mstore_ctx, gl_cn, gl_host3);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_resolver_get(mem_ctx, &mstore_ctx, gl_cn, &count, &hosts);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

} END_TEST


START_TEST(subscription_add) {
	TALLOC_CTX					*mem_ctx;
	struct mapistore_context			mstore_ctx;
	enum mapistore_error				retval;
	struct loadparm_context				*lp_ctx;
	struct mapistore_notification_context		*ctx = NULL;
	struct mapistore_notification_context		_ctx;
	struct mapistore_notification_subscription	r;
	uint16_t					flags;

	/* Check sanity check compliance */
	retval = mapistore_notification_subscription_add(NULL, gl_uuid, gl_handle, gl_flags_newmail,
							 gl_FolderId, gl_MessageId, 0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle,
							 gl_flags_newmail, gl_FolderId,
							 gl_MessageId, 1, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* notification_ctx checks */
	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle,
							 gl_flags_newmail, gl_FolderId,
							 gl_MessageId, 0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle,
							 gl_flags_newmail, gl_FolderId,
							 gl_MessageId, 0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);


	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "subscription_add");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* add subscription on folder */
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle,
							 gl_flags_newmail, gl_FolderId,
							 0x0, 0x0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check if record now exists */
	retval = mapistore_notification_subscription_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* try to add same subscription twice */
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle,
							 gl_flags_newmail, gl_FolderId,
							 0x0, 0x0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_EXIST);

	/* retrieve subscription and check value */
	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(r.vnum, 1);
	ck_assert_int_eq(r.v.v1.count, 1);
	talloc_free(r.v.v1.subscription);

	/* try to add a second subscription */
	flags = sub_WholeStore|sub_ObjectModified;
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle + 1,
							 flags, 0x0, 0x0, 0x0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(r.vnum, 1);
	ck_assert_int_eq(r.v.v1.count, 2);
	ck_assert_int_eq(r.v.v1.subscription[0].flags, gl_flags_newmail);
	ck_assert_int_eq(r.v.v1.subscription[1].flags, flags);
	talloc_free(r.v.v1.subscription);

	/* try to add a table subscription */
	flags = sub_WholeStore|sub_TableModified;
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle + 2,
							 flags, gl_FolderId, 0x0, 2, gl_tags);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(r.vnum, 1);
	ck_assert_int_eq(r.v.v1.count, 3);
	ck_assert_int_eq(r.v.v1.subscription[2].flags, flags);
	ck_assert_int_eq(r.v.v1.subscription[2].count, 2);
	ck_assert_int_eq(r.v.v1.subscription[2].properties[0], gl_tags[0]);
	ck_assert_int_eq(r.v.v1.subscription[2].properties[1], gl_tags[1]);
	talloc_free(r.v.v1.subscription[2].properties);
	talloc_free(r.v.v1.subscription);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(subscription_exist) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_subscription_exist(NULL, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_subscription_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_subscription_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "subscription_exist");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	mstore_ctx.notification_ctx = ctx;

	/* Test gl_uuid existence */
	retval = mapistore_notification_subscription_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Test non existent uuid */
	retval = mapistore_notification_subscription_exist(&mstore_ctx, GUID_random());
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(subscription_get) {
	TALLOC_CTX					*mem_ctx;
	struct mapistore_context			mstore_ctx;
	enum mapistore_error				retval;
	struct loadparm_context				*lp_ctx;
	struct mapistore_notification_context		*ctx = NULL;
	struct mapistore_notification_context		_ctx;
	struct mapistore_notification_subscription	r;

	/* Check sanity check compliance */
	retval = mapistore_notification_subscription_get(NULL, NULL, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_subscription_get(NULL, &mstore_ctx, gl_uuid, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_subscription_get(NULL, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_subscription_get(NULL, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "subscription_get");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to retrieve non existent key */
	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, GUID_random(), &r);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Try to retrieve existent key */
	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(r.v.v1.count, 3);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(subscription_delete) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_subscription_delete(NULL, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_subscription_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_subscription_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "subscription_delete");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to delete non existent key */
	retval = mapistore_notification_subscription_delete(&mstore_ctx, GUID_random());
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* add subscription on folder */
	retval = mapistore_notification_subscription_add(&mstore_ctx, gl_uuid, gl_handle,
							 gl_flags_newmail, gl_FolderId,
							 0x0, 0x0, NULL);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* check if record now exists */
	retval = mapistore_notification_subscription_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Try to delete gl_uuid record */
	retval = mapistore_notification_subscription_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* try to delete gl_uuid twice */
	retval = mapistore_notification_subscription_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

START_TEST(subscription_delete_by_handle) {
	TALLOC_CTX					*mem_ctx;
	struct mapistore_context			mstore_ctx;
	enum mapistore_error				retval;
	struct loadparm_context				*lp_ctx;
	struct mapistore_notification_context		*ctx = NULL;
	struct mapistore_notification_context		_ctx;
	struct mapistore_notification_subscription	r;

	/* Check sanity check compliance */
	retval = mapistore_notification_subscription_delete_by_handle(NULL, gl_uuid, gl_handle);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, gl_handle);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, gl_handle);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "subscription_delete_by_handle");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to delete non existent key */
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, GUID_random(), gl_handle);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Try to delete non existent handle */
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, 6);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Try to delete existing handle */
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, gl_handle);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(r.v.v1.count, 2);
	talloc_free(r.v.v1.subscription);

	/* Try to delete same handle twice */
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, gl_handle);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* try to delete other 2 handles */
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, gl_handle + 1);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_subscription_get(mem_ctx, &mstore_ctx, gl_uuid, &r);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_int_eq(r.v.v1.count, 1);
	talloc_free(r.v.v1.subscription);

	/* Try to delete last handle */
	retval = mapistore_notification_subscription_delete_by_handle(&mstore_ctx, gl_uuid, gl_handle + 2);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	retval = mapistore_notification_subscription_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(deliver_add) {
	TALLOC_CTX				*mem_ctx;
	enum mapistore_error			retval;
	struct mapistore_context		mstore_ctx;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;
	DATA_BLOB				payload;

	/* Set payload */
	payload.data = (uint8_t *) gl_deliver_1;
	payload.length = strlen(gl_deliver_1) + 1;

	/* Check sanity check compliance */
	retval = mapistore_notification_deliver_add(NULL, gl_uuid, payload.data, payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_deliver_add(&mstore_ctx, gl_uuid, NULL, payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_deliver_add(&mstore_ctx, gl_uuid, payload.data, 0);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_deliver_add(&mstore_ctx, gl_uuid, payload.data, payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_deliver_add(&mstore_ctx, gl_uuid, payload.data, payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "deliver_add");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Add a deliver payload */
	retval = mapistore_notification_deliver_add(&mstore_ctx, gl_uuid, payload.data, payload.length);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Check if the record now exists */
	retval = mapistore_notification_deliver_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Retrieve payload */
	retval = mapistore_notification_deliver_get(mem_ctx, &mstore_ctx, gl_uuid,
						    &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_str_eq(payload.data, gl_deliver_1);
	ck_assert_int_eq(payload.length, strlen(gl_deliver_1) + 1);
	talloc_free(payload.data);

	/* Append a second payload */
	payload.data = (uint8_t *) gl_deliver_2;
	payload.length = strlen(gl_deliver_2) + 1;
	retval = mapistore_notification_deliver_add(&mstore_ctx, gl_uuid, payload.data, payload.length);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Retrieve payload */
	retval = mapistore_notification_deliver_get(mem_ctx, &mstore_ctx, gl_uuid,
						    &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(deliver_exist) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_deliver_exist(NULL, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_deliver_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_deliver_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "deliver_exist");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Test gl_uuid existence */
	retval = mapistore_notification_deliver_exist(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Test non existent uuid */
	retval = mapistore_notification_deliver_exist(&mstore_ctx, GUID_random());
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(deliver_get) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;
	DATA_BLOB				payload;

	/* Check sanity check compliance */
	retval = mapistore_notification_deliver_get(NULL, NULL, gl_uuid, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	retval = mapistore_notification_deliver_get(NULL, &mstore_ctx, gl_uuid, NULL, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_deliver_get(NULL, &mstore_ctx, gl_uuid, &payload.data, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_deliver_get(NULL, &mstore_ctx, gl_uuid, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_deliver_get(NULL, &mstore_ctx, gl_uuid, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "deliver_delete");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to get a non existing key */
	retval = mapistore_notification_deliver_get(mem_ctx, &mstore_ctx, GUID_random(), &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	/* Try to get existing key */
	retval = mapistore_notification_deliver_get(mem_ctx, &mstore_ctx, gl_uuid, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	ck_assert_str_eq(payload.data, gl_deliver_1);
	ck_assert_str_eq(payload.data + strlen(gl_deliver_1) + 1, gl_deliver_2);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


START_TEST(deliver_delete) {
	TALLOC_CTX				*mem_ctx;
	struct mapistore_context		mstore_ctx;
	enum mapistore_error			retval;
	struct loadparm_context			*lp_ctx;
	struct mapistore_notification_context	*ctx = NULL;
	struct mapistore_notification_context	_ctx;

	/* Check sanity check compliance */
	retval = mapistore_notification_deliver_delete(NULL, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_INITIALIZED);

	mstore_ctx.notification_ctx = NULL;
	retval = mapistore_notification_deliver_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	mstore_ctx.notification_ctx = &_ctx;
	mstore_ctx.notification_ctx->memc_ctx = NULL;
	retval = mapistore_notification_deliver_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_AVAILABLE);

	/* Initialize mapistore notification system */
	mem_ctx = talloc_named(NULL, 0, "deliver_delete");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	retval = mapistore_notification_init(mem_ctx, lp_ctx, &ctx);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);
	mstore_ctx.notification_ctx = ctx;

	/* Try to delete gl_uuid  */
	retval = mapistore_notification_deliver_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	/* Try to delete gl_uuid twice */
	retval = mapistore_notification_deliver_delete(&mstore_ctx, gl_uuid);
	ck_assert_int_eq(retval, MAPISTORE_ERR_NOT_FOUND);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);
} END_TEST


START_TEST(payload_newmail) {
	enum mapistore_error		retval;
	TALLOC_CTX			*mem_ctx;
	DATA_BLOB			payload;
	char				*backend = "python://";
	char				*eml = "123456.eml";
	char				*folder = "";
	char				separator = '.';
	struct ndr_pull			*ndr;
	enum ndr_err_code		ndr_err_code;
	struct mapistore_notification	r;

	mem_ctx = talloc_named(NULL, 0, "payload_newmail");
	ck_assert(mem_ctx != NULL);

	/* Check sanity check compliance */
	retval = mapistore_notification_payload_newmail(mem_ctx, NULL, eml, NULL, separator, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_payload_newmail(mem_ctx, backend, NULL, folder, separator, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_payload_newmail(mem_ctx, backend, eml, folder, separator, NULL, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_payload_newmail(mem_ctx, backend, eml, NULL, separator, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_notification_payload_newmail(mem_ctx, backend, eml, folder, separator, &payload.data, NULL);
	ck_assert_int_eq(retval, MAPISTORE_ERR_INVALID_PARAMETER);

	/* Build newmail payload */
	retval = mapistore_notification_payload_newmail(mem_ctx, backend, eml, folder, separator, &payload.data, &payload.length);
	ck_assert_int_eq(retval, MAPISTORE_SUCCESS);

	ndr = ndr_pull_init_blob(&payload, mem_ctx);
	ck_assert(ndr != NULL);

	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);
	ndr_err_code = ndr_pull_mapistore_notification(ndr, NDR_SCALARS, &r);
	ck_assert_int_eq(ndr_err_code, NDR_ERR_SUCCESS);

	/* newmail v1 checks */
	ck_assert_int_eq(r.vnum, 1);
	ck_assert_int_eq(r.v.v1.flags, sub_NewMail);
	ck_assert_str_eq(r.v.v1.u.newmail.backend, backend);
	ck_assert_str_eq(r.v.v1.u.newmail.eml, eml);
	ck_assert_int_eq(r.v.v1.u.newmail.separator, separator);

	talloc_free(mem_ctx);

} END_TEST

Suite *mapistore_notification_suite(void)
{
	Suite	*s;
	TCase	*tc_config;
	TCase	*tc_session;
	TCase	*tc_resolver;
	TCase	*tc_subscription;
	TCase	*tc_deliver;
	TCase	*tc_payload;

	s = suite_create("libmapistore notification");

	gl_async_uuid = GUID_random();
	gl_uuid = GUID_random();

	/* Configuration */
	tc_config = tcase_create("initialization");
	tcase_add_test(tc_config, test_initialization);
	suite_add_tcase(s, tc_config);

	/* Session management */
	tc_session = tcase_create("session management");
	tcase_add_test(tc_session, session_add);
	tcase_add_test(tc_session, session_exist);
	tcase_add_test(tc_session, session_get);
	tcase_add_test(tc_session, session_delete);
	suite_add_tcase(s, tc_session);

	/* Resolver */
	tc_resolver = tcase_create("resolver API");
	tcase_add_test(tc_resolver, resolver_add);
	tcase_add_test(tc_resolver, resolver_exist);
	tcase_add_test(tc_resolver, resolver_get);
	tcase_add_test(tc_resolver, resolver_delete);
	suite_add_tcase(s, tc_resolver);

	/* Subscription */
	tc_subscription = tcase_create("subscription API");
	tcase_add_test(tc_subscription, subscription_add);
	tcase_add_test(tc_subscription, subscription_exist);
	tcase_add_test(tc_subscription, subscription_get);
	tcase_add_test(tc_subscription, subscription_delete_by_handle);
	tcase_add_test(tc_subscription, subscription_delete);
	suite_add_tcase(s, tc_subscription);

	/* Deliver */
	tc_deliver = tcase_create("deliver API");
	tcase_add_test(tc_deliver, deliver_add);
	tcase_add_test(tc_deliver, deliver_exist);
	tcase_add_test(tc_deliver, deliver_get);
	tcase_add_test(tc_deliver, deliver_delete);
	suite_add_tcase(s, tc_deliver);

	/* Payload */
	tc_payload = tcase_create("notification payloads");
	tcase_add_test(tc_payload, payload_newmail);
	suite_add_tcase(s, tc_payload);

	return s;
}
