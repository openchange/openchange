/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Julien Kerihuel 2014

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
#include <libmapistore/mapistore.h>
#include <libmapistore/mapistore_errors.h>
#include <libmapistore/backends/namedprops_mysql.h>

START_TEST(test_parameters) {
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	enum mapistore_error		retval;
	struct namedprops_mysql_params	p;
	bool				bret;

	memset(&p, 0, sizeof (struct namedprops_mysql_params));

	mem_ctx = talloc_named(NULL, 0, "test_parameters");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	/* check sanity check compliance */
	retval = mapistore_namedprops_mysql_parameters(NULL, &p);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, NULL);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	/* check missing username */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_ERR_BACKEND_INIT);

	bret = lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_user", "root");
	ck_assert(bret == true);

	/* check missing database */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(p.user && (retval == MAPISTORE_ERR_BACKEND_INIT));

	bret = lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_db", "openchange");
	ck_assert(bret == true);

	/* check sock or host */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(p.user && p.db && (retval == MAPISTORE_ERR_BACKEND_INIT));

	bret = lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_host", "localhost");
	ck_assert(bret == true);

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_SUCCESS);

	/* test all parametric options */
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_sock", "/tmp/mysql.sock") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_host", "localhost") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_user", "root") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_pass", "openchange") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_port", "3305") == true));
	ck_assert((lpcfg_set_cmdline(lp_ctx, "namedproperties:mysql_db", "db_openchange") == true));

	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &p);
	ck_assert(retval == MAPISTORE_SUCCESS);

	ck_assert_str_eq(p.sock, "/tmp/mysql.sock");
	ck_assert_str_eq(p.host, "localhost");
	ck_assert_str_eq(p.user, "root");
	ck_assert_str_eq(p.pass, "openchange");
	ck_assert_str_eq(p.db, "db_openchange");
	ck_assert_int_eq(p.port, 3305);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST


Suite *mapistore_namedprops_mysql_suite(void)
{
	Suite	*s;
	TCase	*tc_config;

	s = suite_create("libmapistore named properties: MySQL backend");

	/* Core / Configuration */
	tc_config =  tcase_create("Configuration");
	tcase_add_test(tc_config, test_parameters);

	suite_add_tcase(s, tc_config);

	return s;
}
