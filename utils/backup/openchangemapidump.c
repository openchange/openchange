/*
   MAPI Backup application suite
   Dump a Mailbox store in a database

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <libmapi/libmapi.h>
#include <samba/popt.h>
#include <param.h>
#include <param/proto.h>

#include "openchangebackup.h"

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

/**
 * write attachment to the database
 */
static enum MAPISTATUS mapidump_write_attachment(TALLOC_CTX *mem_ctx,
						 struct ocb_context *ocb_ctx,
						 struct mapi_SPropValue_array *props,
						 const char *contentdn,
						 const char *uuid)
{
	uint32_t		ret;
	uint32_t		i;

	ret = ocb_record_init(ocb_ctx, contentdn, uuid, props);
	if (ret == -1) return MAPI_E_SUCCESS;
	for (i = 0; i < props->cValues; i++) {
		ret = ocb_record_add_property(ocb_ctx, &props->lpProps[i]);
	}
	ret = ocb_record_commit(ocb_ctx);

	return MAPI_E_SUCCESS;
}

/**
 * write contents to the database (message, appointment, contact, task, note etc.)
 */
static enum MAPISTATUS mapidump_write_content(TALLOC_CTX *mem_ctx,
					      struct ocb_context *ocb_ctx,
					      struct mapi_SPropValue_array *props,
					      const char *contentdn,
					      const char *uuid)
{
	uint32_t		ret;
	uint32_t		i;

	ret = ocb_record_init(ocb_ctx, contentdn, uuid, props);
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
static enum MAPISTATUS mapidump_write_container(TALLOC_CTX *mem_ctx,
						struct ocb_context *ocb_ctx,
						struct mapi_SPropValue_array *props,
						const char *containerdn,
						const char *uuid)
{
	uint32_t		ret;
	uint32_t		i;

	ret = ocb_record_init(ocb_ctx, containerdn, uuid, props);
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
	uint32_t			count = 0;
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
	retval = GetRowCount(&obj_atable, &count);

	while ((retval = QueryRows(&obj_atable, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			attach_num = (const uint32_t *)find_SPropValue_data(&(rowset.aRow[i]), PR_ATTACH_NUM);
			/* Open attachment */
			mapi_object_init(&obj_attach);
			retval = OpenAttach(obj_message, *attach_num, &obj_attach);
			if (retval == MAPI_E_SUCCESS) {
				retval = GetPropsAll(&obj_attach, &props);
				if (retval == MAPI_E_SUCCESS) {
					/* extract unique identifier from PR_RECORD_KEY */
					sbin = (const struct SBinary_short *)find_mapi_SPropValue_data(&props, PR_RECORD_KEY);
					uuid = get_record_uuid(mem_ctx, sbin);
					contentdn = talloc_asprintf(mem_ctx, "cn=%s,%s", uuid, messagedn);
					mapidump_write_attachment(mem_ctx, ocb_ctx, &props, contentdn, uuid);

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
	retval = GetContentsTable(obj_folder, &obj_ctable);
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

	/* Walk through the table */
	retval = GetRowCount(&obj_ctable, &count);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	while ((retval = QueryRows(&obj_ctable, count, TBL_ADVANCE, &rowset)) != MAPI_E_NOT_FOUND && rowset.cRows) {
		for (i = 0; i < rowset.cRows; i++) {
			mapi_object_init(&obj_message);
			fid = (const uint64_t *) get_SPropValue_SRow_data(&rowset.aRow[i], PR_FID);
			mid = (const uint64_t *) get_SPropValue_SRow_data(&rowset.aRow[i], PR_MID);
			/* Open Message */
			retval = OpenMessage(obj_folder, *fid, *mid, &obj_message);
			if (GetLastError() == MAPI_E_SUCCESS) {
				retval = GetPropsAll(&obj_message, &props);
				if (GetLastError() == MAPI_E_SUCCESS) {
					/* extract unique identifier from PR_SOURCE_KEY */
					sbin = (const struct SBinary_short *)find_mapi_SPropValue_data(&props, PR_SOURCE_KEY);
					uuid = get_MAPI_uuid(mem_ctx, sbin);
					contentdn = talloc_asprintf(mem_ctx, "cn=%s,%s", uuid, containerdn);
					mapidump_write_content(mem_ctx, ocb_ctx, &props, contentdn, uuid);

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
	retval = GetPropsAll(&obj_folder, &props);
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
	mapidump_write_container(mem_ctx, ocb_ctx, &props, containerdn, uuid);
	talloc_free(uuid);

	/* Get Contents Table if PR_CONTENT_COUNT >= 1 */
	if (child_content && *child_content >= 1) {
		retval = mapidump_walk_content(mem_ctx, ocb_ctx, &obj_folder, containerdn);
	}

	/* Get Container Table if PR_FOLDER_CHILD_COUNT >= 1 */

	if (child_folder && *child_folder >= 1) {
		mapi_object_init(&obj_htable);
		retval = GetHierarchyTable(&obj_folder, &obj_htable);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
						  PR_FID,
						  PR_CONTENT_COUNT,
						  PR_FOLDER_CHILD_COUNT);
		retval = SetColumns(&obj_htable, SPropTagArray);
		MAPIFreeBuffer(SPropTagArray);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		retval = GetRowCount(&obj_htable, &rcount);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);

		while ((retval = QueryRows(&obj_htable, rcount, TBL_ADVANCE, &rowset) != MAPI_E_NOT_FOUND) && rowset.cRows) {
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
	struct mapi_session		*session = NULL;
	mapi_object_t			obj_store;
	poptContext			pc;
	int				opt;
	/* command line options */
	const char			*opt_profdb = NULL;
	const char			*opt_profname = NULL;
	const char			*opt_password = NULL;
	const char			*opt_config = NULL;
	const char			*opt_backupdb = NULL;
	const char			*opt_debug = NULL;
	bool				opt_dumpdata = false;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, 
	      OPT_MAILBOX, OPT_CONFIG, OPT_BACKUPDB, OPT_PF,
	      OPT_DEBUG, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password"},
		{"config", 'c', POPT_ARG_STRING, NULL, OPT_CONFIG, "set openchangebackup configuration file path"},
		{"backup-db", 'b', POPT_ARG_STRING, NULL, OPT_BACKUPDB, "set the openchangebackup store path"},
		{"debuglevel", 0, POPT_ARG_STRING, NULL, OPT_DEBUG, "set the debug level"},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the hex data"},
		{ NULL }
	};

	mem_ctx = talloc_init("openchangemapidump");

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
			opt_profname = poptGetOptArg(pc);
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_CONFIG:
			opt_config = poptGetOptArg(pc);
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
	retval = MAPIInitialize(opt_profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	/* debug options */
	if (opt_debug) {
		lp_set_cmdline("log level", opt_debug);
	}

	if (opt_dumpdata == true) {
		global_mapi_ctx->dumpdata = true;
	}

	/* If no profile is specified try to load the default one from
	 * the database 
	 */
	if (!opt_profname) {
		retval = GetDefaultProfile(&opt_profname, 0);
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
	retval = MapiLogonProvider(&session, opt_profname, opt_password, PROVIDER_ID_EMSMDB);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	/* Open default message store */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenMsgStore", GetLastError());
		exit (1);
	}

	retval = mapidump_walk(mem_ctx, ocb_ctx, &obj_store);

	/* Uninitialize MAPI and OCB subsystem */
	mapi_object_release(&obj_store);
	MAPIUninitialize();
	ocb_release(ocb_ctx);
	talloc_free(mem_ctx);

	return 0;
}
