/*
   OpenChange MAPI torture suite implementation.

   Common MAPI and torture related operations

   Copyright (C) Julien Kerihuel 2007.
   Copyright (C) Fabien Le Mentec 2007.

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
#include <torture/mapi_torture.h>
#include <torture.h>
#include <torture/torture_proto.h>
#include <torture/mapi_torture.h>
#include <param.h>

const char *get_filename(const char *filename)
{
	const char *substr;

	substr = rindex(filename, '/');
	if (substr) return substr;

	return filename;
}

const char **get_cmdline_recipients(TALLOC_CTX *mem_ctx, const char *type)
{
	const char	**usernames;
	const char	*recipients;
	char		*tmp = NULL;
	uint32_t	j = 0;

	if (!type) {
		return 0;
	}

	recipients = lp_parm_string(global_mapi_ctx->lp_ctx, NULL, "mapi", type);

	/* no recipients */
	if (recipients == 0) {
		printf("no recipients specified for %s\n", type);
		return 0;
	}

	if ((tmp = strtok((char *)recipients, ",")) == NULL) {
		DEBUG(2, ("Invalid mapi:%s string format\n", type));
		return NULL;
	}
	
	usernames = talloc_array(mem_ctx, const char *, 2);
	usernames[0] = strdup(tmp);
	
	for (j = 1; (tmp = strtok(NULL, ",")) != NULL; j++) {
		usernames = talloc_realloc(mem_ctx, usernames, const char *, j+2);
		usernames[j] = strdup(tmp);
	}
	usernames[j] = 0;

	return usernames;
}

const char **collapse_recipients(TALLOC_CTX *mem_ctx, const char **to, const char **cc, const char **bcc)
{
	uint32_t	count;
	uint32_t       	i;
	const char	**usernames;

	if (!to && !cc && !bcc) return NULL;

	count = 0;
	for (i = 0; to && to[i]; i++,  count++);
	for (i = 0; cc && cc[i]; i++,  count++);
	for (i = 0; bcc && bcc[i]; i++, count++);

	usernames = talloc_array(mem_ctx, const char *, count + 1);
	count = 0;

	for (i = 0; to && to[i]; i++, count++) {
		usernames[count] = talloc_strdup(mem_ctx, to[i]);
	}

	for (i = 0; cc && cc[i]; i++, count++) {
		usernames[count] = talloc_strdup(mem_ctx, cc[i]);
	}

	for (i = 0; bcc && bcc[i]; i++, count++) {
		usernames[count] = talloc_strdup(mem_ctx, bcc[i]);
	}

	usernames[count++] = 0;

	return usernames;
}

static bool set_external_recipients(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, const char *username, enum ulRecipClass RecipClass)
{
	uint32_t		last;
	struct SPropValue	SPropValue;

	SRowSet->aRow = talloc_realloc(mem_ctx, SRowSet->aRow, struct SRow, SRowSet->cRows + 2);
	last = SRowSet->cRows;
	SRowSet->aRow[last].cValues = 0;
	SRowSet->aRow[last].lpProps = talloc_zero(mem_ctx, struct SPropValue);
	
	/* PR_OBJECT_TYPE */
	SPropValue.ulPropTag = PR_OBJECT_TYPE;
	SPropValue.value.l = MAPI_MAILUSER;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_TYPE */
	SPropValue.ulPropTag = PR_DISPLAY_TYPE;
	SPropValue.value.l = 0;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_GIVEN_NAME */
	SPropValue.ulPropTag = PR_GIVEN_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_DISPLAY_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_7BIT_DISPLAY_NAME */
	SPropValue.ulPropTag = PR_7BIT_DISPLAY_NAME;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_SMTP_ADDRESS */
	SPropValue.ulPropTag = PR_SMTP_ADDRESS;
	SPropValue.value.lpszA = username;
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	/* PR_ADDRTYPE */
	SPropValue.ulPropTag = PR_ADDRTYPE;
	SPropValue.value.lpszA = "SMTP";
	SRow_addprop(&(SRowSet->aRow[last]), SPropValue);

	SetRecipientType(&(SRowSet->aRow[last]), RecipClass);

	SRowSet->cRows += 1;
	return true;
}

bool set_usernames_RecipientType(TALLOC_CTX *mem_ctx, uint32_t *index, struct SRowSet *rowset, 
					const char **usernames, struct SPropTagArray *flaglist,
					enum ulRecipClass RecipClass)
{
	uint32_t	i;
	uint32_t	count = *index;
	static uint32_t	counter = 0;

	if (count == 0) counter = 0;
	if (!usernames) return false;

	for (i = 0; usernames[i]; i++) {
		if (flaglist->aulPropTag[count] == MAPI_UNRESOLVED) {
			set_external_recipients(mem_ctx, rowset, usernames[i], RecipClass);
		}
		if (flaglist->aulPropTag[count] == MAPI_RESOLVED) {
			SetRecipientType(&(rowset->aRow[counter]), RecipClass);
			counter++;
		}
		count++;
	}
	
	*index = count;
	
	return true;
}

/**
 * Initialize MAPI and Load Profile
 *
 */
enum MAPISTATUS torture_load_profile(TALLOC_CTX *mem_ctx, 
				     struct loadparm_context *lp_ctx,
				     struct mapi_session **s)
{
	enum MAPISTATUS		retval;
	const char		*profdb;
	char			*profname;
	struct mapi_session	*session = NULL;

	profdb = lp_parm_string(lp_ctx, NULL, "mapi", "profile_store");
	if (!profdb) {
		profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB_PATH, getenv("HOME"));
		if (!profdb) {
			DEBUG(0, ("Specify a valid MAPI profile store\n"));
			return MAPI_E_NOT_FOUND;
		}
	}

	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	MAPI_RETVAL_IF(retval, retval, NULL);

	profname = talloc_strdup(mem_ctx, lp_parm_string(lp_ctx, NULL, "mapi", "profile"));
	if (!profname) {
		retval = GetDefaultProfile(&profname);
		MAPI_RETVAL_IF(retval, retval, NULL);
	}

	/* MapiLogonProvider returns MAPI_E_NO_SUPPORT: reset errno */
	retval = MapiLogonProvider(&session, profname, NULL, PROVIDER_ID_NSPI);
	talloc_free(profname);
	errno = 0;

	retval = LoadProfile(session->profile);
	MAPI_RETVAL_IF(retval, retval, NULL);

	*s = session;

	return MAPI_E_SUCCESS;
}

/**
 * Initialize MAPI and logs on the EMSMDB pipe with the default
 * profile
 */

struct mapi_session *torture_init_mapi(TALLOC_CTX *mem_ctx, 
				       struct loadparm_context *lp_ctx)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session;
	const char		*profdb;
	char			*profname;
	const char		*password;

	profdb = lp_parm_string(lp_ctx, NULL, "mapi", "profile_store");
	if (!profdb) {
		profdb = talloc_asprintf(mem_ctx, DEFAULT_PROFDB_PATH, getenv("HOME"));
		if (!profdb) {
			DEBUG(0, ("Specify a valid MAPI profile store\n"));
			return NULL;
		}
	}

	retval = MAPIInitialize(profdb);
	mapi_errstr("MAPIInitialize", GetLastError());
	if (retval != MAPI_E_SUCCESS) return NULL;


	profname = talloc_strdup(mem_ctx, lp_parm_string(lp_ctx, NULL, "mapi", "profile"));
	if (!profname) {
		retval = GetDefaultProfile(&profname);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("Please specify a valid profile\n"));
			return NULL;
		}
	}

	password = lp_parm_string(lp_ctx, NULL, "mapi", "password");
	retval = MapiLogonEx(&session, profname, password);
	talloc_free(profname);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return NULL;

	return session;
}

enum MAPISTATUS torture_simplemail_fromme(struct loadparm_context *lp_ctx,
					  mapi_object_t *obj_parent, 
					  const char *subject, const char *body,
					  uint32_t flags)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval;
	mapi_object_t		obj_message;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SPropValue	SPropValue;
	struct SRowSet		*SRowSet = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	struct SPropValue	props[3];
	struct mapi_session	*session = NULL;
	const char	       	**usernames;
	uint32_t		index = 0;

	mem_ctx = talloc_named(NULL, 0, "torture_simplemail");

	session = mapi_object_get_session(obj_parent);
	MAPI_RETVAL_IF(!session, MAPI_E_NOT_INITIALIZED, NULL);

	mapi_object_init(&obj_message);
	retval = CreateMessage(obj_parent, &obj_message);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	SPropTagArray  = set_SPropTagArray(mem_ctx, 0x6,
					   PR_OBJECT_TYPE,
					   PR_DISPLAY_TYPE,
					   PR_7BIT_DISPLAY_NAME,
					   PR_DISPLAY_NAME,
					   PR_SMTP_ADDRESS,
					   PR_GIVEN_NAME);

	lp_set_cmdline(lp_ctx, "mapi:to", session->profile->username);
	usernames = get_cmdline_recipients(mem_ctx, "to");

	retval = ResolveNames(session, usernames, SPropTagArray, &SRowSet, &flaglist, 0);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	if (!SRowSet) {
		SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	}

	set_usernames_RecipientType(mem_ctx, &index, SRowSet, usernames, flaglist, MAPI_TO);

	SPropValue.ulPropTag = PR_SEND_INTERNET_ENCODING;
	SPropValue.value.l = 0;
	SRowSet_propcpy(mem_ctx, SRowSet, SPropValue);

	retval = ModifyRecipients(&obj_message, SRowSet);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = MAPIFreeBuffer(SRowSet);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);
	retval = MAPIFreeBuffer(flaglist);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	set_SPropValue_proptag(&props[0], PR_SUBJECT, (const void *)subject);
	set_SPropValue_proptag(&props[1], PR_BODY, (const void *)body);
	set_SPropValue_proptag(&props[2], PR_MESSAGE_FLAGS, (const void *)&flags);
	retval = SetProps(&obj_message, props, 3);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	retval = SaveChangesMessage(obj_parent, &obj_message, KeepOpenReadOnly);
	MAPI_RETVAL_IF(retval, retval, mem_ctx);

	mapi_object_release(&obj_message);

	talloc_free(mem_ctx);
	return MAPI_E_SUCCESS;
}

uint32_t get_permission_from_name(const char *role)
{
	if (!role) return -1;

	if (!strncasecmp(role, "RightsNone", strlen(role))) return 0x0;
	if (!strncasecmp(role, "RightsReadItems", strlen(role))) return 0x1;
	if (!strncasecmp(role, "RightsCreateItems", strlen(role))) return 0x2;
	if (!strncasecmp(role, "RightsEditOwn", strlen(role))) return 0x8;
	if (!strncasecmp(role, "RightsDeleteOwn", strlen(role))) return 0x10;
	if (!strncasecmp(role, "RightsEditAll", strlen(role))) return 0x20;
	if (!strncasecmp(role, "RightsDeleteAll", strlen(role))) return 0x40;
	if (!strncasecmp(role, "RightsCreateSubfolders", strlen(role))) return 0x80;
	if (!strncasecmp(role, "RightsFolderOwner", strlen(role))) return 0x100;
	if (!strncasecmp(role, "RightsFolderContact", strlen(role))) return 0x200;
	if (!strncasecmp(role, "RoleNone", strlen(role))) return 0x400;
	if (!strncasecmp(role, "RoleReviewer", strlen(role))) return 0x401;
	if (!strncasecmp(role, "RoleContributor", strlen(role))) return 0x402;
	if (!strncasecmp(role, "RoleNoneditingAuthor", strlen(role))) return 0x413;
	if (!strncasecmp(role, "RoleAuthor", strlen(role))) return 0x41B;
	if (!strncasecmp(role, "RoleEditor", strlen(role))) return 0x47B;
	if (!strncasecmp(role, "RolePublishAuthor", strlen(role))) return 0x49B;
	if (!strncasecmp(role, "RolePublishEditor", strlen(role))) return 0x4FB;
	if (!strncasecmp(role, "RightsAll", strlen(role))) return 0x5FB;
	if (!strncasecmp(role, "RoleOwner", strlen(role))) return 0x7FB;

	return -1;
}
