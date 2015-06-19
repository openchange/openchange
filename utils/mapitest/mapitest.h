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

#ifndef __MAPITEST_H__
#define	__MAPITEST_H__

#include "libmapi/libmapi.h"
#include "libmapi/mapi_nameid.h"
#include "libmapi/fxics.h"

#include <errno.h>
#include <err.h>

/* forward declaration */
struct mapitest;
struct mapitest_suite;


/**
	\file mapitest.h
	Data structures for %mapitest
 */

/**
  Flags for changing test applicability
  
  If you add values here, you also need to add a matching description to
  applicabilityFlagsDescription and suitable logic to
  mapitest_suite_test_is_applicable().
*/
enum TestApplicabilityFlags {
	ApplicableToAllVersions = 0,	/*!< This test is always applicable */
	NotInExchange2010 = 0x1,	/*!< This test is not applicable to Exchange 2010 */
	NotInExchange2010SP0 = 0x2,	/*!< This test is not applicable to Exchange 2010
	                                     Service Pack 0, but is applicable to later versions */
	NotInOpenChange = 0x4,		/*!< This test is not applicable to OpenChange as
					     the ROP is not implemented yet */
	ExpectedFail = 0x8000,          /*!< This test is expected to fail */
	LastTestApplicabilityFlag = 0xFFFF
};

/**
  List of possible test results
*/
enum TestResult {
	Pass,			/*!< The test was expected to pass, and it did */
	Fail,			/*!< The test was expected to pass, but it failed */
	UnexpectedPass,		/*!< The test was expected to fail, but it passed instead */
	ExpectedFailure		/*!< The test was expected to fail, and it did */
};

#include "utils/mapitest/proto.h"

/**
	A list of %mapitest tests

	%mapitest tests are grouped into test suites. This linked
	list data structure represents the various tests as a
	list of tests (i.e. the linked list is a suite of tests).

	The function that executes the test is pointed to by the
	fn element (i.e. fn is a function pointer).
*/
struct mapitest_test {
	struct mapitest_test		*prev;		/*!< The previous test in the list */
	struct mapitest_test		*next;		/*!< The next test in the list */
	char				*name;		/*!< The name of this test */
	char				*description;	/*!< The description of this test */
	void				*fn;		/*!< pointer to the test function */
	enum TestApplicabilityFlags	flags;		/*!< any applicability for this test */
};

/**
	List of test names

	This linked list data structure has a list of names of tests. It is
	used with mapitest_stat to record the failed tests.
*/
struct mapitest_unit {
	struct mapitest_unit	*prev;		/*!< The previous test in the list */
	struct mapitest_unit	*next;		/*!< The next test in the list */
	char			*name;		/*!< The name of the test */
	char			*reason;	/*!< Why this test was skipped or failed (if applicable) */
};

/**
	%mapitest statistics

	During a %mapitest run, %mapitest collects statistics on each test suite.

	This data structure records the results for one run of a test suite.

	There should be one entry in the failure_info list for each failure.
*/
struct mapitest_stat {
	uint32_t		success;        /*!< Number of tests in this suite that passed */
	uint32_t		failure;        /*!< Number of tests in this suite that failed */
	uint32_t		skipped;        /*!< Number of tests in this suite that were skipped */
	uint32_t		x_fail;         /*!< Number of tests in this suite that were expected failures */
	struct mapitest_unit	*failure_info;  /*!< List of names of the tests that failed */
	struct mapitest_unit	*skip_info;     /*!< List of names of the tests that were skipped, and why */
	bool			enabled;        /*!< Whether this statistics structure is valid */
};

/**
	A list of test suites

	%mapitest executes a set of tests. Those tests are grouped into
	suites of related tests (e.g. all tests that do not require a 
	server are in one suite, the tests for NSPI are in another suite,
	and so on). This linked list data structure represents the various
	test suites to be executed.
*/
struct mapitest_suite {
	struct mapitest_suite	*prev;        /*!< Pointer to the previous test suite */
	struct mapitest_suite	*next;        /*!< Pointer to the next test suite */
	char			*name;        /*!< The name of the test suite */
	char			*description; /*!< Description of the test suite */
	bool			online;       /*!< Whether this suite requires a server */
	struct mapitest_test	*tests;       /*!< The tests in this suite */
	struct mapitest_stat	*stat;        /*!< Results of running this test */
};

/**
	The context structure for a %mapitest run
*/
struct mapitest {
	TALLOC_CTX		*mem_ctx;	/*!< talloc memory context for memory allocations */
	struct mapi_context	*mapi_ctx;	/*!< mapi context */
	struct mapi_session	*session;
	bool			confidential;	/*!< true if confidential information should be omitted */
	bool			no_server;	/*!< true if only non-server tests should be run */
	bool			mapi_all;	/*!< true if all tests should be run */
	bool			online;		/*!< true if the server could be accessed */
	bool			color;		/*!< true if the output should be colored */
	bool			subunit_output; /*!< true if we should write output in subunit protocol format */
	struct emsmdb_info	info;
	struct mapi_profile	*profile;
	struct mapitest_suite	*mapi_suite;	/*!< the various test suites */
	struct mapitest_unit   	*cmdline_calls;
	struct mapitest_unit   	*cmdline_suite;
	const char		*org;
	const char		*org_unit;
	FILE			*stream;
	void			*priv;
};

struct mapitest_module {
	char			*name;
	
};

/**
   Context for %mapitest test folder
*/
struct mt_common_tf_ctx
{
	mapi_object_t	obj_store;
	mapi_object_t	obj_top_folder;
	mapi_object_t	obj_test_folder;
	mapi_object_t	obj_test_msg[10];
};


/*
 *  Defines
 */
#define	MAPITEST_SUCCESS	0
#define	MAPITEST_ERROR		-1

#define	MT_STREAM_MAX_SIZE	0x3000

#define	MT_YES			"[yes]"
#define	MT_NO			"[no]"

#define	MT_CONFIDENTIAL	"[Confidential]"

#define	MT_HDR_START		"#############################[mapitest report]#################################\n"
#define	MT_HDR_END		"###############################################################################\n"
#define	MT_HDR_FMT		"[*] %-25s: %-20s\n"
#define	MT_HDR_FMT_DATE		"[*] %-25s: %-20s"
#define	MT_HDR_FMT_SECTION	"[*] %-25s:\n"
#define	MT_HDR_FMT_SUBSECTION	"%-21s: %-10s\n"
#define	MT_HDR_FMT_VER_NORM	"%-21s: %02d.%02d.%04d.%04d\n"

#define	MT_DIRNAME_TOP		"MT Top of Mailbox"
#define	MT_DIRNAME_APPOINTMENT	"MT Calendar"
#define	MT_DIRNAME_CONTACT	"MT Contact"
#define	MT_DIRNAME_JOURNAL	"MT Journal"
#define	MT_DIRNAME_POST		"MT Post"
#define	MT_DIRNAME_NOTE		"MT Note"
#define	MT_DIRNAME_STICKYNOTE	"MT Sticky Notes"
#define	MT_DIRNAME_TASK		"MT Tasks"
#define	MT_DIRNAME_TEST		"MT Test Folder1"

#define	MT_MAIL_SUBJECT		"MT Sample E-MAIL"
#define	MT_MAIL_ATTACH		"Attach1.txt"
#define	MT_MAIL_ATTACH2		"Attach2.txt"

#define	MODULE_TITLE		"[MODULE] %s\n"
#define	MODULE_TITLE_DELIM	'#'
#define	MODULE_TITLE_NEWLINE	2
#define	MODULE_TITLE_LINELEN	80

#define	MODULE_TEST_TITLE	"[TEST] %s\n"
#define	MODULE_TEST_RESULT	"[RESULT] %s: %s\n"
#define	MODULE_TEST_DELIM	'-'
#define	MODULE_TEST_DELIM2	'='
#define	MODULE_TEST_LINELEN	72
#define	MODULE_TEST_NEWLINE	1
#define	MODULE_TEST_SUCCESS	"[SUCCESS]"
#define	MODULE_TEST_FAILURE	"[FAILURE]"

#define	MT_ERROR       	"[ERROR]: %s\n"

#define	MT_STAT_FAILED_TITLE	"[STAT] FAILED TEST CASES\n"
#define	MT_STAT_RESULT	"* %-35s: %s (%s)\n"
#define	MT_STAT_SKIPPED_TITLE	"[STAT] SKIPPED TEST CASES\n"

#define MT_SUMMARY_TITLE "[STAT] TEST SUMMARY\n"

#define	MT_WHITE	   "\033[0;29m"
#define MT_RED             "\033[1;31m"
#define MT_GREEN           "\033[1;32m"

#define Exchange2010SP0Version	0x0E00

#endif /* !__MAPITEST_H__ */
