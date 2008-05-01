/*
   Stand-alone MAPI testsuite

   OpenChange Project - FOLDER OBJECT PROTOCOL operations

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
#include "utils/mapitest/mapitest.h"
#include "utils/mapitest/proto.h"

/**
   \file module_oxcfold.c

   \brief Folder Object Protocol test suite
*/

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

struct test_folders {
	const char	*name;
	const char	*class;
};


static struct test_folders subfolders[] = {
	{ MT_DIRNAME_APPOINTMENT,	IPF_APPOINTMENT	},
	{ MT_DIRNAME_CONTACT,		IPF_CONTACT	},
	{ MT_DIRNAME_JOURNAL,		IPF_JOURNAL	},
	{ MT_DIRNAME_POST,		IPF_POST	},
	{ MT_DIRNAME_NOTE,		IPF_NOTE	},
	{ MT_DIRNAME_STICKYNOTE,	IPF_STICKYNOTE	},
	{ MT_DIRNAME_TASK,		IPF_TASK	},
	{ NULL,				NULL		}
};


/**
   \details Test the OpenFolder (0x2) operation

   This function:
	-# Log on the user private mailbox
	-# Open folders located within the top information store folder
	-# Open folders located within the Inbox folder

   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_OpenFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_store_t	*store;
	mapi_id_t		id_folder;
	int			i;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Prepare the top folder check */
	store = (mapi_object_store_t *) obj_store.private_data;
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

	/* Step2. Check top folders (Private mailbox) */
	mapitest_print(mt, "* Private Mailbox - Message Store (Owner)\n");
	mapitest_indent();
	for (i = 0; top_folders[i].name; i++) {
		mapi_object_init(&obj_folder);
		retval = OpenFolder(&obj_store, top_folders[i].fid, &obj_folder);
		mapitest_print(mt, "* %-25s: 0x%.8x\n", top_folders[i].name, GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
		mapi_object_release(&obj_folder);
	}
	mapitest_deindent();

	/* Step3. Check for Inbox folders */
	mapitest_print(mt, "* Private Mailbox - Mailbox (Owner)\n");
	mapitest_indent();
	for (i = 0; inbox_folders[i].name; i++) {
		mapi_object_init(&obj_folder);
		GetDefaultFolder(&obj_store, &id_folder, inbox_folders[i].id);
		retval = OpenFolder(&obj_store, id_folder, &obj_folder);
		mapitest_print(mt, "* %-25s: 0x%.8x\n", inbox_folders[i].name, GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
		mapi_object_release(&obj_folder);
	}
	mapitest_deindent();

	/* Release */
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the CreateFolder (0x1c) operation

   This function:
	-# Log on the user private mailbox
	-# Open the top information folder
        -# Create a test directory
	-# Create sub directories with different container classes
	-# Empty the folder
	-# Delete the folder

   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_CreateFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	struct SPropValue	lpProp[1];
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_top;
	mapi_object_t		obj_child;
	mapi_id_t		id_folder;
	int			i;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Create the top test folder */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder\n", MT_DIRNAME_TOP);
	mapi_object_init(&obj_top);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create sub directories */
	mapitest_print(mt, "* Creating sub directories\n");
	mapitest_indent();
	for (i = 0; subfolders[i].name; i++) {
		/* Step 4.1. Create the sub folder */
		mapi_object_init(&obj_child);
		retval = CreateFolder(&obj_top, FOLDER_GENERIC, subfolders[i].name, NULL,
				      OPEN_IF_EXISTS, &obj_child);
		mapitest_print(mt, "%d.1 %-20s %15s: 0x%.8x\n", i + 1, subfolders[i].name, "CreateFolder", GetLastError());

		/* Step 4.2. Set its container class */
		set_SPropValue_proptag(&lpProp[0], PR_CONTAINER_CLASS, (const void *) subfolders[i].class);
		retval = SetProps(&obj_child, lpProp, 1);
		mapitest_print(mt, "%d.2 %-20s %15s: 0x%.8x\n", i + 1, subfolders[i].name, "SetProps", GetLastError());
		
		mapi_object_release(&obj_child);
	}
	mapitest_deindent();

	/* Step 5. Empty Folder */
	retval = EmptyFolder(&obj_top);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "EmptyFolder", GetLastError());

	/* Step 6. DeleteFolder */
	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_top));
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "DeleteFolder", GetLastError());
	

	/* Release */
	mapi_object_release(&obj_top);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetHierarchyTable (0x4) operation

  This function:
	-# Log on the user private mailbox
	-# Open the top information folder
	-# Call the GetHierarchyTable operation

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_GetHierarchyTable(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	mapi_id_t		id_folder;
	uint32_t		RowCount;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Get the Hierarchy Table */
	mapi_object_init(&obj_htable);
	retval = GetHierarchyTable(&obj_folder, &obj_htable, 0, &RowCount);
	mapitest_print(mt, "* %-35s: (%d rows) 0x%.8x\n", "GetHierarchyTable", RowCount, GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_htable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the GetContentsTable (0x5) operation

  This function:
	-# Log on the user private mailbox
	-# Open the top information folder
	-# Call the GetContentsTable operation

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_GetContentsTable(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	mapi_id_t		id_folder;
	uint32_t		RowCount;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Get the Contents Table */
	mapi_object_init(&obj_ctable);
	retval = GetContentsTable(&obj_folder, &obj_ctable, 0, &RowCount);
	mapitest_print(mt, "* %-35s: (%d rows) 0x%.8x\n", "GetContentsTable", RowCount, GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Release */
	mapi_object_release(&obj_ctable);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return true;
}


/**
   \details Test the MoveCopyMessages (0x33) opetation.

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder (source)
	-# Open the Deleted Items folder (destination)
	-# Creates 3 sample messages
	-# Move messages from source to destination
 */
_PUBLIC_ bool mapitest_oxcfold_MoveCopyMessages(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder_src;
	mapi_object_t		obj_folder_dst;
	mapi_object_t		obj_message;
	mapi_id_t		msgid[3];
	mapi_id_t		id_folder;
	uint32_t		i;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Source Inbox folder */
	mapi_object_init(&obj_folder_src);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder_src);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}
		

	/* Step 3. Open Destination Deleted Items folder */
	mapi_object_init(&obj_folder_dst);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderDeletedItems);
	retval = OpenFolder(&obj_store, id_folder, &obj_folder_dst);
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create sample messages */
	for (i = 0; i < 3; i++) {
		mapi_object_init(&obj_message);
		retval = mapitest_common_message_create(mt, &obj_folder_src, &obj_message, MT_MAIL_SUBJECT);
		if (GetLastError() != MAPI_E_SUCCESS) {
			ret = false;
		}
		
		retval = SaveChangesMessage(&obj_folder_src, &obj_message);
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
		msgid[i] = mapi_object_get_id(&obj_message);
		mapi_object_release(&obj_message);
	}

	/* Step 5. Move messages from source to destination */
	retval = MoveCopyMessages(&obj_folder_src, &obj_folder_dst, msgid, 0);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "MoveCopyMessages", GetLastError());
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Release */
	mapi_object_release(&obj_folder_src);
	mapi_object_release(&obj_folder_dst);
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the MoveFolder (0x35) opetation.

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder (source)
	-# Create a temporary folder
	-# Create 1 message in this new folder
	-# Open the Deleted Items folder (destination)
	-# Move the temporary folder from Inbox to DeletedItems
	-# Empty and delete the moved temporary folder
 */
_PUBLIC_ bool mapitest_oxcfold_MoveFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_src;
	mapi_object_t		obj_dst;
	mapi_object_t		obj_message;
	bool			ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(&obj_store);
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open the inbox folder */
	mapi_object_init(&obj_src);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_src, olFolderInbox);
	if (ret == false) return ret;

	/* Step 3. Create temporary folder */
	mapi_object_init(&obj_folder);
	retval = CreateFolder(&obj_src, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_folder);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "CreateFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create message within this temporary folder */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);

	retval = SaveChangesMessage(&obj_folder, &obj_message);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	mapi_object_release(&obj_message);

	/* Step 5. Open the Deleted items folder */
	mapi_object_init(&obj_dst);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_dst, olFolderDeletedItems);

	/* Step 6. MoveFolder */
	retval = MoveFolder(&obj_folder, &obj_src, &obj_dst, MT_DIRNAME_TOP, false);
	mapitest_print(mt, "* %-35s: 0x%.8x\n", "MoveFolder", GetLastError());
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapi_object_release(&obj_folder);

	/* Step 7. Delete the moved folder */
	mapi_object_init(&obj_folder);
	ret = mapitest_common_find_folder(mt, &obj_dst, &obj_folder, MT_DIRNAME_TOP);
	mapitest_print(mt, "* %-35s: %s\n", "mapitest_common_find_folder", (ret == true) ? "true" : "false");

	if (ret == true) {
		retval = EmptyFolder(&obj_folder);
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "EmptyFolder", GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	
		retval = DeleteFolder(&obj_dst, mapi_object_get_id(&obj_folder));
		mapitest_print(mt, "* %-35s: 0x%.8x\n", "DeleteFolder", GetLastError());
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	}
	

	/* Release */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_src);
	mapi_object_release(&obj_dst);
	mapi_object_release(&obj_store);

	return true;
}
