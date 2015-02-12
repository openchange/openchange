/*
   Public Folders Administration Tool

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
#include "libmapiadmin/libmapiadmin.h"
#include <popt.h>
#include <param.h>

#include "openchangepfadmin.h"
#include "openchange-tools.h"

static int32_t get_aclrights(const char *permission)
{
	uint32_t	i;

	if (!permission) return -1;

	for (i = 0; aclrights[i].name != NULL; i++) {
		if (!strcmp(permission, aclrights[i].name)) {
			return aclrights[i].value;
		}
	}
	return -1;
}

static void list_aclrights(void)
{
	uint32_t	i;

	printf("Valid permissions:\n");
	for (i = 0; aclrights[i].name != NULL; i++) {
		printf("\t%s\n", aclrights[i].name);
	}
}

static int32_t check_IPF_class(const char *dirclass)
{
	uint32_t	i;

	if (!dirclass) return -1;

	for (i = 0; IPF_class[i]; i++) {
		if (!strcmp(dirclass, IPF_class[i])) {
			return 0;
		}
	}

	return -1;
}

static void list_IPF_class(void)
{
	uint32_t	i;

	printf("Valid IPF Classes:\n");
	for (i = 0; IPF_class[i] != NULL; i++) {
		printf("\t%s\n", IPF_class[i]);
	}
}

static bool get_child_folders_pf(TALLOC_CTX *mem_ctx, mapi_object_t *parent, mapi_id_t folder_id, int count)
{
	enum MAPISTATUS		retval;
	bool			ret;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	const char	       	*name;
	const uint32_t		*child;
	uint32_t		index;
	const uint64_t		*fid;
	int			i;

	mapi_object_init(&obj_folder);
	retval = OpenFolder(parent, folder_id, &obj_folder);
	if (retval != MAPI_E_SUCCESS) return false;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, NULL);
	if (retval != MAPI_E_SUCCESS) return false;

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x3,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID,
					  PR_FOLDER_CHILD_COUNT);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return false;
	
	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME_UNICODE);
			child = (const uint32_t *)find_SPropValue_data(&rowset.aRow[index], PR_FOLDER_CHILD_COUNT);

			for (i = 0; i < count; i++) {
				printf("|   ");
			}
			printf("|---+ %-15s\n", name);
			if (*child) {
				ret = get_child_folders_pf(mem_ctx, &obj_folder, *fid, count + 1);
				if (ret == false) return ret;
			}
			
		}
	}
	return true;
}

static enum MAPISTATUS openchangepfadmin_getdir(TALLOC_CTX *mem_ctx, 
						mapi_object_t *obj_container,
						mapi_object_t *obj_child,
						const char *folder)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		rowset;
	mapi_object_t		obj_htable;
	const char		*name;
	const uint64_t		*fid;
	uint32_t		index;

	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(obj_container, &obj_htable, 0, NULL);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	SPropTagArray = set_SPropTagArray(mem_ctx, 0x2,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_FID);
	retval = SetColumns(&obj_htable, SPropTagArray);
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) return MAPI_E_NOT_FOUND;
	
	while (((retval = QueryRows(&obj_htable, 0x32, TBL_ADVANCE, TBL_FORWARD_READ, &rowset)) != MAPI_E_NOT_FOUND) && rowset.cRows) {
		for (index = 0; index < rowset.cRows; index++) {
			fid = (const uint64_t *)find_SPropValue_data(&rowset.aRow[index], PR_FID);
			name = (const char *)find_SPropValue_data(&rowset.aRow[index], PR_DISPLAY_NAME_UNICODE);

			if (name && fid && !strcmp(name, folder)) {
				retval = OpenFolder(obj_container, *fid, obj_child);
				MAPI_RETVAL_IF(retval, GetLastError(), NULL);
				return MAPI_E_SUCCESS;
			}
		}
	}

	errno = MAPI_E_NOT_FOUND;
	return MAPI_E_NOT_FOUND;
}

static enum MAPISTATUS openchangepfadmin_mkdir(mapi_object_t *obj_container, 
					       const char *name, 
					       const char *comment, 
					       const char *type)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;
	struct SPropValue	props[1];
	uint32_t		prop_count = 0;

	mapi_object_init(&obj_folder);
	retval = CreateFolder(obj_container, FOLDER_GENERIC, name, comment, OPEN_IF_EXISTS, &obj_folder);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	if (type) {
		set_SPropValue_proptag(&props[0], PR_CONTAINER_CLASS, (const void *) type);
		prop_count++;

		retval = SetProps(&obj_folder, 0, props, prop_count);
		mapi_object_release(&obj_folder);
		MAPI_RETVAL_IF(retval, GetLastError(), NULL);
	}
	
	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS openchangepfadmin_rmdir(TALLOC_CTX *mem_ctx,
					       mapi_object_t *obj_container,
					       const char *name)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_folder;

	mapi_object_init(&obj_folder);

	retval = openchangepfadmin_getdir(mem_ctx, obj_container, &obj_folder, name);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	retval = EmptyFolder(&obj_folder);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	retval = DeleteFolder(obj_container, mapi_object_get_id(&obj_folder),
			      DEL_FOLDERS, NULL);
	MAPI_RETVAL_IF(retval, GetLastError(), NULL);

	mapi_object_release(&obj_folder);

	return MAPI_E_SUCCESS;
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_context	*mapi_ctx;
	struct mapi_session	*session = NULL;
	mapi_object_t		obj_store;
	mapi_object_t		obj_ipm_subtree;
	mapi_object_t		obj_folder;
	mapi_id_t		id_pf;
	poptContext		pc;
	int			opt;
	const char		*opt_profdb = NULL;
	char			*opt_profname = NULL;
	const char		*opt_password = NULL;
	const char		*opt_comment = NULL;
	const char		*opt_dirclass = NULL;
	const char		*opt_adduser = NULL;
	const char		*opt_rmuser = NULL;
	const char		*opt_apassword = NULL;
	const char		*opt_adesc = NULL;
	const char		*opt_acomment = NULL;
	const char		*opt_afullname = NULL;
	const char		*opt_addright = NULL;
	bool			opt_rmright = false;
	const char		*opt_modright = NULL;
	const char		*opt_folder = NULL;
	const char		*opt_debug = NULL;
	const char		*opt_username = NULL;	
	int32_t			opt_permission = -1;
	bool			opt_ipm_list = false;
	bool			opt_mkdir = false;
	bool			opt_rmdir = false;
	bool			opt_dumpdata = false;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_PASSWORD, OPT_IPM_LIST, 
	      OPT_MKDIR, OPT_RMDIR, OPT_COMMENT, OPT_DIRCLASS, OPT_ACL, 
	      OPT_ADDUSER, OPT_RMUSER, OPT_DEBUG, OPT_APASSWORD, OPT_ADESC, 
	      OPT_ACOMMENT, OPT_AFULLNAME, OPT_ADDRIGHT, OPT_RMRIGHT, 
	      OPT_MODRIGHT, OPT_USERNAME, OPT_FOLDER, OPT_DUMPDATA};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'p', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"password", 'P', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{"apassword", 0, POPT_ARG_STRING, NULL, OPT_APASSWORD, "set the account password", "PASSWORD"},
		{"adesc", 0, POPT_ARG_STRING, NULL, OPT_ADESC, "set the account description", "DESCRIPTION"},
		{"acomment", 0, POPT_ARG_STRING, NULL, OPT_ACOMMENT, "set the account comment", "COMMENT"},
		{"afullname", 0, POPT_ARG_STRING, NULL, OPT_AFULLNAME, "set the account full name", "NAME"},
		{"list",0, POPT_ARG_NONE, NULL, OPT_IPM_LIST, "list IPM subtree directories", NULL},
		{"mkdir", 0, POPT_ARG_NONE, NULL, OPT_MKDIR, "create a Public Folder directory", NULL},
		{"rmdir", 0, POPT_ARG_NONE, NULL, OPT_RMDIR, "delete a Public Folder directory", NULL},
		{"comment", 0, POPT_ARG_STRING, NULL, OPT_COMMENT, "set the folder comment", "COMMENT"},
		{"dirclass", 0, POPT_ARG_STRING, NULL, OPT_DIRCLASS, "set the folder class", "CLASS"},
		{"adduser", 0, POPT_ARG_STRING, NULL, OPT_ADDUSER, "add Exchange user", "USERNAME"},
		{"rmuser", 0, POPT_ARG_STRING, NULL, OPT_RMUSER, "delete Exchange user", "USERNAME"},
		{"addright", 0, POPT_ARG_STRING, NULL, OPT_ADDRIGHT, "add MAPI permissions to PF folder", "RIGHT"},
		{"rmright", 0, POPT_ARG_NONE, NULL, OPT_RMRIGHT, "remove MAPI permissions to PF folder", NULL},
		{"modright", 0, POPT_ARG_STRING, NULL, OPT_MODRIGHT, "modify MAPI permissions to PF folder", "RIGHT"},
		{"debuglevel", 0, POPT_ARG_STRING, NULL, OPT_DEBUG, "set debug level", "LEVEL"},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "Dump the hex data", NULL},
		{"folder", 0, POPT_ARG_STRING, NULL, OPT_FOLDER, "specify the Public Folder directory", "FOLDER"},
		{"username", 0, POPT_ARG_STRING, NULL, OPT_USERNAME, "specify the username to use", "USERNAME"},
		POPT_OPENCHANGE_VERSION
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "openchangepfadmin");

	pc = poptGetContext("openchangepfadmin", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
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
			opt_profname = talloc_strdup(mem_ctx, poptGetOptArg(pc));
			break;
		case OPT_PASSWORD:
			opt_password = poptGetOptArg(pc);
			break;
		case OPT_IPM_LIST:
			opt_ipm_list = true;
			break;
		case OPT_MKDIR:
			opt_mkdir = true;
			break;
		case OPT_RMDIR:
			opt_rmdir = true;
			break;
		case OPT_COMMENT:
			opt_comment = poptGetOptArg(pc);
			break;
		case OPT_DIRCLASS:
			opt_dirclass = poptGetOptArg(pc);
			break;
		case OPT_ADDUSER:
			opt_adduser = poptGetOptArg(pc);
			break;
		case OPT_RMUSER:
			opt_rmuser = poptGetOptArg(pc);
			break;
		case OPT_APASSWORD:
			opt_apassword = poptGetOptArg(pc);
			break;
		case OPT_ADESC:
			opt_adesc = poptGetOptArg(pc);
			break;
		case OPT_ACOMMENT:
			opt_acomment = poptGetOptArg(pc);
			break;
		case OPT_AFULLNAME:
			opt_afullname = poptGetOptArg(pc);
			break;
		case OPT_ADDRIGHT:
			opt_addright = poptGetOptArg(pc);
			break;
		case OPT_RMRIGHT:
			opt_rmright = true;
			break;
		case OPT_MODRIGHT:
			opt_modright = poptGetOptArg(pc);
			break;
		case OPT_FOLDER:
			opt_folder = poptGetOptArg(pc);
			break;
		case OPT_USERNAME:
			opt_username = poptGetOptArg(pc);
			break;
		default:
			break;
		};
	}

	/* Sanity check on options */

	if ((opt_mkdir == true && !opt_folder) || (opt_rmdir == true && !opt_folder) ||
	    (opt_addright && !opt_folder) || (opt_rmright == true && !opt_folder) || 
	    (opt_modright && !opt_folder)) {
		printf("You need to specify a directory with --folder option\n");
		return (-1);
	}

	if ((opt_addright && !opt_username) || (opt_rmright == true && !opt_username) ||
	    (opt_modright && !opt_username)) {
		printf("You need to specify a username with --username option\n");
		return (-1);
	}

	/* dirclass sanity check */
	if (opt_dirclass) {
		if (check_IPF_class(opt_dirclass) == -1) {
			list_IPF_class();
			return (-1);
		}
	}

	if (!opt_profdb) {
		opt_profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, getenv("HOME"));
	}

	/**
	 * Initialize MAPI subsystem
	 */
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

	/**
	 * If no profile is specified, try to load the default one
	 * from the database
	 */
	if (!opt_profname) {
		retval = GetDefaultProfile(mapi_ctx, &opt_profname);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	retval = MapiLogonEx(mapi_ctx, &session, opt_profname, opt_password);
	talloc_free(opt_profname);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonEx", GetLastError());
		exit (1);
	}

	/**
	 * User management operations
	 */

	if (opt_adduser) {
		struct mapiadmin_ctx	*mapiadmin_ctx;

		mapiadmin_ctx = mapiadmin_init(session);
		mapiadmin_ctx->username = opt_adduser;
		mapiadmin_ctx->password = opt_apassword;
		mapiadmin_ctx->description = opt_adesc;
		mapiadmin_ctx->comment = opt_acomment;
		mapiadmin_ctx->fullname = opt_afullname;

		retval = mapiadmin_user_add(mapiadmin_ctx);
		mapi_errstr("mapiadmin_user_add", GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			exit (1);
		}
		printf("username: %s\n", mapiadmin_ctx->username);
		printf("password: %s\n", mapiadmin_ctx->password);
	}

	if (opt_rmuser) {
		struct mapiadmin_ctx *mapiadmin_ctx;
		
		mapiadmin_ctx = mapiadmin_init(session);
		mapiadmin_ctx->username = opt_rmuser;

		retval = mapiadmin_user_del(mapiadmin_ctx);
		mapi_errstr("mapiadmin_user_del", GetLastError());
	}

	/**
	 * Open Public Folder Store
	 */

	mapi_object_init(&obj_store);
	retval = OpenPublicFolder(session, &obj_store);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenPublicFolder", GetLastError());
		goto end;
	}

	/* Open IPM Subtree (All Public Folders) */
	retval = GetDefaultPublicFolder(&obj_store, &id_pf, olFolderPublicIPMSubtree);
	if (retval != MAPI_E_SUCCESS) {
		goto end;
	}

	mapi_object_init(&obj_ipm_subtree);
	retval = OpenFolder(&obj_store, id_pf, &obj_ipm_subtree);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("OpenFolder", GetLastError());
		goto end;
	}

	/* create directory */
	if (opt_mkdir == true) {
		retval = openchangepfadmin_mkdir(&obj_ipm_subtree, opt_folder, opt_comment, opt_dirclass);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("mkdir", GetLastError());
			goto end;
		}
	}

	/* remove directory */
	if (opt_rmdir == true) {
		retval = openchangepfadmin_rmdir(mem_ctx, &obj_ipm_subtree, opt_folder);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("rmdir", GetLastError());
			goto end;
		}
		
	}

	/* add permissions */
	if (opt_addright) {
		if ((opt_permission = get_aclrights(opt_addright)) == -1) {
			printf("Invalid permission\n");
			list_aclrights();
			goto end;
		} else {
			mapi_object_init(&obj_folder);
			retval = openchangepfadmin_getdir(mem_ctx, &obj_ipm_subtree, &obj_folder, opt_folder);
			if (retval != MAPI_E_SUCCESS) {
				printf("Invalid directory\n");
				goto end;
			}
			retval = AddUserPermission(&obj_folder, opt_username, opt_permission);
			if (retval != MAPI_E_SUCCESS) {
				mapi_errstr("AddUserPermission", GetLastError());
				mapi_object_release(&obj_folder);
				goto end;
			}
			mapi_object_release(&obj_folder);
		}
		printf("Permission %s added for %s on folder %s\n", opt_addright, opt_username, opt_folder);
	}

	/* remove permissions */
	if (opt_rmright == true) {
		mapi_object_init(&obj_folder);
		retval = openchangepfadmin_getdir(mem_ctx, &obj_ipm_subtree, &obj_folder, opt_folder);
		if (retval != MAPI_E_SUCCESS) {
			printf("Invalid directory\n");
			goto end;
		}
		retval = RemoveUserPermission(&obj_folder, opt_username);
		if (retval != MAPI_E_SUCCESS) {
			mapi_errstr("RemoveUserPermission", GetLastError());
			mapi_object_release(&obj_folder);
			goto end;
		}
		mapi_object_release(&obj_folder);
		printf("Permission removed for %s on folder %s\n", opt_username, opt_folder);
	}

	/* modify permissions */
	if (opt_modright) {
		opt_permission = get_aclrights(opt_modright);
		if (opt_permission == -1) {
			printf("Invalid permission\n");
			list_aclrights();
			goto end;
		} else {
			mapi_object_init(&obj_folder);
			retval = openchangepfadmin_getdir(mem_ctx, &obj_ipm_subtree, &obj_folder, opt_folder);
			if (retval != MAPI_E_SUCCESS) {
				printf("Invalid directory\n");
				goto end;
			}
			retval = ModifyUserPermission(&obj_folder, opt_username, opt_permission);
			if (retval != MAPI_E_SUCCESS) {
				mapi_errstr("ModifyUserPermission", GetLastError());
				mapi_object_release(&obj_folder);
				goto end;
			}
			mapi_object_release(&obj_folder);
		}
		printf("Permission changed to %s for %s on folder %s\n", opt_modright, opt_username, opt_folder);
	}

	/* list directories */
	if (opt_ipm_list == true) {
		printf("+ All Public Folders\n");
		get_child_folders_pf(mem_ctx, &obj_ipm_subtree, id_pf, 0);
	}

	/**
	 * Uninitialize MAPI subsystem
	 */
end:
	mapi_object_release(&obj_store);

	MAPIUninitialize(mapi_ctx);
	talloc_free(mem_ctx);
	return 0;
}
