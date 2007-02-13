/*
 *  OpenChange MAPI implementation.
 *  Codepages handled by Microsoft Windows Exchange
 *
 *  Copyright (C) Julien Kerihuel 2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libmapi/libmapi.h>
#include <samba/popt.h>

/* command line options */
static struct {
	uint32_t	 locale_id;
	char		*language_group;
	BOOL		group;
	uint32_t	codepage;
	char		*language_info;
} options;

/*
  print locales information given a locale_id
*/

static void process_one(const char *name)
{
	if (options.group) {
		printf("Language groups:\n");
		print_group();
	}

	if (options.locale_id) {
		if (print_locale(options.locale_id) == False) {
			DEBUG(0, ("Unknown locale id in the locale database (%s)\n", name));
		}
			
	}
	if (options.language_group) {
		if (print_groupmember(lang2nb(options.language_group)) == False) {
			DEBUG(0, ("(%s)\n", name));
			exit (1);
		}
	}

	if (options.codepage) {
		if (print_codepage_infos(options.codepage) == False) {
			DEBUG(0, ("Invalid codepage (%s)\n", name));
			exit (1);
		}
	}
	if (options.language_info) {
		printf("######### BEGIN: (copy and paste into smb.conf) #########\n");
		if (get_locales(options.language_info) == False) {
			DEBUG(0, ("Invalid language (%s)\n", name));
			exit (1);
		}
		printf("######### END:   (copy and paste into smb.conf) #########\n");
	}
}

int main(int argc, const char *argv[])
{
	poptContext pc;
	int opt;
	enum{OPT_LOCALID=1,OPT_LANGROUPS,OPT_GROUPS,OPT_CP,OPT_LANGINFO};
	struct poptOption popt_options[] = {
		POPT_AUTOHELP
		{ "locale_id",		'L', POPT_ARG_STRING,	&options.locale_id,	OPT_LOCALID,   	"Display locale information for a given locale id", "LOCALE ID" },
		{ "language_group",	'G', POPT_ARG_STRING,	&options.language_group,OPT_LANGROUPS,	"Display all languages for the specified language group", "LANGUAGE_GROUP" },
		{"list_groups",		'l', POPT_ARG_NONE,	&options.group,		OPT_GROUPS,	"List existing language groups"},
		{"codepage",		'c', POPT_ARG_INT,	&options.codepage,	OPT_CP,		"Check if CODEPAGE exists and display related information\n", "CODEPAGE"},
		{"language_info",	'i', POPT_ARG_STRING,	&options.language_info,	OPT_LANGINFO,	"Copy and Paste information into your smb.conf file\n", "LANGUAGE"},
		POPT_TABLEEND
	};
	
	pc = poptGetContext(argv[0], argc, argv, popt_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_GROUPS:
			process_one(NULL);
			break;
		case OPT_LOCALID:
		case OPT_LANGROUPS:
		case OPT_CP:
		case OPT_LANGINFO:
			process_one(poptGetOptArg(pc));
			break;
		}
	}
		
	return 0;
}
