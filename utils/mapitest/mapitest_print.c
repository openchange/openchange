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
#include <samba/popt.h>
#include <samba/version.h>
#include <param.h>
#include <param/proto.h>

#include <time.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>

static int	count = 0;

#define	CNT_INDENT()	{ count++; }
#define	CNT_DEINDENT()	{ count--; if (count < 0) count = 0; }
#define	CNT_PRINT(s)	{ int i; for (i = 0; i < count; i++) { fprintf(s, "\t");} }
#define	SYMB_PRINT(s,t,n) { int i; for (i = 0; i < n; i++) {fprintf(s,t);} fprintf(s, "\n"); }


void mapitest_print_line(struct mapitest *mt, unsigned int number)
{
	int	i;

	if (number <= 0) number = 1;

	for (i = 0; i < number; i++) {
		fprintf(mt->stream, "\n");
	}
}


void mapitest_indent(void)
{
	CNT_INDENT();
}


void mapitest_deindent(void)
{
	CNT_DEINDENT();
}

void mapitest_print(struct mapitest *mt, const char *format, ...)
{
	va_list		ap;
	char		*s = NULL;

	va_start(ap, format);
	vasprintf(&s, format, ap);
	va_end(ap);

	CNT_PRINT(mt->stream);
	fprintf(mt->stream, s, strlen(s));
	free(s);
}


void mapitest_print_bool(struct mapitest *mt, const char *format, 
			 const char *section, bool val)
{
	CNT_PRINT(mt->stream);
	fprintf(mt->stream, format, section, (val == true) ? MT_YES : MT_NO);
}


void mapitest_print_headers_start(struct mapitest *mt)
{
	mapitest_print(mt, MT_HDR_START);
}


void mapitest_print_headers_end(struct mapitest *mt)
{
	mapitest_print(mt, MT_HDR_END);
}


void mapitest_print_headers_info(struct mapitest *mt)
{
	time_t		t;
	char		*date;

	if (mt == NULL) return;

	time(&t);
	date = ctime(&t);

	mapitest_print(mt, MT_HDR_FMT_DATE, "Date", date);
	mapitest_print_bool(mt, MT_HDR_FMT, "Confidential mode", mt->confidential);
	mapitest_print(mt, MT_HDR_FMT, "Samba Information", SAMBA_VERSION_STRING);
	mapitest_print(mt, MT_HDR_FMT, "OpenChange Information", OPENCHANGE_VERSION_STRING);

	mapitest_print_line(mt, 1);
	mapitest_print(mt, MT_HDR_FMT_SECTION, "System Information");
	CNT_INDENT();
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Kernel name", OPENCHANGE_SYS_KERNEL_NAME);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Kernel release", OPENCHANGE_SYS_KERNEL_RELEASE);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Processor", OPENCHANGE_SYS_PROCESSOR);	
	CNT_DEINDENT();
}

void mapitest_print_server_info(struct mapitest *mt)
{
	char	*mailbox = NULL;

	if (mt == NULL || mt->info.username == NULL) return;

	mailbox = talloc_asprintf(mt->mem_ctx, "%s%s", 
				  mt->info.mailbox, mt->info.username);

	mapitest_print_line(mt, 1);
	mapitest_print(mt, MT_HDR_FMT_SECTION, "Exchange Server");
	CNT_INDENT();
	mapitest_print(mt, MT_HDR_FMT_STORE_VER, "Store version",
		       mt->info.store_version[0],
		       mt->info.store_version[1],
		       mt->info.store_version[2]);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Username",
		       (mt->confidential == true) ? MT_CONFIDENTIAL : mt->info.username);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Organization", 
		       (mt->confidential == true) ? MT_CONFIDENTIAL : mt->org);
	mapitest_print(mt, MT_HDR_FMT_SUBSECTION, "Organization Unit", 
		       (mt->confidential == true) ? MT_CONFIDENTIAL : mt->org_unit);

	CNT_DEINDENT();
}


void mapitest_print_options(struct mapitest *mt)
{
	mapitest_print_line(mt, 1);
	mapitest_print(mt, MT_HDR_FMT_SECTION, "MapiTest Options");
	CNT_INDENT();
	mapitest_print_bool(mt, MT_HDR_FMT_SUBSECTION, "mapi all", mt->mapi_all);
	mapitest_print_bool(mt, MT_HDR_FMT_SUBSECTION, "mapi atomic calls", mt->mapi_calls);
	CNT_DEINDENT();
}


void mapitest_print_interface_start(struct mapitest *mt, const char *name)
{
	static int int_nb = 1;

	mapitest_print_line(mt, 1);
	mapitest_print(mt, MT_INTERFACE_FMT, int_nb, name);
	SYMB_PRINT(mt->stream, "-", strlen(name) + 7);
	CNT_INDENT();
	int_nb++;
}


void mapitest_print_interface_end(struct mapitest *mt)
{
	CNT_DEINDENT();
}


void mapitest_print_call(struct mapitest *mt, const char *call, 
			 enum MAPISTATUS retval)
{
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print(mt, MT_CALL_FMT_KO, call, GetLastError());
	} else {
		mapitest_print(mt, MT_CALL_FMT_OK, call);
	}
}


void mapitest_print_subcall(struct mapitest *mt, const char *call, 
			    enum MAPISTATUS retval)
{
	if (retval != MAPI_E_SUCCESS) {
		mapitest_print(mt, MT_SUBCALL_FMT_KO, call, GetLastError());
	} else {
		mapitest_print(mt, MT_SUBCALL_FMT_OK, call);
	}
}
