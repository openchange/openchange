/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>


static void mapitest_call_OpenMsgStore(struct mapitest *mt)
{
	mapi_object_t	obj_store;
	enum MAPISTATUS	retval;
	uint32_t	attempt = 0;

	mapi_object_init(&obj_store);

 retry:
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() == MAPI_E_CALL_FAILED && attempt < 3) {
		attempt++;
		goto retry;
	}

	mapitest_print_call(mt, "OpenMsgStore", GetLastError());

	if (retval != MAPI_E_SUCCESS) return;

	mapi_object_release(&obj_store);
}


static void mapitest_call_OpenPublicFolder(struct mapitest *mt)
{
	enum MAPISTATUS	retval;
	mapi_object_t	obj_pf;

	mapi_object_init(&obj_pf);

	retval = OpenPublicFolder(&obj_pf);
	mapitest_print_call(mt, "OpenPublicFolder", GetLastError());

	if (retval != MAPI_E_SUCCESS) return;

	mapi_object_release(&obj_pf);
}


static void mapitest_call_ResolveNames(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SRowSet		*SRowSet = NULL;
	struct FlagList		*flaglist = NULL;
	char			**username;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "ResolveNames (NSPI interface call)");
	mapitest_indent();

	/* Test with PR_DISPLAY_NAME retrieved from MapiLogonEx */
	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = mt->info.username;
	username[1] = NULL;

	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x6,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_7BIT_DISPLAY_NAME,
					  PR_DISPLAY_NAME,
					  PR_SMTP_ADDRESS,
					  PR_GIVEN_NAME);

	retval = ResolveNames((const char **)username, SPropTagArray, &SRowSet, &flaglist, 0);
	mapitest_print_subcall(mt, "ResolveNames PR_DISPLAY_NAME UTF8", GetLastError());

	switch (flaglist->ulFlags[0]) {
	case MAPI_UNRESOLVED:
		mapitest_print(mt, "%-41s: [FAILED]\n", "FlagList (UNRESOLVED)");
		return;
		break;
	case MAPI_AMBIGUOUS:
		mapitest_print(mt, "%-41s: [FAILED]\n", "FlagList (AMBIGUOUS)");
		return;
		break;
	case MAPI_RESOLVED:
		mapitest_print(mt, "%-41s: [PASSED]\n", "FlagList (RESOLVED)");
		break;
	}
	
	if (flaglist->ulFlags[0] == MAPI_RESOLVED && 
	    SRowSet->aRow[0].lpProps[3].value.lpszA) {
		if (!strncmp(mt->info.username, SRowSet->aRow[0].lpProps[3].value.lpszA,
			     strlen(SRowSet->aRow[0].lpProps[3].value.lpszA))) {
			mapitest_print(mt, "%-41s: [PASSED]\n", "PR_DISPLAY_NAME comparison test");
		} else {
			mapitest_print(mt, "%-41s: [FAILED]\n", "PR_DISPLAY_NAME comparison test");
		}
	}

	talloc_free(username);
	

	/* Test with PR_GIVEN_NAME retrieved from previous ResolveNames test */

	username = talloc_array(mt->mem_ctx, char *, 2);
	username[0] = talloc_asprintf(username, "%s%s", mt->info.mailbox, 
				      SRowSet->aRow[0].lpProps[2].value.lpszA);
	username[1] = NULL;

	retval = ResolveNames((const char **)username, SPropTagArray, &SRowSet, &flaglist, 0);
	mapitest_print_subcall(mt, "ResolveNames Mailbox UTF8", GetLastError());

	switch (flaglist->ulFlags[0]) {
	case MAPI_UNRESOLVED:
		mapitest_print(mt, "%-41s: [FAILED]\n", "FlagList (UNRESOLVED)");
		return;
		break;
	case MAPI_AMBIGUOUS:
		mapitest_print(mt, "%-41s: [FAILED]\n", "FlagList (AMBIGUOUS)");
		return;
		break;
	case MAPI_RESOLVED:
		mapitest_print(mt, "%-41s: [PASSED]\n", "FlagList (RESOLVED)");
		break;
	}
	
	if (flaglist->ulFlags[0] == MAPI_RESOLVED && 
	    SRowSet->aRow[0].lpProps[3].value.lpszA) {
		if (!strncmp(mt->info.username, SRowSet->aRow[0].lpProps[3].value.lpszA,
			     strlen(SRowSet->aRow[0].lpProps[3].value.lpszA))) {
			mapitest_print(mt, "%-41s: [PASSED]\n", "PR_DISPLAY_NAME comparison test");
		} else {
			mapitest_print(mt, "%-41s: [FAILED]\n", "PR_DISPLAY_NAME comparison test");
		}
	}

	talloc_free(username);
	MAPIFreeBuffer(SPropTagArray);
	MAPIFreeBuffer(SRowSet);
	MAPIFreeBuffer(flaglist);

	mapitest_deindent();

}


void mapitest_calls_ring1(struct mapitest *mt)
{
	mapitest_print_interface_start(mt, "First Ring");

	/* EMSMDB tests */
	mapitest_call_OpenMsgStore(mt);
	mapitest_call_OpenPublicFolder(mt);

	/* NSPI tests */
	mapitest_call_ResolveNames(mt);

	mapitest_print_interface_end(mt);
}
