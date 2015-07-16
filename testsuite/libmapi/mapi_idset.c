/*
   IDSET Unit Testing

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
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <gen_ndr/ndr_exchange.h>

/* Global test variables */
static TALLOC_CTX *mem_ctx;

// v Unit test ----------------------------------------------------------------

START_TEST (test_IDSET_parse) {
	DATA_BLOB		bin;
	int			i;
	struct idset		*res;
	/* REPLID-based IDSET case (MetaTagIdsetDeleted) */
	const uint8_t		case_0[] =
		{0x1, 0x0, 0x6, 0x0, 0x0, 0x0, 0x1, 0x4, 0x2e, 0x05, 0x0, 0x0, 0x0, 0x1,
		 0x4, 0x52, 0x35, 0x37, 0x50, 0x0};
        /* A REPLID-based IDSET whose length is smaller than REPLGUID-based IDSET minimum length */
	const uint8_t		case_1[] =
		{0x1, 0x0, 0x5, 0x0, 0x0, 0x0, 0x1, 0x4, 0x52, 0x19, 0x1A, 0x50, 0x0};
	/* REPLGUID-based IDSET case (MetaTagCnsetSeen) */
	const uint8_t		 case_2[] =
		{0x9b, 0xb9, 0xff, 0xb5, 0xf, 0x44, 0x77, 0x4f, 0xb4, 0x88, 0xc5, 0xc6, 0x70,
		 0x2e, 0xe3, 0xa8, 0x4, 0x0, 0x0, 0x0, 0x0, 0x52, 0x0, 0xa5, 0x1, 0x51, 0x50, 0x0};
	const uint8_t		 *cases[] = {case_0, case_1, case_2};
	const size_t		 cases_size[] = { sizeof(case_0)/sizeof(uint8_t),
						  sizeof(case_1)/sizeof(uint8_t),
						  sizeof(case_2)/sizeof(uint8_t) };
	const bool		 id_based[] = {true, true, false};
	const uint32_t		 range_count[] = {2, 1, 1};
	const size_t		 CASES_NUM = sizeof(cases)/sizeof(uint8_t*);

	for (i = 0; i < CASES_NUM; i++) {
		bin.length = cases_size[i];
		bin.data = (uint8_t *) cases[i];
		res = IDSET_parse(mem_ctx, bin, id_based[i]);
		ck_assert(res != NULL);
		ck_assert_int_eq(res->idbased, id_based[i]);
		ck_assert_int_eq(res->single, false);
		ck_assert_int_eq(res->range_count, range_count[i]);
	}

} END_TEST

START_TEST (test_IDSET_includes_guid_glob) {
        struct GUID       server_guid = GUID_random();
        struct GUID       random_guid = GUID_random();
        int               i;
        struct idset      *idset_in;
        struct rawidset   *rawidset_in;
        const uint16_t    repl_id = 0x0001;
        const uint64_t    ids[] = {0x180401000000,
                                   0x190401000000,
                                   0x1a0401000000,
                                   0x1b0401000000,
                                   0x500401000000,
                                   0x520401000000,
                                   0x530401000000,
                                   0x710401000000};
        const uint64_t    not_in_id = 0x2a0401000000;
        const size_t      ids_size = sizeof(ids) / sizeof(uint64_t);

        rawidset_in = RAWIDSET_make(mem_ctx, true, false);
        ck_assert(rawidset_in != NULL);
        for (i = 0; i < ids_size; i++) {
                RAWIDSET_push_eid(rawidset_in, (ids[i] << 16) | repl_id);
        }
        ck_assert_int_eq(rawidset_in->count, ids_size);
        rawidset_in->idbased = false;
        rawidset_in->repl.guid = server_guid;
        idset_in = RAWIDSET_convert_to_idset(mem_ctx, rawidset_in);
        ck_assert(idset_in != NULL);

        /* Case: All inserted elements in the range */
        for (i = 0; i < ids_size; i++) {
                ck_assert(IDSET_includes_guid_glob(idset_in, &server_guid, ids[i]));
        }

        /* Case: a different guid for the same id */
        ck_assert(!IDSET_includes_guid_glob(idset_in, &random_guid, ids[0]));

        /* Case: Not in range and different guid */
        ck_assert(!IDSET_includes_guid_glob(idset_in, &random_guid, not_in_id));

        /* Case: Not in range */
        ck_assert(!IDSET_includes_guid_glob(idset_in, &server_guid, not_in_id));

} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void tc_IDSET_parse_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void tc_IDSET_parse_teardown(void)
{
	talloc_free(mem_ctx);
}

static void tc_IDSET_includes_guid_glob_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void tc_IDSET_includes_guid_glob_teardown(void)
{
	talloc_free(mem_ctx);
}

Suite *libmapi_idset_suite(void)
{
	Suite *s = suite_create("libmapi idset");
	TCase *tc;

	tc = tcase_create("IDSET_parse");
	tcase_add_checked_fixture(tc, tc_IDSET_parse_setup, tc_IDSET_parse_teardown);
	tcase_add_test(tc, test_IDSET_parse);
	suite_add_tcase(s, tc);

	tc = tcase_create("IDSET_includes_guid_glob");
	tcase_add_checked_fixture(tc, tc_IDSET_includes_guid_glob_setup, tc_IDSET_includes_guid_glob_teardown);
	tcase_add_test(tc, test_IDSET_includes_guid_glob);
	suite_add_tcase(s, tc);

	return s;
}
