/*
   MAPI Backup application suite
   Dump a Mailbox store in a database

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

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
#include <param.h>

#include "openchangebackup.h"
#include "utils/openchange-tools.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

/**
 * write attachment to the database
 */
static enum MAPISTATUS mapidump_write_attachment(struct ocb_context *ocb_ctx,
						 struct mapi_SPropValue_array *props,
						 const char *contentdn,
						 const char *uuid)
{
	int			ret;
	uint32_t		i;

	ret = ocb_record_init(ocb_ctx, OCB_OBJCLASS_ATTACHMENT, contentdn, uuid, props);
	if (ret == -1) return MAPI_E_SUCCESS;
	for (i = 0; i < props->cValues; i++) {
		ret = ocb_record_add_property(ocb_ctx, &props->lpProps[i]);
	}
	ret = ocb_record_commit(ocb_ctx);

	return MAPI_E_SUCCESS;
}

/**
 * write message to the database (email, appointment, contact, task, note etc.)
 */
static enum MAPISTATUS mapidump_write_message(struct ocb_context *ocb_ctx,
					      struct mapi_SPropValue_array *props,
					      const char *contentdn,
					      const char *uuid)
{
	int			ret;
	uint32_t		i;

	ret = ocb_record_init(ocb_ctx, OCB_OBJCLASS_MESSAGE, contentdn, uuid, props);
	if (ret == -1) return MAPI_E_SUCCESS;
	for (i = 0; i < props->cValues; i++) {
		ret = ocb_record_add_property(ocb_ctx, &props->lpProps[i]);
	}
	ret = ocb_record_commit(ocb_ctx);

	return MAPI_E_SUCCESS;
}

/**
 * write containers to the database (folders)
 */
static enum MAPISTATUS mapidump_write_container(struct ocb_context *ocb_ctx,
						struct mapi_SPropValue_array *props,
						const char *containerdn,
						const char *uuid)
{
	int			ret;
	uint32_t		i;

	ret = ocb_record_init(ocb_ctx, OCB_OBJCLASS_CONTAINER, containerdn, uuid, props);
	if (ret == -1) return MAPI_E_SUCCESS;
	for (i = 0; i < props->cValues; i++) {
		ret = ocb_record_add_property(ocb_ctx, &props->lpProps[i]);
	}
	ret = ocb_record_commit(ocb_ctx);

	return MAPI_E_SUCCESS;
}

/**
 * Retrieve all the attachments for a given message
 */
static enum MAPISTATUS mapidump_walk_attachment(TALLOC_CTX *mem_ctx,
						struct ocb_context *ocb_ctx,
						mapi_object_t *obj_message,
						const char *messagedn)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			rowset;
	struct mapi_SPropValue_array	props;
	mapi_object_t			obj_atable;
	mapi_object_t			obj_attach;
	uint32_t			Numerator = 0;
	uint32_t			Denominator = 0;
	uint32_t			i;
	const uint32_t			*attach_num;
	const struct SBinary_short	*sbin;
	char				*uuid;
	char				*contentdn;

	/* Get attachment table */
	mapi_object_init(&obj_atable);
	retval = GetAttachmentTable(obj_message, &obj_atable);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	/* Customize the table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x1, PR_ATTACH_NUM);
	retval = SetColumns(&obj_atable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	/* Walk through the table */
	retval = QueryPosition(&obj_atable, &Numerator, &Denominator);

	while ((retval = QueryRows(&obj_atable, Denominator, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			attach_num = (const uint32_t *)find_SPropValue_data(&(rowset.aRow[i]), PR_ATTACH_NUM);
			/* Open attachment */
			mapi_object_init(&obj_attach);
			retval = OpenAttach(obj_message, *attach_num, &obj_attach);
			if (retval == MAPI_E_SUCCESS) {
				retval = GetPropsAll(&obj_attach, MAPI_UNICODE, &props);
				if (retval == MAPI_E_SUCCESS) {
					/* extract unique identifier from PR_RECORD_KEY */
					sbin = (const struct SBinary_short *)find_mapi_SPropValue_data(&props, PR_RECORD_KEY);
					uuid = get_record_uuid(mem_ctx, sbin);
					contentdn = talloc_asprintf(mem_ctx, "cn=%s,%s", uuid, messagedn);
					mapidump_write_attachment(ocb_ctx, &props, contentdn, uuid);

					/* free allocated strings */
					talloc_free(uuid);
					talloc_free(contentdn);
				}
			}
			mapi_object_release(&obj_attach);
		}
	}

	mapi_object_release(&obj_atable);
	return MAPI_E_SUCCESS;
}

/**
 * Retrieve all the content within a folder
 */
static enum MAPISTATUS mapidump_walk_content(TALLOC_CTX *mem_ctx,
					     struct ocb_context *ocb_ctx,
					     mapi_object_t *obj_folder,
					     const char *containerdn)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct mapi_SPropValue_array	props;
	struct SRowSet			rowset;
	mapi_object_t			obj_ctable;
	mapi_object_t			obj_message;
	uint32_t			count = 0;
	uint32_t			i;
	const mapi_id_t			*fid;
	const mapi_id_t			*mid;
	char				*uuid;
	const struct SBinary_short     	*sbin;
	const uint8_t			*has_attach;
	char				*contentdn;

	/* Get Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(obj_folder, &obj_ctable, 0, &count);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	/* Customize the table view */
	SPropTagArray = set_SPropTagArray(mem_ctx, 0x5,
					  PR_FID,
					  PR_MID,
					  PR_INST_ID,
					  PR_INSTANCE_NUM,
					  PR_SUBJECT);
	retval = SetColumns(&obj_ctable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	while ((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			fid = (const uint64_t *) get_SPropValue_SRow_data(&rowset.aRow[i], PR_FID);
			mid = (const uint64_t *) get_SPropValue_SRow_data(&rowset.aRow[i], PR_MID);
			/* Open Message */
			retval = OpenMessage(obj_folder, *fid, *mid, &obj_message, 0);
			if (GetLastError() == MAPI_E_SUCCESS) {
				retval = GetPropsAll(&obj_message, MAPI_UNICODE, &props);
				if (GetLastError() == MAPI_E_SUCCESS) {
					/* extract unique identifier from PR_SOURCE_KEY */
					sbin = (const struct SBinary_short *)find_mapi_SPropValue_data(&props, PR_SOURCE_KEY);
					uuid = get_MAPI_uuid(mem_ctx, sbin);
					contentdn = talloc_asprintf(mem_ctx, "cn=%s,%s", uuid, containerdn);
					mapidump_write_message(ocb_ctx, &props, contentdn, uuid);

					/* If Message has attachments then process them */
					has_attach = (const uint8_t *)find_mapi_SPropValue_data(&props, PR_HASATTACH);
					if (has_attach && *has_attach) {
						mapidump_walk_attachment(mem_ctx, ocb_ctx, &obj_message, contentdn);
					}

					/* free allocated strings */
					talloc_free(uuid);
					talloc_free(contentdn);
				}
			}
			mapi_object_release(&obj_message);
		}
	}

	mapi_object_release(&obj_ctable);
	
	return MAPI_E_SUCCESS;
}


/**
 * Recursively retrieve folders
 */
static enum MAPISTATUS mapidump_walk_container(TALLOC_CTX *mem_ctx,
					       struct ocb_context *ocb_ctx,
					       mapi_object_t *obj_parent,
					       mapi_id_t folder_id,
					       char *parentdn,
					       int count)
{
	enum MAPISTATUS			retval;
	struct SPropTagArray		*SPropTagArray;
	struct SRowSet			rowset;
	struct mapi_SPropValue_array	props;
	mapi_object_t			obj_folder;
	mapi_object_t			obj_htable;
	const uint32_t			*child_content;
	const uint32_t			*child_folder;
	char				*containerdn;
	char				*uuid;
	const mapi_id_t			*fid;
	uint32_t			rcount;
	uint32_t			i;
	const struct SBinary_short	*sbin;

	/* Open folder */
	mapi_object_init(&obj_folder);
	retval = OpenFolder(obj_parent, folder_id, &obj_folder);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	/* Retrieve all its properties */
	retval = GetPropsAll(&obj_folder, MAPI_UNICODE, &props);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);
	child_content = (const uint32_t *)find_mapi_SPropValue_data(&props, PR_CONTENT_COUNT);
	child_folder = (const uint32_t *)find_mapi_SPropValue_data(&props, PR_FOLDER_CHILD_COUNT);

	/* extract unique identifier from PR_SOURCE_KEY */
	sbin = (const struct SBinary_short *)find_mapi_SPropValue_data(&props, PR_SOURCE_KEY);
	uuid = get_MAPI_uuid(mem_ctx, sbin);

	if (parentdn == NULL && count == 0) {
		parentdn = talloc_asprintf(mem_ctx, "cn=%s", 
					   get_MAPI_store_guid(mem_ctx, sbin));
	}

	containerdn = talloc_asprintf(mem_ctx, "cn=%s,%s", uuid, parentdn);

	/* Write entry for container */
	mapidump_write_container(ocb_ctx, &props, containerdn, uuid);
	talloc_free(uuid);

	/* Get Contents Table if PR_CONTENT_COUNT >= 1 */
	if (child_content && *child_content >= 1) {
		retval = mapidump_walk_content(mem_ctx, ocb_ctx, &obj_folder, containerdn);
	}

	/* Get Container Table if PR_FOLDER_CHILD_COUNT >= 1 */

	if (child_folder && *child_folder >= 1) {
		mapi_object_init(&obj_htable);
		retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, &rcount);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
						  PR_FID,
						  PR_CONTENT_COUNT,
						  PR_FOLDER_CHILD_COUNT);
		retval = SetColumns(&obj_htable, SPropTagArray);
		MAPIFreeBuffer(SPropTagArray);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		while ((retval = QueryRows(&obj_htable, rcount, TBL_ADVANCE, TBL_FORWARD_READ, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
			for (i = 0; i < rowset.cRows; i++) {
				fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[i], PR_FID);
				retval = mapidump_walk_container(mem_ctx, ocb_ctx, &obj_folder, *fid, containerdn, count + 1);
			}
		}
	} 

	talloc_free(containerdn);
	mapi_object_release(&obj_folder);

	return MAPI_E_SUCCESS;
}

/**
 * Walk through known mapi folders
 */

static enum MAPISTATUS mapidump_walk(TALLOC_CTX *mem_ctx,
					       struct ocb_context *ocb_ctx,
					       mapi_object_t *obj_store)
{
	enum MAPISTATUS			retval;
	mapi_id_t			id_mailbox;

	retval = GetDefaultFolder(obj_store, &id_mailbox, 
				  olFolderTopInformationStore);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	return mapidump_walk_container(mem_ctx, ocb_ctx, obj_store, id_mailbox, NULL, 0);
}


int main(int argc, const char *argv[])
{
	TALLOC_CTX			*mem_ctx;
	enum MAPISTATUS			retval;
	struct ocb_context		*ocb_ctx = NULL;
	struct mapi_context		*mapi_ctx;
	struct mapi_session		*session = NULL;
	mapi_object_t			obj_store;
	poptContext			pc;
	int				opt;
	/* command line options */
	const char			*opt_profdb = NULL;
	char				*opt_profname = NULL;
	const char			*opt_password = NULL;
	const char			*opt_backupdb = NULL;
	const char			*opt_debug = NULL;
	bool				opt_dumpdata = false;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, 
	      OPT_MAILBOX, OPT_CONFIG, OPT_BACKUPDB, OPT_PF,
	      OPT_DEBUG, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", NULL},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", NULL},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", NULL},
		{"backup-db", 'b', POPT_ARG_STRING, NULL, OPT_BACKUPDB, "set the openchangebackup store path", NULL},
		{"debuglevel", 0, POPT_ARG_STRING, NULL, OPT_DEBUG, "set the debug level", NULL},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the hex data", NULL},
		POPT_OPENCHANGE_VERSION
		{ NULL, 0, 0, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "openchangemapidump");

	pc = poptGetContext("openchangemapidump", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt)  {
		case OPT_DEBUG:
			opt_debug = poptGetOptArg(pc);
			break;
		case OPT_DUMPDATA:
			opt_dumpdata = true;
			break;
		case OPT_PROFILE_DB:
			opt_profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			opt_profname = talloc_strdup(mem_ctx, (char *)poptGetOptArg(pc));
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_BACKUPDB:
			opt_backupdb = poptGetOptArg(pc);
			break;
		}
	}

	/* Sanity check on options */
	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	/* Initialize MAPI subsystem */
	retval = MAPIInitialize(&mapi_ctx, opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	/* debug options */
	SetMAPIDumpData(mapi_ctx, opt_dumpdata);

	if (opt_debug) {
		SetMAPIDebugLevel(mapi_ctx, atoi(opt_debug));
	}

	/* If no profile is specified try to load the default one from
	 * the database 
	 */
	if (!opt_profname) {
		retval = GetDefaultProfile(mapi_ctx, &opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	if (!opt_backupdb) {
		opt_backupdb = talloc_asprintf(mem_ctx, DEFAULT_OCBDB, 
					       getenv("HOME"),
					       opt_profname);
	}

	/* Initialize OpenChange Backup subsystem */
	if (!(ocb_ctx = ocb_init(mem_ctx, opt_backupdb))) {
		talloc_free(mem_ctx);
		exit(-1);
	}

	/* We only need to log on EMSMDB to backup Mailbox store or Public Folders */
	retval = MapiLogonProvider(mapi_ctx, &session, opt_profname, opt_password, PROVIDER_ID_EMSMDB);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	/* Open default message store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		exit (1);
	}

	retval = mapidump_walk(mem_ctx, ocb_ctx, &obj_store);

	/* Uninitialize MAPI and OCB subsystem */
	mapi_object_release(&obj_store);
	MAPIUninitialize(mapi_ctx);
	ocb_release(ocb_ctx);
	talloc_free(mem_ctx);

	return 0;
}
