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

#include <libmapi/libmapi.h>
#include <samba/version.h>
#include <utils/mapitest/mapitest.h>

#include <time.h>

static int count = 0;

#define	CNT_INDENT()	{ count++; }
#define	CNT_DEINDENT()	{ count--; if (count < 0) count = 0; }
#define	CNT_PRINT(s)	{ int i; for (i = 0; i < count; i++) { fprintf(s, "\t"); } }

/**
	\file
	Print / display functions for %mapitest output
*/

/**
   \details Indent the mapitest_print tabulation counter
 */
_PUBLIC_ void mapitest_indent(void)
{
	CNT_INDENT();
}


/**
   \details Deindent the mapitest_print tabulation counter
 */
_PUBLIC_ void mapitest_deindent(void)
{
	CNT_DEINDENT();
}


/**
   \details Print tabulations given the internal counter

   \param mt pointer to the top-level mapitest structure
 */
_PUBLIC_ void mapitest_print_tab(struct mapitest *mt)
{
	CNT_PRINT(mt->stream);
}


/**
   \details Print a string in the stream

   \param mt pointer to the top-level mapitest structure
   \param format the format string
   \param ... the format string parameters
 */
_PUBLIC_ void mapitest_print(struct mapitest *mt, const char *format, ...)
{
	va_list		ap;
	char		*s = NULL;
	int		ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	mapitest_print_tab(mt);
	fprintf(mt->stream, s, strlen(s));
	free(s);
}

/**
   \details Print newline characters

   \param mt pointer to the top-level mapitest structure
   \param count number of newline characters to print
 */
_PUBLIC_ void mapitest_print_newline(struct mapitest *mt, int count)
{
	int	i;

	for (i = 0; i < count; i++) {
		fprintf(mt->stream, "\n");
	}
}

/**
   \details Print a line using a delimiter

   \param mt pointer to the top-level mapitest structure
   \param len the length of the line to print
   \param delim the line delimiter
 */
_PUBLIC_ void mapitest_print_line(struct mapitest *mt, int len, char delim)
{
	int	i;

	for (i = 0; i < len; i++) {
		fprintf(mt->stream, "%c", delim);
	}
	mapitest_print_newline(mt, 1);
}


/**
   \details Underline a string

   \param mt pointer to the top-level mapitest structure
   \param str string to underline
   \param delim the line delimiter
 */
_PUBLIC_ void mapitest_underline(struct mapitest *mt, const char *str, char delim)
{
	if (!str) return;

	/* print str */
	mapitest_print_tab(mt);
	fprintf(mt->stream, "%s", str);

	/* underline str using delim */
	mapitest_print_tab(mt);
	mapitest_print_line(mt, strlen(str), delim);
}

/**
   \details Private general routine used to print a title

   Avoid code redundancy over the API

   \param mt pointer to the top-level mapitest structure
   \param str the title
   \param delim the underline delimiter
 */
_PUBLIC_ void mapitest_print_title(struct mapitest *mt, const char *str, char delim)
{
	mapitest_underline(mt, str, delim);
	mapitest_indent();
}


/**
   \details Print the module title

   \param mt pointer to the top-level mapitest structure
   \param str the module title string
 */
_PUBLIC_ void mapitest_print_module_title_start(struct mapitest *mt, const char *str)
{
	char	*title = NULL;

	if (!str) return;

	title = talloc_asprintf(mt->mem_ctx, MODULE_TITLE, str);
	mapitest_print(mt, "%s", title);
	mapitest_print_tab(mt);
	mapitest_print_line(mt, MODULE_TITLE_LINELEN, MODULE_TITLE_DELIM);
	mapitest_indent();
	talloc_free(title);
}

/**
   \details Print the content at the end of the module

   \param mt pointer to the top-level mapitest structure
 */
_PUBLIC_ void mapitest_print_module_title_end(struct mapitest *mt)
{
	mapitest_deindent();

	mapitest_print_tab(mt);
	mapitest_print_line(mt, MODULE_TITLE_LINELEN, MODULE_TITLE_DELIM);
	mapitest_print_newline(mt, MODULE_TITLE_NEWLINE);
}


/**
   \details print the test tile
   
   \param mt pointer to the top-level mapitest structure
   \param str the test title
 */
_PUBLIC_ void mapitest_print_test_title_start(struct mapitest *mt, const char *str)
{
	char		*title = NULL;

	if (!str) return;

	title = talloc_asprintf(mt->mem_ctx, MODULE_TEST_TITLE, str);
	mapitest_print(mt, "%s", title);
	mapitest_print_tab(mt);
	mapitest_print_line(mt, MODULE_TEST_LINELEN, MODULE_TEST_DELIM);
	mapitest_indent();
	talloc_free(title);
}


/**
   \details Write the content at the end of a test

   \param mt pointer to the top-level mapitest structure
 */
_PUBLIC_ void mapitest_print_test_title_end(struct mapitest *mt)
{
	mapitest_deindent();

	mapitest_print_tab(mt);
	mapitest_print_line(mt, MODULE_TEST_LINELEN, MODULE_TEST_DELIM);
}


/**
   \details Starts the header output

   \param mt pointer on the top-level mapitest structure
 */
static void mapitest_print_headers_start(struct mapitest *mt)
{
	mapitest_print(mt, MT_HDR_START);
}


/**
   \details Ends the header output
 */
static void mapitest_print_headers_end(struct mapitest *mt)
{
	mapitest_print(mt, MT_HDR_END);
}

/**
   \details Print mapitest report headers information
   
   \param mt pointer to the top-level mapitest structure
 */
_PUBLIC_ void mapitest_print_headers_info(struct mapitest *mt)
{
	time_t		t;
	char		*date;

	time (&t);
	date = ctime(&t);

	mapitest_print(mt, MT_HDR_FMT_DATE, "Date", date);
	mapitest_print(mt, MT_HDR_FMT, "Confidential mode", 
		       (mt->confidential == true) ? MT_YES : MT_NO);
	mapitest_print(mt, MT_HDR_FMT, "Samba Information", SAMBA_VERSION_STRING);
	mapitest_print(mt, MT_HDR_FMT, "OpenChange Information", OPENCHANGE_VERSION_STRING);

	mapitest_print_newline(mt, 1);
	mapitest_print(mt, MT_HDR_FMT_SECTION, "System Information");
	mapitest_indent();
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Kernel name", OPENCHANGE_SYS_KERNEL_NAME);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Kernel release", OPENCHANGE_SYS_KERNEL_RELEASE);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Processor", OPENCHANGE_SYS_PROCESSOR);
	mapitest_deindent();
}

/**
  \details Print out a normalized version number for a client or server.
  
  \param mt pointer to the top level mapitest structure
  \param label label for the version (e.g. "Store Version" or "Client Version")
  \param word0 highest order word for the version
  \param word1 middle word for the version
  \param word2 low word for the version
*/
static void mapitest_print_version_normalised(struct mapitest *mt, const char* label,
					      const uint16_t word0, const uint16_t word1, const uint16_t word2)
{
	/* See MS-OXRPC Section 3.1.9 to understand this */
	uint16_t normalisedword0;
	uint16_t normalisedword1;
	uint16_t normalisedword2;
	uint16_t normalisedword3;

	if (word1 & 0x8000) {
		/* new format */
		normalisedword0 = (word0 & 0xFF00) >> 8;
		normalisedword1 = (word0 & 0x00FF);
		normalisedword2 = (word1 & 0x7FFF);
		normalisedword3 = word2;
	} else {
		normalisedword0 = word0;
		normalisedword1 = 0;
		normalisedword2 = word1;
		normalisedword3 = word2;
	}
	mapitest_print(mt, MT_HDR_FMT_VER_NORM, label, normalisedword0,
		       normalisedword1, normalisedword2, normalisedword3);
}

/**
   \details Print a report of the Exchange server and account information

   \param mt pointer to the top-level mapitest structure
 */
_PUBLIC_ void mapitest_print_headers_server_info(struct mapitest *mt)
{
	if (mt->online == false) {
		return;
	}

	mapitest_print_newline(mt, 1);
	mapitest_print(mt, MT_HDR_FMT_SECTION, "Exchange Server");
	mapitest_indent();
	mapitest_print_version_normalised(mt, "Store Version",
					  mt->info.rgwServerVersion[0],
					  mt->info.rgwServerVersion[1],
					  mt->info.rgwServerVersion[2]);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Username",
		       (mt->confidential == true) ? MT_CONFIDENTIAL : mt->info.szDisplayName);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Organization",
		       (mt->confidential == true) ? MT_CONFIDENTIAL : mt->org);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Organization Unit",
		       (mt->confidential == true) ? MT_CONFIDENTIAL : mt->org_unit);
	mapitest_deindent();
}


/**
   \details Print mapitest report headers

   \param mt pointer to the top-level mapitest structure
 */
_PUBLIC_ void mapitest_print_headers(struct mapitest *mt)
{
	mapitest_print_headers_start(mt);
	mapitest_indent();
	mapitest_print_headers_info(mt);
	if (mt->no_server == false) {
		mapitest_print_headers_server_info(mt);
	}
	mapitest_deindent();
	mapitest_print_headers_end(mt);
	mapitest_print_newline(mt, 2);
}


/**
   \details Print %mapitest test result

   \param mt pointer to the top-level mapitest structure
   \param name the test name
   \param ret boolean value with the test result
 */
_PUBLIC_ void mapitest_print_test_result(struct mapitest *mt, char *name, bool ret)
{
	mapitest_print(mt, MODULE_TEST_RESULT, name, (ret == true) ? 
		       MODULE_TEST_SUCCESS : MODULE_TEST_FAILURE);
	mapitest_print_tab(mt);
	mapitest_print_line(mt, MODULE_TEST_LINELEN, MODULE_TEST_DELIM2);
	mapitest_print_newline(mt, MODULE_TEST_NEWLINE);
}


/**
   \details Print %mapitest return value

   \param mt pointer to the top-level mapitest structure
   \param name the test name

   \sa mapitest_print_retval_fmt for a version providing an additional format string
 */
_PUBLIC_ void mapitest_print_retval(struct mapitest *mt, char *name)
{
	const char	*retstr = NULL;

	retstr = mapi_get_errstr(GetLastError());

	if (mt->color == true) {
		if (retstr) {
			mapitest_print(mt, "* %-35s: %s %s %s \n", name, (GetLastError() ? MT_RED : MT_GREEN), retstr, MT_WHITE);
		} else {
			mapitest_print(mt, "* %-35s: %s Unknown Error (0x%.8x) %s\n", name, MT_RED, GetLastError(), MT_WHITE);
		}
	} else {
		if (retstr) {
			mapitest_print(mt, "* %-35s: %s\n", name, retstr);
		} else {
			mapitest_print(mt, "* %-35s: Unknown Error (0x%.8x)\n", name, GetLastError());
		}
	}
}


/**
   \details Print %mapitest return value with additional format string

   \param mt pointer to the top-level mapitest structure
   \param name the test name
   \param format the format string
   \param ... the format string parameters
 */
_PUBLIC_ void mapitest_print_retval_fmt(struct mapitest *mt, char *name, const char *format, ...)
{
	const char	*retstr = NULL;
	va_list		ap;
	char		*s = NULL;
	int		ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	retstr = mapi_get_errstr(GetLastError());

	if (mt->color == true) {
		if (retstr) {
			mapitest_print(mt, "* %-35s: %s %s %s %s\n", name, (GetLastError() ? MT_RED: MT_GREEN), retstr, MT_WHITE, s);
		} else {
			mapitest_print(mt, "* %-35s: %s Unknown Error (0x%.8x) %s %s\n", name, MT_RED, GetLastError(), MT_WHITE, s);
		}
	} else {
		if (retstr) {
			mapitest_print(mt, "* %-35s: %s %s\n", name, retstr, s);
		} else {
			mapitest_print(mt, "* %-35s: Unknown Error (0x%.8x) %s\n", name, GetLastError(), s);
		}
	}
	free(s);
}


/**
   \details Print %mapitest return value for a given step

   \param mt pointer tp the top-level mapitest structure
   \param step the test step
   \param name the test name

   \sa mapitest_print_retval_step_fmt for a version providing an additional format string
 */
_PUBLIC_ void mapitest_print_retval_step(struct mapitest *mt, char *step, char *name)
{
	const char	*retstr = NULL;

	retstr = mapi_get_errstr(GetLastError());

	if (mt->color == true) {
		if (retstr) {
			mapitest_print(mt, "* Step %-5s %-35s: %s %s %s\n", step, name, (GetLastError() ? MT_RED : MT_GREEN), retstr, MT_WHITE);
		} else {
			mapitest_print(mt, "* Step %-5s %-35s: %s Unknown Error (0x%.8x) %s\n", step, name, MT_RED, GetLastError(), MT_WHITE);
		}
	} else {
		if (retstr) {
			mapitest_print(mt, "* Step %-5s %-35s: %s\n", step, name, retstr);
		} else {
			mapitest_print(mt, "* Step %-5s %-35s: Unknown Error (0x%.8x)\n", step, name, GetLastError());
		}
	}
}


/**
   \details Print %mapitest return value for a given step with additional format string

   \param mt pointer to the top-level mapitest structure
   \param step the test step
   \param name the test name
   \param format the format string
   \param ... the format string parameters
 */
_PUBLIC_ void mapitest_print_retval_step_fmt(struct mapitest *mt, char *step, char *name, const char *format, ...)
{
	const char	*retstr = NULL;
	va_list		ap;
	char		*s = NULL;
	int		ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	retstr = mapi_get_errstr(GetLastError());

	if (mt->color == true) {
		if (retstr) {
			mapitest_print(mt, "* Step %-5s %-35s: %s %s %s %s\n", step, name, (GetLastError() ? MT_RED : MT_GREEN), retstr, MT_WHITE, s);
		} else {
			mapitest_print(mt, "* Step %-5s %-35s: %s Unknown Error (0x%.8x) %s %s\n", step, name, MT_RED, GetLastError(), MT_WHITE, s);
		}
	} else {
		if (retstr) {
			mapitest_print(mt, "* Step %-5s %-35s: %s %s\n", step, name, retstr, s);
		} else {
			mapitest_print(mt, "* Step %-5s %-35s: Unknown Error (0x%.8x) %s\n", step, name, GetLastError(), s);
		}
	}
	free(s);
}
