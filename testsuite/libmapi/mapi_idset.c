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

START_TEST (test_IDSET_parse_invalid) {
	DATA_BLOB		bin;
	int			i;
	struct idset		*res;
	/* Invalid command */
	const uint8_t		case_0[] =
		{0x1, 0x0, 0xBA, 0x0, 0x0, 0x0};
	/* Too many PUSH commands */
	const uint8_t		case_1[] =
		{0x1, 0x0, 0x5, 0x0, 0x0, 0x0, 0x1, 0x4,
		 0x2, 0x19, 0x1a, 0x1, 0xaa, 0x52, 0x19, 0x1a, 0x50, 0x0};
	const uint8_t		case_2[] =
		{0x1, 0x0, 0x1, 0xa, 0x1, 0xb, 0x1, 0xc,
		 0x1, 0xd, 0x1, 0xe, 0x2, 0xf, 0xe, 0x1,
		 0xa2, 0x1, 0xa1, 0x52, 0x19, 0x1a, 0x50, 0x0};
	const uint8_t		 *cases[] = {case_0, case_1, case_2};
	const size_t		 cases_size[] = { sizeof(case_0)/sizeof(uint8_t),
						  sizeof(case_1)/sizeof(uint8_t),
						  sizeof(case_2)/sizeof(uint8_t) };
	const size_t		 CASES_NUM = sizeof(cases)/sizeof(uint8_t*);

	for (i = 0; i < CASES_NUM; i++) {
		bin.length = cases_size[i];
		bin.data = (uint8_t *) cases[i];
		res = IDSET_parse(mem_ctx, bin, true);
		ck_assert(res == NULL);
	}
} END_TEST

START_TEST (test_IDSET_parse_bitmask_cmd) {
	/* Test GLOBSET encoded using Bitmask command */
	DATA_BLOB		bin;
	struct idset		*res;
	const uint8_t		case_0[] =
		{0x5b, 0x74, 0xc8, 0x0f, 0x23, 0xe2, 0x1f, 0x4f, 0x82, 0x34, 0x40, 0xf0, 0xbe, 0xc4, 0x6e, 0x93,
		 0x4, 0x0, 0x0, 0x0, 0x6, 0x52, 0x4d, 0x5e, 0x89, 0xe6,
		 0x1, 0x89, 0x42, 0xe8, 0x2, 0x50, 0x50, 0x0};
	/* Example from [MS-OXCFXICS] Section 3.1.5.4.3.1.3 */
	const uint8_t		case_1[] =
		{0x1, 0x0,
		 0x5, 0x0, 0x0, 0x0, 0x6, 0x89,
		 0x42, 0x1, 0xeb, 0x50, 0x0};
	const size_t		cases_size[] = { sizeof(case_0)/sizeof(uint8_t),
						 sizeof(case_1)/sizeof(uint8_t) };

	bin.length = cases_size[0];
	bin.data = (uint8_t *)case_0;
	res = IDSET_parse(mem_ctx, bin, false);
	ck_assert(res != NULL);
	ck_assert_int_eq(res->idbased, false);
	ck_assert_int_eq(res->single, false);
	ck_assert_int_eq(res->range_count, 3);
	ck_assert_int_eq(res->ranges->low, 0x5e4d06000000);
	ck_assert_int_eq(res->ranges->high, 0xe68906000000);
	ck_assert_int_eq(res->ranges->next->low, 0xe88906000000);
	ck_assert_int_eq(res->ranges->next->high, res->ranges->next->low);
	ck_assert_int_eq(res->ranges->next->next->low, 0xea8906000000);
	ck_assert_int_eq(res->ranges->next->next->high, res->ranges->next->next->low);

	bin.length = cases_size[1];
	bin.data = (uint8_t *)case_1;
	res = IDSET_parse(mem_ctx, bin, true);
	ck_assert(res != NULL);
	ck_assert_int_eq(res->idbased, true);
	ck_assert_int_eq(res->single, false);
	ck_assert_int_eq(res->range_count, 3);
	ck_assert_int_eq(res->ranges->low, 0x018906000000);
	ck_assert_int_eq(res->ranges->high, 0x038906000000);
	ck_assert_int_eq(res->ranges->next->low, 0x058906000000);
	ck_assert_int_eq(res->ranges->next->high, res->ranges->next->low);
	ck_assert_int_eq(res->ranges->next->next->low, 0x078906000000);
	ck_assert_int_eq(res->ranges->next->next->high, 0x098906000000);
} END_TEST

START_TEST (test_RAWIDSET_push_eid) {
	struct rawidset	    *idbased_rawidset;
	struct rawidset	    *guidbased_rawidset;
        const uint64_t    id_1 = 0x1804010000000001;
        const uint64_t    id_2 = 0x1804010000000002;

	idbased_rawidset = RAWIDSET_make(mem_ctx, true, false);
	ck_assert(idbased_rawidset != NULL);

	guidbased_rawidset = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(guidbased_rawidset != NULL);

	/* Push ID */
	RAWIDSET_push_eid(idbased_rawidset, id_1);
	ck_assert_int_eq(idbased_rawidset->count, 1);
	ck_assert_int_eq(idbased_rawidset->globcnts[0], id_1 >> 16);
	ck_assert_int_eq(idbased_rawidset->repl.id, id_1 & 0xffff);

	/* Push ID with different replica ID */
	RAWIDSET_push_eid(idbased_rawidset, id_2);
	ck_assert(idbased_rawidset->next != NULL);
	ck_assert_int_eq(idbased_rawidset->next->count, 1);
	ck_assert_int_eq(idbased_rawidset->next->globcnts[0], id_2 >> 16);
	ck_assert_int_eq(idbased_rawidset->next->repl.id, id_2 & 0xffff);

	/* Push ID into GUID-based RAWIDSET */
	RAWIDSET_push_eid(guidbased_rawidset, id_1);
	ck_assert_int_eq(guidbased_rawidset->count, 0);

} END_TEST

START_TEST (test_RAWIDSET_push_guid_glob) {
	struct rawidset	    *idbased_rawidset;
	struct rawidset	    *guidbased_rawidset;
	struct GUID	    guid_1 = GUID_random();
	struct GUID	    guid_2 = GUID_random();
	const uint64_t	    id = 0xbc1c02000000;

	guidbased_rawidset = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(guidbased_rawidset != NULL);

	idbased_rawidset = RAWIDSET_make(mem_ctx, true, false);
	ck_assert(idbased_rawidset != NULL);

	/* Push GUID */
	RAWIDSET_push_guid_glob(guidbased_rawidset, &guid_1, id);
	ck_assert_int_eq(guidbased_rawidset->count, 1);
	ck_assert_int_eq(guidbased_rawidset->globcnts[0], id);
	ck_assert(!GUID_compare(&guidbased_rawidset->repl.guid, &guid_1));

	/* Push GUID with a different replica GUID */
	RAWIDSET_push_guid_glob(guidbased_rawidset, &guid_2, id);
	ck_assert_int_eq(guidbased_rawidset->next->count, 1);
	ck_assert_int_eq(guidbased_rawidset->next->globcnts[0], id);
	ck_assert(!GUID_compare(&guidbased_rawidset->next->repl.guid, &guid_2));

	/* Push GUIDID into ID-based RAWIDSET */
	RAWIDSET_push_guid_glob(idbased_rawidset, &guid_1, id);
	ck_assert_int_eq(idbased_rawidset->count, 0);

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

START_TEST (test_IDSET_merge_idsets_consecutive) {
	const struct GUID server_guid = GUID_random();
	struct idset	  *new_idset, *old_idset, *merged_idset;
	int		  i;
	struct rawidset	  *eid_set;
	const uint64_t	  new_id = 0x632304000000;
	const uint64_t	  ids[] = {0x592304000000,
				   0x5a2304000000,
				   0x5b2304000000,
				   0x602304000000,
				   0x612304000000,
				   0x622304000000};
	size_t		  ids_size = sizeof(ids)/sizeof(uint64_t);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	RAWIDSET_push_guid_glob(eid_set, &server_guid, new_id);
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset != NULL);
	ck_assert_int_eq(new_idset->range_count, 1);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	for (i = 0; i < ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, ids[i]);
	}
	old_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(old_idset != NULL);
	ck_assert_int_eq(old_idset->range_count, 2);

	/* Merge it by putting the GLOBCNT from new into last range from old idset */
	merged_idset = IDSET_merge_idsets(mem_ctx, old_idset, new_idset);
	ck_assert(merged_idset != NULL);
	ck_assert_int_eq(merged_idset->range_count, 2);
	ck_assert_int_eq(merged_idset->ranges->low, ids[0]);
	ck_assert_int_eq(merged_idset->ranges->high, 0x5b2304000000);
	ck_assert_int_eq(merged_idset->ranges->next->low, 0x602304000000);
	ck_assert_int_eq(merged_idset->ranges->next->high, new_id);

} END_TEST

START_TEST (test_IDSET_merge_idsets_new_within_old) {
	const struct GUID server_guid = GUID_random();
	struct idset	  *new_idset, *old_idset, *merged_idset;
	int		  i;
	struct rawidset	  *eid_set;
	const uint64_t	  new_ids[] = {0x5a2304000000,
				       0x5b2304000000};
	size_t		  new_ids_size = sizeof(new_ids)/sizeof(uint64_t);
	const uint64_t	  old_ids[] = {0x582304000000,
				       0x592304000000,
				       0x5a2304000000,
				       0x5b2304000000,
				       0x602304000000,
				       0x612304000000,
				       0x622304000000};
	size_t		  old_ids_size = sizeof(old_ids)/sizeof(uint64_t);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	for (i = 0; i < new_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, new_ids[i]);
	}
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset != NULL);
	ck_assert_int_eq(new_idset->range_count, 1);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	for (i = 0; i < old_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, old_ids[i]);
	}
	old_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(old_idset != NULL);
	ck_assert_int_eq(old_idset->range_count, 2);

	/* Merge it by removing the range from new as it is already in old idset */
	merged_idset = IDSET_merge_idsets(mem_ctx, old_idset, new_idset);
	ck_assert(merged_idset != NULL);
	ck_assert_int_eq(merged_idset->range_count, 2);
	ck_assert_int_eq(merged_idset->ranges->low, old_ids[0]);
	ck_assert_int_eq(merged_idset->ranges->high, 0x5b2304000000);

} END_TEST

START_TEST (test_IDSET_merge_idsets_new_intersects_old) {
	const struct GUID server_guid = GUID_random();
	struct idset	  *new_idset, *old_idset, *merged_idset;
	int		  i;
	struct rawidset	  *eid_set;
	const uint64_t	  new_ids[] = {0x5a2304000000,
				       0x5b2304000000,
				       0x5c2304000000,
				       0x5d2304000000};
	size_t		  new_ids_size = sizeof(new_ids)/sizeof(uint64_t);
	const uint64_t	  old_ids[] = {0x582304000000,
				       0x592304000000,
				       0x5a2304000000,
				       0x5b2304000000,
				       0x602304000000,
				       0x612304000000,
				       0x622304000000};
	size_t		  old_ids_size = sizeof(old_ids)/sizeof(uint64_t);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	for (i = 0; i < new_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, new_ids[i]);
	}
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset != NULL);
	ck_assert_int_eq(new_idset->range_count, 1);

	eid_set = RAWIDSET_make(mem_ctx, false, false);
	ck_assert(eid_set != NULL);
	for (i = 0; i < old_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, old_ids[i]);
	}
	old_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(old_idset != NULL);
	ck_assert_int_eq(old_idset->range_count, 2);

	/* Merge it by setting as first range up level from new and
	 * keep old level from old idset */
	merged_idset = IDSET_merge_idsets(mem_ctx, old_idset, new_idset);
	ck_assert(merged_idset != NULL);
	ck_assert_int_eq(merged_idset->range_count, 2);
	ck_assert_int_eq(merged_idset->ranges->low, old_ids[0]);
	ck_assert_int_eq(merged_idset->ranges->high, new_ids[new_ids_size-1]);

} END_TEST

START_TEST (test_IDSET_merge_idsets_single) {
	const struct GUID server_guid = GUID_random();
	struct idset	  *new_idset, *old_idset, *merged_idset;
	int		  i;
	struct rawidset	  *eid_set;
	const uint64_t	  new_upper_ids[] = {0x702304000000,
					     0x722304000000};
	const uint64_t	  new_lower_ids[] = {0x502304000000,
					     0x532304000000};
	size_t		  new_ids_size = sizeof(new_upper_ids)/sizeof(uint64_t);
	const uint64_t	  old_ids[] = {0x582304000000,
				       0x592304000000,
				       0x612304000000,
				       0x622304000000};
	size_t		  old_ids_size = sizeof(old_ids)/sizeof(uint64_t);

	eid_set = RAWIDSET_make(mem_ctx, false, true);
	ck_assert(eid_set != NULL);
	for (i = 0; i < new_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, new_upper_ids[i]);
	}
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset != NULL);
	ck_assert_int_eq(new_idset->range_count, 1);

	eid_set = RAWIDSET_make(mem_ctx, false, true);
	ck_assert(eid_set != NULL);
	for (i = 0; i < old_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, old_ids[i]);
	}
	old_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(old_idset != NULL);
	ck_assert_int_eq(old_idset->range_count, 1);

	/* Merge it to have a single range increasing upper bound */
	merged_idset = IDSET_merge_idsets(mem_ctx, old_idset, new_idset);
	ck_assert(merged_idset != NULL);
	ck_assert_int_eq(merged_idset->range_count, 1);
	ck_assert_int_eq(merged_idset->ranges->low, old_ids[0]);
	ck_assert_int_eq(merged_idset->ranges->high, new_upper_ids[new_ids_size-1]);

	/* Second case */
	eid_set = RAWIDSET_make(mem_ctx, false, true);
	ck_assert(eid_set != NULL);
	for (i = 0; i < new_ids_size; i++) {
		RAWIDSET_push_guid_glob(eid_set, &server_guid, new_lower_ids[i]);
	}
	new_idset = RAWIDSET_convert_to_idset(mem_ctx, eid_set);
	ck_assert(new_idset != NULL);
	ck_assert_int_eq(new_idset->range_count, 1);

	old_idset = merged_idset;
	merged_idset = IDSET_merge_idsets(mem_ctx, old_idset, new_idset);
	ck_assert(merged_idset != NULL);
	ck_assert_int_eq(merged_idset->range_count, 1);
	ck_assert_int_eq(merged_idset->ranges->low, new_lower_ids[0]);
	ck_assert_int_eq(merged_idset->ranges->high, new_upper_ids[new_ids_size-1]);

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
	tcase_add_test(tc, test_IDSET_parse_invalid);
	tcase_add_test(tc, test_IDSET_parse_bitmask_cmd);
	suite_add_tcase(s, tc);

	tc = tcase_create("RAWIDSET_push");
	tcase_add_checked_fixture(tc, tc_mapi_idset_setup, tc_mapi_idset_teardown);
	tcase_add_test(tc, test_RAWIDSET_push_eid);
	tcase_add_test(tc, test_RAWIDSET_push_guid_glob);
	suite_add_tcase(s,tc);

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

	tc = tcase_create("IDSET_merge_idsets");
	tcase_add_checked_fixture(tc, tc_mapi_idset_setup, tc_mapi_idset_teardown);
	tcase_add_test(tc, test_IDSET_merge_idsets_consecutive);
	tcase_add_test(tc, test_IDSET_merge_idsets_new_within_old);
	tcase_add_test(tc, test_IDSET_merge_idsets_new_intersects_old);
	tcase_add_test(tc, test_IDSET_merge_idsets_single);
	suite_add_tcase(s, tc);

	return s;
}
