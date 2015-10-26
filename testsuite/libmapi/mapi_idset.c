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

START_TEST (test_IDSET_remove_rawidset) {
	const uint64_t		ids[] = {0x1d0401000000,
					0x1e0401000000,
					0x1f0401000000,
					0x200401000000,
					0x210401000000,
					0x220401000000,
					0x230401000000,
					0x240401000000};
	int			i;
	size_t			ids_size = sizeof(ids)/sizeof(uint64_t);
	struct idset		*idset_in;
	uint16_t		repl_id = 0x0001;
	struct globset_range	*range;
	struct rawidset		*rawidset_in, *rawidset_rm_0, *rawidset_rm_1, *rawidset_rm_2;

	/* Generate idsets */
	rawidset_in = RAWIDSET_make(mem_ctx, true, false);
	ck_assert(rawidset_in != NULL);
	for (i = 0; i < ids_size; i++) {
		RAWIDSET_push_eid(rawidset_in, (ids[i] << 16) | repl_id);
	}

	idset_in = RAWIDSET_convert_to_idset(mem_ctx, rawidset_in);
	ck_assert(idset_in != NULL);

	rawidset_rm_0 = RAWIDSET_make(mem_ctx, true, true);
	ck_assert(rawidset_rm_0 != NULL);
	RAWIDSET_push_eid(rawidset_rm_0, (ids[0] << 16) | repl_id);

	rawidset_rm_1 = RAWIDSET_make(mem_ctx, true, true);
	ck_assert(rawidset_rm_1 != NULL);
	RAWIDSET_push_eid(rawidset_rm_1, (ids[ids_size - 1] << 16) | repl_id);

	rawidset_rm_2 = RAWIDSET_make(mem_ctx, true, true);
	ck_assert(rawidset_rm_2 != NULL);
	RAWIDSET_push_eid(rawidset_rm_2, (ids[3] << 16) | repl_id);
	RAWIDSET_push_eid(rawidset_rm_2, (ids[4] << 16) | repl_id);

	/* Case: Remove first element */
	IDSET_remove_rawidset(idset_in, rawidset_rm_0);

	range = idset_in->ranges;
	ck_assert_int_eq(idset_in->range_count, 1);
	ck_assert_int_eq(range->low, ids[1]);
	ck_assert_int_eq(range->high, ids[ids_size - 1]);

	/* Case: Remove last element */
	IDSET_remove_rawidset(idset_in, rawidset_rm_1);

	ck_assert_int_eq(idset_in->range_count, 1);
	ck_assert_int_eq(range->low, ids[1]);
	ck_assert_int_eq(range->high, ids[ids_size - 2]);

	/* Case: Remove middle elements (3rd & 4th in the original set) */
	IDSET_remove_rawidset(idset_in, rawidset_rm_2);

	ck_assert_int_eq(idset_in->range_count, 2);
	ck_assert_int_eq(range->low, ids[1]);
	ck_assert_int_eq(range->high, ids[2]);
	range = range->next;
	ck_assert_int_eq(range->low, ids[5]);
	ck_assert_int_eq(range->high, ids[ids_size - 2]);

} END_TEST

START_TEST (test_RAWIDSET_convert_to_idset) {
	const uint64_t	       ids[] = {0xbc1c02000000,
					0x130800000000};
	const uint64_t	       consq_ids[] = {0xbd1c02000000,
					      0xbe1c02000000};
	struct GUID	       server_guid = GUID_random();
	struct idset	       *new_idset;
	int		       i;
	struct rawidset	       *eid_set;
	size_t		       ids_size = sizeof(ids)/sizeof(uint64_t);
	size_t		       consq_ids_size = sizeof(consq_ids)/sizeof(uint64_t);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	for (i = 0; i < ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, ids[i]);
	}

	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset->idbased == false);
	ck_assert(new_idset->single == false);
	ck_assert(new_idset->next == NULL);
	ck_assert_int_eq(new_idset->range_count, ids_size);

	talloc_free(new_idset);

	/* Unify ranges to return a single one */
	eid_set->single = true;
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset->idbased == false);
	ck_assert(new_idset->single == true);
	ck_assert(new_idset->next == NULL);
	ck_assert_int_eq(new_idset->range_count, 1);

	talloc_free(new_idset);

	/* Testing last consequent */
	eid_set->single = false;
	for (i = 0; i < consq_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, consq_ids[i]);
	}
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset->idbased == false);
	ck_assert(new_idset->single == false);
	ck_assert(new_idset->next == NULL);
	ck_assert_int_eq(new_idset->range_count, 2);
	ck_assert_int_ne(new_idset->ranges->next->low, new_idset->ranges->next->high);
	ck_assert_int_eq(new_idset->ranges->next->high, consq_ids[consq_ids_size - 1]);

} END_TEST

START_TEST (test_RAWIDSET_convert_to_idset_one_id) {
	const struct GUID server_guid = GUID_random();
	struct idset	  *new_idset;
	struct rawidset	  *eid_set;
	const uint64_t	  id = 0x121201000000;

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	RAWIDSET_push_guid_glob(eid_set, &server_guid, id);

	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset->idbased == false);
	ck_assert(new_idset->single == false);
	ck_assert(new_idset->next == NULL);
	ck_assert_int_eq(new_idset->range_count, 1);
	ck_assert_int_eq(new_idset->ranges->low, id);
	ck_assert_int_eq(new_idset->ranges->high, id);

	talloc_free(new_idset);

	/* Now with as a single rawidset */
	eid_set->single = true;
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset->idbased == false);
	ck_assert(new_idset->single == true);
	ck_assert(new_idset->next == NULL);
	ck_assert_int_eq(new_idset->range_count, 1);
	ck_assert_int_eq(new_idset->ranges->low, id);
	ck_assert_int_eq(new_idset->ranges->high, id);

} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void tc_mapi_idset_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void tc_mapi_idset_teardown(void)
{
	talloc_free(mem_ctx);
}

Suite *libmapi_idset_suite(void)
{
	Suite *s = suite_create("libmapi idset");
	TCase *tc;

	tc = tcase_create("IDSET_parse");
	tcase_add_checked_fixture(tc, tc_mapi_idset_setup, tc_mapi_idset_teardown);
	tcase_add_test(tc, test_IDSET_parse);
	suite_add_tcase(s, tc);

	tc = tcase_create("IDSET_includes_guid_glob");
	tcase_add_checked_fixture(tc, tc_mapi_idset_setup, tc_mapi_idset_teardown);
	tcase_add_test(tc, test_IDSET_includes_guid_glob);
	suite_add_tcase(s, tc);

	tc = tcase_create("IDSET_remove_rawidset");
	tcase_add_checked_fixture(tc, tc_mapi_idset_setup, tc_mapi_idset_teardown);
	tcase_add_test(tc, test_IDSET_remove_rawidset);
	suite_add_tcase(s, tc);

	tc = tcase_create("RAWIDSET_convert_to_idset");
	tcase_add_checked_fixture(tc, tc_mapi_idset_setup, tc_mapi_idset_teardown);
	tcase_add_test(tc, test_RAWIDSET_convert_to_idset);
	tcase_add_test(tc, test_RAWIDSET_convert_to_idset_one_id);
	suite_add_tcase(s, tc);

	return s;
}
