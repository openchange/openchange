/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Enrique J. Hernández 2015

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
#include <libmapistore/mapistore.h>
#include <libmapistore/mapistore_errors.h>
#include <libmapistore/mapistore_private.h>

/* Global test variables */
static struct mapistore_context	*g_mstore_ctx = NULL;
static const char		*g_test_username = "testuser";

START_TEST (test_sanity_checks) {
	struct GUID			client_guid = GUID_random();
	enum mapistore_error		ret;

	ret = mapistore_replica_mapping_guid_to_replid(g_mstore_ctx, g_test_username, NULL, NULL);
	ck_assert(ret == MAPISTORE_ERR_INVALID_PARAMETER);

	ret = mapistore_replica_mapping_guid_to_replid(g_mstore_ctx, g_test_username, &client_guid, NULL);
	ck_assert(ret == MAPISTORE_ERR_INVALID_PARAMETER);

	ret = mapistore_replica_mapping_replid_to_guid(g_mstore_ctx, g_test_username, 0x23, NULL);
	ck_assert(ret == MAPISTORE_ERR_INVALID_PARAMETER);

} END_TEST

START_TEST (test_guid_to_id) {
	struct GUID			client_guid = GUID_random();
	enum mapistore_error		ret;
	uint16_t			repl_id;

	ret = mapistore_replica_mapping_guid_to_replid(g_mstore_ctx, g_test_username, &client_guid, &repl_id);
	ck_assert(ret == MAPISTORE_SUCCESS);
	ck_assert_int_eq(repl_id, 0x03);

	ret = mapistore_replica_mapping_guid_to_replid(g_mstore_ctx, g_test_username, &client_guid, &repl_id);
	ck_assert(ret == MAPISTORE_SUCCESS);
	ck_assert_int_eq(repl_id, 0x03);

} END_TEST

START_TEST (test_id_to_guid) {
	struct GUID			ret_guid, client_guid = GUID_random();
	enum mapistore_error		ret;
	uint16_t			repl_id;

	ret = mapistore_replica_mapping_replid_to_guid(g_mstore_ctx, g_test_username, 0x23, &ret_guid);
	ck_assert(ret == MAPISTORE_ERR_NOT_FOUND);

	ret = mapistore_replica_mapping_guid_to_replid(g_mstore_ctx, g_test_username, &client_guid, &repl_id);
	ck_assert(ret == MAPISTORE_SUCCESS);

	ret = mapistore_replica_mapping_replid_to_guid(g_mstore_ctx, g_test_username, repl_id, &ret_guid);
	ck_assert(ret == MAPISTORE_SUCCESS);
	ck_assert(GUID_equal(&client_guid, &ret_guid));
	ck_assert(repl_id > 0x01);

} END_TEST

static void tdb_setup(void)
{
	enum mapistore_error	ret;

	ret = mapistore_set_mapping_path("/tmp/");
	ck_assert(ret == MAPISTORE_SUCCESS);

	g_mstore_ctx = talloc_zero(NULL, struct mapistore_context);
	ck_assert(g_mstore_ctx != NULL);
}

static void tdb_teardown(void)
{
	char *repl_mapping_file = NULL;

	repl_mapping_file = talloc_asprintf(g_mstore_ctx, "%s%s/" MAPISTORE_DB_REPLICA_MAPPING,
					    mapistore_get_mapping_path(), g_test_username);
	unlink(repl_mapping_file);
	talloc_free(g_mstore_ctx);
}

Suite *mapistore_replica_mapping_suite(void)
{
	Suite	*s;
	TCase	*tc_interface;

	s = suite_create("libmapistore replica mapping");

	tc_interface = tcase_create("interface");
	tcase_add_checked_fixture(tc_interface, tdb_setup, tdb_teardown);
	tcase_add_test(tc_interface, test_sanity_checks);
	tcase_add_test(tc_interface, test_guid_to_id);
	tcase_add_test(tc_interface, test_id_to_guid);

	suite_add_tcase(s, tc_interface);

	return s;
}
