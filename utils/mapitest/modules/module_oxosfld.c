/*
   Stand-alone MAPI testsuite

   OpenChange Project - SPECIAL FOLDERS PROTOCOL

   Copyright (C) Enrique J. Hernández 2015

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

/*
static const char * root_special_folders[] = {
        "Finder",
        "Reminders",
        "Tracked Mail Processing",
        "To-Do",
        "Common Views",
        "Personal Views",
        "Top of Information Store",
        "Freebusy Data",
        "Spooler Queue",
        "Document Libraries"
};
*/

struct folders {
        uint32_t     idx;
        const char   *name;
        uint32_t     id;
        bool         additional_ren_entry_ids;
};

static struct folders personal_special_folders[] = {
        { 0x0, "Deleted Items", olFolderDeletedItems, false },
        { 0x1, "Outbox", olFolderOutbox, false },
        { 0x2, "Sent", olFolderSentMail, false },
        { 0x3, "Inbox", olFolderInbox, false },
        { 0x4, "Personal Calendar (c)", olFolderCalendar, false },
        { 0x5, "Personal Address Book", olFolderContacts, false },
        /* This special folders are not working in all supported
           versions, so we skip by now [MS-OXOSFLD] Section 6 <1>
        { 0x6, "Suggested Contacts", 0xFFFFFFFF, true },
        { 0x7, "Quick Contacts", 0xFFFFFFFF },
        { 0x8, "IM Contacts List", 0xFFFFFFFF },
        { 0x9, "Contacts Search", 0xFFFFFFFF },
        */
        { 0x6, "Journal", olFolderJournal, false },
        { 0x7, "Notes", olFolderNotes, false },
        { 0x8, "Personal Calendar (t)", olFolderTasks, false },
        { 0x9, "Drafts", olFolderDrafts, false },
        { 0x10, "Sync Issues", olFolderSyncIssues, true },
        { 0x11, "Junk E-mail", olFolderJunk, true },
        { 0x12, "RSS Feeds", 0xFFFFFFFF, true },
        { 0x13, "Conversation Action Settings", 0xFFFFFFFF, true },
        { 0x14, NULL, 0xFFFFFFFF, true }
};

static struct folders sync_issues_special_folders[] = {
        { 0x0, "Conflicts", olFolderConflicts },
        { 0x1, "Local Failures", olFolderLocalFailures },
        { 0x2, "Server Failures", olFolderServerFailures },
        { 0x3, NULL, 0xFFFFFFFF }
};

/* Schedule, Shortcuts? */

/**
    \details Test the creation of some special folders

    This function:
        -# Log on the user private mailbox
        -# Open the top information store folder
        -# Loop over every known special folder name
           in English's locale under Top of Information Store
        -# Try to create the folder with NONE as flag
        -# Loop over every known special folder name
           in English's locale under Sync Issues folder
        -# Try to create the folder with NONE as flag

    \param mt pointer to the top level mapitest structure

    \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_oxosfld_CreateFolder(struct mapitest *mt)
{
        enum MAPISTATUS         additional_ren_entry_ids, retval;
        mapi_id_t               id_folder;
        mapi_object_t           obj_tis_folder, obj_sync_folder, obj_folder, obj_store;
        bool                    test_result = true;
        uint32_t                i;

        mapi_object_init(&obj_store);
        mapi_object_init(&obj_tis_folder);
        mapi_object_init(&obj_sync_folder);

        /* Step 1. Logon */
        retval = OpenMsgStore(mt->session, &obj_store);
        mapitest_print_retval(mt, "OpenMsgStore");
        if (GetLastError() != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        /* Step 2. Open Top Information Store folder */
        retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
        mapitest_print_retval_clean(mt, "GetDefaultFolder", retval);
        if (retval != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        retval = OpenFolder(&obj_store, id_folder, &obj_tis_folder);
        mapitest_print_retval(mt, "OpenFolder");
        if (GetLastError() != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        additional_ren_entry_ids = GetSpecialAdditionalFolder(&obj_store, &id_folder, olFolderSyncIssues);

        /* Step 3. Loop over every special folder in Top Information Store folder */
        for (i = 0; personal_special_folders[i].name; i++) {
                if (additional_ren_entry_ids != MAPI_E_SUCCESS && personal_special_folders[i].additional_ren_entry_ids) {
                        /* Skip test if the user has never logged on */
                        continue;
                }
                mapi_object_init(&obj_folder);
                retval = CreateFolder(&obj_tis_folder, FOLDER_GENERIC, personal_special_folders[i].name,
                                      NULL, NONE, &obj_folder);
                mapitest_print_retval_fmt_clean(mt, "CreateFolder", retval, "(%s)", personal_special_folders[i].name);
                if (retval != MAPI_E_SUCCESS) {
                        test_result = false;
                }
                mapi_object_release(&obj_folder);
        }

        /* Step 4. Open Sync Issues folder */
        mapitest_print_retval_clean(mt, "GetSpecialAdditionalFolder (Sync Issues)", additional_ren_entry_ids);
        if (additional_ren_entry_ids != MAPI_E_SUCCESS) {
                mapitest_print(mt, "* It seems not all information about some special folders is found. Some tests skipped\n");
                goto cleanup;
        }

        retval = OpenFolder(&obj_tis_folder, id_folder, &obj_sync_folder);
        mapitest_print_retval(mt, "OpenFolder (Sync Issues)");
        if (retval != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        /* Step 5. Loop over every special folder in Sync Issues folder */
        for (i = 0; sync_issues_special_folders[i].name; i++) {
                mapi_object_init(&obj_folder);
                retval = CreateFolder(&obj_sync_folder, FOLDER_GENERIC, sync_issues_special_folders[i].name,
                                      NULL, NONE, &obj_folder);
                mapitest_print_retval_fmt(mt, "CreateFolder", "(%s)", sync_issues_special_folders[i].name);
                if (retval != MAPI_E_SUCCESS) {
                        test_result = false;
                }
                mapi_object_release(&obj_folder);
        }

cleanup:
        /* Release */
        mapi_object_release(&obj_sync_folder);
        mapi_object_release(&obj_tis_folder);
        mapi_object_release(&obj_store);

        return test_result;
}

/**
    \details Test the deletion of some special folders

    This function:
        -# Log on the user private mailbox
        -# Open the top information store folder
        -# Loop over every known special folder name
           under Top of Information Store
        -# Try to delete the folder
        -# Loop over every known special folder name
           under Sync Issues folder
        -# Try to delete the folder

    \param mt pointer to the top level mapitest structure

    \return true on success, otherwise false
*/
_PUBLIC_ bool mapitest_oxosfld_DeleteFolder(struct mapitest *mt)
{
        bool                    partial_completion = false;
        bool                    test_result = true;
        const char              *get_op_name;
        enum MAPISTATUS         additional_ren_entry_ids, retval;
        mapi_id_t               id_folder;
        mapi_object_t           obj_tis_folder, obj_sync_folder, obj_store;
        uint32_t                i;

        mapi_object_init(&obj_store);
        mapi_object_init(&obj_tis_folder);
        mapi_object_init(&obj_sync_folder);

        /* Step 1. Logon */
        retval = OpenMsgStore(mt->session, &obj_store);
        mapitest_print_retval(mt, "OpenMsgStore");
        if (GetLastError() != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        /* Step 2. Open Top Information Store folder */
        retval = GetDefaultFolder(&obj_store, &id_folder, olFolderTopInformationStore);
        mapitest_print_retval_clean(mt, "GetDefaultFolder", retval);
        if (retval != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        retval = OpenFolder(&obj_store, id_folder, &obj_tis_folder);
        mapitest_print_retval(mt, "OpenFolder");
        if (GetLastError() != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        additional_ren_entry_ids = GetSpecialAdditionalFolder(&obj_store, &id_folder, olFolderSyncIssues);

        /* Step 3. Loop over every special folder in Top Information Store folder */
        for (i = 0; personal_special_folders[i].name; i++) {
                if (personal_special_folders[i].id == 0xFFFFFFFF) {
                        /* Not yet supported by GetDefaultFolder */
                        continue;
                }
                if (additional_ren_entry_ids != MAPI_E_SUCCESS && personal_special_folders[i].additional_ren_entry_ids) {
                        /* Skip test if the user has never logged on */
                        continue;
                }
                if (personal_special_folders[i].additional_ren_entry_ids) {
                        retval = GetSpecialAdditionalFolder(&obj_store, &id_folder,
                                                            personal_special_folders[i].id);
                        get_op_name = "GetSpecialAdditionalFolder";
                } else {
                        retval = GetDefaultFolder(&obj_store, &id_folder, personal_special_folders[i].id);
                        get_op_name = "GetDefaultFolder";
                }
                mapitest_print_retval_fmt_clean(mt, (char *)get_op_name, retval,
                                                "(%s)", personal_special_folders[i].name);
                if (retval != MAPI_E_SUCCESS) {
                        test_result = false;
                        continue;
                }
                retval = DeleteFolder(&obj_tis_folder, id_folder,
                                      0, &partial_completion);
                mapitest_print_retval_fmt(mt, "DeleteFolder", "(%s)", personal_special_folders[i].name);
                if (retval != MAPI_E_NO_ACCESS || partial_completion) {
                        test_result = false;
                }
        }

        /* Step 4. Open Sync Issues folder */
        retval = GetSpecialAdditionalFolder(&obj_store, &id_folder, olFolderSyncIssues);
        mapitest_print_retval_clean(mt, "GetDefaultFolder (Sync Issues)", retval);
        if (retval != MAPI_E_SUCCESS) {
                mapitest_print(mt, "* It seems not all information about some special folders is found. Some tests skipped\n");
                goto cleanup;
        }

        retval = OpenFolder(&obj_tis_folder, id_folder, &obj_sync_folder);
        mapitest_print_retval(mt, "OpenFolder (Sync Issues)");
        if (retval != MAPI_E_SUCCESS) {
                test_result = false;
                goto cleanup;
        }

        /* Step 5. Loop over every special folder in Sync Issues folder */
        for (i = 0; sync_issues_special_folders[i].name; i++) {
                retval = GetSpecialAdditionalFolder(&obj_store, &id_folder,
                                                    sync_issues_special_folders[i].id);
                mapitest_print_retval_fmt_clean(mt, "GetSpecialAdditionalFolder", retval,
                                                "(%s)", sync_issues_special_folders[i].name);
                if (retval != MAPI_E_SUCCESS) {
                        test_result = false;
                        continue;
                }
                retval = DeleteFolder(&obj_tis_folder, id_folder,
                                      0, &partial_completion);
                mapitest_print_retval_fmt(mt, "DeleteFolder", "(%s)", sync_issues_special_folders[i].name);
                if (retval != MAPI_E_NO_ACCESS || partial_completion) {
                        test_result = false;
                }
        }

cleanup:
        /* Release */
        mapi_object_release(&obj_sync_folder);
        mapi_object_release(&obj_tis_folder);
        mapi_object_release(&obj_store);

        return test_result;
}
