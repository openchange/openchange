/*
   Stand-alone MAPI testsuite

   OpenChange Project - FOLDER OBJECT PROTOCOL operations

   Copyright (C) Julien Kerihuel 2008
   Copyright (C) Brad Hards 2009-2010

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
	{ 0x0, 0x0, "Mailbox Root Folder" },
	{ 0x1, 0x0, "Deferred Actions" },
	{ 0x2, 0x0, "Spooler Queue" },
	{ 0x3, 0x0, "Top of Information Store" },
	{ 0x4, 0x0, "Inbox" },
	{ 0x5, 0x0, "Outbox" },
	{ 0x6, 0x0, "Sent Items" },
	{ 0x7, 0x0, "Deleted Items" },
	{ 0x8, 0x0, "Common Views" },
	{ 0x9, 0x0, "Schedule" },
	{ 0xa, 0x0, "Search" },
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
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Prepare the top folder check */
	store = (mapi_object_store_t *) obj_store.private_data;
	top_folders[0].fid = store->fid_mailbox_root;
	top_folders[1].fid = store->fid_deferred_actions;
	top_folders[2].fid = store->fid_spooler_queue;
	top_folders[3].fid = store->fid_top_information_store;
	top_folders[4].fid = store->fid_inbox;
	top_folders[5].fid = store->fid_outbox;
	top_folders[6].fid = store->fid_sent_items;
	top_folders[7].fid = store->fid_deleted_items;
	top_folders[8].fid = store->fid_common_views;
	top_folders[9].fid = store->fid_schedule;
	top_folders[10].fid = store->fid_search;
	top_folders[11].fid = store->fid_views;
	top_folders[12].fid = store->fid_shortcuts;

	/* Step2. Check top folders (Private mailbox) */
	mapitest_print(mt, "* Private Mailbox - Message Store (Owner)\n");
	mapitest_indent();
	for (i = 0; top_folders[i].name; i++) {
		mapi_object_init(&obj_folder);
		retval = OpenFolder(&obj_store, top_folders[i].fid, &obj_folder);
		mapitest_print_retval_fmt(mt, "OpenFolder", "(%s)", top_folders[i].name);
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
		retval = GetDefaultFolder(&obj_store, &id_folder, inbox_folders[i].id);
		mapitest_print_retval_fmt(mt, "GetDefaultFolder", "(%s)", inbox_folders[i].name);
		if (retval != MAPI_E_SUCCESS) {
			return false;
		}
		retval = OpenFolder(&obj_store, id_folder, &obj_folder);
		mapitest_print_retval_fmt(mt, "OpenFolder", "(%s)", inbox_folders[i].name);
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
   \details Test the CreateFolder (0x1c) and DeleteFolder (0x1d) operations

   This is a simpler version of the CreateFolder test below.

   This function:
	-# Log on the user private mailbox
	-# Open the top information folder
        -# Create a test directory
	-# Delete the folder

   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_CreateDeleteFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_top;
	mapi_object_t		obj_child;
	mapi_object_t		obj_grandchild;
	mapi_id_t		id_folder;
	bool			ret = true;

	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_top);
	mapi_object_init(&obj_child);
	mapi_object_init(&obj_grandchild);

	/* Step 1. Logon */
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create the top test folder */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top);
	mapitest_print_retval(mt, "CreateFolder - top");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Create child folder */
	mapitest_print(mt, "* Create GENERIC child folder\n");
	retval = CreateFolder(&obj_top, FOLDER_GENERIC, "MT Child folder", NULL,
			      OPEN_IF_EXISTS, &obj_child);
	mapitest_print_retval(mt, "CreateFolder - child");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Create child of the child (grandchild) folder */
	mapitest_print(mt, "* Create GENERIC grandchild folder\n");
	retval = CreateFolder(&obj_child, FOLDER_GENERIC, "MT grandchild folder", NULL,
			      OPEN_IF_EXISTS, &obj_grandchild);
	mapitest_print_retval(mt, "CreateFolder - grandchild");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6. DeleteFolder on the child (and recurse to grandchild) */
	retval = DeleteFolder(&obj_top, mapi_object_get_id(&obj_child),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder - child");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. DeleteFolder on the top folder */
	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_top),
			      DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder - top");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

cleanup:
	/* Release */
	mapi_object_release(&obj_grandchild);
	mapi_object_release(&obj_child);
	mapi_object_release(&obj_top);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
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
	bool			ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_top);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create the top test folder */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top);
	mapitest_print_retval(mt, "CreateFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Create sub directories */
	mapitest_print(mt, "* Creating sub directories\n");
	mapitest_indent();
	for (i = 0; subfolders[i].name; i++) {
		/* Step 4.1. Create the sub folder */
		mapi_object_init(&obj_child);
		retval = CreateFolder(&obj_top, FOLDER_GENERIC, subfolders[i].name, NULL,
				      OPEN_IF_EXISTS, &obj_child);
		mapitest_print_retval_fmt(mt, "CreateFolder", "(%s)", subfolders[i].name);
		if (retval != MAPI_E_SUCCESS) {
			mapitest_deindent();
			mapi_object_release(&obj_child);
			ret = false;
			goto cleanup;
		}
		/* Step 4.2. Set its container class */
		set_SPropValue_proptag(&lpProp[0], PR_CONTAINER_CLASS, (const void *) subfolders[i].class);
		retval = SetProps(&obj_child, 0, lpProp, 1);
		mapitest_print_retval_fmt(mt, "SetProps", "(%s)", subfolders[i].name);		
		if (retval != MAPI_E_SUCCESS) {
			mapitest_deindent();
			mapi_object_release(&obj_child);
			ret = false;
			goto cleanup;
		}
		mapi_object_release(&obj_child);
	}
	mapitest_deindent();

	/* Step 5. Empty Folder */
	retval = EmptyFolder(&obj_top);
	mapitest_print_retval(mt, "EmptyFolder");
	if (retval != MAPI_E_SUCCESS) {
	      ret = false;
	      goto cleanup;
	}

	/* Step 6. DeleteFolder */
	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_top),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");
	if (retval != MAPI_E_SUCCESS) {
	      ret = false;
	      goto cleanup;
	}

cleanup:
	/* Release */
	mapi_object_release(&obj_top);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test the CreateFolder (0x1c) operations

   This tests different combinations of folder creation.

   This function:
	-# Log on the user private mailbox
	-# Open the top information folder
	-# Create a test directory (or open the directory if it already exists)
	-# Delete the test directory
	-# Create the test directory again (it should not exist)
	-# Try to create another directory with the same name (should error out)
	-# Try to create another directory with the same name, but use open if exists
	-# Create a generic subfolder
	-# Try to create another generic subfolder with the same name (should error out)
	-# Try to create another generic subfolder with the same name, but use open if exists
	-# Delete the generic subfolder
	-# Delete the test directory
   \param mt the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_CreateFolderVariants(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_top1, obj_top2, obj_top3;
	mapi_object_t		obj_child1, obj_child2, obj_child3;
	mapi_id_t		id_folder;
	bool			ret = true;

	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_top1);
	mapi_object_init(&obj_top2);
	mapi_object_init(&obj_top3);
	mapi_object_init(&obj_child1);
	mapi_object_init(&obj_child2);
	mapi_object_init(&obj_child3);

	/* Step 1. Logon */
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_clean(mt, "OpenMsgStore", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	mapitest_print_retval_clean(mt, "GetDefaultFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval_clean(mt, "OpenFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create the top test folder */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top1);
	mapitest_print_retval_clean(mt, "CreateFolder - top", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. DeleteFolder on the top folder */
	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_top1),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_clean(mt, "DeleteFolder - top", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Create the top test folder (again) */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      0, &obj_top1);
	mapitest_print_retval_clean(mt, "CreateFolder - top", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6. Create the top test folder (again) - should error out*/
	mapitest_print(mt, "* Create GENERIC \"%s\" folder (duplicate name - expect collision)\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      0, &obj_top2);
	mapitest_print_retval_clean(mt, "CreateFolder - top", retval);
	if (retval != MAPI_E_COLLISION) {
		ret = false;
		goto cleanup;
	}

	/* Step 7. Create the top test folder (again), using OPEN_IF_EXISTS */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder (open if exists)\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top3);
	mapitest_print_retval_clean(mt, "CreateFolder - top", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 8. Create child folder */
	mapitest_print(mt, "* Create GENERIC child folder\n");
	retval = CreateFolder(&obj_top3, FOLDER_GENERIC, "MT Child folder", NULL,
			      0, &obj_child1);
	mapitest_print_retval_clean(mt, "CreateFolder - child", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 9. Create the child test folder (again) - should error out*/
	mapitest_print(mt, "* Create GENERIC child folder (duplicate name - expect collision)\n");
	retval = CreateFolder(&obj_top3, FOLDER_GENERIC, "MT Child folder", NULL,
			      0, &obj_child2);
	mapitest_print_retval_clean(mt, "CreateFolder - child", retval);
	if (retval != MAPI_E_COLLISION) {
		ret = false;
		goto cleanup;
	}

	/* Step 10. Create the child test folder (again), using OPEN_IF_EXISTS */
	mapitest_print(mt, "* Create GENERIC child folder (open if exists)\n");
	retval = CreateFolder(&obj_top3, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_child3);
	mapitest_print_retval_clean(mt, "CreateFolder - child", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	
	/* Step 11. DeleteFolder on the child */
	retval = DeleteFolder(&obj_top3, mapi_object_get_id(&obj_child3),
			      DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_clean(mt, "DeleteFolder - child", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		/* we want to fall through on the off-chance this fails */
	}

	/* Step 12. DeleteFolder on the top folder */
	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_top3),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_clean(mt, "DeleteFolder - top", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
cleanup:
	/* Release */
	mapi_object_release(&obj_child3);
	mapi_object_release(&obj_child2);
	mapi_object_release(&obj_child1);
	mapi_object_release(&obj_top3);
	mapi_object_release(&obj_top2);
	mapi_object_release(&obj_top1);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
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
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_htable;
	mapi_id_t		id_folder;
	uint32_t		RowCount;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);

	GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");

	OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Get the Hierarchy Table */
	mapi_object_init(&obj_htable);
	GetHierarchyTable(&obj_folder, &obj_htable, 0, &RowCount);
	mapitest_print_retval_fmt(mt, "GetHierarchyTable", "(%d rows)", RowCount);
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
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_ctable;
	mapi_id_t		id_folder;
	uint32_t		RowCount;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Top Information Store folder */
	mapi_object_init(&obj_folder);
	GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	mapitest_print_retval(mt, "GetDefaultFolder");

	OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Get the Contents Table */
	mapi_object_init(&obj_ctable);
	GetContentsTable(&obj_folder, &obj_ctable, 0, &RowCount);
	mapitest_print_retval_fmt(mt, "GetContentsTable", "(%d rows)", RowCount);
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
   \details Test the SetSearchCriteria (0x30) operation

   This function:
	-# Log on the user private mailbox
	-# Retrieve the inbox folder ID
	-# Open the default search folder
	-# Create a search folder within this folder
	-# Set the message class property on this container
	-# Set a restriction criteria
	-# Call SetSearchCriteria
	-# Delete the test search folder

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_SetSearchCriteria(struct mapitest *mt)
{
	bool				ret = true;
	mapi_object_t			obj_store;
	mapi_object_t			obj_search;
	mapi_object_t			obj_searchdir;
	mapi_id_t			id_inbox;
	mapi_id_t			id_search;
	mapi_id_array_t			id;
	struct SPropValue		lpProps[1];
	struct mapi_SRestriction	res;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Open Inbox folder */
	GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Open Search folder */
	GetDefaultFolder(&obj_store, &id_search, olFolderFinder);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_search);
	OpenFolder(&obj_store, id_search, &obj_search);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create a search folder */
	mapi_object_init(&obj_searchdir);
	CreateFolder(&obj_search, FOLDER_SEARCH, "mapitest", 
			      "mapitest search folder", OPEN_IF_EXISTS,
			      &obj_searchdir);
	mapitest_print_retval(mt, "CreateFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Set properties on this search folder */
	lpProps[0].ulPropTag = PR_CONTAINER_CLASS;
	lpProps[0].value.lpszA = IPF_NOTE;
	SetProps(&obj_searchdir, 0, lpProps, 1);
	mapitest_print_retval(mt, "SetProps");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto error;
	}

	/* Step 6. Search criteria on this folder */
	mapi_id_array_init(mt->mapi_ctx->mem_ctx, &id);
	mapi_id_array_add_id(&id, id_inbox);

	res.rt = RES_CONTENT;
	res.res.resContent.fuzzy = FL_SUBSTRING;
	res.res.resContent.ulPropTag = PR_SUBJECT;
	res.res.resContent.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resContent.lpProp.value.lpszA = "MT";

	SetSearchCriteria(&obj_searchdir, &res,
				   BACKGROUND_SEARCH|RECURSIVE_SEARCH, &id);
	mapitest_print_retval(mt, "SetSearchCriteria");
	mapi_id_array_release(&id);
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

error:
	DeleteFolder(&obj_search, mapi_object_get_id(&obj_searchdir),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");
	mapi_object_release(&obj_searchdir);
	mapi_object_release(&obj_search);
	mapi_object_release(&obj_store);
	return ret;
}


/**
   \details Test the GetSearchCriteria (0x31) operation

   This function:
	-# Log on the user private mailbox
	-# Retrieve the inbox folder ID
	-# Open the default search folder
	-# Create a search folder within this folder
	-# Set the message class property on this container
	-# Set a restriction criteria
	-# Call SetSearchCriteria
	-# Call GetSearchCriteria
	-# Delete the test search folder

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_GetSearchCriteria(struct mapitest *mt)
{
	bool				ret = true;
	mapi_object_t			obj_store;
	mapi_object_t			obj_search;
	mapi_object_t			obj_searchdir;
	mapi_id_t			id_inbox;
	mapi_id_t			id_search;
	mapi_id_array_t			id;
	struct SPropValue		lpProps[1];
	struct mapi_SRestriction	res;
	uint32_t			ulSearchFlags;
	uint16_t			count;
	mapi_id_t			*fid;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Open Inbox folder */
	GetDefaultFolder(&obj_store, &id_inbox, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 3. Open Search folder */
	GetDefaultFolder(&obj_store, &id_search, olFolderFinder);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	mapi_object_init(&obj_search);
	OpenFolder(&obj_store, id_search, &obj_search);
	mapitest_print_retval(mt, "OpenFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create a search folder */
	mapi_object_init(&obj_searchdir);
	CreateFolder(&obj_search, FOLDER_SEARCH, "mapitest", 
			      "mapitest search folder", OPEN_IF_EXISTS,
			      &obj_searchdir);
	mapitest_print_retval(mt, "CreateFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 5. Set properties on this search folder */
	lpProps[0].ulPropTag = PR_CONTAINER_CLASS;
	lpProps[0].value.lpszA = IPF_NOTE;
	SetProps(&obj_searchdir, 0, lpProps, 1);
	mapitest_print_retval(mt, "SetProps");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto error;
	}

	/* Step 6. Search criteria on this folder */
	mapi_id_array_init(mt->mapi_ctx->mem_ctx, &id);
	mapi_id_array_add_id(&id, id_inbox);

	res.rt = RES_CONTENT;
	res.res.resContent.fuzzy = FL_SUBSTRING;
	res.res.resContent.ulPropTag = PR_SUBJECT;
	res.res.resContent.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resContent.lpProp.value.lpszA = "MT";

	SetSearchCriteria(&obj_searchdir, &res,
				   BACKGROUND_SEARCH|RECURSIVE_SEARCH, &id);
	mapitest_print_retval(mt, "SetSearchCriteria");
	mapi_id_array_release(&id);
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto error;
	}

	/* Step 7. Call GetSearchCriteria */
	GetSearchCriteria(&obj_searchdir, &res, &ulSearchFlags, &count, &fid);
	mapitest_print_retval(mt, "GetSearchCriteria");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
		goto error;
	}


error:
	DeleteFolder(&obj_search, mapi_object_get_id(&obj_searchdir),
		     DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");
	mapi_object_release(&obj_searchdir);
	mapi_object_release(&obj_search);
	mapi_object_release(&obj_store);
	return ret;
}


/**
   \details Test the MoveCopyMessages (0x33) operation.

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder (source)
	-# Open the Deleted Items folder (destination)
	-# Creates 3 sample messages
	-# Move messages from source to destination

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_MoveCopyMessages(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			common_result;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder_src;
	mapi_object_t		obj_folder_dst;
	mapi_object_t		obj_message;
	mapi_object_t		dst_contents;
	uint32_t		dst_count;
	struct mapi_SRestriction res;
	struct SPropTagArray    *SPropTagArray;
	struct SRowSet		SRowSet;
	mapi_id_array_t		msg_id_array;
	mapi_id_t		msgid[20];
	mapi_id_t		id_folder;
	uint32_t		i;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 2. Open Source Inbox folder */
	mapi_object_init(&obj_folder_src);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");

	retval = OpenFolder(&obj_store, id_folder, &obj_folder_src);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
		
	/* Step 3. Open Destination Deleted Items folder */
	mapi_object_init(&obj_folder_dst);
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderDeletedItems);
	mapitest_print_retval(mt, "GetDefaultFolder");

	retval = OpenFolder(&obj_store, id_folder, &obj_folder_dst);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		return false;
	}
	mapi_object_init(&(dst_contents));
	retval = GetContentsTable(&(obj_folder_dst), &(dst_contents), 0, &dst_count);
	mapitest_print_retval(mt, "GetContentsTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 4. Create sample messages */
	mapi_id_array_init(mt->mapi_ctx->mem_ctx, &msg_id_array);
	for (i = 0; i < 3; i++) {
		mapi_object_init(&obj_message);
		common_result = mapitest_common_message_create(mt, &obj_folder_src, &obj_message, MT_MAIL_SUBJECT);
		if (!common_result) {
			mapitest_print(mt, "* mapitest_common_message_create() failed\n");
			ret = false;
			goto release;
		}
		
		retval = SaveChangesMessage(&obj_folder_src, &obj_message, KeepOpenReadOnly);
		mapitest_print_retval(mt, "SaveChangesMessage");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			goto release;
		}
		mapi_id_array_add_obj(&msg_id_array, &obj_message);
		mapi_object_release(&obj_message);
	}

	/* Step 5. Move messages from source to destination */
	retval = MoveCopyMessages(&obj_folder_src, &obj_folder_dst, &msg_id_array, 0);
	mapitest_print_retval(mt, "MoveCopyMessages");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}
	mapi_id_array_release(&msg_id_array);

	/* Step 6. Apply a filter */
	res.rt = RES_PROPERTY;
	res.res.resProperty.relop = RES_PROPERTY;
	res.res.resProperty.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.value.lpszA = MT_MAIL_SUBJECT;

	retval = Restrict(&(dst_contents), &res, NULL);
	mapitest_print_retval(mt, "Restrict");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	/* Step 7. Get the filtered row */
        SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_MID);
        retval = SetColumns(&(dst_contents), SPropTagArray);
	mapitest_print_retval(mt, "SetColumns");
	MAPIFreeBuffer(SPropTagArray);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto release;
	}

	retval = QueryRows(&(dst_contents), 20, TBL_NOADVANCE, TBL_FORWARD_READ, &SRowSet);
	mapitest_print_retval(mt, "QueryRows");
	if ( (retval == MAPI_E_SUCCESS) && (SRowSet.cRows > 0) ) {
		for (i = 0; i < SRowSet.cRows; ++i) {
			msgid[i] = SRowSet.aRow[i].lpProps[0].value.d;
		}
	}

	/* Step 8. Delete Messages */
	retval = DeleteMessage(&obj_folder_dst, msgid, i); 
	mapitest_print_retval(mt, "DeleteMessage");

release:
	/* Release */
	mapi_object_release(&dst_contents);
	mapi_object_release(&obj_folder_src);
	mapi_object_release(&obj_folder_dst);
	mapi_object_release(&obj_store);

	return ret;
}


/**
   \details Test the MoveFolder (0x35) operation.

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder (source)
	-# Create a temporary folder
	-# Create 1 message in this new folder
	-# Open the Deleted Items folder (destination)
	-# Move the temporary folder from Inbox to DeletedItems
	-# Empty and delete the moved temporary folder

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
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
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
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
	mapitest_print_retval(mt, "CreateFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create message within this temporary folder */
	mapi_object_init(&obj_message);
	ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);
	mapitest_print_retval(mt, "mapitest_common_message_create");

	retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
	mapitest_print_retval(mt, "SaveChangesMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}

	mapi_object_release(&obj_message);

	/* Step 5. Open the Deleted items folder */
	mapi_object_init(&obj_dst);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_dst, olFolderDeletedItems);

	/* Step 6. MoveFolder */
	retval = MoveFolder(&obj_folder, &obj_src, &obj_dst, MT_DIRNAME_TOP, false);
	mapitest_print_retval(mt, "MoveFolder");
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
		mapitest_print_retval(mt, "EmptyFolder");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	
		retval = DeleteFolder(&obj_dst, mapi_object_get_id(&obj_folder),
				      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
		mapitest_print_retval(mt, "DeleteFolder");
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


/**
   \details Test the CopyFolder (0x36) operation.

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder (source)
	-# Create a temporary folder
	-# Create a subdirectory
	-# Open the Deleted Items folder (destination)
	-# Copy the temporary folder from Inbox to DeletedItems
	-# Empty and delete the original and copied folder

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_CopyFolder(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;	
	mapi_object_t		obj_subfolder;
	mapi_object_t		obj_src;
	mapi_object_t		obj_dst;
	bool			ret = true;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
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
	mapitest_print_retval(mt, "CreateFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		return false;
	}

	/* Step 4. Create the sub folder */
	mapi_object_init(&obj_subfolder);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_NOTE, NULL,
			      OPEN_IF_EXISTS, &obj_subfolder);
	mapitest_print_retval(mt, "CreateFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapi_object_release(&obj_subfolder);

	/* Step 5. Open the Deleted items folder */
	mapi_object_init(&obj_dst);
	ret = mapitest_common_folder_open(mt, &obj_store, &obj_dst, olFolderDeletedItems);

	/* Step 6. CopyFolder */
	retval = CopyFolder(&obj_folder, &obj_src, &obj_dst, MT_DIRNAME_TOP, false, false);
	mapitest_print_retval(mt, "CopyFolder");
	if (GetLastError() != MAPI_E_SUCCESS) {
		ret = false;
	}

	/* Step 7. Delete the original and copied folder */
	retval = EmptyFolder(&obj_folder);
	mapitest_print_retval(mt, "EmptyFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	
	retval = DeleteFolder(&obj_src, mapi_object_get_id(&obj_folder),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval(mt, "DeleteFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
	}
	mapi_object_release(&obj_folder);

	mapi_object_init(&obj_folder);
	ret = mapitest_common_find_folder(mt, &obj_dst, &obj_folder, MT_DIRNAME_TOP);
	mapitest_print(mt, "* %-35s: %s\n", "mapitest_common_find_folder", (ret == true) ? "true" : "false");

	if (ret == true) {
		retval = EmptyFolder(&obj_folder);
		mapitest_print_retval(mt, "EmptyFolder");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	
		retval = DeleteFolder(&obj_dst, mapi_object_get_id(&obj_folder),
				      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
		mapitest_print_retval(mt, "DeleteFolder");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
		}
	}
	
	/* Release */
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_src);
	mapi_object_release(&obj_dst);
	mapi_object_release(&obj_store);


	return ret;
}

/**
   \details Test the HardDeleteMessages (0x91) operation.

   This function:
	-# Log on the user private mailbox
	-# Open the Inbox folder (source)
	-# Creates 3 sample messages
	-# Hard delete the sample messages

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_HardDeleteMessages(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_store;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_message;
	mapi_object_t		contents;
	struct mapi_SRestriction res;
	struct SPropTagArray    *SPropTagArray;
	struct SRowSet		SRowSet;
	mapi_id_array_t		msg_id_array;
	mapi_id_t		msgid[50];
	mapi_id_t		id_folder;
	uint32_t		i;
	uint32_t		count = 0;

	/* Step 1. Logon */
	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&contents);

	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval(mt, "OpenMsgStore");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Source Inbox folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderInbox);
	mapitest_print_retval(mt, "GetDefaultFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	mapitest_print_retval(mt, "OpenFolder");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = GetContentsTable(&(obj_folder), &(contents), 0, &count);
	mapitest_print_retval(mt, "GetContentsTable");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create sample messages */
	mapi_id_array_init(mt->mapi_ctx->mem_ctx, &msg_id_array);
	for (i = 0; i < 3; i++) {
		mapi_object_init(&obj_message);
		ret = mapitest_common_message_create(mt, &obj_folder, &obj_message, MT_MAIL_SUBJECT);
		if (!ret) {
			mapitest_print(mt, "failed to create message %i\n", i);
			ret = false;
			goto cleanup;
		}
		
		retval = SaveChangesMessage(&obj_folder, &obj_message, KeepOpenReadOnly);
		mapitest_print_retval(mt, "SaveChangesMessage");
		if (retval != MAPI_E_SUCCESS) {
			ret = false;
			goto cleanup;
		}
		mapi_id_array_add_obj(&msg_id_array, &obj_message);
		mapi_object_release(&obj_message);
	}


	/* Step 4. Apply a filter */
	res.rt = RES_PROPERTY;
	res.res.resProperty.relop = RES_PROPERTY;
	res.res.resProperty.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.ulPropTag = PR_SUBJECT;
	res.res.resProperty.lpProp.value.lpszA = MT_MAIL_SUBJECT;

	retval = Restrict(&(contents), &res, NULL);
	mapitest_print_retval(mt, "Restrict");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Get the filtered rows */
        SPropTagArray = set_SPropTagArray(mt->mem_ctx, 0x1, PR_MID);
        SetColumns(&(contents), SPropTagArray);
	mapitest_print_retval(mt, "SetColumns");
	MAPIFreeBuffer(SPropTagArray);

	retval = QueryRows(&(contents), 50, TBL_NOADVANCE, TBL_FORWARD_READ, &SRowSet);
	mapitest_print_retval(mt, "QueryRows");
	if (retval == MAPI_E_SUCCESS) {
		for (i = 0; i < SRowSet.cRows; ++i) {
			msgid[i] = SRowSet.aRow[i].lpProps[0].value.d;
		}
		mapitest_print(mt, "%i Messages created successfully\n", SRowSet.cRows);
	}

	/* Step 6. Delete Messages */
	retval = HardDeleteMessage(&obj_folder, msgid, i); 
	mapitest_print_retval(mt, "HardDeleteMessage");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 7. Check the restriction again */
	retval = QueryRows(&(contents), 50, TBL_NOADVANCE, TBL_FORWARD_READ, &SRowSet);
	mapitest_print_retval(mt, "QueryRows");
	if ( retval != MAPI_E_SUCCESS ) {
		ret = false;
		goto cleanup;
	}

	if (SRowSet.cRows == 0) {
		mapitest_print(mt, "successfully deleted messages\n");
	} else {
		mapitest_print(mt, "failed to delete messages\n");
		ret = false;
	}

cleanup:
	/* Release */
	mapi_object_release(&contents);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}

/**
   \details Test the HardDeleteMessagesAndSubfolder (0x92) operation.

   This function:
	-# Creates a filled test folder
	-# Creates 2 subdirectories in the test folder
	-# Hard deletes the sample messages and subdirectories

   \param mt pointer to the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_HardDeleteMessagesAndSubfolders(struct mapitest *mt)
{
	struct mt_common_tf_ctx	*context;
	enum MAPISTATUS		retval;
	bool			ret = true;
	mapi_object_t		obj_htable;
	mapi_object_t		subfolder1;
	mapi_object_t		subfolder2;
	uint32_t		unread = 0;
	uint32_t		total = 0;

        /* Step 1. Logon and create a filled test folder */
        if (! mapitest_common_setup(mt, &obj_htable, NULL)) {
                return false;
        }

	context = mt->priv;

	/* Step 2. Create two subfolders */
	mapi_object_init(&subfolder1);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "SubFolder1", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &subfolder1);
	mapi_object_release(&subfolder1);
	mapitest_print_retval(mt, "Create Subfolder1");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	mapi_object_init(&subfolder2);
	retval = CreateFolder(&(context->obj_test_folder), FOLDER_GENERIC,
			      "SubFolder2", NULL /*folder comment*/,
			      OPEN_IF_EXISTS, &subfolder2);
	mapi_object_release(&subfolder2);
	mapitest_print_retval(mt, "Create Subfolder2");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	retval = GetFolderItemsCount(&(context->obj_test_folder), &unread, &total);
	mapitest_print_retval(mt, "GetFolderItemsCount");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	mapitest_print(mt, "* Folder count: %i (%i unread)\n", total, unread);

	/* Step 3. Hard delete contents */
	retval = HardDeleteMessagesAndSubfolders(&(context->obj_test_folder));
	mapitest_print_retval(mt, "HardDeleteMessagesAndSubfolders");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Check successful deletion */
	retval = GetFolderItemsCount(&(context->obj_test_folder), &unread, &total);
	mapitest_print_retval(mt, "GetFolderItemsCount");
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	mapitest_print(mt, "* Folder count: %i (%i unread)\n", total, unread);

	if (total != 0 || unread != 0) {
		ret = false;
	}

 cleanup:
        /* Cleanup and release */
        mapi_object_release(&obj_htable);
        mapitest_common_cleanup(mt);

        return ret;
}

/**
   \details Test the DeleteMessages (0x1e) operation

   This function:
   -# Log on the user private mailbox
   -# Open the top of information store
   -# Create a test folder
   -# Create and save three messages
   -# Save the messages
   -# Delete the messages
   -# Delete the test folder

   \param mt pointer on the top-level mapitest structure

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_oxcfold_DeleteMessages(struct mapitest *mt)
{
	enum MAPISTATUS		retval;
	mapi_object_t		obj_store;
	mapi_id_t		id_folder;
	mapi_object_t		obj_folder;
	mapi_object_t		obj_top;
	mapi_object_t		obj_message1;
	mapi_object_t		obj_message2;
	mapi_object_t		obj_message3;
	mapi_id_t		id_msgs[3];
	bool			ret = true; /* success */

	mapi_object_init(&obj_store);
	mapi_object_init(&obj_folder);
	mapi_object_init(&obj_top);
	mapi_object_init(&obj_message1);
	mapi_object_init(&obj_message2);
	mapi_object_init(&obj_message3);

	/* Step 1. Logon */ 
	retval = OpenMsgStore(mt->session, &obj_store);
	mapitest_print_retval_clean(mt, "OpenMsgStore", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 2. Open Top Information Store folder */
	retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = OpenFolder(&obj_store, id_folder, &obj_folder);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 3. Create the top test folder */
	mapitest_print(mt, "* Create GENERIC \"%s\" folder\n", MT_DIRNAME_TOP);
	retval = CreateFolder(&obj_folder, FOLDER_GENERIC, MT_DIRNAME_TOP, NULL,
			      OPEN_IF_EXISTS, &obj_top);
	mapitest_print_retval_clean(mt, "CreateFolder", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 4. Create the messages */
	retval = CreateMessage(&obj_top, &obj_message1);
	mapitest_print_retval_clean(mt, "CreateMessage - 1", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = CreateMessage(&obj_top, &obj_message2);
	mapitest_print_retval_clean(mt, "CreateMessage - 2", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = CreateMessage(&obj_top, &obj_message3);
	mapitest_print_retval_clean(mt, "CreateMessage - 3", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 5. Save the messages */
	retval = SaveChangesMessage(&obj_top, &obj_message1, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesMessage - 1", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = SaveChangesMessage(&obj_top, &obj_message2, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesMessage - 2", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}
	retval = SaveChangesMessage(&obj_top, &obj_message3, KeepOpenReadOnly);
	mapitest_print_retval_clean(mt, "SaveChangesMessage - 3", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 6. Delete the saved messages */
	id_msgs[0] = mapi_object_get_id(&obj_message1);
	id_msgs[1] = mapi_object_get_id(&obj_message2);
	id_msgs[2] = mapi_object_get_id(&obj_message3);
	retval = DeleteMessage(&obj_top, id_msgs, 3);
	mapitest_print_retval_clean(mt, "DeleteMessage", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Step 7. DeleteFolder on the top folder */
	retval = DeleteFolder(&obj_folder, mapi_object_get_id(&obj_top),
			      DEL_MESSAGES|DEL_FOLDERS|DELETE_HARD_DELETE, NULL);
	mapitest_print_retval_clean(mt, "DeleteFolder - top", retval);
	if (retval != MAPI_E_SUCCESS) {
		ret = false;
		goto cleanup;
	}

	/* Release */
cleanup:
	mapi_object_release(&obj_message3);
	mapi_object_release(&obj_message2);
	mapi_object_release(&obj_message1);
	mapi_object_release(&obj_top);
	mapi_object_release(&obj_folder);
	mapi_object_release(&obj_store);

	return ret;
}
