/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

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

/**
   \file oxosfld.c

   \brief Server-side procedures about special folders
 */

#include "dcesrv_exchange_emsmdb.h"

#include <gen_ndr/ndr_property.h>

static bool fid_in_entry_id(struct Binary_r *checked_entry_id,
                            uint64_t parent_fid,
                            struct FolderEntryId *entry_id, uint64_t entry_id_fid)
{
        bool                 ret = false;
        struct FolderEntryId *checked_folder_entry_id;
        enum MAPISTATUS      retval;
        uint64_t             checked_fid;

        retval = GetFIDFromEntryID(checked_entry_id->cb, checked_entry_id->lpb,
                                   parent_fid, &checked_fid);
        if (retval != MAPI_E_SUCCESS) {
                return false;
        }

        checked_folder_entry_id = get_FolderEntryId(NULL, checked_entry_id);

        if (checked_folder_entry_id) {
                ret = (GUID_compare(&(checked_folder_entry_id->FolderDatabaseGuid), &(entry_id->FolderDatabaseGuid)) == 0
                       && GUID_compare(&(checked_folder_entry_id->ProviderUID), &(entry_id->ProviderUID)) == 0
                       && (checked_fid == entry_id_fid));
        }

        talloc_free(checked_folder_entry_id);
        return ret;
}

/**
   \details Check if a fid is an special following [MS-OXOSFLD] Section 2.2.2 rules.

   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param fid the folder identifier to check

   \return true on success, otherwise false

   \note An error on any call is interpreted as false
 */
_PUBLIC_ bool oxosfld_is_special_folder(struct emsmdbp_context *emsmdbp_ctx, uint64_t fid)
{
        bool                    ret = false;
        struct Binary_r         *entry_id;
        struct BinaryArray_r    *entries_ids;
        struct FolderEntryId    folder_entry_id;
        int                     i, system_idx;
        enum MAPISTATUS         retval;
        TALLOC_CTX              *local_mem_ctx;
        const uint32_t          identification_properties[] = { PidTagIpmAppointmentEntryId, PidTagIpmContactEntryId,
                                                                PidTagIpmJournalEntryId, PidTagIpmNoteEntryId,
                                                                PidTagIpmTaskEntryId, PidTagRemindersOnlineEntryId,
                                                                PidTagIpmDraftsEntryId };
        const uint32_t          IDENTIFICATION_PROP_COUNT = sizeof(identification_properties)/sizeof(uint32_t);
        const uint32_t          ADDITIONAL_REN_ENTRY_IDS_COUNT = 5, FREEBUSY_DATA_ENTRYID_IDX = 3;
        uint64_t                inbox_fid, mailbox_fid;

        /* Sanity checks */
        if (!emsmdbp_ctx) return false;

        /* 1. The fids returned in RopLogon are special folders, as they are set with SystemIdx
           higher than -1. See emsmdbp_provisioning.c for details */
        retval = openchangedb_get_system_idx(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fid,
                                             &system_idx);
        if (retval == MAPI_E_SUCCESS && system_idx > -1) {
                OC_DEBUG(5, "Fid 0x%"PRIx64 " is a system special folder whose system_idx is %d", fid, system_idx);
                return true;
        }

        retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
                                                 EMSMDBP_MAILBOX_ROOT, &mailbox_fid);
        if (retval == MAPI_E_SUCCESS && mailbox_fid == fid) {
                OC_DEBUG(5, "Fid 0x%"PRIx64 " is the mailbox ID", fid);
                return true;
        }

        /* 2. Set of binary properties on the root folder or inbox folder */
        /* TODO: work in Delegate mode */
        retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
                                                 EMSMDBP_INBOX, &inbox_fid);
        if (retval != MAPI_E_SUCCESS) {
                return false;
        }

        memset(&folder_entry_id, 0, sizeof(struct FolderEntryId));
        retval = openchangedb_get_MailboxGuid(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
                                              &folder_entry_id.ProviderUID);

        if (retval != MAPI_E_SUCCESS) {
                return false;
        }
        retval = openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
                                                 NULL, &folder_entry_id.FolderDatabaseGuid);
        if (retval != MAPI_E_SUCCESS) {
                return false;
        }

        local_mem_ctx = talloc_new(NULL);
        if (!local_mem_ctx) return false;

        for (i = 0; i < IDENTIFICATION_PROP_COUNT; i++) {
                retval = openchangedb_get_folder_property(local_mem_ctx, emsmdbp_ctx->oc_ctx,
                                                          emsmdbp_ctx->username, identification_properties[i],
                                                          inbox_fid, (void **) &entry_id);
                if (retval == MAPI_E_SUCCESS
                    && fid_in_entry_id(entry_id, inbox_fid, &folder_entry_id, fid)) {
                        OC_DEBUG(5, "The fid 0x%"PRIx64 " found in %s property", fid, get_proptag_name(identification_properties[i]));
                        ret = true;
                        goto end;
                }
        }

        /* 3. The PidTagAdditionalRenEntryIds contains an array of entry IDs with special folders */
        retval = openchangedb_get_folder_property(local_mem_ctx, emsmdbp_ctx->oc_ctx,
                                                  emsmdbp_ctx->username, PidTagAdditionalRenEntryIds,
                                                  inbox_fid, (void **) &entries_ids);
        if (retval == MAPI_E_SUCCESS && entries_ids
            && entries_ids->cValues >= ADDITIONAL_REN_ENTRY_IDS_COUNT) {
                for (i = 0; i < ADDITIONAL_REN_ENTRY_IDS_COUNT; i++) {
                        if (fid_in_entry_id((entries_ids->lpbin + i), inbox_fid, &folder_entry_id, fid)) {
                                OC_DEBUG(5, "The fid 0x%"PRIx64 " found as %d index in PidTagAdditionalRenEntryIds", fid, i);
                                ret = true;
                                goto end;
                        }
                }
        }

        /* 4. The PidTagAdditionalRenEntryIdsEx on the store object */
        retval = openchangedb_get_folder_property(local_mem_ctx, emsmdbp_ctx->oc_ctx,
                                                  emsmdbp_ctx->username, PidTagAdditionalRenEntryIdsEx,
                                                  inbox_fid, (void **) &entry_id);
        if (retval == MAPI_E_SUCCESS) {
                int j, k;
                struct PersistDataArray *persist_data_array;

                persist_data_array = get_PersistDataArray(local_mem_ctx, entry_id);
                if (persist_data_array && persist_data_array->cValues > 0) {
                        struct Binary_r               checked_entry_id;
                        DATA_BLOB                     checked_folder_entry_id;
                        struct PersistData            persist_data;
                        struct PersistElement         persist_element;
                        struct PersistElementArray    persist_element_array;

                        for (j = 0; j < persist_data_array->cValues; j++) {
                                persist_data = persist_data_array->lpPersistData[j];
                                if (persist_data.PersistID == PERSIST_SENTINEL) {
                                        /* This is the last PersistData */
                                        break;
                                }
                                persist_element_array = persist_data.DataElements;
                                for (k = 0; k < persist_element_array.cValues; k++) {
                                        persist_element = persist_element_array.lpPersistElement[k];
                                        if (persist_element.ElementID == PERSIST_SENTINEL) {
                                                /* This is the last PersistElement */
                                                break;
                                        }
                                        if (persist_element.ElementID == RSF_ELID_ENTRYID) {
                                                checked_folder_entry_id = persist_element.ElementData.rsf_elid_entryid;
                                                checked_entry_id.cb = checked_folder_entry_id.length;
                                                checked_entry_id.lpb = checked_folder_entry_id.data;
                                                if (fid_in_entry_id(&checked_entry_id, inbox_fid, &folder_entry_id, fid)) {
                                                        OC_DEBUG(5, "The fid 0x%"PRIx64 " found as %d entry in PidTagAdditionalRenEntryIdsEx", fid, persist_data.PersistID);
                                                        ret = true;
                                                        goto end;
                                                }
                                        }
                                }
                        }
                } else {
                        OC_DEBUG(5, "Cannot parse PersistDataArray");
                }
        }


        /* 5. The entry ID indexed by 3 in PidTagFreeBusyEntryIds on the root folder stores
           the Freebusy data folder */
        retval = openchangedb_get_folder_property(local_mem_ctx, emsmdbp_ctx->oc_ctx,
                                                  emsmdbp_ctx->username, PidTagFreeBusyEntryIds,
                                                  inbox_fid, (void **) &entries_ids);
        if (retval == MAPI_E_SUCCESS && entries_ids
            && entries_ids->cValues >= FREEBUSY_DATA_ENTRYID_IDX
            && fid_in_entry_id((entries_ids->lpbin + FREEBUSY_DATA_ENTRYID_IDX),
                               mailbox_fid, &folder_entry_id, fid)) {
                OC_DEBUG(5, "The fid 0x%"PRIx64 " found as Freebusy Data\n", fid);
                ret = true;
                goto end;
        }

        /* 6. The FID returned by RopGetReceiveFolder */
        /* According to [MS-OXOSFLD] Section 2.2.7, we should search
           for the default Receive folder for the Store object. And
           according to [MS-OXCSTOR] Section 3.2.5.2, it should be the
           user's inbox folder and it is already checked above. */

end:
        talloc_free(local_mem_ctx);

        return ret;
}
