/*
   Wrapper over the MAPI Profile API

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

#include <sys/stat.h>
#include <sys/types.h>

#define	DEFAULT_DIR	"%s/.openchange"
#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"

static void mapiprofile_createdb(const char *profdb, const char *ldif_path)
{
	enum MAPISTATUS retval;

	retval = CreateProfileStore(profdb, ldif_path);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("CreateProfileStore", GetLastError());
		exit (1);
	}
}

static uint32_t callback(struct SRowSet *rowset, void *private)
{
	uint32_t		i;
	struct SPropValue	*lpProp;
	FILE			*fd;
	uint32_t		index;
	char     		entry[10];
	const char		*label = (const char *)private;

	printf("%s:\n", label);
	for (i = 0; i < rowset->cRows; i++) {
		lpProp = get_SPropValue_SRow(&(rowset->aRow[i]), PR_DISPLAY_NAME);
		if (lpProp && lpProp->value.lpszA) {
			printf("\t[%d] %s\n", i, lpProp->value.lpszA);
		}
	}
	printf("\t[%d] cancel operation\n", i);
	fd = fdopen(0, "r");
getentry:
	printf("Enter username id [0]: ");
	fgets(entry, 10, fd);
	index = atoi(entry);
	if (index > i) {
		printf("Invalid id - Must be contained between 0 and %d\n", i);
		goto getentry;
	}
	
	fclose(fd);
	return (index);
}

static void mapiprofile_create(const char *profdb, const char *profname,
			       const char *pattern, const char *username, 
			       const char *password, const char *address, 
			       const char *workstation, const char *domain,
			       uint32_t flags)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;

	retval = MAPIInitialize(profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	retval = CreateProfile(profname, username, password, flags);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("CreateProfile", GetLastError());
		exit (1);
	}

	mapi_profile_add_string_attr(profname, "binding", address);
	mapi_profile_add_string_attr(profname, "workstation", workstation);
	mapi_profile_add_string_attr(profname, "domain", domain);

	/* This is only convenient here and should be replaced at some point */
	mapi_profile_add_string_attr(profname, "codepage", "0x4e4");
	mapi_profile_add_string_attr(profname, "language", "0x40c");
	mapi_profile_add_string_attr(profname, "method", "0x409");

	dcerpc_init();

	retval = MapiLogonProvider(&session, profname, password, PROVIDER_ID_NSPI);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonProvider", GetLastError());
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(profname, 0)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", GetLastError());
		}
		exit (1);
	}

	if (pattern) {
		username = pattern;
	}

	retval = ProcessNetworkProfile(session, username, (mapi_profile_callback_t) callback, "Select a user id");
	if (retval != MAPI_E_SUCCESS && retval != 0x1) {
		mapi_errstr("ProcessNetworkProfile", GetLastError());
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(profname, 0)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", GetLastError());
		}
		exit (1);
	}

	printf("Profile %s completed and added to database %s\n", profname, profdb);

	MAPIUninitialize();
}

static void mapiprofile_delete(const char *profdb, const char *profname)
{
	enum MAPISTATUS retval;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if ((retval = DeleteProfile(profname, 0)) != MAPI_E_SUCCESS) {
		mapi_errstr("DeleteProfile", GetLastError());
		exit (1);
	}

	printf("Profile %s deleted from database %s\n", profname, profdb);

	MAPIUninitialize();
}

static void mapiprofile_set_default(const char *profdb, const char *profname)
{
	enum MAPISTATUS retval;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if ((retval = SetDefaultProfile(profname, 0)) != MAPI_E_SUCCESS) {
		mapi_errstr("SetDefaultProfile", GetLastError());
		exit (1);
	}

	printf("Profile %s is now set the default one\n", profname);

	MAPIUninitialize();
}

static void mapiprofile_get_default(const char *profdb)
{
	enum MAPISTATUS retval;
	const char	*profname;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}
	
	if ((retval = GetDefaultProfile(&profname, 0)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetDefaultProfile", GetLastError());
		exit (1);
	}

	printf("Default profile is set to %s\n", profname);

	MAPIUninitialize();
}

static void mapiprofile_list(const char *profdb)
{
	enum MAPISTATUS retval;
	struct SRowSet	proftable;
	uint32_t	count = 0;

	if ((retval = MAPIInitialize(profdb))) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	memset(&proftable, 0, sizeof (struct SRowSet));
	if ((retval = GetProfileTable(&proftable, 0)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", GetLastError());
		exit (1);
	}

	printf("We have %d profiles in the database:\n", proftable.cRows);

	for (count = 0; count != proftable.cRows; count++) {
		const char	*name = NULL;
		uint32_t	dflt = 0;

		name = proftable.aRow[count].lpProps[0].value.lpszA;
		dflt = proftable.aRow[count].lpProps[1].value.l;

		if (dflt) {
			printf("\tProfile = %s [default]\n", name);
		} else {
			printf("\tProfile = %s\n", name);
		}

	}

	MAPIUninitialize();
}

static void mapiprofile_dump(const char *profdb, const char *profname)
{
	enum MAPISTATUS		retval;
	struct mapi_profile	profile;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if (!profname) {
		if ((retval = GetDefaultProfile(&profname, 0)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	retval = OpenProfile(&profile, profname, NULL);
	if (retval && retval != MAPI_E_INVALID_PARAMETER) {
		mapi_errstr("OpenProfile", GetLastError());
		exit (1);
	}

	printf("Profile: %s\n", profile.profname);
	printf("\tusername       == %s\n", profile.username);
	printf("\tpassword       == %s\n", profile.password);
	printf("\tmailbox        == %s\n", profile.mailbox);
	printf("\tworkstation    == %s\n", profile.workstation);
	printf("\trealm          == %s\n", profile.realm);
	printf("\tserver         == %s\n", profile.server);

	MAPIUninitialize();
}

static void mapiprofile_attribute(const char *profdb, const char *profname, 
				  const char *attribute)
{
	enum MAPISTATUS		retval;
	struct mapi_profile	profile;
	char			**value = NULL;
	unsigned int		count = 0;
	int			i;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if (!profname) {
		if ((retval = GetDefaultProfile(&profname, 0)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	}

	retval = OpenProfile(&profile, profname, NULL);
	if (retval && retval != MAPI_E_INVALID_PARAMETER) {
		mapi_errstr("OpenProfile", GetLastError());
		exit (1);
	}

	if ((retval = GetProfileAttr(&profile, attribute, &count, &value))) {
		mapi_errstr("ProfileGetAttr", GetLastError());
		exit (1);
	}

	printf("Profile %s: results(%d)\n", profname, count);
	for (i = 0; i < count; i++) {
		printf("\t%s = %s\n", attribute, value[i]);
	}
	MAPIFreeBuffer(value);

	MAPIUninitialize();
}

static void show_help(poptContext pc, const char *param)
{
	printf("%s argument missing\n", param);
	poptPrintUsage(pc, stderr, 0);
	exit (1);
}

int main(int argc, const char *argv[])
{
	TALLOC_CTX	*mem_ctx;
	int		error;
	poptContext	pc;
	int		opt;
	char		*default_path;
	bool		create = false;
	bool		delete = false;
	bool		list = false;
	bool		dump = false;
	bool		newdb = false;
	bool		setdflt = false;
	bool		getdflt = false;
	const char	*ldif = NULL;
	const char	*address = NULL;
	const char	*workstation = NULL;
	const char	*domain = NULL;
	const char	*username = NULL;
	const char	*pattern = NULL;
	const char	*password = NULL;
	const char	*profdb = NULL;
	const char	*profname = NULL;
	const char	*attribute = NULL;
	uint32_t	nopass = 0;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_ADDRESS, OPT_WORKSTATION,
	      OPT_DOMAIN, OPT_USERNAME, OPT_PASSWORD, OPT_CREATE_PROFILE, 
	      OPT_DELETE_PROFILE, OPT_LIST_PROFILE, OPT_DUMP_PROFILE, 
	      OPT_DUMP_ATTR, OPT_PROFILE_NEWDB, OPT_PROFILE_LDIF, 
	      OPT_PROFILE_SET_DFLT, OPT_PROFILE_GET_DFLT, OPT_PATTERN,
	      OPT_NOPASS};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"ldif", 'L', POPT_ARG_STRING, NULL, OPT_PROFILE_LDIF, "set the ldif path"},
		{"getdefault", 'G', POPT_ARG_NONE, NULL, OPT_PROFILE_GET_DFLT, "get the default profile "},
		{"default", 'S', POPT_ARG_NONE, NULL, OPT_PROFILE_SET_DFLT, "set the default profile"},
		{"newdb", 'n', POPT_ARG_NONE, NULL, OPT_PROFILE_NEWDB, "create a new profile store"},
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path"},
		{"profile", 'P', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name"},
		{"address", 'I', POPT_ARG_STRING, NULL, OPT_ADDRESS, "set the exchange server address"},
		{"workstation", 'M', POPT_ARG_STRING, NULL, OPT_WORKSTATION, "set the workstation"},
		{"domain", 'D', POPT_ARG_STRING, NULL, OPT_DOMAIN, "set the domain"},
		{"username", 'u', POPT_ARG_STRING, NULL, OPT_USERNAME, "set the profile username"},
		{"pattern", 's', POPT_ARG_STRING, NULL, OPT_PATTERN, "username to search"},
		{"password", 'p', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password"},
		{"nopass", 0, POPT_ARG_NONE, NULL, OPT_NOPASS, "do not save password in the profile"},
		{"create", 'c', POPT_ARG_NONE, NULL, OPT_CREATE_PROFILE, "create a profile in the database"},
		{"delete", 'r', POPT_ARG_NONE, NULL, OPT_DELETE_PROFILE, "delete a profile in the database"},
		{"list", 'l', POPT_ARG_NONE, NULL, OPT_LIST_PROFILE, "list existing profiles in the database"},
		{"dump", 'd', POPT_ARG_NONE, NULL, OPT_DUMP_PROFILE, "dump a profile entry"},
		{"attr", 'a', POPT_ARG_STRING, NULL, OPT_DUMP_ATTR, "print an attribute value"},
		{ NULL }
	};

	mem_ctx = talloc_init("mapiprofile");

	pc = poptGetContext("mapiprofile", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch(opt) {
		case OPT_PROFILE_LDIF:
			ldif = poptGetOptArg(pc);
			break;
		case OPT_PROFILE_NEWDB:
			newdb = true;
			break;
		case OPT_PROFILE_SET_DFLT:
			setdflt = true;
			break;
		case OPT_PROFILE_GET_DFLT:
			getdflt = true;
			break;
		case OPT_PROFILE_DB:
			profdb = poptGetOptArg(pc);
			break;
		case OPT_PROFILE:
			profname = poptGetOptArg(pc);
			break;
		case OPT_ADDRESS:
			address = poptGetOptArg(pc);
			break;
		case OPT_WORKSTATION:
			workstation = poptGetOptArg(pc);
			break;
		case OPT_DOMAIN:
			domain = poptGetOptArg(pc);
			break;
		case OPT_USERNAME:
			username = poptGetOptArg(pc);
			break;
		case OPT_PATTERN:
			pattern = poptGetOptArg(pc);
			break;
		case OPT_PASSWORD:
			password = poptGetOptArg(pc);
			break;
		case OPT_NOPASS:
			nopass = 1;
			break;
		case OPT_CREATE_PROFILE:
			create = true;
			break;
		case OPT_DELETE_PROFILE:
			delete = true;
			break;
		case OPT_LIST_PROFILE:
			list = true;
			break;
		case OPT_DUMP_PROFILE:
			dump = true;
			break;
		case OPT_DUMP_ATTR:
			attribute = poptGetOptArg(pc);
			break;
		}
	}

	/* Sanity check on options */
	if (!profdb) {
		default_path = talloc_asprintf(mem_ctx, DEFAULT_DIR, getenv("HOME"));
		error = mkdir(default_path, 0700);
		talloc_free(default_path);
		if (error == -1 && errno != EEXIST) {
			perror("mkdir");
			talloc_free(mem_ctx);
			exit (1);
		}
		profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, 
					 getenv("HOME"));
	}

	if ((list == false) && (newdb == false) 
	    && (getdflt == false) && (dump == false) && 
	    (!attribute) && (!profname || !profdb)) {
		poptPrintUsage(pc, stderr, 0);
		exit (1);
	}

	if (newdb == true) {
		if (!ldif) {
			ldif = talloc_strdup(mem_ctx, mapi_profile_get_ldif_path());
		}
		if (!ldif) show_help(pc, "ldif");
		mapiprofile_createdb(profdb, ldif);
	}

	/* Process the code here */

	if (create == true) {
		if (!profname) show_help(pc, "profile");
		if (!username) show_help(pc, "username");
		if (!address) show_help(pc, "address");
		if (!workstation) show_help(pc, "workstation");
		if (!domain) show_help(pc, "domain");

		mapiprofile_create(profdb, profname, pattern, username, password, address,
				   workstation, domain, nopass);
	}

	if (setdflt == true) {
		mapiprofile_set_default(profdb, profname);
	}

	if (getdflt == true) {
		mapiprofile_get_default(profdb);
	}

	if (delete == true) {
		mapiprofile_delete(profdb, profname);
	}

	if (list == true) {
		mapiprofile_list(profdb);
	}

	if (dump == true) {
		mapiprofile_dump(profdb, profname);
	}

	if (attribute) {
		mapiprofile_attribute(profdb, profname, attribute);
	}

	talloc_free(mem_ctx);

	return (0);
}
