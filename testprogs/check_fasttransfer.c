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
#include "mapiproxy/libmapistore/mapistore.h"
#include "mapiproxy/libmapistore/mapistore_errors.h"
#include "utils/dlinklist.h"

#include <popt.h>
#include <ldb.h>
#include <talloc.h>
#include <inttypes.h>

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

/* These callbacks are for deserialising the fast transfer stream to a mapistore instance */
struct parent_fid {
        struct parent_fid	*prev;
        struct parent_fid	*next;
        uint64_t		fid;
};

struct mapistore_output_ctx {
	struct mapistore_context	*mstore_ctx;
	uint32_t			mapistore_context_id;
	uint64_t			root_fid;
	void				*root_folder;
	struct parent_fid		*parent_fids; /* stack */
	uint64_t			current_id;
	uint8_t				current_output_type;
	struct SRow			*proplist; /* the properties on the "current" object */
};

static enum MAPISTATUS mapistore_marker(uint32_t marker, void *priv)
{
	struct mapistore_output_ctx	*mapistore = priv;
	/* TALLOC_CTX			*mem_ctx; */
	/* void				*message; */

	if (mapistore->proplist) {
		struct parent_fid *it;
		printf("parent_fids: ");
		for (it = mapistore->parent_fids; it->next; it = it->next) {
			printf("0x%016"PRIx64",", it->fid);
		}
		printf("\n");
		if (mapistore->current_id == mapistore->root_fid) {
			/* This is the top level folder */
			abort();
			/* obsolete code: */
			/* mapistore_setprops(mapistore->mstore_ctx, mapistore->mapistore_context_id, */
			/* 		   mapistore->root_fid, mapistore->current_output_type, */
			/* 		   mapistore->proplist); */
		} else if (mapistore->current_output_type == MAPISTORE_FOLDER) {
			abort();
			/* obsolete code: */
                        /* struct parent_fid *element = talloc_zero(mapistore->mstore_ctx, struct parent_fid); */
			/* mapistore_folder_create_folder(mapistore->mstore_ctx, mapistore->mapistore_context_id, */
			/* 			       mapistore->parent_fids->fid, mapistore->current_id, */
			/* 			       mapistore->proplist); */
                        /* element->fid = mapistore->current_id; */
			/* DLIST_ADD(mapistore->parent_fids, element); */
		} else {
			abort();
			/* obsolete code: */
			/* mem_ctx = talloc_zero(NULL, TALLOC_CTX); */
			/* mapistore_folder_create_message(mapistore->mstore_ctx, mapistore->mapistore_context_id, */
			/* 				mem_ctx, mapistore->parent_fids->fid, mapistore->current_id, false, */
			/* 				&message); */
			/* mapistore_properties_set_properties(mapistore->mstore_ctx, mapistore->mapistore_context_id, */
			/* 				    message, mapistore->proplist); */
			/* mapistore_message_save(mapistore->mstore_ctx, mapistore->mapistore_context_id, */
			/* 		       message); */
			/* talloc_free(mem_ctx); */
		}
		talloc_free(mapistore->proplist);
		mapistore->proplist = 0;
		(mapistore->current_id)++;
	}

	switch (marker) {
	case StartTopFld:
	{
		/* start collecting properties */
		struct SPropValue one_prop;
		struct parent_fid *element = talloc_zero(mapistore->mstore_ctx, struct parent_fid);
		mapistore->proplist = talloc_zero(mapistore->mstore_ctx, struct SRow);
		one_prop.ulPropTag = PR_FID;
		one_prop.dwAlignPad = 0;
		one_prop.value.d = mapistore->root_fid;
		SRow_addprop(mapistore->proplist, one_prop);
		one_prop.ulPropTag = PR_FOLDER_TYPE;
		one_prop.value.l = MAPISTORE_FOLDER;
		SRow_addprop(mapistore->proplist, one_prop);
		mapistore->current_id = mapistore->root_fid;
		mapistore->parent_fids = talloc_zero(mapistore->mstore_ctx, struct parent_fid);
		element->fid = mapistore->current_id;
		DLIST_ADD(mapistore->parent_fids, element);
		mapistore->current_output_type = MAPISTORE_FOLDER;
		break;
	}
	case StartSubFld:
		mapistore->proplist = talloc_zero(mapistore->mstore_ctx, struct SRow);
		mapistore->current_output_type = MAPISTORE_FOLDER;
		break;
	case StartMessage:
		mapistore->proplist = talloc_zero(mapistore->mstore_ctx, struct SRow);
		mapistore->current_output_type = MAPISTORE_MESSAGE;
		break;
	case StartFAIMsg:
	case StartRecip:
	case StartEmbed:
	case NewAttach:
		break;
	case EndFolder:
		DLIST_REMOVE(mapistore->parent_fids, mapistore->parent_fids);
		break;
	case EndMessage:
	case EndToRecip:
	case EndAttach:
	case EndEmbed:
		break;
	default:
		printf("***unhandled *** TODO: Marker: %s (0x%08x)\n", get_proptag_name(marker), marker);
	}
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS mapistore_delprop(uint32_t proptag, void *priv)
{
	printf("TODO: DelProps: 0x%08x (%s)\n", proptag, get_proptag_name(proptag));
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS mapistore_namedprop(uint32_t proptag, struct MAPINAMEID nameid, void *priv)
{
	TALLOC_CTX *mem_ctx;
	// struct mapistore_output_ctx *mapistore = priv;
	mem_ctx = talloc_init("process_namedprop");
	printf("TODO: Named Property: 0x%08x has GUID %s and ", proptag, GUID_string(mem_ctx, &(nameid.lpguid)));
	if (nameid.ulKind == MNID_ID) {
		printf("dispId 0x%08x\n", nameid.kind.lid);
	} else if (nameid.ulKind == MNID_STRING) {
		printf("name %s\n", nameid.kind.lpwstr.Name);
	}
	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS mapistore_property(struct SPropValue prop, void *priv)
{
	struct mapistore_output_ctx *mapistore = priv;
	SRow_addprop(mapistore->proplist, prop);
	return MAPI_E_SUCCESS;
}

/* These callbacks are for the "dump-data" mode, so they don't do much */

static enum MAPISTATUS dump_marker(uint32_t marker, void *priv)
{
	printf("Marker: %s (0x%08x)\n", get_proptag_name(marker), marker);
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS dump_delprop(uint32_t proptag, void *priv)
{
	printf("DelProps: 0x%08x (%s)\n", proptag, get_proptag_name(proptag));
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS dump_namedprop(uint32_t proptag, struct MAPINAMEID nameid, void *priv)
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

static enum MAPISTATUS dump_property(struct SPropValue prop, void *priv)
{
	mapidump_SPropValue(prop, "\t");
	return MAPI_E_SUCCESS;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	enum mapistore_error		mretval;
	struct mapi_context		*mapi_ctx;
	struct mapi_session		*session = NULL;
	mapi_object_t			obj_store;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_fx_context;
	mapi_id_t			id_folder;

	uint16_t			progressCount;
	uint16_t			totalStepCount;
	int				transfers = 0;
	enum TransferStatus		fxTransferStatus;
	DATA_BLOB			transferdata;
	struct fx_parser_context	*parser;
	struct loadparm_context		*lp_ctx;
	struct mapistore_output_ctx	output_ctx;
	poptContext			pc;
	int				opt;
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;
	uint32_t			opt_maxsize = 0;
	const char			*opt_mapistore = NULL;
	bool				opt_showprogress = false;
	bool				opt_dumpdata = false;
	const char			*opt_debug = NULL;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_MAXDATA, OPT_SHOWPROGRESS, OPT_MAPISTORE, OPT_DEBUG, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{"maxdata", 0, POPT_ARG_INT, NULL, OPT_MAXDATA, "the maximum transfer data size", "SIZE"},
		{"showprogress", 0, POPT_ARG_NONE, NULL, OPT_SHOWPROGRESS, "enable progress display", NULL},
		{"mapistore", 0, POPT_ARG_STRING, NULL, OPT_MAPISTORE, "serialise to mapistore", "FILESYSTEM_PATH"},
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
		case OPT_MAPISTORE:
			opt_mapistore = poptGetOptArg(pc);
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

	retval = MAPIInitialize(&mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		exit (1);
	}

	/* debug options */
	SetMAPIDumpData(mapi_ctx, opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(mapi_ctx, atoi(opt_debug));
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

	/* Open the default message store */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	/* mapi_object_init(&obj_table); */
	mapi_object_init(&obj_fx_context);

	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", retval);
		exit (1);
	}

	/* Open the top level folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("GetReceiveFolder", retval);
		exit (1);
	}

	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", retval);
		exit (1);
	}

	retval = FXCopyFolder(&obj_folder, FastTransferCopyFolder_CopySubfolders, FastTransfer_Unicode, &obj_fx_context);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("FXCopyFolder", retval);
		exit (1);
	}

	if (opt_mapistore) {
		char *root_folder;

		// TODO: check the path is valid / exists / can be opened, etc.
		// TODO: maybe allow a URI instead of path.
		output_ctx.root_fid = 0x0000000000010001;
		output_ctx.current_id = output_ctx.root_fid;
		root_folder = talloc_asprintf(mem_ctx, "fsocpf://%s/0x%016"PRIx64, opt_mapistore, output_ctx.root_fid);
		parser = fxparser_init(mem_ctx, &output_ctx);
		mretval = mapistore_set_mapping_path(opt_mapistore);
		if (mretval != MAPISTORE_SUCCESS) {
			mapi_errstr("mapistore_set_mapping_path", retval);
			exit (1);
		}

		/* Initialize configuration */
		lp_ctx = loadparm_init(mem_ctx);
		lpcfg_load_default(lp_ctx);

		output_ctx.mstore_ctx = mapistore_init(mem_ctx, lp_ctx, NULL);
		if (!(output_ctx.mstore_ctx)) {
			mapi_errstr("mapistore_init", retval);
			exit (1);
		}

		mretval = mapistore_add_context(output_ctx.mstore_ctx, "openchange", root_folder, output_ctx.root_fid, &(output_ctx.mapistore_context_id), &output_ctx.root_folder);
		if (mretval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("%s\n", mapistore_errstr(mretval)));
			exit (1);
		}
		
		output_ctx.proplist = 0;

		fxparser_set_marker_callback(parser, mapistore_marker);
		fxparser_set_delprop_callback(parser, mapistore_delprop);
		fxparser_set_namedprop_callback(parser, mapistore_namedprop);
		fxparser_set_property_callback(parser, mapistore_property);
	} else if (opt_dumpdata) {
		parser = fxparser_init(mem_ctx, NULL);
		fxparser_set_marker_callback(parser, dump_marker);
		fxparser_set_delprop_callback(parser, dump_delprop);
		fxparser_set_namedprop_callback(parser, dump_namedprop);
		fxparser_set_property_callback(parser, dump_property);
	} else {
		parser = fxparser_init(mem_ctx, NULL);
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
	} while ((fxTransferStatus == TransferStatus_Partial) || (fxTransferStatus == TransferStatus_NoRoom));

	printf("total transfers: %i\n", transfers);

	if (opt_mapistore) {
		mretval = mapistore_del_context(output_ctx.mstore_ctx, output_ctx.mapistore_context_id);
		if (mretval != MAPISTORE_SUCCESS) {
			printf("mapistore_del_context: %s\n", mapistore_errstr(mretval));
			exit (1);
		}

		mretval = mapistore_release(output_ctx.mstore_ctx);
		if (mretval != MAPISTORE_SUCCESS) {
			printf("mapistore_release: %s\n", mapistore_errstr(mretval));
			exit (1);
		}
	}

	talloc_free(parser);

	mapi_object_release(&obj_fx_context);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);
	MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

	return 0;
}
