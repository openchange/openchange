/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Enrique J. Hernández 2016

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testsuite.h"
#include "testsuite_common.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "libmapi/libmapi.h"

static TALLOC_CTX *g_mem_ctx;


// v Unit test ----------------------------------------------------------------

START_TEST (test_search_and_set_logon) {
	const char **username;
	const char *USER_1 = "Penny", *USER_2 = "Wise";
	enum MAPISTATUS retval;
	struct mapi_logon_context *logon_ctx;

	username = talloc_zero(g_mem_ctx, const char *);

	logon_ctx = mapi_logon_init(g_mem_ctx);
	ck_assert(logon_ctx != NULL);

	retval = mapi_logon_search(logon_ctx, 0, username);
	ck_assert(retval == MAPI_E_NOT_FOUND);

	retval = mapi_logon_set(logon_ctx, 12, USER_1);
	ck_assert(retval == MAPI_E_SUCCESS);

	retval = mapi_logon_search(logon_ctx, 12, username);
	ck_assert(retval == MAPI_E_SUCCESS);
	ck_assert(strcmp(*username, USER_1) == 0);

	retval = mapi_logon_search(logon_ctx, 0, username);
	ck_assert(retval == MAPI_E_NOT_FOUND);

	retval = mapi_logon_set(logon_ctx, 13, USER_2);
	ck_assert(retval == MAPI_E_SUCCESS);

	retval = mapi_logon_search(logon_ctx, 13, username);
	ck_assert(retval == MAPI_E_SUCCESS);
	ck_assert(strcmp(*username, USER_2) == 0);

	retval = mapi_logon_set(logon_ctx, 12, USER_2);
	ck_assert(retval == MAPI_E_SUCCESS);

	retval = mapi_logon_search(logon_ctx, 12, username);
	ck_assert(retval == MAPI_E_SUCCESS);
	ck_assert(strcmp(*username, USER_2) == 0);

	retval = mapi_logon_release(logon_ctx);
	ck_assert(retval == MAPI_E_SUCCESS);
} END_TEST


// ^ Unit test ----------------------------------------------------------------

// v Suite definition ---------------------------------------------------------

static void logon_table_setup(void)
{
	g_mem_ctx = talloc_new(talloc_autofree_context());
}

static void logon_table_teardown(void)
{
	talloc_free(g_mem_ctx);
}

Suite *mapiproxy_logon_table_suite(void)
{
	Suite *s = suite_create("mapi logon");

	TCase *tc = tcase_create("mapi logon interface");
	tcase_add_unchecked_fixture(tc, logon_table_setup, logon_table_teardown);

	tcase_add_test(tc, test_search_and_set_logon);

	suite_add_tcase(s, tc);
	return s;
}
