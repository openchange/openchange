/*
   Fetch the nameid of a named property

   OpenChange Project

   Copyright (C) Wolfgang Sourdeau <wsourdeau@inverse.ca> 2011

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

/* gcc fetch-nameid.c -o fetch-nameid -I /usr/local/samba/include
 *                    -L/usr/local/samba/lib -lmapi -ltalloc -lpopt -lndr */

#include <stdbool.h>
#include <stdio.h>
#include "gen_ndr/exchange.h"

#include "libmapi/libmapi.h"

#include <popt.h>
#include <ldb.h>
#include <talloc.h>

static void popt_openchange_version_callback(poptContext con,
                                             enum poptCallbackReason reason,
                                             const struct poptOption *opt,
                                             const char *arg,
                                             const void *data)
{
        switch (opt->val) {
        case 'V':
                printf("Version %s\n", OPENCHANGE_VERSION_STRING);
                exit (0);
        }
}

struct poptOption popt_openchange_version[] = {
        { NULL, '\0', POPT_ARG_CALLBACK, (void *)popt_openchange_version_callback, '\0', NULL, NULL },
        { "version", 'V', POPT_ARG_NONE, NULL, 'V', "Print version ", NULL },
        POPT_TABLEEND
};

#define DEFAULT_PROFDB  "%s/.openchange/profiles.ldb"

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_context		*mapi_ctx;
	struct mapi_session		*session = NULL;
	mapi_object_t			obj_store, obj_folder;
	uint32_t			prop_id;
	int64_t				query_fid = 0;
	poptContext			pc;
	int				opt;
	const char			*prop_id_str;
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;
	const char			*opt_folder_id = NULL;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_FOLDER_ID};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 0, POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{"folder-id", 'F', POPT_ARG_STRING, NULL, OPT_FOLDER_ID, "folder id to query (mandatory)", "FOLDER_ID"},
		{ NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_openchange_version, 0, "Common openchange options:", NULL },
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "test-folders");

	pc = poptGetContext("fetch-nameid", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = talloc_strdup(mem_ctx, (char *)poptGetOptArg(pc));
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_FOLDER_ID:
			opt_folder_id = poptGetOptArg(pc);
			break;
		}
	}

	/**
	 * Sanity checks
	 */

	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	/**
	 * Initialize MAPI subsystem
	 */

	retval = MAPIInitialize(&mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		exit (1);
	}

	/* if no profile is supplied use the default one */
	if (!opt_profname) {
		retval = GetDefaultProfile(mapi_ctx, &opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			printf("No profile specified and no default profile found\n");
			exit (1);
		}
	}

	retval = MapiLogonEx(mapi_ctx, &session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", retval);
		exit (1);
	}

	if (!opt_folder_id) {
		fprintf (stderr, "Folder ID parameter is required.\n");
		exit (1);
	}
	query_fid = strtoull(opt_folder_id, NULL, 0);

	prop_id_str = poptGetArg(pc);
	if (!prop_id_str) {
		fprintf (stderr, "Property ID parameter is required.\n");
		exit (1);
	}
	prop_id = strtol(prop_id_str, NULL, 0);
	if ((prop_id & 0xffff) == prop_id) {
		prop_id <<= 16;
	}

	/* Open the default message store */
	mapi_object_init(&obj_store);

	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", retval);
		exit (1);
	}

	mapi_object_init(&obj_folder);
	retval = OpenFolder(&obj_store, query_fid, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", retval);
		exit (1);
	}

	{
		uint16_t pcount;
		struct MAPINAMEID *nameid;

		GetNamesFromIDs(&obj_folder, prop_id, &pcount, &nameid);
		if (pcount) {
			char *guid;

			printf ("type: %s\n", (nameid->ulKind == MNID_ID ?
					       "id" : "string"));
			guid = GUID_string2(mem_ctx, &nameid->lpguid);
			printf ("guid: %s\n", guid);

			switch(nameid->ulKind) {
			case MNID_ID:
				printf ("lid: %.8x\n", nameid->kind.lid);
				break;
			case MNID_STRING:
				printf ("name: %*s\n", nameid->kind.lpwstr.NameSize, nameid->kind.lpwstr.Name);
				break;
			};
		}
		else {
			printf ("property not found\n");
		}
	}
	
	Release(&obj_folder);


	/* mapi_object_release(&obj_msg2); */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

	return 0;
}
