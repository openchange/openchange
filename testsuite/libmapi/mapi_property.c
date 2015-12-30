/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Kamen Mazdrashki 2014
   Copyright (C) Javier Amor García 2015
   Copyright (C) Enrique J. Hernández 2015

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
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include <gen_ndr/ndr_exchange.h>

/* Global test variables */
static TALLOC_CTX *mem_ctx;
struct SRow *test_srow;


static int SPropValue_cmp(const struct SPropValue *left, const struct SPropValue *right)
{
	uint16_t prop_type;

	ck_assert(left != NULL);
	ck_assert(right != NULL);
	ck_assert(left->ulPropTag == right->ulPropTag);

	prop_type = (left->ulPropTag & 0xFFFF);
	switch (prop_type) {
	case PT_NULL:
	case PT_OBJECT:
		return left->value.null - right->value.null;
	case PT_I2:
		return left->value.i - right->value.i;
	case PT_LONG:
		return left->value.l - right->value.l;
	case PT_FLOAT: /* no PIDL for float, so handle it as double */
	case PT_DOUBLE:
		return left->value.dbl - right->value.dbl;
	case PT_ERROR:
		return left->value.err - right->value.err;
	case PT_BOOLEAN:
		return left->value.b - right->value.b;
	case PT_I8:
		return left->value.d - right->value.d;
	case PT_STRING8:
		return strcmp((const char *) left->value.lpszA, (const char *) right->value.lpszA);
	case PT_UNICODE:
		return strcmp((const char *) left->value.lpszW, (const char *) right->value.lpszW);
	case PT_SVREID:
	case PT_BINARY:
	{
		int res = left->value.bin.cb - right->value.bin.cb;
		if (res != 0) {
			return res;
		}
		return memcmp(left->value.bin.lpb, right->value.bin.lpb, left->value.bin.cb);
	}
	case PT_CLSID:
		return memcmp(left->value.lpguid, right->value.lpguid, sizeof(*left->value.lpguid));
	case PT_MV_LONG:
	{
		int res = left->value.MVl.cValues - right->value.MVl.cValues;
		if (res != 0) {
			return res;
		}
		return memcmp(left->value.MVl.lpl, right->value.MVl.lpl, left->value.MVl.cValues * sizeof(left->value.MVl.lpl[0]));
	}
	case PT_MV_STRING8:
	{
		int i;
		int res = left->value.MVszA.cValues - right->value.MVszA.cValues;
		if (res != 0) {
			return res;
		}
		for (i = 0; i < left->value.MVszA.cValues; i++) {
			res = strcmp((const char *) left->value.MVszA.lppszA[i], (const char *) right->value.MVszA.lppszA[i]);
			if (res != 0) {
				return res;
			}
		}
		return 0;
	}
	case PT_MV_UNICODE:
	{
		int i;
		int res = left->value.MVszW.cValues - right->value.MVszW.cValues;
		if (res != 0) {
			return res;
		}
		for (i = 0; i < left->value.MVszW.cValues; i++) {
			res = strcmp(left->value.MVszW.lppszW[i], right->value.MVszW.lppszW[i]);
			if (res != 0) {
				return res;
			}
		}
		return 0;
	}
	case PT_MV_BINARY:
	{
		int i;
		int res = left->value.MVbin.cValues - right->value.MVbin.cValues;
		if (res != 0) {
			return res;
		}
		for (i = 0; i < left->value.MVbin.cValues; i++) {
			res = left->value.MVbin.lpbin[i].cb - right->value.MVbin.lpbin[i].cb;
			if (res != 0) {
				return res;
			}
			res = memcmp(left->value.MVbin.lpbin[i].lpb, right->value.MVbin.lpbin[i].lpb, left->value.MVbin.lpbin[i].cb);
			if (res != 0) {
				return res;
			}
		}
		return 0;
	}
	default:
		break;
	}

	return memcmp(left, right, sizeof(*left));
}

// v Unit test ----------------------------------------------------------------

START_TEST (test_mapi_copy_spropvalues) {
	int i;
	struct SRow *dest_row;

	dest_row = talloc_zero(mem_ctx, struct SRow);
	dest_row->lpProps = talloc_array(dest_row, struct SPropValue, test_srow->cValues);
	mapi_copy_spropvalues(dest_row, test_srow->lpProps, dest_row->lpProps, test_srow->cValues);

	for (i = 0; i < test_srow->cValues; i++) {
		struct SPropValue *left = &test_srow->lpProps[i];
		struct SPropValue *right = &dest_row->lpProps[i];
		ck_assert(SPropValue_cmp(left, right) == 0);
	}

} END_TEST

START_TEST (test_get_RecurrencePattern) {
	struct Binary_r		    bin;
	struct RecurrencePattern    *res;
	/* case 0 comes from an event created with outlook, weekly recurrence and with canceled instances and exceptions */
	const uint8_t case_0[] =
		{0x04, 0x30, 0x04, 0x30, 0x0b, 0x20, 0x01, 0x00, 0x00, 0x00, 0x60, 0x27, 0x00, 0x00, 0x02, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x23, 0x20, 0x00, 0x00, 0x0a, 0x00,
		 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xe0, 0xbc, 0xfb, 0x0c, 0xc0, 0x54,
		 0xfc, 0x0c, 0x01, 0x00, 0x00, 0x00, 0xc0, 0x54, 0xfc, 0x0c, 0xe0, 0xbc, 0xfb, 0x0c, 0xdf, 0x80,
		 0xe9, 0x5a};
	const size_t		    case_size = sizeof(case_0)/sizeof(uint8_t);

	bin.cb = case_size;
	bin.lpb = (uint8_t *) case_0;
	res = get_RecurrencePattern(mem_ctx, &bin);

	ck_assert(res != NULL);
	ck_assert_int_eq(talloc_parent(res), mem_ctx);
	ck_assert_int_eq(res->DeletedInstanceCount, 2);
	ck_assert_int_eq(talloc_parent(res->DeletedInstanceDates), mem_ctx);
	ck_assert_int_eq(res->ModifiedInstanceCount, 1);
	ck_assert_int_eq(talloc_parent(res->ModifiedInstanceDates), mem_ctx);

} END_TEST

START_TEST (test_get_AppointmentRecurrencePattern) {
	int i;
	int nCases = 5;
	struct Binary_r cases[nCases];
	
	/* case 0 comes from a crash report. It is cut just when ReservedBlock1Size should begin. */
	uint8_t case0_lpb[]=  {4, 48, 4, 48, 13, 32, 3, 0, 0, 0, 128, 168, 4, 0, 12, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 35, 32, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 12, 199, 12, 223, 128, 233, 90, 6, 48, 0, 0, 7, 48, 0, 0, 0, 0, 0, 0, 160, 5, 0, 0, 0, 0};
	cases[0].cb = 76;
	cases[0].lpb = &case0_lpb[0];
	
	/* case 1 comes from a event created with outlook, yearly recurrence */
	uint8_t case1_lpb[]=  {4, 48, 4, 48, 13, 32, 3, 0, 0, 0, 96, 174, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 3, 0, 0, 0, 35, 32, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 149, 251, 12, 223, 128, 233, 90, 6, 48, 0, 0, 9, 48, 0, 0, 162, 3, 0, 0, 192, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	cases[1].cb = 84;
	cases[1].lpb = &case1_lpb[0];

	/* case 2 comes from a event created with outlook, weekly recurrence */
	uint8_t case2_lpb[]=  {4, 48, 4, 48, 11, 32, 1, 0, 0, 0, 96, 39, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 35, 32, 0, 0, 10, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 224, 188, 251, 12, 223, 128, 233, 90, 6, 48, 0, 0, 9, 48, 0, 0, 102, 3, 0, 0, 132, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	cases[2].cb = 80;
	cases[2].lpb = &case2_lpb[0];

	/* case 3, same than case 2 but with canceled instances and exceptions */
	uint8_t case3_lpb[]=  {4, 48, 4, 48, 11, 32, 1, 0, 0, 0, 96, 39, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0, 0, 35, 32, 0, 0, 10, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 224, 188, 251, 12, 192, 84, 252, 12, 1, 0, 0, 0, 192, 84, 252, 12, 224, 188, 251, 12, 223, 128, 233, 90, 6, 48, 0, 0, 9, 48, 0, 0, 102, 3, 0, 0, 132, 3, 0, 0, 1, 0, 38, 88, 252, 12, 68, 88, 252, 12, 38, 88, 252, 12, 0, 2, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	cases[3].cb = 118;
	cases[3].lpb = &case3_lpb[0];

        /* case 4. Another crash. As case 0 is cut at the beginning of ReservedBlock1Size */
	uint8_t case4_lpb[]=  {4, 48, 4, 48, 11, 32, 1, 0, 0, 0, 192, 33, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 33, 32, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 96, 94, 188, 12, 32, 173, 188, 12, 6, 48, 0, 0, 6, 48, 0, 0, 56, 4, 0, 0, 146, 4, 0, 0, 0, 0};
	cases[4].cb = 72;
	cases[4].lpb = &case4_lpb[0];

	for(i=0; i < nCases; i++) {
		struct AppointmentRecurrencePattern *res;
		res = get_AppointmentRecurrencePattern(mem_ctx, &cases[i]);
		ck_assert(res != NULL);
		ck_assert_int_eq(talloc_parent(res), mem_ctx);
		if (res->ExceptionCount > 0) {
			ck_assert_int_eq(talloc_parent(res->ExceptionInfo), mem_ctx);
		}
	}
} END_TEST

START_TEST (test_get_PersistDataArray) {
	struct Binary_r		bin;
	int			i;
	struct PersistDataArray *res;
	/* Outlook 2010 standard case */
	const uint8_t		case_0[] =
		{0x1, 0x80, 0x32, 0x0, 0x1, 0x0, 0x2e, 0x0, 0x0, 0x0, 0x0, 0x0, 0x71, 0x83, 0xdc,
		 0x72, 0x83, 0x71, 0x28, 0x45, 0xb9, 0xd1, 0x7, 0x13, 0xca, 0xd2, 0x4b, 0xf4,
		 0x1, 0x0, 0x99, 0xb0, 0x3e, 0xa2, 0xa1, 0x66, 0xf9, 0x48, 0xba, 0x88, 0xdc,
		 0x7a, 0x58, 0x87, 0x1a, 0xd8, 0x0, 0x0, 0x0, 0x1, 0x19, 0xdc, 0x0, 0x0, 0x6,
		 0x80, 0x32, 0x0, 0x1, 0x0, 0x2e, 0x0, 0x0, 0x0, 0x0, 0x0, 0x71, 0x83, 0xdc,
		 0x72, 0x83, 0x71, 0x28, 0x45, 0xb9, 0xd1, 0x7, 0x13, 0xca, 0xd2, 0x4b, 0xf4,
		 0x1, 0x0, 0x99, 0xb0, 0x3e, 0xa2, 0xa1, 0x66, 0xf9, 0x48, 0xba, 0x88, 0xdc,
		 0x7a, 0x58, 0x87, 0x1a, 0xd8, 0x0, 0x0, 0x0, 0x1, 0x19, 0xdd, 0x0, 0x0, 0x7,
		 0x80, 0x32, 0x0, 0x1, 0x0, 0x2e, 0x0, 0x0, 0x0, 0x0, 0x0, 0x71, 0x83, 0xdc,
		 0x72, 0x83, 0x71, 0x28, 0x45, 0xb9, 0xd1, 0x7, 0x13, 0xca, 0xd2, 0x4b, 0xf4,
		 0x1, 0x0, 0x99, 0xb0, 0x3e, 0xa2, 0xa1, 0x66, 0xf9, 0x48, 0xba, 0x88, 0xdc,
		 0x7a, 0x58, 0x87, 0x1a, 0xd8, 0x0, 0x0, 0x0, 0x1, 0x19, 0xde, 0x0, 0x0, 0x8,
		 0x80, 0x32, 0x0, 0x1, 0x0, 0x2e, 0x0, 0x0, 0x0, 0x0, 0x0, 0x71, 0x83, 0xdc,
		 0x72, 0x83, 0x71, 0x28,0x45, 0xb9, 0xd1, 0x7, 0x13, 0xca, 0xd2, 0x4b, 0xf4,
		 0x1, 0x0, 0x99, 0xb0, 0x3e, 0xa2, 0xa1, 0x66, 0xf9, 0x48, 0xba, 0x88, 0xdc,
		 0x7a, 0x58, 0x87, 0x1a, 0xd8, 0x0, 0x0, 0x0, 0x1, 0x19, 0xbc, 0x0, 0x0, 0x0,
		 0x0, 0x0, 0x0};
	/* Empty PersistData */
	const uint8_t		 case_1[] =
		{0x0, 0x0, 0x0, 0x0};
	/* PersistElement Header and Sentinel and no PersistDataSentinel */
	const uint8_t		 case_2[] =
		{0x1, 0x80, 0x3e, 0x0, 0x2, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x2e, 0x0,
		 0x0, 0x0, 0x0, 0x0, 0x71, 0x83, 0xdc, 0x72, 0x83, 0x71, 0x28, 0x45, 0xb9, 0xd1,
		 0x7, 0x13, 0xca, 0xd2, 0x4b, 0xf4, 0x1, 0x0, 0x99, 0xb0, 0x3e, 0xa2, 0xa1, 0x66,
		 0xf9, 0x48, 0xba, 0x88, 0xdc, 0x7a, 0x58, 0x87, 0x1a, 0xd8, 0x0, 0x0, 0x0, 0x1,
		 0x19, 0xdc, 0x0, 0x0,
		 0x0, 0x0, 0x0, 0x0};
	const uint8_t		 *cases[] = {case_0, case_1, case_2};
	const size_t		 cases_size[] = { sizeof(case_0)/sizeof(uint8_t),
						  sizeof(case_1)/sizeof(uint8_t),
						  sizeof(case_2)/sizeof(uint8_t) };
	const uint32_t		 cases_cValues[] = {5, 1, 1};
	const size_t		 CASES_NUM = sizeof(cases)/sizeof(uint8_t*);

	for (i = 0; i < CASES_NUM; i++) {
		bin.cb = cases_size[i];
		bin.lpb = (uint8_t *) cases[i];
		res = get_PersistDataArray(mem_ctx, &bin);
		ck_assert(res != NULL);
		ck_assert_int_eq(res->cValues, cases_cValues[i]);
	}

} END_TEST

START_TEST (test_push_PersistDataArray) {

	struct ndr_push		*ndr;
	enum ndr_err_code	ndr_err_code;
	struct PersistDataArray persist_data_array;
	struct PersistElement	*persist_element;
	const uint8_t		data[4] = { 0x0, 0x1, 0x2, 0x3 };

	persist_data_array.cValues = 1;
	persist_data_array.lpPersistData = talloc_zero_array(mem_ctx, struct PersistData, 1);
	persist_data_array.lpPersistData[0].PersistID = RSF_PID_RSS_SUBSCRIPTION;
	persist_data_array.lpPersistData[0].DataElementsSize = 8;
	persist_data_array.lpPersistData[0].DataElements.cValues = 1;
	persist_data_array.lpPersistData[0].DataElements.lpPersistElement = talloc_zero_array(mem_ctx, struct PersistElement, 1);
	persist_element = persist_data_array.lpPersistData[0].DataElements.lpPersistElement;
	persist_element->ElementID = RSF_ELID_ENTRYID;
	persist_element->ElementDataSize = 4;
	persist_element->ElementData.rsf_elid_entryid.length = 4;
	persist_element->ElementData.rsf_elid_entryid.data = talloc_zero_array(mem_ctx, uint8_t, 4);
	persist_element->ElementData.rsf_elid_entryid.data = (uint8_t *) data;

	ndr = ndr_push_init_ctx(mem_ctx);
	ndr->offset = 0;

	ndr_err_code = ndr_push_PersistDataArray(ndr, NDR_SCALARS|NDR_BUFFERS, &persist_data_array);
	ck_assert(ndr_err_code == NDR_ERR_SUCCESS);
	ck_assert_int_eq(ndr->offset, 12);

} END_TEST

START_TEST (test_get_SizedXidArray) {
	struct Binary_r		bin;
	int			i;
	struct SizedXid         *res;
        uint32_t                count;
	/* Empty SizedXid Array */
	const uint8_t		case_0[] = {};
	/* Predecessor Change List with a single change */
	const uint8_t		case_1[] =
		{0x14, 0xbb, 0x1c, 0x15, 0x82, 0x39, 0x31, 0x93, 0x41, 0xbf, 0x51, 0x52, 0x52, 0xd1, 0x2a,
                 0xe8, 0xaa, 0x0, 0x0, 0x8, 0xd};
	/* Predecessor Change List with two changes */
	const uint8_t		 case_2[] =
		{0x16, 0xa8, 0xc1, 0x14, 0x3, 0x7, 0x8d, 0xc3, 0x47, 0x9d, 0x9e, 0x5e, 0x84, 0xfd,
                 0x4a, 0xad, 0x99, 0x0, 0x0, 0x0, 0x0, 0x0, 0x92, 0x14, 0xbb, 0x1c, 0x15, 0xb2, 0x39,
                 0x31, 0x93, 0x41, 0xbf, 0x51, 0x52, 0x52, 0xd1, 0x2a, 0xe8, 0xaa, 0x0, 0x0, 0x8, 0xc};
	const uint8_t		 *cases[] = {case_0, case_1, case_2};
	const size_t		 cases_size[] = { sizeof(case_0)/sizeof(uint8_t),
						  sizeof(case_1)/sizeof(uint8_t),
						  sizeof(case_2)/sizeof(uint8_t) };
	const uint32_t		 cases_count[] = {0, 1, 2};
	const size_t		 cases_num = sizeof(cases)/sizeof(uint8_t*);

	for (i = 0; i < cases_num; i++) {
		bin.cb = cases_size[i];
		bin.lpb = (uint8_t *) cases[i];
		res = get_SizedXidArray(mem_ctx, &bin, &count);
                if (cases_count[i]) {
                        ck_assert(res != NULL);
                        ck_assert_int_eq(count, cases_count[i]);
                        if (count > 1) {
                                ck_assert(!GUID_equal(&res[0].XID.NameSpaceGuid, &res[1].XID.NameSpaceGuid));
                                ck_assert(res[0].XidSize > res[1].XidSize);
                                ck_assert(res[0].XID.LocalId.length > res[1].XID.LocalId.length);

                        }
                } else {
                        ck_assert(res == NULL);
                }
	}

} END_TEST

START_TEST (test_get_TimeZoneDefinition) {
	struct Binary_r		    bin;
	struct TimeZoneDefinition   *res;
	/* TimeZoneDefinition with 2 TZRules, as in [MS-OXOCAL] 4.1.4 */
	const uint8_t		    case_0[] =
		{0x02, 0x01, 0x30, 0x00, 0x02, 0x00, 0x15, 0x00, 0x50, 0x00, 0x61, 0x00, 0x63, 0x00, 0x69,
		 0x00, 0x66, 0x00, 0x69, 0x00, 0x63, 0x00, 0x20, 0x00, 0x53, 0x00, 0x74, 0x00, 0x61, 0x00,
		 0x6E, 0x00, 0x64, 0x00, 0x61, 0x00, 0x72, 0x00, 0x64, 0x00, 0x20, 0x00, 0x54, 0x00, 0x69,
		 0x00, 0x6D, 0x00, 0x65, 0x00, 0x02, 0x00, 0x02, 0x01, 0x3E, 0x00, 0x00, 0x00, 0xD6, 0x07,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0,
		 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x0A, 0x00,
		 0x00, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
		 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01,
		 0x3E, 0x00, 0x02, 0x00, 0xD7, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0xFF,
		 0xFF, 0xFF, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
		 0x00, 0x00, 0x00, 0x00};
	const size_t		    case_size = sizeof(case_0)/sizeof(uint8_t);

	bin.cb = case_size;
	bin.lpb = (uint8_t *) case_0;
	res = get_TimeZoneDefinition(mem_ctx, &bin);

	ck_assert(res != NULL);
	ck_assert_int_eq(talloc_parent(res), mem_ctx);
	ck_assert_int_eq(res->cRules, 2);
	ck_assert_int_eq(talloc_parent(res->TZRules), mem_ctx);

} END_TEST

// ^ unit tests ---------------------------------------------------------------

// v suite definition ---------------------------------------------------------

static void _make_test_srow(TALLOC_CTX *mem_ctx)
{
	struct SPropValue prop_val;

	ZERO_STRUCT(prop_val);
	/* PT_I8 */
	prop_val.ulPropTag = PR_FID;
	prop_val.value.d = 0x0123456789ABCDEFul;
	SRow_addprop(test_srow, prop_val);
	/* PT_MV_STRING8 */
	prop_val.ulPropTag = PR_EMS_AB_PROXY_ADDRESSES;
	prop_val.value.MVszA.cValues = 2;
	prop_val.value.MVszA.lppszA = talloc_array(test_srow, uint8_t *, prop_val.value.MVszA.cValues);
	prop_val.value.MVszA.lppszA[0] = (uint8_t *) "string 1";
	prop_val.value.MVszA.lppszA[1] = (uint8_t *) "string 2";
	SRow_addprop(test_srow, prop_val);
	/* PT_MV_UNICODE - same layout as PT_MV_STRING8 */
	prop_val.ulPropTag = PR_EMS_AB_PROXY_ADDRESSES_UNICODE;
	SRow_addprop(test_srow, prop_val);
	/* PT_MV_BINARY */
	prop_val.ulPropTag = PR_FREEBUSY_ENTRYIDS;
	prop_val.value.MVbin.cValues = 2;
	prop_val.value.MVbin.lpbin = talloc_array(test_srow, struct Binary_r, prop_val.value.MVbin.cValues);
	prop_val.value.MVbin.lpbin[0].cb = 9;
	prop_val.value.MVbin.lpbin[0].lpb = (uint8_t *)"string 1";
	prop_val.value.MVbin.lpbin[1].cb = 9;
	prop_val.value.MVbin.lpbin[1].lpb = (uint8_t *)"string 2";
	SRow_addprop(test_srow, prop_val);
}

static void tc_mapi_copy_spropvalues_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
	test_srow = talloc_zero(mem_ctx, struct SRow);

	_make_test_srow(mem_ctx);
}

static void tc_mapi_copy_spropvalues_teardown(void)
{
	talloc_free(mem_ctx);
}

static void get_RecurrencePattern_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void get_RecurrencePattern_teardown(void)
{
	talloc_free(mem_ctx);
}

static void tc_get_AppointmentRecurrencePattern_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void tc_get_AppointmentRecurrencePattern_teardown(void)
{
	talloc_free(mem_ctx);
}

static void get_PersistDataArray_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void get_PersistDataArray_teardown(void)
{
	talloc_free(mem_ctx);
}

static void get_SizedXidArray_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void get_SizedXidArray_teardown(void)
{
	talloc_free(mem_ctx);
}

static void get_TimeZoneDefinition_setup(void)
{
	mem_ctx = talloc_new(talloc_autofree_context());
}

static void get_TimeZoneDefinition_teardown(void)
{
	talloc_free(mem_ctx);
}

Suite *libmapi_property_suite(void)
{
	Suite *s = suite_create("libmapi property");
	TCase *tc;

	tc = tcase_create("mapi_copy_spropvalues");
	tcase_add_checked_fixture(tc, tc_mapi_copy_spropvalues_setup, tc_mapi_copy_spropvalues_teardown);
	tcase_add_test(tc, test_mapi_copy_spropvalues);
	suite_add_tcase(s, tc);

	tc = tcase_create("get_RecurrencePattern");
	tcase_add_unchecked_fixture(tc, get_RecurrencePattern_setup, get_RecurrencePattern_teardown);
	tcase_add_test(tc, test_get_RecurrencePattern);
	suite_add_tcase(s, tc);

	tc = tcase_create("get_AppointmentRecurrencePattern");
	tcase_add_checked_fixture(tc, tc_get_AppointmentRecurrencePattern_setup,
				  tc_get_AppointmentRecurrencePattern_teardown);
	tcase_add_test(tc, test_get_AppointmentRecurrencePattern);
	suite_add_tcase(s, tc);

	tc = tcase_create("get_PersistDataArray");
	tcase_add_unchecked_fixture(tc, get_PersistDataArray_setup, get_PersistDataArray_teardown);
	tcase_add_test(tc, test_get_PersistDataArray);
	tcase_add_test(tc, test_push_PersistDataArray);
	suite_add_tcase(s, tc);

	tc = tcase_create("get_SizedXidArray");
	tcase_add_unchecked_fixture(tc, get_SizedXidArray_setup, get_SizedXidArray_teardown);
	tcase_add_test(tc, test_get_SizedXidArray);
	suite_add_tcase(s, tc);

	tc = tcase_create("get_TimeZoneDefinition");
	tcase_add_unchecked_fixture(tc, get_TimeZoneDefinition_setup, get_TimeZoneDefinition_teardown);
	tcase_add_test(tc, test_get_TimeZoneDefinition);
	suite_add_tcase(s, tc);

	return s;
}
