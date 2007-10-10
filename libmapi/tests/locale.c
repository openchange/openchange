/*
   OpenChange MAPI implementation.
   Codepages handled by Microsoft Exchange Server

   Copyright (C) Julien Kerihuel 2005.

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

#include <talloc.h>
#include <popt.h>
#include <util.h>

/* command line options */
static struct {
	bool locale_id;
	bool language_group;
	bool group;
} options;

/*
  print locales information given a locale_id
*/

static void process_one(const char *name)
{
	uint32_t value = 0;

	if (options.group) {
		printf("Language groups:\n");
		print_group();
	}

	else if (options.locale_id) {
		value = (uint32_t)strtol(name, NULL, 16);
		if (print_locale(value) == False) {
			DEBUG(0, ("Unknown locale id in the locale database (%s)\n", name));
		}
			
	}
	else if (options.language_group) {
		if ((value = strtol(name, NULL, 10)) == 0) {
			if ((value = lang2nb(name)) == -1) {
				DEBUG(0, ("Invalid language group provided (%s)\n", name));
				exit (1);
			}
		}
		DEBUG(3, ("value = %d\n", value));
		print_groupmember(value);
	}
}

int main(int argc, const char *argv[])
{
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "locale_id", 'L', POPT_ARG_VAL, &options.locale_id, 
		  'L', "Display locale information for a given locale id", "LOCALE ID" },

		{ "language_group", 'G', POPT_ARG_VAL, &options.language_group,
		  'G', "Display all languages for the specified language group", "LANGUAGE_GROUP" },

		{"list_groups", 'l', POPT_ARG_NONE, &options.group,
		 True, "List existing language groups"},

		{ 0, 0, 0, 0 }
	};
	
	pc = poptGetContext("locale", argc, (const char **)argv, long_options,
			    POPT_CONTEXT_KEEP_FIRST);

	while ((poptGetNextOpt(pc) != -1)) /* noop */;

	/* swallow argv[0] */
	poptGetArg(pc);

	if (!poptPeekArg(pc)) {
		poptPrintUsage(pc, stderr, 0);
		exit (1);
	}

	while (poptPeekArg(pc)) {
		const char *name = poptGetArg(pc);

		process_one(name);
	}

	poptFreeContext(pc);

	return 0;
}
