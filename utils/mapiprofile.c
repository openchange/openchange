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

#include "libmapi/libmapi.h"
#include <popt.h>
#include <param.h>
#include "openchange-tools.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>

#define	DEFAULT_DIR			"%s/.openchange"
#define	DEFAULT_PROFDB			"%s/.openchange/profiles.ldb"
#define	DEFAULT_EXCHANGE_VERSION	"2000"

/**
   MAPI version:
   #- 0: use EcDoConnect (0x0) / EcDoRpc (0x2) RPC calls
   #- 1: use EcDoConnectEx (0xA) / EcDoRpcExt2 (0xB) RPC calls
   #- 2: Same as 1, but require sealed pipe
 */
struct exchange_version {
	const char	*name;
	uint8_t		version;
};

static const struct exchange_version exchange_version[] = {
	{ DEFAULT_EXCHANGE_VERSION,	0 },
	{ "2003",			1 },
	{ "2007",			1 },
	{ "2010",			2 },
	{ NULL,				0 }
};

static bool mapiprofile_createdb(const char *profdb, const char *ldif_path)
{
	enum MAPISTATUS retval;

	if (access(profdb, F_OK) == 0) {
		fprintf(stderr, "[ERROR] mapiprofile: %s already exists\n", profdb);
		return false;
	}

	retval = CreateProfileStore(profdb, ldif_path);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("CreateProfileStore", retval);
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
struct mapi_context *g_mapi_ctx;

static void signal_delete_profile(void)
{
	enum MAPISTATUS	retval;

	fprintf(stderr, "CTRL-C caught ... Deleting profile\n");
	if ((retval = DeleteProfile(g_mapi_ctx, g_profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("DeleteProfile", retval);
	}

	(void) signal(SIGINT, SIG_DFL);
	exit (1);
}

static bool mapiprofile_create(struct mapi_context *mapi_ctx,
			       const char *profdb, const char *profname,
			       const char *pattern, const char *username, 
			       const char *password, const char *address, 
			       const char *language, const char *workstation,
			       const char *domain, const char *realm,
			       uint32_t flags, bool seal,
			       bool opt_dumpdata, const char *opt_debuglevel,
			       uint8_t exchange_version, const char *kerberos,
			       bool roh, bool roh_tls, uint32_t roh_rpc_proxy_port,
			       const char *roh_rpc_proxy_server)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	TALLOC_CTX		*mem_ctx;
	struct mapi_profile	*profile;
	const char		*locale;
	uint32_t		cpid = 0;
	uint32_t		lcid = 0;
	char			*exchange_version_str;
	char			*cpid_str;
	char			*lcid_str;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_create");
	profile = talloc(mem_ctx, struct mapi_profile);

	/* catch CTRL-C */
	g_profname = profname;
	g_mapi_ctx = mapi_ctx;

#if defined (__FreeBSD__)
	(void) signal(SIGINT, (sig_t) signal_delete_profile);
#elif defined (__SunOS)
        (void) signal(SIGINT, signal_delete_profile);
#else
	(void) signal(SIGINT, (sighandler_t) signal_delete_profile);
#endif

	/* debug options */
	SetMAPIDumpData(mapi_ctx, opt_dumpdata);

	if (opt_debuglevel) {
		SetMAPIDebugLevel(mapi_ctx, atoi(opt_debuglevel));
	}

	/* Sanity check */
	retval = OpenProfile(mapi_ctx, profile, profname, NULL);
	if (retval == MAPI_E_SUCCESS) {
		fprintf(stderr, "[ERROR] mapiprofile: profile \"%s\" already exists\n", profname);
		talloc_free(mem_ctx);
		return false;
	}

	retval = CreateProfile(mapi_ctx, profname, username, password, flags);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("CreateProfile", retval);
		talloc_free(mem_ctx);
		return false;
	}

	mapi_profile_add_string_attr(mapi_ctx, profname, "binding", address);
	mapi_profile_add_string_attr(mapi_ctx, profname, "workstation", workstation);
	mapi_profile_add_string_attr(mapi_ctx, profname, "domain", domain);
	mapi_profile_add_string_attr(mapi_ctx, profname, "seal", (seal == true) ? "true" : "false");
	mapi_profile_add_string_attr(mapi_ctx, profname, "roh", (roh == true) ? "true" : "false");
	if (roh) {
		char *roh_rpc_proxy_port_str = NULL;

		roh_rpc_proxy_port_str = talloc_asprintf(mem_ctx, "%d", roh_rpc_proxy_port);
		if (roh_rpc_proxy_port_str == NULL) {
			mapi_errstr("CreateProfile", retval);
			talloc_free(mem_ctx);
			return NULL;
		}
		mapi_profile_add_string_attr(mapi_ctx, profname, "roh_tls", (roh_tls == true) ? "true" : "false");
		mapi_profile_add_string_attr(mapi_ctx, profname, "roh_rpc_proxy_server", roh_rpc_proxy_server);
		mapi_profile_add_string_attr(mapi_ctx, profname, "roh_rpc_proxy_port", roh_rpc_proxy_port_str);
		talloc_free(roh_rpc_proxy_port_str);
	}

	exchange_version_str = talloc_asprintf(mem_ctx, "%d", exchange_version);
	mapi_profile_add_string_attr(mapi_ctx, profname, "exchange_version", exchange_version_str);
	talloc_free(exchange_version_str);

	if (realm) {
		mapi_profile_add_string_attr(mapi_ctx, profname, "realm", realm);
	}

	locale = (const char *) (language) ? mapi_get_locale_from_language(language) : mapi_get_system_locale();

	if (locale) {
		cpid = mapi_get_cpid_from_locale(locale);
		lcid = mapi_get_lcid_from_locale(locale);
	}

	if (!locale || !cpid || !lcid) {
		printf("Invalid Language supplied or unknown system language '%s\n'", language);
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", retval);
		}
		talloc_free(mem_ctx);
		return false;
	}

	cpid_str = talloc_asprintf(mem_ctx, "%d", cpid);
	lcid_str = talloc_asprintf(mem_ctx, "%d", lcid);

	mapi_profile_add_string_attr(mapi_ctx, profname, "codepage", cpid_str);
	mapi_profile_add_string_attr(mapi_ctx, profname, "language", lcid_str);
	mapi_profile_add_string_attr(mapi_ctx, profname, "method", lcid_str);

	talloc_free(cpid_str);
	talloc_free(lcid_str);

	if (kerberos) {
		mapi_profile_add_string_attr(mapi_ctx, profname, "kerberos", kerberos);
	}

	retval = MapiLogonProvider(mapi_ctx, &session, profname, password, PROVIDER_ID_NSPI);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonProvider", retval);
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", retval);
		}
		talloc_free(mem_ctx);
		return false;
	}

	if (pattern) {
		username = pattern;
	}

	retval = ProcessNetworkProfile(session, username, (mapi_profile_callback_t) callback, "Select a user id");
	if (retval != MAPI_E_SUCCESS && retval != 0x1) {
		mapi_errstr("ProcessNetworkProfile", retval);
		printf("Deleting profile\n");
		if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("DeleteProfile", retval);
		}
		talloc_free(mem_ctx);
		return false;
	}

	printf("Profile %s completed and added to database %s\n", profname, profdb);

	talloc_free(mem_ctx);

	return true;
}

static void mapiprofile_delete(struct mapi_context *mapi_ctx, const char *profdb, const char *profname)
{
	enum MAPISTATUS		retval;

	if ((retval = DeleteProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("DeleteProfile", retval);
		exit (1);
	}

	printf("Profile %s deleted from database %s\n", profname, profdb);
}


static void mapiprofile_rename(struct mapi_context *mapi_ctx, const char *profdb, const char *old_profname, const char *new_profname)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	char			*profname;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_rename");

	if (!old_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, old_profname);
	}

	if ((retval = RenameProfile(mapi_ctx, profname, new_profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("RenameProfile", retval);
		talloc_free(profname);
		talloc_free(mem_ctx);
		exit (1);
	}

	talloc_free(profname);
	talloc_free(mem_ctx);
}


static void mapiprofile_set_default(struct mapi_context *mapi_ctx, const char *profdb, const char *profname)
{
	enum MAPISTATUS		retval;

	if ((retval = SetDefaultProfile(mapi_ctx, profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("SetDefaultProfile", retval);
		exit (1);
	}

	printf("Profile \"%s\" is now the default profile\n", profname);
}

static void mapiprofile_get_default(struct mapi_context *mapi_ctx, const char *profdb)
{
	enum MAPISTATUS		retval;
	char			*profname;

	if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetDefaultProfile", retval);
		exit (1);
	}

	printf("Default profile is set to %s\n", profname);

	talloc_free(profname);
}

static void mapiprofile_get_fqdn(struct mapi_context *mapi_ctx,
				 const char *profdb,
				 const char *opt_profname,
				 const char *password,
				 bool opt_dumpdata)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_session	*session = NULL;
	const char		*serverFQDN;
	char			*profname;

	SetMAPIDumpData(mapi_ctx, opt_dumpdata);

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_get_fqdn");

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (char *)opt_profname);
	}

	retval = MapiLogonProvider(mapi_ctx, &session, profname, password, PROVIDER_ID_NSPI);
	talloc_free(profname);
	talloc_free(mem_ctx);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("MapiLogonProvider", retval);
		exit (1);
	}

	retval = RfrGetFQDNFromLegacyDN(mapi_ctx, session, &serverFQDN);
	if (retval != MAPI_E_SUCCESS) {
		mapi_errstr("RfrGetFQDNFromLegacyDN", retval);
		exit (1);
	}

	printf("%s is at %s\n", mapi_ctx->session->profile->homemdb, serverFQDN);
}

static void mapiprofile_list(struct mapi_context *mapi_ctx, const char *profdb)
{
	enum MAPISTATUS		retval;
	struct SRowSet		proftable;
	uint32_t		count = 0;

	memset(&proftable, 0, sizeof (struct SRowSet));
	if ((retval = GetProfileTable(mapi_ctx, &proftable)) != MAPI_E_SUCCESS) {
		mapi_errstr("GetProfileTable", retval);
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
}

static void mapiprofile_dump(struct mapi_context *mapi_ctx, const char *profdb, const char *opt_profname)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_profile	*profile;
	char			*profname;
	char			*exchange_version = NULL;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_dump");
	profile = talloc(mem_ctx, struct mapi_profile);

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			talloc_free(mem_ctx);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(mapi_ctx, profile, profname, NULL);
	talloc_free(profname);

	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		talloc_free(mem_ctx);
		mapi_errstr("OpenProfile", retval);
		exit (1);
	}

	switch (profile->exchange_version) {
	case 0x0:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2000");
		break;
	case 0x1:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2003/2007");
		break;
	case 0x2:
		exchange_version = talloc_strdup(mem_ctx, "exchange 2010");
		break;
	default:
		printf("Error: unknown Exchange server\n");
		goto end;
	}

	printf("Profile: %s\n", profile->profname);
	printf("\texchange server == %s\n", exchange_version);
	printf("\tencryption      == %s\n", (profile->seal == true) ? "yes" : "no");
	printf("\tusername        == %s\n", profile->username);
	printf("\tpassword        == %s\n", profile->password);
	printf("\tmailbox         == %s\n", profile->mailbox);
	printf("\tworkstation     == %s\n", profile->workstation);
	printf("\tdomain          == %s\n", profile->domain);
	printf("\tserver          == %s\n", profile->server);
	printf("\n");
	printf("\tRPC over HTTP              == %s\n", (profile->roh == true) ? "yes" : "no");
        if (profile->roh) {
                printf("\tRPC over HTTP use TLS      == %s\n", (profile->roh_tls == true) ? "yes" : "no");
                printf("\tRPC over HTTP proxy server == %s\n", profile->roh_rpc_proxy_server);
                printf("\tRPC over HTTP proxy port   == %d\n", profile->roh_rpc_proxy_port);
        }
end:
	talloc_free(mem_ctx);
}

static void mapiprofile_attribute(struct mapi_context *mapi_ctx, const char *profdb, const char *opt_profname, 
				  const char *attribute)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	struct mapi_profile	*profile;
	char			*profname = NULL;
	char			**value = NULL;
	unsigned int		count = 0;
	unsigned int		i;

	mem_ctx = talloc_named(mapi_ctx->mem_ctx, 0, "mapiprofile_attribute");
	profile = talloc(mem_ctx, struct mapi_profile);

	if (!opt_profname) {
		if ((retval = GetDefaultProfile(mapi_ctx, &profname)) != MAPI_E_SUCCESS) {
			mapi_errstr("GetDefaultProfile", retval);
			exit (1);
		}
	} else {
		profname = talloc_strdup(mem_ctx, (const char *)opt_profname);
	}

	retval = OpenProfile(mapi_ctx, profile, profname, NULL);
	if (retval && (retval != MAPI_E_INVALID_PARAMETER)) {
		mapi_errstr("OpenProfile", retval);
		talloc_free(profname);
		talloc_free(mem_ctx);
		exit (1);
	}

	if ((retval = GetProfileAttr(profile, attribute, &count, &value))) {
		mapi_errstr("ProfileGetAttr", retval);
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
	struct mapi_context *mapi_ctx = NULL;
	int		error;
	poptContext	pc;
	int		opt;
	int		i;
	char		*default_path;
	bool		found = false;
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
	bool		opt_roh = false;
	bool		opt_roh_tls = true;
	const char	*opt_debuglevel = NULL;
	const char	*ldif = NULL;
	const char	*address = NULL;
	const char	*workstation = NULL;
	const char	*domain = NULL;
	const char	*realm = NULL;
	const char	*username = NULL;
	const char      *language = NULL;
	const char	*pattern = NULL;
	const char	*password = NULL;
	const char	*profdb = NULL;
	const char	*profname = NULL;
	const char	*rename = NULL;
	const char	*attribute = NULL;
	const char	*opt_tmp = NULL;
	const char	*opt_krb = NULL;
	const char	*version = NULL;
	const char	*opt_roh_rpc_proxy_server = NULL;
	const char	*opt_roh_rpc_proxy_port = NULL;
	uint32_t	nopass = 0;
	uint32_t	roh_rpc_proxy_port = 443;
	char		hostname[256];
	int		retcode = EXIT_SUCCESS;

	enum {OPT_PROFILE_DB=1000, OPT_PROFILE, OPT_ADDRESS, OPT_WORKSTATION,
	      OPT_DOMAIN, OPT_REALM, OPT_USERNAME, OPT_LANGUAGE, OPT_PASSWORD, 
	      OPT_CREATE_PROFILE, OPT_DELETE_PROFILE, OPT_LIST_PROFILE, OPT_DUMP_PROFILE, 
	      OPT_DUMP_ATTR, OPT_PROFILE_NEWDB, OPT_PROFILE_LDIF, OPT_LIST_LANGS,
	      OPT_PROFILE_SET_DFLT, OPT_PROFILE_GET_DFLT, OPT_PATTERN, OPT_GETFQDN,
	      OPT_NOPASS, OPT_RENAME_PROFILE, OPT_DUMPDATA, OPT_DEBUGLEVEL,
	      OPT_ENCRYPT_CONN, OPT_EXCHANGE_VERSION, OPT_KRB,
	      OPT_ROH, OPT_ROH_NO_TLS, OPT_ROH_RPC_PROXY_PORT,
	      OPT_ROH_RPC_PROXY_SERVER};

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
		{"exchange-version", 'v', POPT_ARG_STRING, NULL, OPT_EXCHANGE_VERSION, "specify Exchange server version", "2000" },
		{"username", 'u', POPT_ARG_STRING, NULL, OPT_USERNAME, "set the profile username", "USERNAME"},
		{"language", 'C', POPT_ARG_STRING, NULL, OPT_LANGUAGE, "set the user's language (if different from system one)", "LANGUAGE"},
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
		{"kerberos", 'k', POPT_ARG_STRING, NULL, OPT_KRB, "specify kerberos behavior (guess by default)", "{yes|no}"},
		{"roh", 'H', POPT_ARG_NONE, NULL, OPT_ROH, "connect to Exchange server over HTTP (RPC over HTTP)", NULL },
		{"roh-no-tls", 0, POPT_ARG_NONE, NULL, OPT_ROH_NO_TLS, "Do not use TLS to connect to Exchange server over HTTP", NULL },
		{"roh-rpc-proxy-port", 0, POPT_ARG_STRING, NULL, OPT_ROH_RPC_PROXY_PORT, "RPC over HTTP proxy port", "443" },
		{"roh-rpc-proxy", 0, POPT_ARG_STRING, NULL, OPT_ROH_RPC_PROXY_SERVER, "RPC over HTTP proxy server", NULL },
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
		case OPT_EXCHANGE_VERSION:
			opt_tmp = poptGetOptArg(pc);
			version = talloc_strdup(mem_ctx, opt_tmp);
			free((void *)opt_tmp);
			opt_tmp = NULL;
			break;
		case OPT_USERNAME:
			username = poptGetOptArg(pc);
			break;
		case OPT_LANGUAGE:
			opt_tmp = poptGetOptArg(pc);
			language = talloc_strdup(mem_ctx, opt_tmp);
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
		case OPT_KRB:
			opt_krb = poptGetOptArg(pc);
			break;
		case OPT_ROH:
			opt_roh = true;
			break;
		case OPT_ROH_NO_TLS:
			opt_roh_tls = false;
			if (roh_rpc_proxy_port == 443) {
				roh_rpc_proxy_port = 80;
			}
			break;
		case OPT_ROH_RPC_PROXY_PORT:
			opt_tmp = poptGetOptArg(pc);
			opt_roh_rpc_proxy_port = talloc_strdup(mem_ctx, opt_tmp);
			if (!opt_roh_rpc_proxy_port) {
				talloc_free(mem_ctx);
				printf("Not enough memory\n");
				exit(1);
			}
			roh_rpc_proxy_port = strtol(opt_roh_rpc_proxy_port, NULL, 10);
			if (!roh_rpc_proxy_port) {
				printf("Cannot parse RPC proxy port number\n");
				retcode = EXIT_FAILURE;
				goto cleanup;
			}
			free((void *)opt_tmp);
			opt_tmp = NULL;
			break;
		case OPT_ROH_RPC_PROXY_SERVER:
			opt_tmp = poptGetOptArg(pc);
			opt_roh_rpc_proxy_server = talloc_strdup(mem_ctx, opt_tmp);
			if (!opt_roh_rpc_proxy_server) {
				talloc_free(mem_ctx);
				printf("Not enough memory\n");
				exit(1);
			}
			free((void *)opt_tmp);
			opt_tmp = NULL;
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

	if (MAPIInitialize(&mapi_ctx, profdb) != MAPI_E_SUCCESS) {
		mapi_errstr("MAPIInitialize", GetLastError());
		exit (1);
	}

	if (opt_krb) {
		if (strncmp(opt_krb, "yes", 3) && strncmp(opt_krb, "no", 2)) {
			fprintf(stderr,
			        "kerberos value must be 'yes' or 'no'\n");
			poptPrintUsage(pc, stderr, 0);
			retcode = EXIT_FAILURE;
			goto cleanup;
		}
	}

	if (opt_roh && opt_roh_rpc_proxy_server == NULL) {
		opt_roh_rpc_proxy_server = talloc_strdup(mem_ctx, address);
	}

	/* Process the code here */

	if (!workstation) {
		gethostname(hostname, sizeof(hostname) - 1);
		hostname[sizeof(hostname) - 1] = 0;
		workstation = hostname;
	}

	if (create == true) {
		if (!profname) show_help(pc, "profile");
		if (!password && !nopass) show_help(pc, "password");
		if (!username) show_help(pc, "username");
		if (!address) show_help(pc, "address");

		if (!version) {
			version = talloc_strdup(mem_ctx, DEFAULT_EXCHANGE_VERSION);
		}

		for (i = 0; exchange_version[i].name; i++) {
			if (!strcasecmp(version, exchange_version[i].name)) {
				version = talloc_strdup(mem_ctx, exchange_version[i].name);
				found = true;
				break;
			}
		}
		if (found == false) {
			printf("Invalid Exchange server version. Possible values are:\n");
			for (i = 0; exchange_version[i].name; i++) {
				printf("\t[*] %s\n", exchange_version[i].name);
			}
			goto cleanup;
		}

		/* Force encrypt parameter if exchange2010 is specified */
		if (!strcasecmp(version, "2010")) {
			opt_seal = true;
		}
		talloc_free((void *)version);

		if (! mapiprofile_create(mapi_ctx, profdb, profname, pattern, username, password, address,
					 language, workstation, domain, realm, nopass, opt_seal, 
					 opt_dumpdata, opt_debuglevel,
					 exchange_version[i].version,
					 opt_krb, opt_roh, opt_roh_tls,
					 roh_rpc_proxy_port,
					 opt_roh_rpc_proxy_server)) {
			retcode = EXIT_FAILURE;
			goto cleanup;
		}
	}

	if (rename) {
		mapiprofile_rename(mapi_ctx, profdb, profname, rename);
	}

	if (getfqdn == true) {
		mapiprofile_get_fqdn(mapi_ctx, profdb, profname, password, opt_dumpdata);
	}

	if (listlangs == true) {
		mapidump_languages_list();
	}

	if (setdflt == true) {
		mapiprofile_set_default(mapi_ctx, profdb, profname);
	}

	if (getdflt == true) {
		mapiprofile_get_default(mapi_ctx, profdb);
	}

	if (delete == true) {
		mapiprofile_delete(mapi_ctx, profdb, profname);
	}

	if (list == true) {
		mapiprofile_list(mapi_ctx, profdb);
	}

	if (dump == true) {
		mapiprofile_dump(mapi_ctx, profdb, profname);
	}

	if (attribute) {
		mapiprofile_attribute(mapi_ctx, profdb, profname, attribute);
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

	if (mapi_ctx)
		MAPIUninitialize(mapi_ctx);
	poptFreeContext(pc);
	talloc_free(mem_ctx);

	return retcode;
}
