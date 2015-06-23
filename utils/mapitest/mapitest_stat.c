/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include "utils/mapitest/mapitest.h"
#include "utils/dlinklist.h"

#include <assert.h>

/**
	\file
	mapitest statistics functions

	mapitest records and prints the results of each test
	using these functions
*/

/**
  Descriptions of TestApplicabilityFlags
*/
struct TestApplicabilityDescription {
	enum TestApplicabilityFlags	flags;		/*!< The flag value */
	char*				description;	/*!< A user-visible string equivalent */
} applicabilityFlagsDescription[] = {
	{ ApplicableToAllVersions, 	"Applicable to all server versions" },
	{ NotInExchange2010, 		"Not applicable to Exchange 2010" },
	{ NotInExchange2010SP0,		"Not applicable to Exchange 2010 SP0" },
	{ NotInOpenChange,		"Not implemented in OpenChange" },
	{ LastTestApplicabilityFlag,	"Sentinel Value" }
};

/**
   \details Initialize the mapitest statistic structure
   
   \param mem_ctx memory allocation context

   \return Allocated stat structure on success, otherwise NULL
 */
_PUBLIC_ struct mapitest_stat *mapitest_stat_init(TALLOC_CTX *mem_ctx)
{
	struct mapitest_stat	*stat = NULL;

	/* Sanity check */
	if (!mem_ctx) return NULL;

	stat = talloc_zero(mem_ctx, struct mapitest_stat);
	stat->success = 0;
	stat->failure = 0;
	stat->skipped = 0;
	stat->x_fail = 0;
	stat->failure_info = NULL;
	stat->skip_info = NULL;
	stat->enabled = false;

	return stat;
}


/**
   \details Add test result to the suite statistic parameter

   \param suite the suite container
   \param name the test name
   \param testresult the test result

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t mapitest_stat_add_result(struct mapitest_suite *suite,
					   const char *name,
					   enum TestResult testresult)
{
	struct mapitest_unit	*el = NULL;

	/* Sanity check */
	if (!suite || !suite->stat || !name) return MAPITEST_ERROR;

	if (testresult == Pass) {
		suite->stat->success++;
	} else {
		el = talloc_zero((TALLOC_CTX *) suite->stat, struct mapitest_unit);
		el->name = talloc_strdup((TALLOC_CTX *)el, (char *)name);

		if (testresult == Fail) {
			suite->stat->failure++;
			el->reason = talloc_strdup((TALLOC_CTX *) el, "Unexpected Fail");
		} else if ((int)testresult == (int)ExpectedFailure) {
			suite->stat->x_fail++;
			el->reason = talloc_strdup((TALLOC_CTX *) el, "Expected Fail");
		} else if (testresult == UnexpectedPass) {
			suite->stat->failure++;
			el->reason = talloc_strdup((TALLOC_CTX *) el, "Unexpected Pass");
		} else {
			assert(0); /* this indicates that we're passing a bad enum into this function */
		}
		DLIST_ADD_END(suite->stat->failure_info, el, struct mapitest_unit *);
	}

	suite->stat->enabled = true;

	return MAPITEST_SUCCESS;
}

static void mapitest_stat_add_skipped_test_reason(struct mapitest_unit *stat_unit, enum TestApplicabilityFlags flags)
{
	int i;
	for (i = 0; applicabilityFlagsDescription[i].flags != LastTestApplicabilityFlag; ++i) {
		if (flags & applicabilityFlagsDescription[i].flags) {
			if (!stat_unit->reason) {
				stat_unit->reason = talloc_strdup((TALLOC_CTX *)stat_unit, applicabilityFlagsDescription[i].description);
			} else {
				stat_unit->reason = talloc_asprintf_append(stat_unit->reason, ", %s", applicabilityFlagsDescription[i].description);
			}
		}
	}
	if (!stat_unit->reason) {
		stat_unit->reason = talloc_strdup((TALLOC_CTX *)stat_unit, "Unknown reason");
	}
}

/**
   \details Add a skipped test to the suite statistic parameters

   \param suite the suite container
   \param name the test name
   \param flags flags to indicate the reason why the test was skipped

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
 */
_PUBLIC_ uint32_t mapitest_stat_add_skipped_test(struct mapitest_suite *suite,
						 const char *name,
						 enum TestApplicabilityFlags flags)
{
	struct mapitest_unit	*el = NULL;

	/* Sanity check */
	if (!suite || !suite->stat || !name) return MAPITEST_ERROR;

	suite->stat->skipped++;
	el = talloc_zero((TALLOC_CTX *) suite->stat, struct mapitest_unit);
	el->name = talloc_strdup((TALLOC_CTX *)el, (char *)name);
	mapitest_stat_add_skipped_test_reason(el, flags);
	DLIST_ADD_END(suite->stat->skip_info, el, struct mapitest_unit *);

	suite->stat->enabled = true;

	return MAPITEST_SUCCESS;
}

/**
   \details Dump mapitest statistics about test failures

   \param mt the global mapitest structure

   \return the number of test steps that failed (i.e. 0 for all success)
 */
_PUBLIC_ int32_t mapitest_stat_dump(struct mapitest *mt)
{
	struct mapitest_suite	*suite;
	struct mapitest_unit	*el;
	int32_t 		num_passed_tests = 0;
	int32_t 		num_failed_tests = 0;
	int32_t 		num_skipped_tests = 0;
	int32_t			num_xfail_tests = 0;

	mapitest_print_title(mt, MT_STAT_SKIPPED_TITLE, MODULE_TITLE_DELIM);
	for (suite = mt->mapi_suite; suite; suite = suite->next) {
		if (suite->stat->enabled == true) {
			num_skipped_tests += suite->stat->skipped;
			if (suite->stat->skipped) {
				for (el = suite->stat->skip_info; el; el = el->next) {
					mapitest_print(mt, MT_STAT_RESULT, suite->name, el->name, el->reason);
				}
			}
		}
	}

	mapitest_print_test_title_end(mt);

	mapitest_print_title(mt, MT_STAT_FAILED_TITLE, MODULE_TITLE_DELIM);

	for (suite = mt->mapi_suite; suite; suite = suite->next) {
		if (suite->stat->enabled == true) {
			num_passed_tests += suite->stat->success;
			num_failed_tests += suite->stat->failure;
			num_xfail_tests += suite->stat->x_fail;
			if (suite->stat->failure || suite->stat->x_fail) {
				for (el = suite->stat->failure_info; el; el = el->next) {
					mapitest_print(mt, MT_STAT_RESULT, suite->name, el->name, el->reason);
				}
			}
		}
	}

	mapitest_print_test_title_end(mt);

	mapitest_print_title(mt, MT_SUMMARY_TITLE, MODULE_TITLE_DELIM);
	mapitest_print(mt, "Number of passing tests: %i\n", num_passed_tests);
	mapitest_print(mt, "Number of failing tests: %i\n", num_failed_tests);
	mapitest_print(mt, "Number of skipped tests: %i\n", num_skipped_tests);
	mapitest_print(mt, "Number of expected fails: %i\n", num_xfail_tests);
	mapitest_print_test_title_end(mt);

	return num_failed_tests;
}
