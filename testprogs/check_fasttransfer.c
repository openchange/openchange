/*
   Test fast transfer operations and parser

   OpenChange Project

   Copyright (C) Brad Hards <bradh@openchange.org> 2010

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

#include "libmapi/libmapi.h"
#include <samba/popt.h>
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

#define POPT_OPENCHANGE_VERSION { NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_openchange_version, 0, "Common openchange options:", NULL },
#define DEFAULT_PROFDB  "%s/.openchange/profiles.ldb"

static enum MAPISTATUS process_marker(uint32_t marker)
{
	printf("Marker: %s (0x%08x)\n", get_proptag_name(marker), marker);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS process_delprop(uint32_t proptag)
{
	printf("DelProps: 0x%08x (%s)\n", proptag, get_proptag_name(proptag));
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS process_namedprop(uint32_t proptag, struct MAPINAMEID nameid)
{
	TALLOC_CTX *mem_ctx;
	mem_ctx = talloc_init("process_namedprop");
	printf("Named Property: 0x%08x has GUID %s and ", proptag, GUID_string(mem_ctx, &(nameid.lpguid)));
	if (nameid.ulKind == MNID_ID) {
		printf("dispId 0x%08x\n", nameid.kind.lid);
	} else if (nameid.ulKind == MNID_STRING) {
		printf("name %s\n", nameid.kind.lpwstr.Name);
	}
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS process_property(struct SPropValue prop)
{
	mapidump_SPropValue(prop, "\t");
	return MAPI_E_SUCCESS;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_session		*session = NULL;
	struct mapi_profile		*profile;
	mapi_object_t			obj_store;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_table;
	mapi_object_t			obj_fx_context;
	mapi_id_t			id_inbox;

	uint16_t			progressCount;
	uint16_t			totalStepCount;
	int				transfers = 0;
	enum TransferStatus		fxTransferStatus;
	DATA_BLOB			transferdata;
	struct fx_parser_context	*parser;

	uint32_t			count;
	struct SPropTagArray		*SPropTagArray = NULL;
#if 0
	unsigned int			i;
	mapi_object_t			obj_message;
	struct SRowSet			rowset;
#endif
	poptContext			pc;
	int				opt;
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;
	uint32_t			opt_maxsize = 0;
	bool				opt_showprogress = false;
	bool				opt_dumpdata = false;
	const char			*opt_debug = NULL;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_MAXDATA, OPT_SHOWPROGRESS, OPT_DEBUG, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{"maxdata", 0, POPT_ARG_INT, NULL, OPT_MAXDATA, "the maximum transfer data size", "SIZE"},
		{"showprogress", 0, POPT_ARG_NONE, NULL, OPT_SHOWPROGRESS, "enable progress display", NULL},
		{"debuglevel", 'd', POPT_ARG_STRING, NULL, OPT_DEBUG, "set the debug level", "LEVEL"},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the transfer data", NULL},
		POPT_OPENCHANGE_VERSION
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "check_fasttransfer");

	pc = poptGetContext("check_fasttransfer", argc, argv, long_options, 0);

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
		case OPT_MAXDATA:
			opt_maxsize = *poptGetOptArg(pc);
			break;
		case OPT_SHOWPROGRESS:
			opt_showprogress = true;
			break;
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_DUMPDATA:
			opt_dumpdata = true;
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

	retval = MAPIInitialize(opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		exit (1);
	}

	/* debug options */
	SetMAPIDumpData(opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(atoi(opt_debug));
	}

	/* if no profile is supplied use the default one */
	if (!opt_profname) {
		retval = GetDefaultProfile(&opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			printf("No profile specified and no default profile found\n");
			exit (1);
		}
	}

	retval = MapiLogonEx(&session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", retval);
		exit (1);
	}
	profile = session->profile;

	/* Open the default message store */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_inbox);
	mapi_object_init(&obj_table);
	mapi_object_init(&obj_fx_context);

	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", retval);
		exit (1);
	}

	/* Open Inbox */
	retval = GetReceiveFolder(&obj_store, &id_inbox, NULL);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetReceiveFolder", retval);
		exit (1);
	}

	retval = OpenFolder(&obj_store, id_inbox, &obj_inbox);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", retval);
		exit (1);
	}

	retval = GetContentsTable(&obj_inbox, &obj_table, 0, &count);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetContentsTable", retval);
		exit (1);
	}

	retval = FXCopyFolder(&obj_inbox, FastTransferCopyFolder_CopySubfolders, FastTransfer_Unicode, &obj_fx_context);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("FXCopyFolder", retval);
		exit (1);
	}

	parser = fxparser_init(mem_ctx);
	if (opt_dumpdata) {
		fxparser_set_marker_callback(parser, process_marker);
		fxparser_set_delprop_callback(parser, process_delprop);
		fxparser_set_namedprop_callback(parser, process_namedprop);
		fxparser_set_property_callback(parser, process_property);
	}

	do {
		retval = FXGetBuffer(&obj_fx_context, opt_maxsize, &fxTransferStatus, &progressCount, &totalStepCount, &transferdata);
		transfers++;
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("FXGetBuffer", retval);
			exit (1);
		}

		if (opt_showprogress) {
			printf("progress: (%d/%d)\n", progressCount, totalStepCount);
			printf("status: 0x%04x\n", fxTransferStatus);
		}

		fxparser_parse(parser, &transferdata);
	} while (fxTransferStatus == TransferStatus_Partial);

	printf("total transfers: %i\n", transfers);

	talloc_free(parser);

	mapi_object_release(&obj_fx_context);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_FID,
					  PR_MID);
	retval = SetColumns(&obj_table, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);
#if 0
	// Do some CopyTo or CopyFolder here.
	while ((retval = QueryRows(&obj_table, 0xa, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			retval = OpenMessage(&obj_store,
					     rowset.aRow[i].lpProps[0].value.d,
					     rowset.aRow[i].lpProps[1].value.d,
					     &obj_message, 0);
			printf("FID: 0x%016lx, MID: 0x%016lx\n", rowset.aRow[i].lpProps[0].value.d, rowset.aRow[i].lpProps[1].value.d);
			// TODO: do some CopyMessages here
			mapi_object_release(&obj_message);
		}
	}
#endif
	mapi_object_release(&obj_table);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);
	MAPIUninitialize();

	talloc_free(mem_ctx);

	return 0;
}
