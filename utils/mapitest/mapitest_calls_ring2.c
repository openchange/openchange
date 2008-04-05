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

struct folders {
	uint32_t	id;
	uint64_t	fid;
	const char	*name;
};

static struct folders top_folders[] = {
	{ 0x0, 0x0, "NON_IPM_SUBTREE" },
	{ 0x1, 0x0, "Deferred Actions" },
	{ 0x2, 0x0, "Spooler Queue" },
	{ 0x3, 0x0, "Top of Information Store" },
	{ 0x4, 0x0, "Inbox" },
	{ 0x5, 0x0, "Outbox" },
	{ 0x6, 0x0, "Sent Items" },
	{ 0x7, 0x0, "Deleted Items" },
	{ 0x8, 0x0, "Common Views" },
	{ 0x9, 0x0, "Schedule" },
	{ 0xa, 0x0, "Finder" },
	{ 0xb, 0x0, "Views" },
	{ 0xc, 0x0, "Shortcuts" },
	{ 0xd, 0x0, NULL }
};

static struct folders inbox_folders[] = {
	{ olFolderCalendar,		0x0, "Calendar"		},
	{ olFolderContacts,		0x0, "Contacts"		},
	{ olFolderJournal,		0x0, "Journal"		},
	{ olFolderNotes,		0x0, "Notes"		},
	{ olFolderTasks,		0x0, "Tasks"		},
	{ olFolderDrafts,		0x0, "Drafts"		},
	{ 0x0,				0x0, NULL }
};


static void mapitest_call_AddressTypes(struct mapitest *mt,
				       mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	uint16_t		cValues;
	struct mapi_LPSTR	*transport = NULL;
	uint32_t		i;
	char			*str = NULL;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "AddressTypes");
	mapitest_indent();

	retval = AddressTypes(obj_store, &cValues, &transport);
	mapitest_print_subcall(mt, "AddressTypes", GetLastError());
	for (i = 0; i < cValues; i++) {
		str = talloc_asprintf(mt->mem_ctx, "Recipient Type: %s", transport[i].lppszA);
		mapitest_print_subcall(mt, str, GetLastError());
		talloc_free(str);
	}

	mapitest_deindent();
}

static void mapitest_call_GetReceiveFolderTable(struct mapitest *mt,
						mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	struct SRowSet		SRowSet;

	retval = GetReceiveFolderTable(obj_store, &SRowSet);
	mapitest_print_call(mt, "GetReceiveFolderTable", GetLastError());
	mapitest_indent();
	mapidump_SRowSet(&SRowSet, "\t\t[*] ");
	mapitest_deindent();
	MAPIFreeBuffer(SRowSet.aRow);
}


static void mapitest_call_GetReceiveFolder(struct mapitest *mt, 
					   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	uint64_t		id_inbox;

	retval = GetReceiveFolder(obj_store, &id_inbox);
	mapitest_print_call(mt, "GetReceiveFolder", GetLastError());
}


static void mapitest_call_GetPropsAll(struct mapitest *mt, 
				      mapi_object_t *obj_store)
{
	enum MAPISTATUS			retval;
	struct mapi_SPropValue_array	properties_array;

	retval = GetPropsAll(obj_store, &properties_array);
	mapitest_print_call(mt, "GetPropsAll", GetLastError());
}


static void mapitest_call_GetPropList(struct mapitest *mt,
				      mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;

	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(obj_store, SPropTagArray);
	mapitest_print_call(mt, "GetPropList", GetLastError());
	MAPIFreeBuffer(SPropTagArray);
}


static void mapitest_call_GetProps(struct mapitest *mt,
				   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	struct SPropTagArray	*SPropTagArray;
	struct SPropValue	*lpProps;
	uint32_t		cValues;
	
	SPropTagArray = talloc_zero(mt->mem_ctx, struct SPropTagArray);
	retval = GetPropList(obj_store, SPropTagArray);
	retval = GetProps(obj_store, SPropTagArray, &lpProps, &cValues);
	mapitest_print_call(mt, "GetProps", GetLastError());
	MAPIFreeBuffer(SPropTagArray);

}


static void mapitest_call_SetProps(struct mapitest *mt,
				   mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	struct SPropValue	*lpProps;
	struct SPropValue	lpProp[1];
	struct SPropTagArray	*SPropTagArray;
	const char		*mailbox = NULL;
	const char		*new_mailbox = NULL;
	uint32_t		cValues;

	mapitest_print(mt, MT_HDR_FMT_SECTION, "SetProps");
	mapitest_indent();

	/* Step 0: GetProps, retreive mailbox name */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);

	if (cValues && lpProps[0].value.lpszA) {
		mailbox = lpProps[0].value.lpszA;
	} else {
		mapitest_print(mt, MT_ERROR, "Step0 - GetProps: No mailbox name");
		mapitest_deindent();
		return;
	}

	/* Step 1.1: SetProps with new value */
	cValues = 1;
	new_mailbox = talloc_asprintf(mt->mem_ctx, "%s [MAPITEST]", mailbox);
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, 
			       (const void *) new_mailbox);
	retval = SetProps(obj_store, lpProp, cValues);
	mapitest_print_subcall(mt, "Step1.1 - Set:   NEW mailbox name", GetLastError());

	/* Step 1.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(new_mailbox, lpProps[0].value.lpszA, strlen(lpProps[0].value.lpszA))) {
			mapitest_print_subcall(mt, "Step1.2 - Check: NEW mailbox name", MAPI_E_SUCCESS);
		} else {
			mapitest_print_subcall(mt, "Step1.2 - Check: NEW mailbox name", MAPI_E_RESERVED);
		}
	}
	MAPIFreeBuffer((void *)new_mailbox);

	/* Step 2.1: Reset mailbox to its original value */
	cValues = 1;
	set_SPropValue_proptag(&lpProp[0], PR_DISPLAY_NAME, (const void *)mailbox);
	retval = SetProps(obj_store, lpProp, cValues);
	mapitest_print_subcall(mt, "Step2.1 - Set:   OLD mailbox name", GetLastError());
	
	/* Step 2.2: Double check with GetProps */
	SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_DISPLAY_NAME);
	retval = GetProps(obj_store, SPropTagArray, &lpProps, &cValues);
	MAPIFreeBuffer(SPropTagArray);
	if (lpProps[0].value.lpszA) {
		if (!strncmp(mailbox, lpProps[0].value.lpszA, strlen(lpProps[0].value.lpszA))) {
			mapitest_print_subcall(mt, "Step2.2 - Check: OLD mailbox name", MAPI_E_SUCCESS);
		} else {
			mapitest_print_subcall(mt, "Step2.2 - Check: OLD mailbox name", MAPI_E_RESERVED);
		}
	}

	mapitest_deindent();

}


static void mapitest_call_OpenFolder(struct mapitest *mt,
				     mapi_object_t *obj_store)
{
	enum MAPISTATUS		retval;
	mapi_object_store_t	*store;
	mapi_object_t		obj_folder;
	uint64_t		id_folder;
	int			i;

	/* Prepare the top folder check */
	store = (mapi_object_store_t *) obj_store->private_data;
	top_folders[0].fid = store->fid_non_ipm_subtree;
	top_folders[1].fid = store->fid_deferred_actions;
	top_folders[2].fid = store->fid_spooler_queue;
	top_folders[3].fid = store->fid_top_information_store;
	top_folders[4].fid = store->fid_inbox;
	top_folders[5].fid = store->fid_outbox;
	top_folders[6].fid = store->fid_sent_items;
	top_folders[7].fid = store->fid_deleted_items;
	top_folders[8].fid = store->fid_common_views;
	top_folders[9].fid = store->fid_schedule;
	top_folders[10].fid = store->fid_finder;
	top_folders[11].fid = store->fid_views;
	top_folders[12].fid = store->fid_shortcuts;

	/* Check top folders */
	mapitest_print(mt, MT_HDR_FMT_SECTION, "OpenFolder - Message Store (Top Folders)");
	mapitest_indent();
	for (i = 0; top_folders[i].name; i++) {
		mapi_object_init(&obj_folder);
		retval = OpenFolder(obj_store, top_folders[i].fid, &obj_folder);
		mapitest_print_subcall(mt, top_folders[i].name, GetLastError());
		mapi_object_release(&obj_folder);
	}
	mapitest_deindent();

	/* Check inbox folders */
	mapitest_print(mt, MT_HDR_FMT_SECTION, "OpenFolder - Message Store (Inbox Folders)");
	mapitest_indent();
	for (i = 0; inbox_folders[i].name; i++) {
		mapi_object_init(&obj_folder);
		GetDefaultFolder(obj_store, &id_folder, inbox_folders[i].id);
		retval = OpenFolder(obj_store, id_folder, &obj_folder);
		mapitest_print_subcall(mt, inbox_folders[i].name, GetLastError());
		mapi_object_release(&obj_folder);
	}
	
	mapitest_deindent();
}


void mapitest_calls_ring2(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	uint32_t		attempt = 0;

	mapitest_print_interface_start(mt, "Second Ring");
retry:
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS && attempt < 3) {
		attempt++;
		goto retry;
	}

	mapitest_call_AddressTypes(mt, &obj_store);
	mapitest_call_GetReceiveFolderTable(mt, &obj_store);
	mapitest_call_GetReceiveFolder(mt, &obj_store);
	mapitest_call_GetPropList(mt, &obj_store);
	mapitest_call_GetPropsAll(mt, &obj_store);
	mapitest_call_GetProps(mt, &obj_store);
	mapitest_call_SetProps(mt, &obj_store);
	mapitest_call_OpenFolder(mt, &obj_store);

	mapi_object_release(&obj_store);

	mapitest_print_interface_end(mt);
}
