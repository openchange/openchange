/*
   Test asynchronous notifications

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

#include <popt.h>
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

static int callback(uint16_t NotificationType, void *NotificationData, void *private_data)
{
	struct HierarchyTableChange    	*htable;
	struct ContentsTableChange     	*ctable;
	struct ContentsTableChange     	*stable;

	switch(NotificationType) {
	case fnevNewMail:
	case fnevNewMail|fnevMbit:
		OC_DEBUG(0, "[+] New mail Received");
		break;
	case fnevObjectCreated:
		OC_DEBUG(0, "[+] Folder Created");
		break;
	case fnevObjectDeleted:
		OC_DEBUG(0, "[+] Folder Deleted");
		break;
	case fnevObjectModified:
	case fnevTbit|fnevObjectModified:
	case fnevUbit|fnevObjectModified:
	case fnevTbit|fnevUbit|fnevObjectModified:
		OC_DEBUG(0, "[+] Folder Modified");
		break;
	case fnevObjectMoved:
		OC_DEBUG(0, "[+] Folder Moved");
		break;
	case fnevObjectCopied:
		OC_DEBUG(0, "[+] Folder Copied");
		break;
	case fnevSearchComplete:
		OC_DEBUG(0, "[+] Search complete in search folder");
		break;
	case fnevTableModified:
		htable = (struct HierarchyTableChange *) NotificationData;
		switch (htable->TableEvent) {
		case TABLE_CHANGED:
			OC_DEBUG(0, "[+] Hierarchy Table:  changed");
			break;
		case TABLE_ROW_ADDED:
			OC_DEBUG(0, "[+] Hierarchy Table: row added");
			break;
		case TABLE_ROW_DELETED:
			OC_DEBUG(0, "[+] Hierarchy Table: row deleted");
			break;
		case TABLE_ROW_MODIFIED:
			OC_DEBUG(0, "[+] Hierarchy Table: row modified");
			break;
		case TABLE_RESTRICT_DONE:
			OC_DEBUG(0, "[+] Hierarchy Table: restriction done");
			break;
		default:
			OC_DEBUG(0, "[+] Hierarchy Table: ");
			break;
		}
		break;
	case fnevStatusObjectModified:
		OC_DEBUG(0, "[+] ICS Notification");
		break;
	case fnevMbit|fnevObjectCreated:
		OC_DEBUG(0, "[+] Message created");
		break;
	case fnevMbit|fnevObjectDeleted:
		OC_DEBUG(0, "[+] Message deleted");
	case fnevMbit|fnevObjectModified:
		OC_DEBUG(0, "[+] Message modified");
	case fnevMbit|fnevObjectMoved:
		OC_DEBUG(0, "[+] Message moved");
	case fnevMbit|fnevObjectCopied:
		OC_DEBUG(0, "[+] Message copied");
	case fnevMbit|fnevTableModified:
		ctable = (struct ContentsTableChange *) NotificationData;
		switch (ctable->TableEvent) {
		case TABLE_CHANGED:
			OC_DEBUG(0, "[+] Contents Table:  changed");
			break;
		case TABLE_ROW_ADDED:
			OC_DEBUG(0, "[+] Contents Table: row added");
			break;
		case TABLE_ROW_DELETED:
			OC_DEBUG(0, "[+] Contents Table: row deleted");
			break;
		case TABLE_ROW_MODIFIED:
			OC_DEBUG(0, "[+] Contents Table: row modified");
			break;
		case TABLE_RESTRICT_DONE:
			OC_DEBUG(0, "[+] Contents Table: restriction done");
			break;
		default:
			OC_DEBUG(0, "[+] Contents Table: ");
			break;
		}
		break;
	case fnevMbit|fnevSbit|fnevObjectDeleted:
		OC_DEBUG(0, "[+] A message is no longer part of a search folder");
		break;
	case fnevMbit|fnevSbit|fnevObjectModified:
		OC_DEBUG(0, "[+] A property on a message in a search folder has changed");
	case fnevMbit|fnevSbit|fnevTableModified:
		stable = (struct ContentsTableChange *) NotificationData;
		switch (stable->TableEvent) {
		case TABLE_CHANGED:
			OC_DEBUG(0, "[+] Search Table:  changed");
			break;
		case TABLE_ROW_ADDED:
			OC_DEBUG(0, "[+] Search Table: row added");
			break;
		case TABLE_ROW_DELETED:
			OC_DEBUG(0, "[+] Search Table: row deleted");
			break;
		case TABLE_ROW_MODIFIED:
			OC_DEBUG(0, "[+] Search Table: row modified");
			break;
		case TABLE_RESTRICT_DONE:
			OC_DEBUG(0, "[+] Search Table: restriction done");
			break;
		default:
			OC_DEBUG(0, "[+] Search Table: ");
			break;
		}
		break;
	default:
		OC_DEBUG(0, "[+] Unsupported notification (0x%x)", NotificationType);
		break;
	}

	return 0;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct mapi_session		*session = NULL;
	struct mapi_context		*mapi_ctx;
	mapi_object_t			obj_store;
	mapi_object_t			obj_inbox;
	mapi_object_t			obj_contentstable;
	uint32_t			count;
	mapi_id_t			fid;
	poptContext			pc;
	int				opt;
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;
	bool				opt_dumpdata = false;
	const char			*opt_debug = NULL;
	int				exit_code = 0;
	uint32_t			notificationFlag = 0;
	uint32_t			ulConnection;
	uint16_t			ulEventMask = fnevNewMail;
	//uint16_t			ulEventMask = fnevNewMail|fnevObjectCreated|fnevObjectDeleted|fnevObjectModified|fnevObjectMoved|fnevObjectCopied|fnevSearchComplete;
	bool				wholeStore = true;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_DEBUG, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
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
			exit_code = 1;
			goto cleanup;
		}
	}

	retval = MapiLogonEx(mapi_ctx, &session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", retval);
		exit_code = 1;
		goto cleanup;
	}

	/* Open the default message store */
	mapi_object_init(&obj_store);

	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", retval);
		exit_code = 1;
		goto cleanup;
	}

	retval = GetReceiveFolder(&obj_store, &fid, NULL);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(&obj_inbox);
	retval = OpenFolder(&obj_store, fid, &obj_inbox);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_init(&obj_contentstable);
	retval = GetContentsTable(&obj_inbox, &obj_contentstable, 0, &count);
	printf("mailbox contains %i messages\n", count);

	retval = Subscribe(&obj_store, &ulConnection, ulEventMask, wholeStore, &callback, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("Subscribe", retval);
		exit_code = 2;
		goto cleanup;
	}

	printf("about to start a long wait\n");
	while ((retval = RegisterAsyncNotification(session, &notificationFlag)) == MAPI_E_SUCCESS ||
	       retval == MAPI_E_TIMEOUT) {
		if (retval == MAPI_E_SUCCESS && notificationFlag != 0x00000000) {
			printf("Got a Notification: 0x%08x, woo hoo!\n", notificationFlag);
			mapi_object_release(&obj_contentstable);
			mapi_object_init(&obj_contentstable);
			retval = GetContentsTable(&obj_inbox, &obj_contentstable, 0, &count);
			printf("\tNew inbox count is %i\n", count);
		} else {
			printf("going around again, ^C to break out\n");
		}
	}
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("RegisterAsyncNotification", retval);
		exit_code = 2;
		goto cleanup;
	}

cleanup:
	mapi_object_release(&obj_contentstable);
	mapi_object_release(&obj_inbox);
	mapi_object_release(&obj_store);
	MAPIUninitialize(mapi_ctx);

	talloc_free(mem_ctx);

	exit(exit_code);
}
