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

#include <libmapi/libmapi.h>

#include <errno.h>
#include <err.h>

/* forward declaration */
struct mapitest;
struct mapitest_suite;

#include "utils/mapitest/proto.h"

/**
 * Data structures
 */

struct mapitest_test {
	struct mapitest_test	*prev;
	struct mapitest_test	*next;
	char			*name;
	char			*description;
	void			*fn;
};

struct mapitest_unit {
	struct mapitest_unit	*prev;
	struct mapitest_unit	*next;
	char			*name;
};


struct mapitest_stat {
	uint32_t		success;
	uint32_t		failure;
	struct mapitest_unit	*failure_info;
	bool			enabled;
};

struct mapitest_suite {
	struct mapitest_suite	*prev;
	struct mapitest_suite	*next;
	char			*name;
	char			*description;
	bool			online;
	struct mapitest_test	*tests;
	struct mapitest_stat	*stat;
};

struct mapitest {
	TALLOC_CTX		*mem_ctx;
	struct mapi_session	*session;
	bool			confidential;
	bool			no_server;
	bool			mapi_all;
	bool			online;
	bool			color;
	struct emsmdb_info	info;
	struct mapitest_suite	*mapi_suite;
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
   Context for mapitest test folder
*/
struct mt_common_tf_ctx
{
	mapi_object_t	obj_store;
	mapi_object_t	obj_top_folder;
	mapi_object_t	obj_test_folder;
	mapi_object_t	obj_test_msg[10];
};


/**
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
#define	MT_HDR_FMT_STORE_VER	"%-21s: %d.%d.%d\n"

#define	MT_DIRNAME_TOP		"[MT] Top of Mailbox"
#define	MT_DIRNAME_APPOINTMENT	"[MT] Calendar"
#define	MT_DIRNAME_CONTACT	"[MT] Contact"
#define	MT_DIRNAME_JOURNAL	"[MT] Journal"
#define	MT_DIRNAME_POST		"[MT] Post"
#define	MT_DIRNAME_NOTE		"[MT] Note"
#define	MT_DIRNAME_STICKYNOTE	"[MT] Sticky Notes"
#define	MT_DIRNAME_TASK		"[MT] Tasks"
#define	MT_DIRNAME_TEST		"[MT] Test Folder1"

#define	MT_MAIL_SUBJECT		"[MT] Sample E-MAIL"
#define	MT_MAIL_ATTACH		"[MT]_Sample_Attachment.txt"
#define	MT_MAIL_ATTACH2		"[MT]_Sample_Attachment2.txt"

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

#define	MT_STAT_TITLE	"[STAT] FAILURE REPORT\n"
#define	MT_STAT_FAILURE	"* %-35s: %s\n"

#define	MT_WHITE	   "\033[0;29m"
#define MT_RED             "\033[1;31m"
#define MT_GREEN           "\033[1;32m"

#endif /* !__MAPITEST_H__ */
