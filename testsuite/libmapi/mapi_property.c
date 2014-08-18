/*
   OpenChange Unit Testing

   OpenChange Project

   copyright (C) Kamen Mazdrashki 2014

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
		return strcmp(left->value.lpszA, right->value.lpszA);
	case PT_UNICODE:
		return strcmp(left->value.lpszW, right->value.lpszW);
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
			res = strcmp(left->value.MVszA.lppszA[i], right->value.MVszA.lppszA[i]);
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
	prop_val.value.MVszA.lppszA = talloc_array(test_srow, const char *, prop_val.value.MVszA.cValues);
	prop_val.value.MVszA.lppszA[0] = "string 1";
	prop_val.value.MVszA.lppszA[1] = "string 2";
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
	mem_ctx = talloc_named(NULL, 0, "libmapi_property_suite");
	test_srow = talloc_zero(mem_ctx, struct SRow);

	_make_test_srow(mem_ctx);
}

static void tc_mapi_copy_spropvalues_teardown(void)
{
	talloc_free(mem_ctx);
}

Suite *libmapi_property_suite(void)
{
	Suite *s = suite_create("libmapi property");

	TCase *tc = tcase_create("mapi_copy_spropvalues");
	tcase_add_checked_fixture(tc, tc_mapi_copy_spropvalues_setup, tc_mapi_copy_spropvalues_teardown);

	tcase_add_test(tc, test_mapi_copy_spropvalues);

	suite_add_tcase(s, tc);

	return s;
}
