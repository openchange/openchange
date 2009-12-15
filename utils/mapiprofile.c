/*
   Wrapper over the MAPI Profile API

   OpenChange Project

   Copyright (C) Julien Kerihuel 2007-2009

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

#include <libmapi/libmapi.h>
#include <samba/popt.h>
#include <param.h>
#include "openchange-tools.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>

#define	DEFAULT_DIR	"%s/.openchange"
#define	DEFAULT_PROFDB	"%s/.openchange/profiles.ldb"
#define	DEFAULT_LCID	"0x409" /* language code ID: en-US */

static bool mapiprofile_createdb(const char *profdb, const char *ldif_path)
{
	enum MAPISTATUS retval;
	
	if (access(profdb, F_OK) == 0) {
		fprintf(stderr, "[ERROR] mapiprofile: %s already exists\n", profdb);
		return false;
	}

	retval = CreateProfileStore(profdb, ldif_path);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("CreateProfileStore", GetLastError());
		return false;
	}
	return true;
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
			printf("\t[%u] %s\n", i, lpProp->value.lpszA);
		}
	}
	printf("\t[%u] cancel operation\n", i);
	fd = fdopen(0, "r");
getentry:
	printf("Enter username id [0]: ");
	if (fgets(entry, 10, fd) == NULL) {
		printf("Failed to read string\n");
		exit(1);
	}

	index = atoi(entry);
	if (index > i) {
		printf("Invalid id - Must be a value between 0 and %u\n", i);
		goto getentry;
	}
	
	fclose(fd);
	return (index);
}

const char *g_profname;

static void signal_delete_profile(void)
{
	enum MAPISTATUS	retval;

	fprintf(stderr, "CTRL-C caught ... Deleting profile\n");
	if ((retval = DeleteProfile(g_profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("DeleteProfile", GetLastError());
	}

	(void) signal(SIGINT, SIG_DFL);
	exit (1);
}

static bool mapiprofile_create(const char *profdb, const char *profname,
			       const char *pattern, const char *username, 
			       const char *password, const char *address, 
			       const char *lcid, const char *workstation,
			       const char *domain, const char *realm,
			       uint32_t flags, bool seal,
			       bool opt_dumpdata, const char *opt_debuglevel)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	profile;

	mem_ctx = talloc_named(NULL, 0, "mapiprofile_create");

	/* catch CTRL-C */
	g_profname = profname;

#if defined (__FreeBSD__)
	(void) signal(SIGINT, (sig_t) signal_delete_profile);
#elif defined (__SunOS)
        (void) signal(SIGINT, signal_delete_profile);
#else
	(void) signal(SIGINT, (sighandler_t) signal_delete_profile);
#endif

	retval = MAPIInitialize(profdb);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		talloc_free(mem_ctx);
		return false;
	}

	/* debug options */
	SetMAPIDumpData(opt_dumpdata);

	if (opt_debuglevel) {
		SetMAPIDebugLevel(atoi(opt_debuglevel));
	}

	/* Sanity check */
	retval = OpenProfile(&profile, profname, NULL);
	if (retval == MAPI_E_SUCCESS) {
		fprintf(stderr, "[ERROR] mapiprofile: profile \"%s\" already exists\n", profname);
		MAPIUninitialize();
		talloc_free(mem_ctx);
		return false;
	}

	retval = CreateProfile(profname, username, password, flags);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("CreateProfile", GetLastError());
		talloc_free(mem_ctx);
		return false;
	}

	mapi_profile_add_string_attr(profname, "binding", address);
	mapi_profile_add_string_attr(profname, "workstation", workstation);
	mapi_profile_add_string_attr(profname, "domain", domain);
	mapi_profile_add_string_attr(profname, "seal", (seal == true) ? "true" : "false");

	if (realm) {
		mapi_profile_add_string_attr(profname, "realm", realm);
	}

	if (strncmp(lcid, "0x", 2) != 0) {
		/* it doesn't look like a hex id, so try to convert it from
		   a string name (like "English_Australian" to a language code
		   ID string (like "0x0c09")
		*/
		lcid = talloc_asprintf(mem_ctx, "0x%04x", lcid_lang2lcid(lcid));
	}
	if (!lcid_valid_locale(strtoul(lcid, 0, 16))) {
		lcid = DEFAULT_LCID;
		printf("Language code not recognised, using default (%s) instead\n", lcid);
	}

	/* This is only convenient here and should be replaced at some point */
	mapi_profile_add_string_attr(profname, "codepage", "0x4e4");
	mapi_profile_add_string_attr(profname, "language", lcid );
	mapi_profile_add_string_attr(profname, "method", "0x409");

	retval = MapiLogonProvider(&session, profname, password, PROVIDER_ID_NSPI);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonProvider", GetLastError());
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", GetLastError());
		}
		talloc_free(mem_ctx);
		return false;
	}

	if (pattern) {
		username = pattern;
	}

	retval = ProcessNetworkProfile(session, username, (mapi_profile_callback_t) callback, "Select a user id");
	if (retval != MAPI_E_SUCCESS && retval != 0x1) {
		mapi_errstr("ProcessNetworkProfile", GetLastError());
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", GetLastError());
		}
		talloc_free(mem_ctx);
		return false;
	}

	printf("Profile %s completed and added to database %s\n", profname, profdb);

	talloc_free(mem_ctx);

	MAPIUninitialize();

	return true;
}

static void mapiprofile_delete(const char *profdb, const char *profname)
{
	enum MAPISTATUS retval;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if ((retval = DeleteProfile(profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("DeleteProfile", GetLastError());
		exit (1);
	}

	printf("Profile %s deleted from database %s\n", profname, profdb);

	MAPIUninitialize();
}


static void mapiprofile_rename(const char *profdb, const char *old_profname, const char *new_profname)
{
	TALLOC_CTX	*mem_ctx;
	enum MAPISTATUS	retval;
	char		*profname;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", retval);
		exit (1);
	}

	mem_ctx = talloc_named(NULL, 0, "mapiprofile_rename");

	if (!old_profname) {
		if ((retval = GetDefaultProfile(&profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, old_profname);
	}

	if ((retval = RenameProfile(profname, new_profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("RenameProfile", retval);
		talloc_free(profname);
		talloc_free(mem_ctx);
		exit (1);
	}

	talloc_free(profname);
	talloc_free(mem_ctx);
	MAPIUninitialize();
}


static void mapiprofile_set_default(const char *profdb, const char *profname)
{
	enum MAPISTATUS retval;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if ((retval = SetDefaultProfile(profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("SetDefaultProfile", GetLastError());
		exit (1);
	}

	printf("Profile %s is now set the default one\n", profname);

	MAPIUninitialize();
}

static void mapiprofile_get_default(const char *profdb)
{
	enum MAPISTATUS retval;
	char		*profname;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}
	
	if ((retval = GetDefaultProfile(&profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetDefaultProfile", GetLastError());
		exit (1);
	}

	printf("Default profile is set to %s\n", profname);

	talloc_free(profname);
	MAPIUninitialize();
}

static void mapiprofile_get_fqdn(const char *profdb, 
				 const char *opt_profname, 
				 const char *password,
				 bool opt_dumpdata)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	const char		*serverFQDN;
	char			*profname;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	SetMAPIDumpData(opt_dumpdata);

	mem_ctx = talloc_named(NULL, 0, "mapiprofile_get_fqdn");

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(&profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (char *)opt_profname);
	}

	retval = MapiLogonProvider(&session, profname, password, PROVIDER_ID_NSPI);
	talloc_free(profname);
	talloc_free(mem_ctx);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonProvider", GetLastError());
		exit (1);
	}

	retval = RfrGetFQDNFromLegacyDN(session, &serverFQDN);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("RfrGetFQDNFromLegacyDN", GetLastError());
		exit (1);
	}

	printf("%s is at %s\n", global_mapi_ctx->session->profile->homemdb, serverFQDN);

	MAPIUninitialize();
}

static void mapiprofile_list(const char *profdb)
{
	enum MAPISTATUS retval;
	struct SRowSet	proftable;
	uint32_t	count = 0;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	memset(&proftable, 0, sizeof (struct SRowSet));
	if ((retval = GetProfileTable(&proftable)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", GetLastError());
		exit (1);
	}

	printf("We have %u profiles in the database:\n", proftable.cRows);

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

static void mapiprofile_dump(const char *profdb, const char *opt_profname)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_profile	profile;
	char			*profname;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	mem_ctx = talloc_named(NULL, 0, "mapiprofile_dump");

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(&profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(&profile, profname, NULL);
	talloc_free(profname);
	talloc_free(mem_ctx);
	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		mapi_errstr("OpenProfile", GetLastError());
		exit (1);
	}

	printf("Profile: %s\n", profile.profname);
	printf("\tusername       == %s\n", profile.username);
	printf("\tpassword       == %s\n", profile.password);
	printf("\tmailbox        == %s\n", profile.mailbox);
	printf("\tworkstation    == %s\n", profile.workstation);
	printf("\tdomain         == %s\n", profile.domain);
	printf("\tserver         == %s\n", profile.server);

	MAPIUninitialize();
}

static void mapiprofile_attribute(const char *profdb, const char *opt_profname, 
				  const char *attribute)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_profile	profile;
	char			*profname = NULL;
	char			**value = NULL;
	unsigned int		count = 0;
	unsigned int		i;

	if ((retval = MAPIInitialize(profdb)) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	mem_ctx = talloc_named(NULL, 0, "mapiprofile_attribute");

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(&profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", GetLastError());
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(&profile, profname, NULL);
	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		mapi_errstr("OpenProfile", GetLastError());
		talloc_free(profname);
		talloc_free(mem_ctx);
		exit (1);
	}

	if ((retval = GetProfileAttr(&profile, attribute, &count, &value))) {
		mapi_errstr("ProfileGetAttr", GetLastError());
		talloc_free(profname);
		talloc_free(mem_ctx);
		exit (1);
	}

	printf("Profile %s: results(%u)\n", profname, count);
	for (i = 0; i < count; i++) {
		printf("\t%s = %s\n", attribute, value[i]);
	}
	MAPIFreeBuffer(value);
	talloc_free(profname);
	talloc_free(mem_ctx);

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
	bool		listlangs = false;
	bool		dump = false;
	bool		newdb = false;
	bool		setdflt = false;
	bool		getdflt = false;
	bool		getfqdn = false;
	bool		opt_dumpdata = false;
	bool		opt_seal = false;
	const char	*opt_debuglevel = NULL;
	const char	*ldif = NULL;
	const char	*address = NULL;
	const char	*workstation = NULL;
	const char	*domain = NULL;
	const char	*realm = NULL;
	const char	*username = NULL;
	const char      *lcid = NULL;
	const char	*pattern = NULL;
	const char	*password = NULL;
	const char	*profdb = NULL;
	const char	*profname = NULL;
	const char	*rename = NULL;
	const char	*attribute = NULL;
	const char	*opt_tmp = NULL;
	uint32_t	nopass = 0;
	char		hostname[256];
	int		retcode = EXIT_SUCCESS;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_ADDRESS, OPT_WORKSTATION,
	      OPT_DOMAIN, OPT_REALM, OPT_USERNAME, OPT_LCID, OPT_PASSWORD, 
	      OPT_CREATE_PROFILE, OPT_DELETE_PROFILE, OPT_LIST_PROFILE, OPT_DUMP_PROFILE, 
	      OPT_DUMP_ATTR, OPT_PROFILE_NEWDB, OPT_PROFILE_LDIF, OPT_LIST_LANGS,
	      OPT_PROFILE_SET_DFLT, OPT_PROFILE_GET_DFLT, OPT_PATTERN, OPT_GETFQDN,
	      OPT_NOPASS, OPT_RENAME_PROFILE, OPT_DUMPDATA, OPT_DEBUGLEVEL,
	      OPT_ENCRYPT_CONN};

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"ldif", 'L', POPT_ARG_STRING, NULL, OPT_PROFILE_LDIF, "set the ldif path", "PATH"},
		{"getdefault", 'G', POPT_ARG_NONE, NULL, OPT_PROFILE_GET_DFLT, "get the default profile", NULL},
		{"default", 'S', POPT_ARG_NONE, NULL, OPT_PROFILE_SET_DFLT, "set the default profile", NULL},
		{"newdb", 'n', POPT_ARG_NONE, NULL, OPT_PROFILE_NEWDB, "create a new profile store", NULL},
		{"database", 'f', POPT_ARG_STRING, NULL, OPT_PROFILE_DB, "set the profile database path", "PATH"},
		{"profile", 'P', POPT_ARG_STRING, NULL, OPT_PROFILE, "set the profile name", "PROFILE"},
		{"address", 'I', POPT_ARG_STRING, NULL, OPT_ADDRESS, "set the exchange server IP address", "xxx.xxx.xxx.xxx"},
		{"workstation", 'M', POPT_ARG_STRING, NULL, OPT_WORKSTATION, "set the workstation", "WORKSTATION_NAME"},
		{"domain", 'D', POPT_ARG_STRING, NULL, OPT_DOMAIN, "set the domain/workgroup", "DOMAIN"},
		{"realm", 'R', POPT_ARG_STRING, NULL, OPT_REALM, "set the realm", "REALM"},
		{"encrypt", 'E', POPT_ARG_NONE, NULL, OPT_ENCRYPT_CONN, "enable encryption with Exchange server", NULL },
		{"username", 'u', POPT_ARG_STRING, NULL, OPT_USERNAME, "set the profile username", "USERNAME"},
		{"langcode", 'C', POPT_ARG_STRING, NULL, OPT_LCID, "set the language code ID", "LANGCODE"},
		{"pattern", 's', POPT_ARG_STRING, NULL, OPT_PATTERN, "username to search for", "USERNAME"},
		{"password", 'p', POPT_ARG_STRING, NULL, OPT_PASSWORD, "set the profile password", "PASSWORD"},
		{"nopass", 0, POPT_ARG_NONE, NULL, OPT_NOPASS, "do not save password in the profile", NULL},
		{"create", 'c', POPT_ARG_NONE, NULL, OPT_CREATE_PROFILE, "create a profile in the database", NULL},
		{"delete", 'r', POPT_ARG_NONE, NULL, OPT_DELETE_PROFILE, "delete a profile in the database", NULL},
		{"rename", 'R', POPT_ARG_STRING, NULL, OPT_RENAME_PROFILE, "rename a profile in the database", NULL},
		{"list", 'l', POPT_ARG_NONE, NULL, OPT_LIST_PROFILE, "list existing profiles in the database", NULL},
		{"listlangs", 0, POPT_ARG_NONE, NULL, OPT_LIST_LANGS, "list all recognised languages", NULL},
		{"dump", 0, POPT_ARG_NONE, NULL, OPT_DUMP_PROFILE, "dump a profile entry", NULL},
		{"attr", 'a', POPT_ARG_STRING, NULL, OPT_DUMP_ATTR, "print an attribute value", "VALUE"},
		{"dump-data", 0, POPT_ARG_NONE, NULL, OPT_DUMPDATA, "dump the hex data", NULL},
		{"debuglevel", 'd', POPT_ARG_STRING, NULL, OPT_DEBUGLEVEL, "set the debug level", "LEVEL"},
		{"getfqdn", 0, POPT_ARG_NONE, NULL, OPT_GETFQDN, "returns the DNS FQDN of the NSPI server matching the legacyDN", NULL},
		POPT_OPENCHANGE_VERSION
		{ NULL, 0, POPT_ARG_NONE, NULL, 0, NULL, NULL }
	};

	mem_ctx = talloc_named(NULL, 0, "mapiprofile");

	pc = poptGetContext("mapiprofile", argc, argv, long_options, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch(opt) {
		case OPT_DUMPDATA:
			opt_dumpdata = true;
			break;
		case OPT_DEBUGLEVEL:
			opt_debuglevel = poptGetOptArg(pc);
			break;
		case OPT_PROFILE_LDIF:
			opt_tmp = poptGetOptArg(pc);
			ldif = talloc_strdup(mem_ctx, opt_tmp);
			free((void*)opt_tmp);
			opt_tmp = NULL;
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
			opt_tmp = poptGetOptArg(pc);
			profdb = talloc_strdup(mem_ctx, opt_tmp); 
			free((void*)opt_tmp);
			opt_tmp = NULL;
			break;
		case OPT_PROFILE:
			profname = poptGetOptArg(pc);
			break;
		case OPT_ADDRESS:
			address = poptGetOptArg(pc);
			break;
		case OPT_WORKSTATION:
			opt_tmp = poptGetOptArg(pc);
			workstation = talloc_strdup(mem_ctx, opt_tmp);
			free((void*)opt_tmp);
			opt_tmp = NULL;
			break;
		case OPT_DOMAIN:
			domain = poptGetOptArg(pc);
			break;
		case OPT_REALM:
			realm = poptGetOptArg(pc);
			break;
		case OPT_ENCRYPT_CONN:
			opt_seal = true;
			break;
		case OPT_USERNAME:
			username = poptGetOptArg(pc);
			break;
		case OPT_LCID:
			opt_tmp = poptGetOptArg(pc);
			lcid = talloc_strdup(mem_ctx, opt_tmp);
			free((void*)opt_tmp);
			opt_tmp = NULL;
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
		case OPT_RENAME_PROFILE:
			rename = poptGetOptArg(pc);
			break;
		case OPT_LIST_PROFILE:
			list = true;
			break;
		case OPT_LIST_LANGS:
			listlangs = true;
			break;
		case OPT_DUMP_PROFILE:
			dump = true;
			break;
		case OPT_DUMP_ATTR:
			attribute = poptGetOptArg(pc);
			break;
		case OPT_GETFQDN:
			getfqdn = true;
			break;
		}
	}

	/* Sanity check on options */
	if (!profdb) {
		default_path = talloc_asprintf(mem_ctx, DEFAULT_DIR, getenv("HOME"));
		error = mkdir(default_path, 0700);
		talloc_free(default_path);
		if ((error == -1) && (errno != EEXIST)) {
			perror("mkdir");
			retcode = EXIT_FAILURE;
			goto cleanup;
		}
		profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB, 
					 getenv("HOME"));
	}

	if ((list == false) && (getfqdn == false) && (newdb == false) && (listlangs == false)
	    && (getdflt == false) && (dump == false) && (rename == NULL) && 
	    (!attribute) && (!profname || !profdb)) {
		poptPrintUsage(pc, stderr, 0);
		retcode = EXIT_FAILURE;
		goto cleanup;
	}

	if (newdb == true) {
		if (!ldif) {
			ldif = talloc_strdup(mem_ctx, mapi_profile_get_ldif_path());
		}
		if (!ldif) show_help(pc, "ldif");
		if (!mapiprofile_createdb(profdb, ldif)) {
			retcode = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* Process the code here */

	if (!workstation) {
		gethostname(hostname, sizeof(hostname) - 1);
		hostname[sizeof(hostname) - 1] = 0;
		workstation = hostname;
	}

	if (create == true) {
		if (!profname) show_help(pc, "profile");
		if (!password) show_help(pc, "password");
		if (!username) show_help(pc, "username");
		if (!address) show_help(pc, "address");
		if (!domain) show_help(pc, "domain");

		if (!lcid) {
		  lcid = talloc_strdup(mem_ctx, DEFAULT_LCID);
		}
		if (! mapiprofile_create(profdb, profname, pattern, username, password, address,
					 lcid, workstation, domain, realm, nopass, opt_seal, 
					 opt_dumpdata, opt_debuglevel) ) {
			retcode = EXIT_FAILURE;
			goto cleanup;
		}
	}

	if (rename) {
		mapiprofile_rename(profdb, profname, rename);
	}

	if (getfqdn == true) {
		mapiprofile_get_fqdn(profdb, profname, password, opt_dumpdata);
	}

	if (listlangs == true) {
		lcid_print_languages();
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

cleanup:
	free((void*)opt_debuglevel);
	free((void*)profname);
	free((void*)address);
	free((void*)domain);
	free((void*)realm);
	free((void*)username);
	free((void*)pattern);
	free((void*)password);
	free((void*)rename);
	free((void*)attribute);

	poptFreeContext(pc);
	talloc_free(mem_ctx);

	return retcode;
}
