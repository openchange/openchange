/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Wolfgang Sourdeau 2012

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

/**
   \file emsmdbp_provisioning.c

   \brief Account provisioning
 */

#include <ctype.h>
#include <string.h>
#include <sys/mman.h>


#include "dcesrv_exchange_emsmdb.h"

#include <gen_ndr/ndr_property.h>


#define MAILBOX_ROOT_NAME "OpenChange: %s"

static struct emsmdbp_special_folder *get_special_folders(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx)
{
	static struct emsmdbp_special_folder default_values[PROVISIONING_SPECIAL_FOLDERS_SIZE] = {
		{MAPISTORE_DRAFTS_ROLE, 	PR_IPM_DRAFTS_ENTRYID, 		NULL},
		{MAPISTORE_CALENDAR_ROLE, 	PR_IPM_APPOINTMENT_ENTRYID, 	NULL},
		{MAPISTORE_CONTACTS_ROLE, 	PR_IPM_CONTACT_ENTRYID, 	NULL},
		{MAPISTORE_TASKS_ROLE, 		PR_IPM_TASK_ENTRYID, 		NULL},
		{MAPISTORE_NOTES_ROLE, 		PR_IPM_NOTE_ENTRYID, 		NULL},
		{MAPISTORE_JOURNAL_ROLE, 	PR_IPM_JOURNAL_ENTRYID, 	NULL}
	};
	size_t i, total_special_folders = PROVISIONING_SPECIAL_FOLDERS_SIZE;
	const char **names = emsmdbp_get_special_folders(mem_ctx, emsmdbp_ctx);

	struct emsmdbp_special_folder *ret = talloc_memdup(mem_ctx, default_values, sizeof(struct emsmdbp_special_folder) * total_special_folders);
	for (i = 0; i < total_special_folders; i++) {
		ret[i].name = names[i];
	}
	return ret;
}

static const char **get_folders_names(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx)
{
	const char **names = emsmdbp_get_folders_names(mem_ctx, emsmdbp_ctx);
	const char **ret = (const char **)talloc_zero_array(mem_ctx, char *, PROVISIONING_FOLDERS_SIZE + 1);
	memcpy(ret+1, names, sizeof(char *) * PROVISIONING_FOLDERS_SIZE);
	return ret;
}

static enum MAPISTATUS get_new_public_folder_id(struct emsmdbp_context *emsmdbp_ctx, uint64_t parent_fid, uint64_t *fid)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;
	enum mapistore_error	ret;

	if (openchangedb_is_public_folder_id(emsmdbp_ctx->oc_ctx, parent_fid)) {
		retval = openchangedb_get_new_public_folderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fid);
	} else {
		ret = mapistore_indexing_get_new_folderID(emsmdbp_ctx->mstore_ctx, fid);
		if (ret != MAPISTORE_SUCCESS) {
			retval = MAPI_E_CALL_FAILED;
		}
	}

	return retval;
}
/**
   \details Provision the Local FreeBusy message for the user in
   Public Folder store.

   \param emsmdbp_ctx pointer to the emsmdbp context
   \param EssDN pointer to enterprise distinguished name (X500 DN)

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS emsmdbp_mailbox_provision_public_freebusy(struct emsmdbp_context *emsmdbp_ctx, const char *EssDN)
{
	TALLOC_CTX		*mem_ctx;
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;
	char			*dn_root = NULL;
	char			*dn_user = NULL;
	char			*cn_ptr = NULL;
	uint64_t		public_fb_fid;
	uint64_t		group_fid;
	uint64_t		fb_mid;
	uint64_t		change_num;
	size_t			i, max;
	void			*message_object;
	struct SRow		property_row;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!EssDN, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	dn_root = talloc_asprintf(mem_ctx, "EX:%s", EssDN);
	OPENCHANGE_RETVAL_IF(!dn_root, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	cn_ptr = strstr(dn_root, "/cn");
	OPENCHANGE_RETVAL_IF(!cn_ptr, MAPI_E_INVALID_PARAMETER, mem_ctx);

	dn_user = talloc_asprintf(mem_ctx, "USER-%s", cn_ptr);
	OPENCHANGE_RETVAL_IF(!dn_user, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	*cn_ptr = 0;

	/* convert dn_root to lowercase */
	max = cn_ptr - dn_root;
	for (i = 3; i < max; i++) {
		dn_root[i] = tolower(dn_root[i]);
	}

	/* convert dn_user to uppercase. Yes, it's stupid like that. */
	max = strlen(dn_user);
	for (i = 5; i < max; i++) {
		dn_user[i] = toupper(dn_user[i]);
	}

	retval = openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, EMSMDBP_PF_FREEBUSY, &public_fb_fid);
	if (retval != MAPI_E_SUCCESS) {
		DEBUG(5, ("[%s:%d] provisioning: freebusy root folder not found in OpenChange database.\n", __FUNCTION__, __LINE__));
		goto end;
	}

	retval = openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, public_fb_fid, dn_root, &group_fid);
	if (retval != MAPI_E_SUCCESS) {
		retval = get_new_public_folder_id(emsmdbp_ctx, public_fb_fid, &group_fid);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot get new public folder id\n", __FUNCTION__, __LINE__));
			goto end;
		}
		retval = openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, &change_num);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot get new change number\n", __FUNCTION__, __LINE__));
			goto end;
		}
		openchangedb_create_folder(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, public_fb_fid, group_fid, change_num, NULL, -1);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot create new folder\n", __FUNCTION__, __LINE__));
			goto end;
		}
	}

	retval = openchangedb_get_mid_by_subject(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, group_fid, dn_user, false, &fb_mid);
	if (retval != MAPI_E_SUCCESS) {
		retval = get_new_public_folder_id(emsmdbp_ctx, group_fid, &fb_mid);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot get new public folder id\n", __FUNCTION__, __LINE__));
			goto end;
		}
		retval = openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, &change_num);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot get new change number\n", __FUNCTION__, __LINE__));
			goto end;
		}
		retval = openchangedb_message_create(mem_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fb_mid, group_fid, false, &message_object);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot create new message\n", __FUNCTION__, __LINE__));
			goto end;
		}
		property_row.cValues = 1;
		property_row.lpProps = talloc_zero(mem_ctx, struct SPropValue);
		OPENCHANGE_RETVAL_IF(!property_row.lpProps, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
		property_row.lpProps[0].ulPropTag = PR_NORMALIZED_SUBJECT_UNICODE;
		property_row.lpProps[0].value.lpszW = dn_user;
		retval = openchangedb_message_set_properties(mem_ctx, emsmdbp_ctx->oc_ctx, message_object, &property_row);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot set properties on new message\n", __FUNCTION__, __LINE__));
			goto end;
		}
		retval = openchangedb_message_save(emsmdbp_ctx->oc_ctx, message_object, 0);
		if (retval != MAPI_E_SUCCESS) {
			DEBUG(0, ("[%s:%d] Cannot save message\n", __FUNCTION__, __LINE__));
			goto end;
		}
	}

end:
	talloc_free(mem_ctx);

	return retval;
}

_PUBLIC_ enum MAPISTATUS emsmdbp_mailbox_provision(struct emsmdbp_context *emsmdbp_ctx, const char *username)
{

	return MAPI_E_SUCCESS;

/* auto-provisioning:

  During private logon:
   - fetch list of available folders from all backends + list of capabilities (handled folder types) + fallback entry
   - if fallback is not available: FAIL
   - fetch list of existing folders from openchangedb
   - if mailbox does not exist:
    - create basic structure
    - if certain folders are not available (Inbox, Deleted Items, Spooler Queue, Outbox), use fallback
   - synchronize list of non-mandatory and secondary folders
    - update relevant entry ids in relevant objects
   - ability to leave creation of fallback and other url to the relevant backend
   - freebusy entry

  folder creation = createfolder + setprops (class) + release


* RopLogon entries (mandatory) (autoprovisioning)

mailbox (Root): (None)

"Common Views": (None) (sogo://wsourdeau:wsourdeau@common-views/)          -> fallback
"Deferred Action": (None) (sogo://wsourdeau:wsourdeau@deferred-actions/)   -> fallback
"Search" (Finder): None (sogo://wsourdeau:wsourdeau@search/)               -> fallback
"Schedule": (None) (sogo://wsourdeau:wsourdeau@schedule/)                  -> fallback
"Shortcuts": (None) (sogo://wsourdeau:wsourdeau@shortcuts/)                -> fallback
"Spooler Queue": (None) (sogo://wsourdeau:wsourdeau@spooler-queue/)        -> fallback
"Views": (sogo://wsourdeau:wsourdeau@views/)                               -> fallback

"IPM Subtree" (Top of Personal Folders): (None)

* autoprovisioning backend based

"Inbox": IPF.Note (sogo://wsourdeau:wsourdeau@inbox/)
"Outbox": IPF.Note (sogo://wsourdeau:wsourdeau@outbox/)
"Sent Items": IPF.Note (sogo://wsourdeau:wsourdeau@sent-items/)
"Deleted Items": IPF.Note (sogo://wsourdeau:wsourdeau@deleted-items/)

* additional special folders
"Calendar": IPF.Appointment (PidTagIpmAppointmentEntryId)
"Contacts": IPF.Contact (PidTagIpmContactEntryId)
"Notes": IPF.StickyNote (PidTagIpmNoteEntryId)
"Tasks": IPF.Task (PidTagIpmTaskEntryId)
"Journal": IPF.Journal (PidTagIpmJournalEntryId)
"Drafts": IPF.Note (PidTagIpmDraftsEntryId)

* client-created:

"Freebusy Data": 
"Reminders": Outlook.Reminder (sogo://wsourdeau:wsourdeau@reminders/)  (PidTagRemindersOnlineEntryId)    -> fallback
"To-Do blabnla"
"Sync Issues": IPF.Note (PidTagAdditionalRenEntryIds)
"Junk E-mail": IPF.Note (PidTagAdditionalRenEntryIds)
...



Exchange hierarchy for virgin account:

FolderId: 0x67ca828f02000001      Display Name: "                        ";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: TRUE; 

  FolderId: 0x71ca828f02000001    Display Name: "            Common Views";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
  FolderId: 0x69ca828f02000001    Display Name: "         Deferred Action";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
  FolderId: 0x6fca828f02000001    Display Name: "                  Finder";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
  FolderId: 0x72ca828f02000001    Display Name: "                Schedule";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
  FolderId: 0x73ca828f02000001    Display Name: "               Shortcuts";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
  FolderId: 0x6aca828f02000001    Display Name: "           Spooler Queue";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
  FolderId: 0x70ca828f02000001    Display Name: "                   Views";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 

  FolderId: 0x68ca828f02000001    Display Name: "Top of Information Store";  Container Class: MAPI_E_NOT_FOUND;        Message Class: MAPI_E_NOT_FOUND; Has Subfolders: TRUE; 

    FolderId: 0x6bca828f02000001  Display Name: "      Boîte de réception";  Container Class: "            IPF.Note";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x6cca828f02000001  Display Name: "           Boîte d'envoi";  Container Class: "            IPF.Note";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x67c3828f02000001  Display Name: "              Brouillons";  Container Class: "            IPF.Note";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x65c3828f02000001  Display Name: "              Calendrier";  Container Class: "     IPF.Appointment";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x66c3828f02000001  Display Name: "                Contacts";  Container Class: "         IPF.Contact";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x6dca828f02000001  Display Name: "        Éléments envoyés";  Container Class: "            IPF.Note";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x6eca828f02000001  Display Name: "      Éléments supprimés";  Container Class: "            IPF.Note";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x68c3828f02000001  Display Name: "                 Journal";  Container Class: "         IPF.Journal";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x69c3828f02000001  Display Name: "                   Notes";  Container Class: "      IPF.StickyNote";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 
    FolderId: 0x6ac3828f02000001  Display Name: "                  Tâches";  Container Class: "            IPF.Task";  Message Class: MAPI_E_NOT_FOUND; Has Subfolders: FALSE; 



 */
	TALLOC_CTX				*mem_ctx;
	enum MAPISTATUS				ret;
	enum mapistore_error			retval;
	struct mapistore_contexts_list		*contexts_list;
	struct StringArrayW_r			*existing_uris;
	struct mapistore_contexts_list		*main_entries[MAPISTORE_MAX_ROLES], *secondary_entries[MAPISTORE_MAX_ROLES], *next_entry, *current_entry;
	const char				**folder_names;
	struct emsmdbp_special_folder		*special_folders;
	const char				**container_classes;
	const char				*search_container_classes[] = {"Outlook.Reminder", "IPF.Task", "IPF.Note"};
	uint32_t				context_id, row_count;
	uint64_t				mailbox_fid = 0, ipm_fid, inbox_fid = 0, current_fid, current_mid, found_fid, current_cn;
	char					*fallback_url, *entryid_dump, *url;
	const char				*mapistore_url, *current_name, *base_name;
	struct emsmdbp_special_folder		*current_folder;
	struct SRow				property_row;
	int					i, j;
	DATA_BLOB				entryid_data;
	struct FolderEntryId			folder_entryid;
	struct Binary_r				*entryId;
	bool					exists, reminders_created;
	void					*backend_object, *backend_table, *backend_message;
	char	 				*organization_name, *group_name;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	/* Retrieve list of folders from backends */
	retval = mapistore_list_contexts_for_user(emsmdbp_ctx->mstore_ctx, username, mem_ctx, &contexts_list);
	if (retval != MAPISTORE_SUCCESS) {
		talloc_free(mem_ctx);
		return MAPI_E_DISK_ERROR;
	}

	/* Fix mapistore uris in returned entries */
	current_entry = contexts_list;
	while (current_entry) {
		mapistore_url = current_entry->url;
		if (mapistore_url) {
			if (mapistore_url[strlen(mapistore_url)-1] != '/') {
				current_entry->url = talloc_asprintf(mem_ctx, "%s/", mapistore_url);
			}
			/* DEBUG(5, ("received entry: '%s' (%p)\n", current_entry->url, current_entry)); */
		}
		else {
			DEBUG(3, ("[%s:%d] received entry without uri\n", __FUNCTION__, __LINE__));
			talloc_free(mem_ctx);
			return MAPI_E_CALL_FAILED;
		}
		current_entry = current_entry->next;
	}

	openchangedb_transaction_start(emsmdbp_ctx->oc_ctx);

	/* Retrieve list of existing entries */
	ret = openchangedb_get_MAPIStoreURIs(emsmdbp_ctx->oc_ctx, username, mem_ctx, &existing_uris);
	if (ret == MAPI_E_SUCCESS) {
		for (i = 0; i < existing_uris->cValues; i++) {
			/* DEBUG(5, ("checking entry '%s'\n", existing_uris->lppszW[i])); */
			exists = false;
			mapistore_url = existing_uris->lppszW[i];
			if (mapistore_url[strlen(mapistore_url)-1] != '/') {
				DEBUG(3, ("[%s:%d] Bad formed URI returned, this should never happen\n", __FUNCTION__, __LINE__));
				openchangedb_transaction_commit(emsmdbp_ctx->oc_ctx);
				talloc_free(mem_ctx);
				return MAPI_E_BAD_VALUE;
			}
			current_entry = contexts_list;
			while (!exists && current_entry) {
				/* DEBUG(5, ("  compare with '%s'\n", current_entry->url)); */
				if (strcmp(mapistore_url, current_entry->url) == 0) {
					/* DEBUG(5, ("  entry found\n")); */
					exists = true;
				}
				current_entry = current_entry->next;
			}
			if (!exists) {
				DEBUG(5, ("  removing entry '%s'\n", mapistore_url));
				openchangedb_get_fid(emsmdbp_ctx->oc_ctx, mapistore_url, &found_fid);
				openchangedb_delete_folder(emsmdbp_ctx->oc_ctx, username, found_fid);
			}
		}
	}

	container_classes = talloc_array(mem_ctx, const char *, MAPISTORE_MAX_ROLES);
	for (i = MAPISTORE_MAIL_ROLE; i < MAPISTORE_MAX_ROLES; i++) {
		container_classes[i] = "IPF.Note";
	}
	container_classes[MAPISTORE_CALENDAR_ROLE] = "IPF.Appointment";
	container_classes[MAPISTORE_CONTACTS_ROLE] = "IPF.Contact";
	container_classes[MAPISTORE_TASKS_ROLE] = "IPF.Task";
	container_classes[MAPISTORE_NOTES_ROLE] = "IPF.StickyNote";
	container_classes[MAPISTORE_JOURNAL_ROLE] = "IPF.Journal";

	memset(&property_row, 0, sizeof(struct SRow));
	memset(main_entries, 0, sizeof(struct mapistore_contexts_list *) * MAPISTORE_MAX_ROLES);
	memset(secondary_entries, 0, sizeof(struct mapistore_contexts_list *) * MAPISTORE_MAX_ROLES);

	/* Sort them between our main_entries and secondary_entries */
	current_entry = contexts_list;
	while (current_entry) {
		next_entry = current_entry->next;
		current_entry->next = NULL;
		current_entry->prev = NULL;
		if (current_entry->main_folder) {
			if (main_entries[current_entry->role]) {
				DEBUG(5, ("duplicate entry for role %d ignored\n  existing entry: %s\n  current entry: %s\n",
					  current_entry->role, main_entries[current_entry->role]->url, current_entry->url));
			}
			else {
				main_entries[current_entry->role] = current_entry;
			}
		}
		else {
			DLIST_ADD_END(secondary_entries[current_entry->role], current_entry, void);
		}
		current_entry = next_entry;
	}

	/* Fallback role MUST exist */
	if (!main_entries[MAPISTORE_FALLBACK_ROLE]) {
		DEBUG(5, ("[%s:%d] No fallback provisioning role was found while such role is mandatory. "
			  "Provisioning must be done manually.\n", __FUNCTION__, __LINE__));
		openchangedb_transaction_commit(emsmdbp_ctx->oc_ctx);
		talloc_free(mem_ctx);
		return MAPI_E_DISK_ERROR;
	}
	fallback_url = main_entries[MAPISTORE_FALLBACK_ROLE]->url;
	if (fallback_url[strlen(fallback_url)-1] != '/') {
		fallback_url = talloc_asprintf(mem_ctx, "%s/", fallback_url);
	}

	/* Mailbox and subfolders */
	folder_names = get_folders_names(mem_ctx, emsmdbp_ctx);
	ret = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_MAILBOX_ROOT, &mailbox_fid);
	if (ret != MAPI_E_SUCCESS) {
		mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &mailbox_fid);
		// FIXME Behavior watched on Outlook 2007. First time mailbox is open
		// we can see SOGo inbox name, after first /resetfoldernames we see the
		// value set here. After that outlook won't update that value, no matter
		// how many times /resetfoldernames is executed again.
		current_name = talloc_asprintf(mem_ctx, MAILBOX_ROOT_NAME, username);

		ret = emsmdbp_fetch_organizational_units(mem_ctx, emsmdbp_ctx, &organization_name, &group_name);
		if (ret != MAPI_E_SUCCESS) {
			DEBUG(0, ("Error provisioning mailbox, we couldn't fetch organizational unit of the user %s", username));
			return MAPI_E_NOT_FOUND;
		}
		openchangedb_create_mailbox(emsmdbp_ctx->oc_ctx, username, organization_name, group_name, mailbox_fid, current_name);
		openchangedb_set_locale(emsmdbp_ctx->oc_ctx, username, emsmdbp_ctx->userLanguage);
	}

	property_row.lpProps = talloc_array(mem_ctx, struct SPropValue, 4); /* allocate max needed until the end of the function */
	property_row.cValues = 1;
	property_row.lpProps[0].ulPropTag = PR_DISPLAY_NAME_UNICODE;
	for (i = EMSMDBP_DEFERRED_ACTION; i < EMSMDBP_REMINDERS; i++) {
		/* TODO: mapistore_tag change */
		ret = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, i, &current_fid);
		if (ret != MAPI_E_SUCCESS) {
			mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_fid);
			openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);
			mapistore_url = talloc_asprintf(mem_ctx, "%s0x%"PRIx64"/", fallback_url, current_fid);
			openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, mailbox_fid, current_fid, current_cn, mapistore_url, i);
			property_row.lpProps[0].value.lpszW = folder_names[i];
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);

			/* instantiate the new folder in the backend to make sure it is initialized properly */
			retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, username, mapistore_url, current_fid, &context_id, &backend_object);
			mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, context_id, username, current_fid);
			mapistore_del_context(emsmdbp_ctx->mstore_ctx, context_id);
		}
	}

	reminders_created = false;
	j = 0;
	property_row.cValues = 3;
	property_row.lpProps[1].ulPropTag = PidTagContainerClass;
	property_row.lpProps[2].ulPropTag = PidTagFolderType;
	property_row.lpProps[2].value.l = 2;
	for (i = EMSMDBP_REMINDERS; i < EMSMDBP_TOP_INFORMATION_STORE; i++) {
		ret = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, i, &current_fid);
		if (ret != MAPI_E_SUCCESS) {
			mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_fid);
			openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);
			openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, mailbox_fid, current_fid, current_cn, NULL, i);
			property_row.lpProps[0].value.lpszW = folder_names[i];
			property_row.lpProps[1].value.lpszW = search_container_classes[j];
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);
			if (i == EMSMDBP_REMINDERS) {
				reminders_created = true;
			}
		}
		j++;
	}

	/* IPM and subfolders */
	ret = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_TOP_INFORMATION_STORE, &ipm_fid);
	if (ret != MAPI_E_SUCCESS) {
		mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &ipm_fid);
		openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);
		property_row.cValues = 2;
		property_row.lpProps[1].ulPropTag = PR_SUBFOLDERS;
		property_row.lpProps[0].value.lpszW = folder_names[EMSMDBP_TOP_INFORMATION_STORE];
		property_row.lpProps[1].value.b = true;
		openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, mailbox_fid, ipm_fid, current_cn, NULL, EMSMDBP_TOP_INFORMATION_STORE);
		openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, ipm_fid, &property_row);
		openchangedb_set_ReceiveFolder(emsmdbp_ctx->oc_ctx, username, "IPC", mailbox_fid);
	}
	property_row.cValues = 2;
	property_row.lpProps[1].ulPropTag = PR_CONTAINER_CLASS_UNICODE;
	property_row.lpProps[1].value.lpszW = "IPF.Note";
	/* TODO: mapistore_url/mapistore_tag change */
	for (i = EMSMDBP_INBOX; i < EMSMDBP_MAX_MAILBOX_SYSTEMIDX; i++) {
		ret = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, i, &current_fid);
		if (ret == MAPI_E_SUCCESS) {
			if (i == EMSMDBP_INBOX) {
				inbox_fid = current_fid;
			}
		} else {
			mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_fid);
			openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);
			current_name = folder_names[i];

			switch (i) {
			case EMSMDBP_INBOX:
				current_entry = main_entries[MAPISTORE_MAIL_ROLE];
				inbox_fid = current_fid;
				break;
			case EMSMDBP_OUTBOX:
				current_entry = main_entries[MAPISTORE_OUTBOX_ROLE];
				break;
			case EMSMDBP_SENT_ITEMS:
				current_entry = main_entries[MAPISTORE_SENTITEMS_ROLE];
				break;
			case EMSMDBP_DELETED_ITEMS:
				current_entry = main_entries[MAPISTORE_DELETEDITEMS_ROLE];
				break;
			default:
				current_entry = NULL;
			}

			if (current_entry) {
				if (current_entry->name) {
					current_name = current_entry->name;
				}
				mapistore_url = current_entry->url;
			}
			else {
				mapistore_url = talloc_asprintf(mem_ctx, "%s0x%"PRIx64"/", fallback_url, current_fid);
			}

			/* Ensure the name is unique */
			base_name = current_name;
			j = 1;
			while (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, username, ipm_fid, current_name, &found_fid) == MAPI_E_SUCCESS) {
				current_name = talloc_asprintf(mem_ctx, "%s (%d)", base_name, j);
				j++;
			}

			openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, ipm_fid, current_fid, current_cn, mapistore_url, i);
			property_row.lpProps[0].value.lpszW = current_name;
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);

			/* instantiate the new folder in the backend to make sure it is initialized properly */
			retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, username, mapistore_url, current_fid, &context_id, &backend_object);
			mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, context_id, username, current_fid);
			mapistore_del_context(emsmdbp_ctx->mstore_ctx, context_id);

			if (i == EMSMDBP_INBOX) {
				/* set INBOX as receive folder for "All", "IPM", "Report.IPM" */
				openchangedb_set_ReceiveFolder(emsmdbp_ctx->oc_ctx, username, "All", inbox_fid);
				openchangedb_set_ReceiveFolder(emsmdbp_ctx->oc_ctx, username, "IPM", inbox_fid);
				openchangedb_set_ReceiveFolder(emsmdbp_ctx->oc_ctx, username, "Report.IPM", inbox_fid);
			}
		}
	}

	/* Main special folders */
	/* TODO: handle entryId change + mapistore_url/mapistore_tag change */

	memset(&folder_entryid, 0, sizeof(struct FolderEntryId));
	openchangedb_get_MailboxGuid(emsmdbp_ctx->oc_ctx, username, &folder_entryid.ProviderUID);
	folder_entryid.FolderType = eitLTPrivateFolder;
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, username, NULL, &folder_entryid.FolderDatabaseGuid);

	special_folders = get_special_folders(mem_ctx, emsmdbp_ctx);
	for (i = 0; i < PROVISIONING_SPECIAL_FOLDERS_SIZE; i++) {
		current_folder = special_folders + i;
		ret = openchangedb_get_folder_property(mem_ctx, emsmdbp_ctx->oc_ctx, username, current_folder->entryid_property, mailbox_fid, (void **) &entryId);
		if (ret != MAPI_E_SUCCESS) {
			property_row.cValues = 2;
			property_row.lpProps[0].ulPropTag = PR_DISPLAY_NAME_UNICODE;

			mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_fid);
			openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);

			current_name = current_folder->name;
			current_entry = main_entries[current_folder->role];
			if (current_entry) {
				if (current_entry->name) {
					current_name = current_entry->name;
				}
				mapistore_url = current_entry->url;
			}
			else {
				mapistore_url = talloc_asprintf(mem_ctx, "%s0x%"PRIx64"/", fallback_url, current_fid);
			}

			/* Ensure the name is unique */
			base_name = current_name;
			j = 1;
			while (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, username, ipm_fid, current_name, &found_fid) == MAPI_E_SUCCESS) {
				current_name = talloc_asprintf(mem_ctx, "%s (%d)", base_name, j);
				j++;
			}

			property_row.lpProps[0].value.lpszW = current_name;
			property_row.lpProps[1].value.lpszW = container_classes[current_folder->role];
			openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, ipm_fid, current_fid, current_cn, mapistore_url, i);
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);

			/* instantiate the new folder in the backend to make sure it is initialized properly */
			retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, username, mapistore_url, current_fid, &context_id, &backend_object);
			mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, context_id, username, current_fid);
			mapistore_del_context(emsmdbp_ctx->mstore_ctx, context_id);

			/* set entryid on mailbox and inbox */
			folder_entryid.FolderGlobalCounter.value = (current_fid >> 16);
			ndr_push_struct_blob(&entryid_data, mem_ctx, &folder_entryid, (ndr_push_flags_fn_t)ndr_push_FolderEntryId);
			property_row.cValues = 1;
			property_row.lpProps[0].ulPropTag = current_folder->entryid_property;
			property_row.lpProps[0].value.bin.cb = entryid_data.length;
			property_row.lpProps[0].value.bin.lpb = entryid_data.data;

			entryid_dump = ndr_print_struct_string(mem_ctx, (ndr_print_fn_t) ndr_print_FolderEntryId, current_name, &folder_entryid);
			DEBUG(5, ("%s\n", entryid_dump));

			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, mailbox_fid, &property_row);
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, inbox_fid, &property_row);
		}
	}
	/* DEBUG(5, ("size of operation: %ld\n", talloc_total_size(mem_ctx))); */

	/* PidTagRemindersOnlineEntryId */
	if (reminders_created) {
		openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_REMINDERS, &found_fid);
		folder_entryid.FolderGlobalCounter.value = (found_fid >> 16);
		ndr_push_struct_blob(&entryid_data, mem_ctx, &folder_entryid, (ndr_push_flags_fn_t)ndr_push_FolderEntryId);
		property_row.cValues = 1;
		property_row.lpProps[0].ulPropTag = PidTagRemindersOnlineEntryId;
		property_row.lpProps[0].value.bin.cb = entryid_data.length;
		property_row.lpProps[0].value.bin.lpb = entryid_data.data;

		entryid_dump = ndr_print_struct_string(mem_ctx, (ndr_print_fn_t) ndr_print_FolderEntryId, "Reminders", &folder_entryid);
		DEBUG(5, ("%s\n", entryid_dump));

		openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, mailbox_fid, &property_row);
		openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, inbox_fid, &property_row);
	}

	/* secondary folders */
	property_row.cValues = 2;
	property_row.lpProps[0].ulPropTag = PR_DISPLAY_NAME_UNICODE;

	for (i = MAPISTORE_MAIL_ROLE; i < MAPISTORE_MAX_ROLES; i++) {
		/* secondary fallback roles are only used for synchronization */
		if (i == MAPISTORE_FALLBACK_ROLE) {
			continue;
		}

		property_row.lpProps[1].value.lpszW = container_classes[i];
		current_entry = secondary_entries[i];
		while (current_entry) {
			mapistore_url = current_entry->url;
			if (openchangedb_get_fid(emsmdbp_ctx->oc_ctx, mapistore_url, &found_fid) != MAPI_E_SUCCESS) {
				/* DEBUG(5, ("creating secondary entry '%s'\n", current_entry->url)); */
				mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_fid);
				openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);

				current_name = current_entry->name;
				/* Ensure the name is unique */
				base_name = current_name;
				j = 1;
				while (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, username, ipm_fid, current_name, &found_fid) == MAPI_E_SUCCESS) {
					current_name = talloc_asprintf(mem_ctx, "%s (%d)", base_name, j);
					j++;
				}
				property_row.lpProps[0].value.lpszW = current_name;

				openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, ipm_fid, current_fid, current_cn, mapistore_url, -1);
				openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);

				/* instantiate the new folder in the backend to make sure it is initialized properly */
				retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, username, mapistore_url, current_fid, &context_id, &backend_object);
				mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, context_id, username, current_fid);
				mapistore_del_context(emsmdbp_ctx->mstore_ctx, context_id);
			}
			else {
				/* DEBUG(5, ("secondary entry '%s' already exists\n", current_entry->url)); */
			}
			current_entry = current_entry->next;
		}
	}

	/** create root/Freebusy Data folder + "LocalFreebusy" message (OXODLGT) */
	/* FIXME: the problem here is that only the owner of a mailbox can create the delegation message and its container, meaning that when sharing an object, a delegate must at least login once to OpenChange before any one subscribes to his resources */
	if (strcmp(emsmdbp_ctx->username, username) == 0) {
		struct mapi_SRestriction restriction;
		uint8_t status;

		/* find out whether the "Freebusy Data" folder exists at the mailbox root */
		ret = openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, username, mailbox_fid, "Freebusy Data", &current_fid);
		if (ret == MAPI_E_NOT_FOUND) {
			/* create the folder */
			mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_fid);
			openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, username, &current_cn);

			property_row.cValues = 3;
			property_row.lpProps[0].ulPropTag = PidTagDisplayName;
			property_row.lpProps[0].value.lpszW = "Freebusy Data";
			property_row.lpProps[1].ulPropTag = PidTagParentFolderId;
			property_row.lpProps[1].value.d = mailbox_fid;
			property_row.lpProps[2].ulPropTag = PidTagChangeNumber;
			property_row.lpProps[2].value.d = current_cn;

			mapistore_url = talloc_asprintf(mem_ctx, "%s0x%"PRIx64"/", fallback_url, current_fid);
			openchangedb_create_folder(emsmdbp_ctx->oc_ctx, username, mailbox_fid, current_fid, current_cn, mapistore_url, -1);
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);

			/* instantiate the new folder in the backend to make sure it is initialized properly */
			mapistore_add_context(emsmdbp_ctx->mstore_ctx, username, mapistore_url, current_fid, &context_id, &backend_object);
			mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, context_id, username, current_fid);
			mapistore_properties_set_properties(emsmdbp_ctx->mstore_ctx, context_id, backend_object, &property_row);
		}
		else {
			/* open the existing folder */
			openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx, username, current_fid, &url, true);
			mapistore_add_context(emsmdbp_ctx->mstore_ctx, username, url, current_fid, &context_id, &backend_object);
			/* if (emsmdbp_ctx->mstore_ctx */
			/* mapistore_search_context_by_uri(emsmdbp_ctx->mstore_ctx, url, &context_id, &backend_object); */
		}

		/* find out whether "LocalFreebusy" message exists */
		mapistore_folder_open_table(emsmdbp_ctx->mstore_ctx, context_id, backend_object, mem_ctx, MAPISTORE_MESSAGE_TABLE, 0, &backend_table, &row_count);
		restriction.rt = RES_PROPERTY;
		restriction.res.resProperty.relop = RELOP_EQ;
		restriction.res.resProperty.ulPropTag = restriction.res.resProperty.lpProp.ulPropTag = PidTagSubject;
		restriction.res.resProperty.lpProp.value.lpszW = "LocalFreebusy";
		mapistore_table_set_restrictions(emsmdbp_ctx->mstore_ctx, context_id, backend_table, &restriction, &status);
		mapistore_table_get_row_count(emsmdbp_ctx->mstore_ctx, context_id, backend_table, MAPISTORE_PREFILTERED_QUERY, &row_count);
		if (row_count == 0) {
			/* create the message */
			retval = mapistore_indexing_get_new_folderID_as_user(emsmdbp_ctx->mstore_ctx, username, &current_mid);
			if (retval != MAPISTORE_SUCCESS) goto error;
			retval = mapistore_folder_create_message(emsmdbp_ctx->mstore_ctx, context_id, backend_object, mem_ctx, current_mid, false, &backend_message);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(3, ("[%s:%d] Error creating a message in mapistore %s\n",
					  __FUNCTION__, __LINE__, mapistore_errstr(retval)));
				goto error;
			}

			property_row.cValues = 3;
			property_row.lpProps[0].ulPropTag = PidTagMessageClass;
			property_row.lpProps[0].value.lpszW = "IPM.Microsoft.ScheduleData.FreeBusy";
			property_row.lpProps[1].ulPropTag = PidTagSubjectPrefix;
			property_row.lpProps[1].value.lpszW = "";
			property_row.lpProps[2].ulPropTag = PidTagNormalizedSubject;
			property_row.lpProps[2].value.lpszW = "LocalFreebusy";
			mapistore_properties_set_properties(emsmdbp_ctx->mstore_ctx, context_id, backend_message, &property_row);
			mapistore_message_save(emsmdbp_ctx->mstore_ctx, context_id, backend_message, mem_ctx);
		}

		mapistore_del_context(emsmdbp_ctx->mstore_ctx, context_id);
	}

	// Update mailbox's locale
	if (openchangedb_set_locale(emsmdbp_ctx->oc_ctx, username, emsmdbp_ctx->userLanguage)) {
		// Locale has changed from previous session, we have to update
		// folder's names created when provisioning the first time
		property_row.cValues = 1;
		property_row.lpProps[0].ulPropTag = PidTagDisplayName;
		for (i = EMSMDBP_MAILBOX_ROOT; i < EMSMDBP_MAX_MAILBOX_SYSTEMIDX; i++) {
			openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, i, &current_fid);
			if (i == EMSMDBP_MAILBOX_ROOT) {
				property_row.lpProps[0].value.lpszW = talloc_asprintf(mem_ctx, folder_names[i], username);
			} else {
				property_row.lpProps[0].value.lpszW = folder_names[i];
			}
			DEBUG(3, ("Changing name of system folder to %s\n", folder_names[i]));
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);
		}
		for (i = 0; i < PROVISIONING_SPECIAL_FOLDERS_SIZE; i++) {
			current_folder = special_folders + i;
			openchangedb_get_SpecialFolderID(emsmdbp_ctx->oc_ctx, username, i, &current_fid);
			property_row.lpProps[0].value.lpszW = current_folder->name;
			DEBUG(3, ("Changing name of special folder to %s\n", current_folder->name));
			openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, username, current_fid, &property_row);
		}
	}

	openchangedb_transaction_commit(emsmdbp_ctx->oc_ctx);


	/* TODO: rename/create/delete folders at IPM level */

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;

error:
	openchangedb_transaction_commit(emsmdbp_ctx->oc_ctx);
	talloc_free(mem_ctx);
	return MAPI_E_CALL_FAILED;
}
