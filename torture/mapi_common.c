/*
   OpenChange MAPI implementation testsuite

   Common MAPI and torture related operations

   Copyright (C) Fabien Le Mentec 2007
   
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
#include <torture/torture.h>
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

char **get_cmdline_recipients(TALLOC_CTX *mem_ctx, const char *type)
{
	char		**usernames;
	const char	*recipients;
	char		*tmp = NULL;
	uint32_t	j = 0;

	if (!type) {
		return 0;
	}

	recipients = lp_parm_string(-1, "mapi", type);

	/* no recipients */
	if (recipients == 0) {
		printf("no recipients specified for %s\n", type);
		return 0;
	}

	if ((tmp = strtok((char *)recipients, ",")) == NULL) {
		DEBUG(2, ("Invalid mapi:%s string format\n", type));
		return NULL;
	}
	
	usernames = talloc_array(mem_ctx, char *, 2);
	usernames[0] = strdup(tmp);
	
	for (j = 1; (tmp = strtok(NULL, ",")) != NULL; j++) {
		usernames = talloc_realloc(mem_ctx, usernames, char *, j+2);
		usernames[j] = strdup(tmp);
	}
	usernames[j] = 0;

	return (usernames);
}

char **collapse_recipients(TALLOC_CTX *mem_ctx, char **to, char **cc, char **bcc)
{
	uint32_t	count;
	uint32_t       	i;
	char		**usernames;

	if (!to && !cc && !bcc) return NULL;

	count = 0;
	for (i = 0; to && to[i]; i++,  count++);
	for (i = 0; cc && cc[i]; i++,  count++);
	for (i = 0; bcc && bcc[i]; i++, count++);

	usernames = talloc_array(mem_ctx, char *, count + 1);
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

BOOL set_usernames_RecipientType(uint32_t *index, struct SRowSet *rowset, char **usernames, struct FlagList *flaglist,
				 enum ulRecipClass RecipClass)
{
	uint32_t	i;
	uint32_t	count = *index;

	if (!usernames) return False;

	for (i = 0; usernames[i]; i++) {
		if (flaglist->ulFlags[count] == MAPI_RESOLVED) {
			SetRecipientType(&(rowset->aRow[count]), RecipClass);
			count++;
		}
	}
	
	*index = count;
	
	return True;
}

struct mapi_session *torture_init_mapi(TALLOC_CTX *mem_ctx)
{
	enum MAPISTATUS		retval;
	struct mapi_session	*session;
	const char		*profdb;
	const char		*profname;

	profdb = lp_parm_string(-1, "mapi", "profile_store");
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


	profname = lp_parm_string(-1, "mapi", "profile");
	if (!profname) {
		retval = GetDefaultProfile(&profname, 0);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("Please specify a valid profile\n"));
			return NULL;
		}
	}

	retval = MapiLogonEx(&session, profname);
	mapi_errstr("MapiLogonEx", GetLastError());
	if (retval != MAPI_E_SUCCESS) return NULL;

	return session;
}

uint64_t GetCalendarFID(struct SRowSet SRowSet)
{
	uint32_t	i;

	for (i = 0; i < SRowSet.cRows; i++) {
		if (SRowSet.aRow[i].lpProps[1].value.lpszA && 
		    !strcmp(SRowSet.aRow[i].lpProps[1].value.lpszA, "Calendar")) {
			return SRowSet.aRow[i].lpProps[0].value.d;
		}
	}
	return 0;
}

uint64_t GetContactsFID(struct SRowSet SRowSet)
{
	uint32_t	i;

	for (i = 0; i < SRowSet.cRows; i++) {
		if (SRowSet.aRow[i].lpProps[1].value.lpszA && 
		    !strcmp(SRowSet.aRow[i].lpProps[1].value.lpszA, "Contacts")) {
			return SRowSet.aRow[i].lpProps[0].value.d;
		}
	}
	return 0;
}

uint64_t GetTasksFID(struct SRowSet SRowSet)
{
	uint32_t	i;

	for (i = 0; i < SRowSet.cRows; i++) {
		if (SRowSet.aRow[i].lpProps[1].value.lpszA && 
		    !strcmp(SRowSet.aRow[i].lpProps[1].value.lpszA, "Tasks")) {
			return SRowSet.aRow[i].lpProps[0].value.d;
		}
	}
	return 0;
}
