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
#include <libmapistore/backends/namedprops_backend.h>

START_TEST (test_init) {
	TALLOC_CTX			*mem_ctx;
	struct loadparm_context		*lp_ctx;
	enum mapistore_error		retval;
	int				ret;
	struct namedprops_context	*nprops_ctx;

	mem_ctx = talloc_named(NULL, 0, "test_init");
	ck_assert(mem_ctx != NULL);

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);
	lpcfg_load_default(lp_ctx);


	/* check sanity checks compliance */
	retval = mapistore_namedprops_init(NULL, lp_ctx, &nprops_ctx);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_namedprops_init(mem_ctx, NULL, &nprops_ctx);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	retval = mapistore_namedprops_init(mem_ctx, lp_ctx, NULL);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	/* check invalid backend */
	ret = lpcfg_set_cmdline(lp_ctx, "mapistore:namedproperties", "invalid");
	ck_assert(ret);
	
	retval = mapistore_namedprops_init(mem_ctx, lp_ctx, &nprops_ctx);
	ck_assert(retval == MAPISTORE_ERR_INVALID_PARAMETER);

	talloc_free(lp_ctx);
	talloc_free(mem_ctx);

} END_TEST

Suite *mapistore_namedprops_suite(void)
{
	Suite	*s;
	TCase	*tc_intf;

	s = suite_create("libmapistore named properties: interface");

	tc_intf = tcase_create("Interface");
	tcase_add_test(tc_intf, test_init);

	suite_add_tcase(s, tc_intf);

	return s;
}
