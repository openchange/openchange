/*
   OpenChange Unit Testing

   OpenChange Project

   Copyright (C) Kamen Mazdrashki <kamenim@openchange.org> 2014
                 Enrique J. Hernandez 2015

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

#include "openchangedb_logger.h"

#include "../libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"


struct ocdb_logger_data {
	int log_level;
	const char *log_prefix;
	struct openchangedb_context *backend;
};

static struct ocdb_logger_data * _ocdb_logger_data_get(struct openchangedb_context *self)
{
	return talloc_get_type(self->data, struct ocdb_logger_data);
}

static enum MAPISTATUS get_SpecialFolderID(struct openchangedb_context *self,
					  const char *recipient, uint32_t system_idx,
					  uint64_t *folder_id)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s], system_idx=[0x%08"PRIx32"]",
					priv_data->log_prefix, recipient, system_idx);
	retval = priv_data->backend->get_SpecialFolderID(priv_data->backend, recipient, system_idx, folder_id);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], folder_id=[0x%016"PRIx64"]",
					priv_data->log_prefix, mapi_get_errstr(retval), *folder_id);

	return retval;
}

static enum MAPISTATUS get_SystemFolderID(struct openchangedb_context *self,
					  const char *recipient, uint32_t SystemIdx,
					  uint64_t *FolderId)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s], system_idx=[0x%08"PRIx32"]",
					priv_data->log_prefix, recipient, SystemIdx);
	retval = priv_data->backend->get_SystemFolderID(priv_data->backend, recipient, SystemIdx, FolderId);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], folder_id=[0x%016"PRIx64"]",
					priv_data->log_prefix, mapi_get_errstr(retval), *FolderId);

	return retval;
}

static enum MAPISTATUS get_PublicFolderID(struct openchangedb_context *self,
					  const char *username,
					  uint32_t SystemIdx,
					  uint64_t *FolderId)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], system_idx=[0x%08"PRIx32"]",
					priv_data->log_prefix, username, SystemIdx);
	retval = priv_data->backend->get_PublicFolderID(priv_data->backend, username, SystemIdx, FolderId);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], folder_id=[0x%016"PRIx64"]",
					priv_data->log_prefix, mapi_get_errstr(retval), *FolderId);

	return retval;
}

static enum MAPISTATUS get_distinguishedName(TALLOC_CTX *parent_ctx,
					     struct openchangedb_context *self,
					     uint64_t fid,
					     char **distinguishedName)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: system_idx=[0x%016"PRIx64"]",
					priv_data->log_prefix, fid);
	retval = priv_data->backend->get_distinguishedName(parent_ctx, priv_data->backend, fid, distinguishedName);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], distinguishedName=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval), *distinguishedName);

	return retval;
}

static enum MAPISTATUS get_MailboxGuid(struct openchangedb_context *self,
				       const char *recipient,
				       struct GUID *MailboxGUID)
{
	char			*guid_str = NULL;
	enum MAPISTATUS		retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s]",
					priv_data->log_prefix, recipient);
	retval = priv_data->backend->get_MailboxGuid(priv_data->backend, recipient, MailboxGUID);
	if (retval == MAPI_E_SUCCESS && MailboxGUID) {
		guid_str = GUID_string(NULL, MailboxGUID);
		OPENCHANGE_RETVAL_IF(!guid_str, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	}
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], MailboxGUID=[%s]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (guid_str) ? guid_str : "");

	talloc_free(guid_str);

	return retval;
}

static enum MAPISTATUS get_MailboxReplica(struct openchangedb_context *self,
					  const char *recipient, uint16_t *ReplID,
				  	  struct GUID *ReplGUID)
{
	char			*guid_str = NULL;
	enum MAPISTATUS		retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s]",
					priv_data->log_prefix, recipient);
	retval = priv_data->backend->get_MailboxReplica(priv_data->backend, recipient, ReplID, ReplGUID);
	if (retval == MAPI_E_SUCCESS && ReplGUID) {
		guid_str = GUID_string(NULL, ReplGUID);
		OPENCHANGE_RETVAL_IF(!guid_str, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	}
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], ReplID=[%d] ReplGUID=[%s]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (retval == MAPI_E_SUCCESS && ReplID) ? *ReplID : -1,
		 (guid_str) ? guid_str : "");

	talloc_free(guid_str);

	return retval;
}

static enum MAPISTATUS get_PublicFolderReplica(struct openchangedb_context *self,
					       const char *username,
					       uint16_t *ReplID,
					       struct GUID *ReplGUID)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
				     priv_data->log_prefix, username);
	retval = priv_data->backend->get_PublicFolderReplica(priv_data->backend, username, ReplID, ReplGUID);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_mapistoreURI(TALLOC_CTX *parent_ctx,
				        struct openchangedb_context *self,
				        const char *username,
				        uint64_t fid, char **mapistoreURL,
				        bool mailboxstore)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], fid=[0x%016"PRIx64"]",
					priv_data->log_prefix, username, fid);
	retval = priv_data->backend->get_mapistoreURI(parent_ctx, priv_data->backend, username, fid, mapistoreURL, mailboxstore);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], mapistoreURL=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval),
					retval == MAPI_E_SUCCESS ? *mapistoreURL : "undefined");

	return retval;
}

static enum MAPISTATUS set_mapistoreURI(struct openchangedb_context *self,
					const char *username, uint64_t fid,
					const char *mapistoreURL)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], fid=[0x%016"PRIx64"], mapistoreURL=[%s]",
					priv_data->log_prefix, username, fid, mapistoreURL);
	retval = priv_data->backend->set_mapistoreURI(priv_data->backend, username, fid, mapistoreURL);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_parent_fid(struct openchangedb_context *self,
				      const char *username, uint64_t fid,
				      uint64_t *parent_fidp, bool mailboxstore)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], fid=[0x%016"PRIx64"]",
					priv_data->log_prefix, username, fid);
	retval = priv_data->backend->get_parent_fid(priv_data->backend, username, fid, parent_fidp, mailboxstore);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], parent_fid=[0x%016"PRIx64"]",
					priv_data->log_prefix, mapi_get_errstr(retval), *parent_fidp);

	return retval;
}

static enum MAPISTATUS get_fid(struct openchangedb_context *self,
			       const char *mapistoreURL, uint64_t *fidp)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: mapistoreURL=[%s]",
					priv_data->log_prefix, mapistoreURL);
	retval = priv_data->backend->get_fid(priv_data->backend, mapistoreURL, fidp);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], fid=[0x%016"PRIx64"]",
					priv_data->log_prefix, mapi_get_errstr(retval), *fidp);

	return retval;
}

static enum MAPISTATUS get_MAPIStoreURIs(struct openchangedb_context *self,
					 const char *username,
					 TALLOC_CTX *mem_ctx,
					 struct StringArrayW_r **urisP)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_MAPIStoreURIs(priv_data->backend, username, mem_ctx, urisP);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_ReceiveFolder(TALLOC_CTX *parent_ctx,
					 struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass,
					 uint64_t *fid,
					 const char **ExplicitMessageClass)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s]",
					priv_data->log_prefix, recipient);
	retval = priv_data->backend->get_ReceiveFolder(parent_ctx, priv_data->backend, recipient, MessageClass, fid, ExplicitMessageClass);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_ReceiveFolderTable(TALLOC_CTX *parent_ctx,
					      struct openchangedb_context *self,
					      const char *recipient,
					      uint32_t *cValues,
					      struct ReceiveFolder **entries)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s]",
					priv_data->log_prefix, recipient);
	retval = priv_data->backend->get_ReceiveFolderTable(parent_ctx, priv_data->backend,
							    recipient, cValues, entries);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], cValues=[%d]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (retval == MAPI_E_SUCCESS && cValues) ? *cValues : -1);

	return retval;
}

static enum MAPISTATUS get_TransportFolder(struct openchangedb_context *self,
					   const char *recipient,
					   uint64_t *FolderId)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s]",
					priv_data->log_prefix, recipient);
	retval = priv_data->backend->get_TransportFolder(priv_data->backend, recipient, FolderId);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_folder_count(struct openchangedb_context *self,
					const char *username, uint64_t fid,
					uint32_t *RowCount)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_folder_count(priv_data->backend, username, fid, RowCount);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS lookup_folder_property(struct openchangedb_context *self,
					      uint32_t proptag, uint64_t fid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: ",
					priv_data->log_prefix);
	retval = priv_data->backend->lookup_folder_property(priv_data->backend, proptag, fid);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_new_changeNumber(struct openchangedb_context *self,
					    const char *username, uint64_t *cn)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_new_changeNumber(priv_data->backend, username, cn);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s] cn=[0x%016"PRIx64"]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (retval == MAPI_E_SUCCESS) ? *cn : 0);

	return retval;
}

static enum MAPISTATUS get_new_changeNumbers(struct openchangedb_context *self,
					     TALLOC_CTX *mem_ctx,
					     const char *username,
					     uint64_t max,
					     struct UI8Array_r **cns_p)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], max=[0x%016"PRIx64"]",
				     priv_data->log_prefix, username, max);
	retval = priv_data->backend->get_new_changeNumbers(priv_data->backend, mem_ctx, username, max, cns_p);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s] last_cn=[0x%016"PRIx64"]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (retval == MAPI_E_SUCCESS) ? (*cns_p)->lpui8[max - 1] : 0);

	return retval;
}

static enum MAPISTATUS get_next_changeNumber(struct openchangedb_context *self,
					     const char *username,
					     uint64_t *cn)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
				     priv_data->log_prefix, username);
	retval = priv_data->backend->get_next_changeNumber(priv_data->backend, username, cn);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], cn=[0x%016"PRIx64"]",
				     priv_data->log_prefix, mapi_get_errstr(retval), *cn);

	return retval;
}

static enum MAPISTATUS get_folder_property(TALLOC_CTX *parent_ctx,
					   struct openchangedb_context *self,
					   const char *username,
					   uint32_t proptag, uint64_t fid,
					   void **data)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_folder_property(parent_ctx, priv_data->backend, username, proptag, fid, data);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS set_folder_properties(struct openchangedb_context *self,
					     const char *username, uint64_t fid,
					     struct SRow *row)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->set_folder_properties(priv_data->backend, username, fid, row);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_table_property(TALLOC_CTX *parent_ctx,
					  struct openchangedb_context *self,
					  const char *ldb_filter,
					  uint32_t proptag, uint32_t pos,
					  void **data)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->get_table_property(parent_ctx, priv_data->backend, ldb_filter, proptag, pos, data);

	return retval;
}

static enum MAPISTATUS get_fid_by_name(struct openchangedb_context *self,
				       const char *username,
				       uint64_t parent_fid,
				       const char* foldername, uint64_t *fid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], parent_fid=[0x%016"PRIx64"], foldername=[%s]",
					priv_data->log_prefix, username, parent_fid, foldername);
	retval = priv_data->backend->get_fid_by_name(priv_data->backend, username, parent_fid, foldername, fid);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_mid_by_subject(struct openchangedb_context *self,
					  const char *username,
					  uint64_t parent_fid,
					  const char *subject,
					  bool mailboxstore, uint64_t *mid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_mid_by_subject(priv_data->backend, username, parent_fid, subject, mailboxstore, mid);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS delete_folder(struct openchangedb_context *self,
				     const char *username, uint64_t fid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->delete_folder(priv_data->backend, username, fid);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS set_ReceiveFolder(struct openchangedb_context *self,
					 const char *recipient,
					 const char *MessageClass, uint64_t fid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: recipient=[%s]",
					priv_data->log_prefix, recipient);
	retval = priv_data->backend->set_ReceiveFolder(priv_data->backend, recipient, MessageClass, fid);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_fid_from_partial_uri(struct openchangedb_context *self,
						const char *partialURI, uint64_t *fid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->get_fid_from_partial_uri(priv_data->backend, partialURI, fid);

	return retval;
}

static enum MAPISTATUS get_users_from_partial_uri(TALLOC_CTX *parent_ctx,
						  struct openchangedb_context *self,
						  const char *partialURI,
						  uint32_t *count,
						  char ***MAPIStoreURI,
						  char ***users)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->get_users_from_partial_uri(parent_ctx, priv_data->backend, partialURI, count, MAPIStoreURI, users);

	return retval;
}

static enum MAPISTATUS create_mailbox(struct openchangedb_context *self,
				      const char *username,
				      const char *organization_name,
				      const char *groupo_name,
				      int systemIdx, uint64_t fid,
				      const char *display_name)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s], org_name=[%s], groupo_name=[%s],"
				     "systemIdx=[%d], fid=[0x%016"PRIx64"], display_name=[%s]",
				     priv_data->log_prefix,
				     username, organization_name, groupo_name,
				     systemIdx, fid, display_name);
	retval = priv_data->backend->create_mailbox(priv_data->backend, username,
						    organization_name, groupo_name,
						    systemIdx, fid, display_name);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
				     priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS create_folder(struct openchangedb_context *self,
				     const char *username,
				     uint64_t parentFolderID, uint64_t fid,
				     uint64_t changeNumber,
				     const char *MAPIStoreURI, int systemIdx)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->create_folder(priv_data->backend, username, parentFolderID, fid, changeNumber, MAPIStoreURI, systemIdx);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_message_count(struct openchangedb_context *self,
					 const char *username, uint64_t fid,
					 uint32_t *RowCount, bool fai)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_message_count(priv_data->backend, username, fid, RowCount, fai);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS get_system_idx(struct openchangedb_context *self,
				      const char *username, uint64_t fid,
				      int *system_idx_p)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_system_idx(priv_data->backend, username, fid, system_idx_p);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], system_idx=[%d]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 retval == MAPI_E_SUCCESS ? *system_idx_p : -1);

	return retval;
}

static enum MAPISTATUS set_system_idx(struct openchangedb_context *self,
				      const char *username, uint64_t fid,
				      int system_idx)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s] system_idx=[%d]",
		 priv_data->log_prefix, username, system_idx);
	retval = priv_data->backend->set_system_idx(priv_data->backend, username, fid, system_idx);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS transaction_start(struct openchangedb_context *self)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->transaction_start(priv_data->backend);

	return retval;
}

static enum MAPISTATUS transaction_commit(struct openchangedb_context *self)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->transaction_commit(priv_data->backend);

	return retval;
}

static enum MAPISTATUS get_new_public_folderID(struct openchangedb_context *self,
					       const char *username,
					       uint64_t *fid)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->get_new_public_folderID(priv_data->backend, username, fid);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static bool is_public_folder_id(struct openchangedb_context *self, uint64_t fid)
{
	bool ret;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	ret = priv_data->backend->is_public_folder_id(priv_data->backend, fid);

	return ret;
}

static enum MAPISTATUS get_indexing_url(struct openchangedb_context *self,
					const char *username,
					const char **indexing_url)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
				     priv_data->log_prefix, username);
	retval = priv_data->backend->get_indexing_url(priv_data->backend, username, indexing_url);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], indexing_url=[%s]",
				     priv_data->log_prefix, mapi_get_errstr(retval),
				     *indexing_url);

	return retval;
}

static bool set_locale(struct openchangedb_context *self, const char *username, uint32_t lcid)
{
	bool ret;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	ret = priv_data->backend->set_locale(priv_data->backend, username, lcid);
	OC_DEBUG(priv_data->log_level, "%s[out]: ret=[%d]",
					priv_data->log_prefix, ret);

	return ret;
}

static const char **get_folders_names(TALLOC_CTX *mem_ctx, struct openchangedb_context *self, const char *locale, const char *type)
{
	const char **names;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	names = priv_data->backend->get_folders_names(mem_ctx, priv_data->backend, locale, type);

	return names;
}
// ^ openchangedb -------------------------------------------------------------

// v replica mapping

static enum MAPISTATUS replica_mapping_guid_to_replid(struct openchangedb_context *self, const char *username, const struct GUID *guid, uint16_t *replid_p)
{
	char			*guid_str;
	enum MAPISTATUS		retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	guid_str = GUID_string(NULL, guid);
	OPENCHANGE_RETVAL_IF(!guid_str, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s] GUID=[%s]",
		 priv_data->log_prefix, username, guid_str);
	retval = priv_data->backend->replica_mapping_guid_to_replid(priv_data->backend, username, guid, replid_p);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], replid=[%d]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (retval == MAPI_E_SUCCESS) ? *replid_p : -1);

	talloc_free(guid_str);

	return retval;
}

static enum MAPISTATUS replica_mapping_replid_to_guid(struct openchangedb_context *self, const char *username, uint16_t replid, struct GUID *guid)
{
	char			*guid_str = NULL;
	enum MAPISTATUS		retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s] replid=[%d]",
		 priv_data->log_prefix, username, replid);
	retval = priv_data->backend->replica_mapping_replid_to_guid(priv_data->backend, username, replid, guid);
	if (retval == MAPI_E_SUCCESS) {
		guid_str = GUID_string(NULL, guid);
		OPENCHANGE_RETVAL_IF(!guid_str, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
	}
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s], guid=[%s]",
		 priv_data->log_prefix, mapi_get_errstr(retval),
		 (retval == MAPI_E_SUCCESS) ? guid_str : "");

	talloc_free(guid_str);

	return retval;
}

// ^ replica mapping

// v openchangedb table -------------------------------------------------------

static enum MAPISTATUS table_init(TALLOC_CTX *mem_ctx,
				  struct openchangedb_context *self,
				  const char *username,
				  uint8_t table_type, uint64_t folderID,
				  void **table_object)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->table_init(mem_ctx, priv_data->backend, username, table_type, folderID, table_object);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS table_set_sort_order(struct openchangedb_context *self,
					    void *table_object,
					    struct SSortOrderSet *lpSortCriteria)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->table_set_sort_order(priv_data->backend, table_object, lpSortCriteria);

	return retval;
}

static enum MAPISTATUS table_set_restrictions(struct openchangedb_context *self,
					      void *table_object,
					      struct mapi_SRestriction *res)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->table_set_restrictions(priv_data->backend, table_object, res);

	return retval;
}

static enum MAPISTATUS table_get_property(TALLOC_CTX *mem_ctx,
					  struct openchangedb_context *self,
					  void *table_object,
					  enum MAPITAGS proptag, uint32_t pos,
					  bool live_filtered, void **data)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	retval = priv_data->backend->table_get_property(mem_ctx, priv_data->backend, table_object, proptag, pos, live_filtered, data);

	return retval;
}

// ^ openchangedb table -------------------------------------------------------

// v openchangedb message -----------------------------------------------------

static enum MAPISTATUS message_create(TALLOC_CTX *mem_ctx,
				      struct openchangedb_context *self,
				      const char *username,
				      uint64_t messageID, uint64_t folderID,
				      bool fai, void **message_object)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]",
					priv_data->log_prefix, username);
	retval = priv_data->backend->message_create(mem_ctx, priv_data->backend, username, messageID, folderID, fai, message_object);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]",
					priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS message_save(struct openchangedb_context *self,
				    void *_msg, uint8_t SaveFlags)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: ", priv_data->log_prefix);
	retval = priv_data->backend->message_save(priv_data->backend, _msg, SaveFlags);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]", priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS message_open(TALLOC_CTX *mem_ctx,
				    struct openchangedb_context *self,
				    const char *username,
				    uint64_t messageID, uint64_t folderID,
				    void **message_object, void **msgp)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]: username=[%s]", priv_data->log_prefix, username);
	retval = priv_data->backend->message_open(mem_ctx, priv_data->backend, username, messageID, folderID, message_object, msgp);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]", priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS message_get_property(TALLOC_CTX *mem_ctx,
					    struct openchangedb_context *self,
					    void *message_object,
					    uint32_t proptag, void **data)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]", priv_data->log_prefix);
	retval = priv_data->backend->message_get_property(mem_ctx, priv_data->backend, message_object, proptag, data);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]", priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

static enum MAPISTATUS message_set_properties(TALLOC_CTX *mem_ctx,
					      struct openchangedb_context *self,
					      void *message_object,
					      struct SRow *row)
{
	enum MAPISTATUS retval;
	struct ocdb_logger_data *priv_data = _ocdb_logger_data_get(self);

	OC_DEBUG(priv_data->log_level, "%s[in]", priv_data->log_prefix);
	retval = priv_data->backend->message_set_properties(mem_ctx, priv_data->backend, message_object, row);
	OC_DEBUG(priv_data->log_level, "%s[out]: retval=[%s]", priv_data->log_prefix, mapi_get_errstr(retval));

	return retval;
}

// ^ openchangedb message -----------------------------------------------------

_PUBLIC_ enum MAPISTATUS openchangedb_logger_initialize(TALLOC_CTX *mem_ctx,
							int log_level,
							const char *log_prefix,
							struct openchangedb_context *backend,
							struct openchangedb_context **ctx)
{
	struct openchangedb_context	*oc_ctx = talloc_zero(mem_ctx, struct openchangedb_context);
	struct ocdb_logger_data 	*data;

	OPENCHANGE_RETVAL_IF(oc_ctx == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, NULL);
	data = talloc_zero(oc_ctx, struct ocdb_logger_data);
	OPENCHANGE_RETVAL_IF(data == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, oc_ctx);

	data->backend = backend;
	data->log_level = log_level;
	data->log_prefix = log_prefix ? talloc_strdup(oc_ctx, log_prefix) : "";

	oc_ctx->data = data;

	// Initialize struct with function pointers
	oc_ctx->backend_type = talloc_strdup(oc_ctx, "logger_module");
	OPENCHANGE_RETVAL_IF(oc_ctx->backend_type == NULL, MAPI_E_NOT_ENOUGH_RESOURCES, oc_ctx);

	oc_ctx->get_new_changeNumber = get_new_changeNumber;
	oc_ctx->get_new_changeNumbers = get_new_changeNumbers;
	oc_ctx->get_next_changeNumber = get_next_changeNumber;
	oc_ctx->get_SystemFolderID = get_SystemFolderID;
	oc_ctx->get_SpecialFolderID = get_SpecialFolderID;
	oc_ctx->get_PublicFolderID = get_PublicFolderID;
	oc_ctx->get_distinguishedName = get_distinguishedName;
	oc_ctx->get_MailboxGuid = get_MailboxGuid;
	oc_ctx->get_MailboxReplica = get_MailboxReplica;
	oc_ctx->get_PublicFolderReplica = get_PublicFolderReplica;
	oc_ctx->get_parent_fid = get_parent_fid;
	oc_ctx->get_MAPIStoreURIs = get_MAPIStoreURIs;
	oc_ctx->get_mapistoreURI = get_mapistoreURI;
	oc_ctx->set_mapistoreURI = set_mapistoreURI;
	oc_ctx->get_fid = get_fid;
	oc_ctx->get_ReceiveFolder = get_ReceiveFolder;
	oc_ctx->get_ReceiveFolderTable = get_ReceiveFolderTable;
	oc_ctx->get_TransportFolder = get_TransportFolder;
	oc_ctx->lookup_folder_property = lookup_folder_property;
	oc_ctx->set_folder_properties = set_folder_properties;
	oc_ctx->get_folder_property = get_folder_property;
	oc_ctx->get_folder_count = get_folder_count;
	oc_ctx->get_message_count = get_message_count;
	oc_ctx->get_system_idx = get_system_idx;
	oc_ctx->set_system_idx = set_system_idx;
	oc_ctx->get_table_property = get_table_property;
	oc_ctx->get_fid_by_name = get_fid_by_name;
	oc_ctx->get_mid_by_subject = get_mid_by_subject;
	oc_ctx->set_ReceiveFolder = set_ReceiveFolder;
	oc_ctx->create_mailbox = create_mailbox;
	oc_ctx->create_folder = create_folder;
	oc_ctx->delete_folder = delete_folder;
	oc_ctx->get_fid_from_partial_uri = get_fid_from_partial_uri;
	oc_ctx->get_users_from_partial_uri = get_users_from_partial_uri;

	oc_ctx->table_init = table_init;
	oc_ctx->table_set_sort_order = table_set_sort_order;
	oc_ctx->table_set_restrictions = table_set_restrictions;
	oc_ctx->table_get_property = table_get_property;

	oc_ctx->message_create = message_create;
	oc_ctx->message_save = message_save;
	oc_ctx->message_open = message_open;
	oc_ctx->message_get_property = message_get_property;
	oc_ctx->message_set_properties = message_set_properties;

	oc_ctx->transaction_start = transaction_start;
	oc_ctx->transaction_commit = transaction_commit;

	oc_ctx->get_new_public_folderID = get_new_public_folderID;
	oc_ctx->is_public_folder_id = is_public_folder_id;

	oc_ctx->get_indexing_url = get_indexing_url;
	oc_ctx->set_locale = set_locale;
	oc_ctx->get_folders_names = get_folders_names;

	oc_ctx->replica_mapping_guid_to_replid = replica_mapping_guid_to_replid;
	oc_ctx->replica_mapping_replid_to_guid = replica_mapping_replid_to_guid;

	*ctx = oc_ctx;

	return MAPI_E_SUCCESS;
}
