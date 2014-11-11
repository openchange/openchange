/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2011
   Copyright (C) Jesús García Sáez 2014

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
   \file openchangedb.c

   \brief OpenChange Dispatcher database routines
 */

#include <inttypes.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/servers/default/emsmdb/dcesrv_exchange_emsmdb.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_ldb.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_logger.h"

const char *nil_string = "<nil>";


_PUBLIC_ enum MAPISTATUS openchangedb_initialize(TALLOC_CTX *mem_ctx,
						 struct loadparm_context *lp_ctx,
						 struct openchangedb_context **oc_ctx)
{
	enum MAPISTATUS retval;
	const char *openchangedb_backend = lpcfg_parm_string(lp_ctx, NULL, "mapiproxy",
							     "openchangedb");
	if (openchangedb_backend) {
		DEBUG(0, ("Using MySQL backend for openchangedb: %s\n", openchangedb_backend));
		retval = openchangedb_mysql_initialize(mem_ctx, lp_ctx, oc_ctx);
	} else {
		DEBUG(0, ("Using default backend for openchangedb\n"));
		retval = openchangedb_ldb_initialize(mem_ctx,
						     lpcfg_private_dir(lp_ctx),
						     oc_ctx);
	}

	if (retval != MAPI_E_SUCCESS) {
		return retval;
	}

	if (lpcfg_parm_bool(lp_ctx, NULL, "mapiproxy", "openchangedb_logger", false)) {
		const char *prefix = lpcfg_parm_string(lp_ctx, NULL, "mapiproxy",
						       "openchangedb_logger_prefix");
		DEBUG(0, ("Loading OpenchangeDB logger module\n"));
		retval = openchangedb_logger_initialize(mem_ctx, 0, prefix, *oc_ctx, oc_ctx);
	}

	return retval;
}

/**
   \details Retrieve the folder id of a special folder with the specific
   SystemIdx for given recipient from openchange dispatcher database.

   \param oc_ctx pointer to the OpenChangeDB context
   \param recipient the mailbox username
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_SpecialFolderID(struct openchangedb_context *oc_ctx,
							  const char *recipient, uint32_t SystemIdx,
							  uint64_t *FolderId)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_SpecialFolderID(oc_ctx, recipient, SystemIdx, FolderId);
}

/**
   \details Retrieve the mailbox FolderID for given recipient from
   openchange dispatcher database

   \param oc_ctx pointer to the OpenChangeDB context
   \param recipient the mailbox username
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_SystemFolderID(struct openchangedb_context *oc_ctx,
							 const char *recipient, uint32_t SystemIdx,
							 uint64_t *FolderId)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);
	
	return oc_ctx->get_SystemFolderID(oc_ctx, recipient, SystemIdx, FolderId);
}

/**
   \details Retrieve the public folder FolderID (fid) for a given folder type

   \param oc_ctx pointer to the OpenChangeDB context
   \param username current user
   \param SystemIdx the system folder index
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_PublicFolderID(struct openchangedb_context *oc_ctx,
							 const char *username,
							 uint32_t SystemIdx, uint64_t *FolderId)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_PublicFolderID(oc_ctx, username, SystemIdx, FolderId);
}

/**
   FIXME Not used anywhere. Remove it?
   \details Retrieve the distinguishedName associated to a mailbox
   system folder.

   \param parent_ctx pointer to the parent memory context
   \param oc_ctx pointer to the openchange DB context
   \param fid the Folder identifier to search for
   \param distinguishedName pointer on pointer to the
   distinguishedName string the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_distinguishedName(TALLOC_CTX *parent_ctx,
							    struct openchangedb_context *oc_ctx,
							    uint64_t fid, char **distinguishedName)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!distinguishedName, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_distinguishedName(parent_ctx, oc_ctx, fid, distinguishedName);
}

/**
   \details Retrieve the mailbox GUID for given recipient from
   openchange dispatcher database

   \param oc_ctx pointer to the OpenChangeDB context
   \param recipient the mailbox username
   \param MailboxGUID pointer to the mailbox GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxGuid(struct openchangedb_context *oc_ctx,
						      const char *recipient, struct GUID *MailboxGUID)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MailboxGUID, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_MailboxGuid(oc_ctx, recipient, MailboxGUID);
}

/**
   \details Retrieve the mailbox replica identifier and GUID for given
   recipient from openchange dispatcher database

   \param oc_ctx pointer to the OpenChangeDB context
   \param recipient the mailbox username
   \param ReplID pointer to the replica identifier the function returns
   \param ReplGUID pointer to the replica GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MailboxReplica(struct openchangedb_context *oc_ctx,
							 const char *recipient, uint16_t *ReplID,
							 struct GUID *ReplGUID)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_MailboxReplica(oc_ctx, recipient, ReplID, ReplGUID);
}

/**
   \details Retrieve the public folder replica identifier and GUID
   from the openchange dispatcher database

   \param oc_ctx pointer to the OpenChangeDB context
   \param username current user
   \param ReplID pointer to the replica identifier the function returns
   \param ReplGUID pointer to the replica GUID the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_PublicFolderReplica(struct openchangedb_context *oc_ctx,
							      const char *username,
							      uint16_t *ReplID, struct GUID *ReplGUID)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_PublicFolderReplica(oc_ctx, username, ReplID, ReplGUID);
}

/**
   \details Retrieve the mapistore URI associated to a mailbox system folder.

   \param parent_ctx pointer to the memory context
   \param oc_ctx pointer to the openchange DB context
   \param username current user
   \param fid the Folder identifier to search for
   \param mapistoreURL pointer on pointer to the mapistore URI the
   function returns
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not
   FIXME mailboxstore always true? remove it?

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mapistoreURI(TALLOC_CTX *parent_ctx,
						       struct openchangedb_context *oc_ctx,
						       const char *username,
						       uint64_t fid,
						       char **mapistoreURL,
						       bool mailboxstore)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapistoreURL, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_mapistoreURI(parent_ctx, oc_ctx, username, fid,
					mapistoreURL, mailboxstore);
}

/**
   \details Store the mapistore URI associated to a mailbox system folder.

   \param oc_ctx pointer to the openchange DB context
   \param username current user
   \param fid the Folder identifier to search for
   \param mapistoreURL The mapistore URI to set

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_set_mapistoreURI(struct openchangedb_context *oc_ctx,
						       const char *username,
						       uint64_t fid,
						       const char *mapistoreURL)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mapistoreURL, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->set_mapistoreURI(oc_ctx, username, fid, mapistoreURL);
}

/**
   \details Retrieve the parent fid associated to a mailbox system
   folder.

   \param oc_ctx pointer to the openchange DB context
   \param username the mailbox name
   \param fid the Folder identifier to search for
   \param parent_fidp pointer to the parent_fid the function returns
   \param mailboxstore boolean value which defines whether the record
   has to be searched within Public folders hierarchy or not

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_parent_fid(struct openchangedb_context *oc_ctx,
						     const char *username,
						     uint64_t fid,
						     uint64_t *parent_fidp,
						     bool mailboxstore)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!parent_fidp, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_parent_fid(oc_ctx, username, fid, parent_fidp,
				      mailboxstore);
}

/**
   \details Retrieve the fid associated with a mapistore URI.

   \param oc_ctx pointer to the openchange DB context
   \param mapistoreURL mapistore URI to look for
   \param fidp pointer to fid the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_fid(struct openchangedb_context *oc_ctx,
					      const char *mapistoreURL,
					      uint64_t *fidp)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!mapistoreURL, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fidp, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_fid(oc_ctx, mapistoreURL, fidp);
}

/**
   \details Retrieve a list of mapistore URI in use for a certain user

   \param oc_ctx pointer to the openchange DB context
   \param username the mailbox name
   \param mem_ctx memory context where found URIs will be allocated
   \param urisP where the Mapistore URIs will be stored

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_MAPIStoreURIs(struct openchangedb_context *oc_ctx,
							const char *username, TALLOC_CTX *mem_ctx,
							struct StringArrayW_r **urisP)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!urisP, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_MAPIStoreURIs(oc_ctx, username, mem_ctx, urisP);
}

/**
   \details Retrieve the Explicit message class and Folder identifier
   associated to the MessageClass search pattern.

   \param parent_ctx pointer to the memory context
   \param oc_ctx pointer to the openchange DB context
   \param recipient pointer to the mailbox's username
   \param MessageClass substring to search for
   \param fid pointer to the folder identifier the function returns
   \param ExplicitMessageClass pointer on pointer to the complete
   message class the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_ReceiveFolder(TALLOC_CTX *parent_ctx,
							struct openchangedb_context *oc_ctx,
							const char *recipient,
							const char *MessageClass,
							uint64_t *fid,
							const char **ExplicitMessageClass)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MessageClass, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!ExplicitMessageClass, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_ReceiveFolder(parent_ctx, oc_ctx, recipient,
					 MessageClass, fid, ExplicitMessageClass);
}


/**
   \details Retrieve the list of receive folder identifier, message
   class along with latest modification time for the mailbox of the
   user.

   \param mem_ctx pointer to the memory context
   \param oc_ctx pointer to the openchange DB context
   \param recipient pointer to the mailbox's username
   \param cValues pointer to the number of ReceiveFolder records the
   function returns
   \param entries pointer on pointer to the list of ReceiveFolder structure to
   return

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_ReceiveFolderTable(TALLOC_CTX *mem_ctx,
							     struct openchangedb_context *oc_ctx,
							     const char *recipient,
							     uint32_t *cValues,
							     struct ReceiveFolder **entries)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!cValues, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!entries, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_ReceiveFolderTable(mem_ctx, oc_ctx, recipient,
					      cValues, entries);
}

/**
   \details Retrieve the Transport Folder FolderID for given recipient
   from openchange dispatcher database

   \param oc_ctx pointer to the OpenChangeDB context
   \param recipient the mailbox username
   \param FolderId pointer to the folder identifier the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_TransportFolder(struct openchangedb_context *oc_ctx,
							  const char *recipient, uint64_t *FolderId)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!FolderId, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_TransportFolder(oc_ctx, recipient, FolderId);
}

/**
   \details Retrieve the number of sub folders for a given fid

   \param oc_ctx pointer to the openchange DB context
   \param username name of the mailbox where the folder is
   \param fid the folder identifier to use for the search
   \param RowCount pointer to the returned number of results

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_count(struct openchangedb_context *oc_ctx,
						       const char *username,
						       uint64_t fid,
						       uint32_t *RowCount)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!RowCount, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_folder_count(oc_ctx, username, fid, RowCount);
}

/**
   FIXME Not used anywhere. Remove it?
   \details Check if a property exists within an openchange dispatcher
   database record

   \param oc_ctx pointer to the openchange DB context
   \param proptag the MAPI property tag to lookup
   \param fid the record folder identifier

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_lookup_folder_property(struct openchangedb_context *oc_ctx,
							     uint32_t proptag, uint64_t fid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->lookup_folder_property(oc_ctx, proptag, fid);
}

/**
   \details Build a MAPI property suitable for a OpenChange LDB message

   \param mem_ctx pointer to the memory context
   \param value the MAPI property

   \return valid string pointer on success, otherwise NULL
 */
_PUBLIC_ char *openchangedb_set_folder_property_data(TALLOC_CTX *mem_ctx, 
						     struct SPropValue *value)
{
	char			*data, *subdata;
	struct SPropValue	*subvalue;
	NTTIME			nt_time;
	uint32_t		i;
	size_t		        data_len, subdata_len;
	struct BinaryArray_r	*bin_array;

	switch (value->ulPropTag & 0xFFFF) {
	case PT_BOOLEAN:
		data = talloc_strdup(mem_ctx, value->value.b ? "TRUE" : "FALSE");
		break;
	case PT_LONG:
		data = talloc_asprintf(mem_ctx, "%d", value->value.l);
		break;
	case PT_I8:
		data = talloc_asprintf(mem_ctx, "%"PRIu64, value->value.d);
		break;
	case PT_STRING8:
		data = talloc_strdup(mem_ctx, value->value.lpszA);
		break;
	case PT_UNICODE:
		data = talloc_strdup(mem_ctx, value->value.lpszW);
		break;
	case PT_SYSTIME:
		nt_time = ((uint64_t) value->value.ft.dwHighDateTime << 32) | value->value.ft.dwLowDateTime;
		data = talloc_asprintf(mem_ctx, "%"PRIu64, nt_time);
		break;
	case PT_BINARY:
		/* ldb silently prevents empty strings from being added to the db */
		if (value->value.bin.cb > 0)
			data = ldb_base64_encode(mem_ctx, (char *) value->value.bin.lpb, value->value.bin.cb);
		else
			data = talloc_strdup(mem_ctx, nil_string);
		break;
	case PT_MV_BINARY:
		bin_array = &value->value.MVbin;
		data = talloc_asprintf(mem_ctx, "0x%.8x", bin_array->cValues);
		data_len = strlen(data);
		for (i = 0; i < bin_array->cValues; i++) {
			subvalue = talloc_zero(NULL, struct SPropValue);
			subvalue->ulPropTag = (value->ulPropTag & 0xffff0fff);
			subvalue->value.bin = bin_array->lpbin[i];
			subdata = openchangedb_set_folder_property_data(subvalue, subvalue);
			subdata_len = strlen(subdata);
			data = talloc_realloc(mem_ctx, data, char, data_len + subdata_len + 2);
			*(data + data_len) = ';';
			memcpy(data + data_len + 1, subdata, subdata_len);
			data_len += subdata_len + 1;
			talloc_free(subvalue);
		}
		*(data + data_len) = 0;
		break;
	default:
		DEBUG(0, ("[%s:%d] Property Type 0x%.4x not supported\n",
		          __FUNCTION__, __LINE__, (value->ulPropTag & 0xFFFF)));
		data = NULL;
	}

	return data;
}

/**
   \details Allocates a new change number and returns it
   
   \param oc_ctx pointer to the openchange DB context
   \param username current user
   \param cn pointer to the cn value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_changeNumber(struct openchangedb_context *oc_ctx, const char *username, uint64_t *cn)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!cn, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_new_changeNumber(oc_ctx, username, cn);
}

/**
   \details Allocates a batch of new change numbers and returns them
   
   \param oc_ctx pointer to the openchange DB context
   \param mem_ctx memory context where the change numbers will be allocated
   \param username current user
   \param max number of change number to allocate
   \param cns_p array of pointers to the change numbers values the function
   returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_changeNumbers(struct openchangedb_context *oc_ctx,
							    TALLOC_CTX *mem_ctx,
							    const char *username,
							    uint64_t max,
							    struct UI8Array_r **cns_p)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!cns_p, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_new_changeNumbers(oc_ctx, mem_ctx, username, max, cns_p);
}

/**
   \details Returns the change number that will be allocated when
   openchangedb_get_new_changeNumber is next invoked
   
   \param oc_ctx pointer to the openchange DB context
   \param username current user
   \param cn pointer to the cn value the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_next_changeNumber(struct openchangedb_context *oc_ctx, const char *username, uint64_t *cn)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!cn, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_next_changeNumber(oc_ctx, username, cn);
}

/**
   \details Retrieve a MAPI property value from a folder record

   \param parent_ctx pointer to the memory context
   \param oc_ctx pointer to the openchange DB context
   \param username mailbox name where the folder is
   \param proptag the MAPI property tag to retrieve value for
   \param fid the record folder identifier
   \param data pointer on pointer to the data the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_folder_property(TALLOC_CTX *parent_ctx, 
							  struct openchangedb_context *oc_ctx,
							  const char *username,
							  uint32_t proptag,
							  uint64_t fid,
							  void **data)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!data, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_folder_property(parent_ctx, oc_ctx, username,
					   proptag, fid, data);
}

/**
   \details Set a MAPI property value from a folder record

   \param oc_ctx pointer to the openchange DB context
   \param username name of the mailbox where the folder is
   \param fid the record folder identifier
   \param row the MAPI property to set

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_set_folder_properties(struct openchangedb_context *oc_ctx,
							    const char *username,
							    uint64_t fid,
							    struct SRow *row)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!row, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->set_folder_properties(oc_ctx, username, fid, row);
}

/**
   FIXME Not used anywhere, remove it? see mapiproxy/servers/default/emsmdb/emsmdbp_object.c +1712
   \details Retrieve a MAPI property from a table (ldb search results)

   \param parent_ctx pointer to the memory context
   \param oc_ctx pointer to the openchange DB context
   \param ldb_filter the ldb search string
   \param proptag the MAPI property tag to retrieve value for
   \param pos the record position in search results
   \param data pointer on pointer to the data the function returns

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_table_property(TALLOC_CTX *parent_ctx,
							 struct openchangedb_context *oc_ctx,
							 const char *ldb_filter,
							 uint32_t proptag,
							 uint32_t pos,
							 void **data)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!ldb_filter, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!data, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_table_property(parent_ctx, oc_ctx, ldb_filter, proptag, pos, data);
}

/**
   \details Retrieve the folder ID associated with a given folder name

   This function looks up the specified foldername (as a PidTagDisplayName)
   and returns the associated folder ID. Note that folder names are only
   unique in the context of a parent folder, so the parent folder needs to
   be provided.

   \param oc_ctx pointer to the openchange DB context
   \param username mailbox name where the folder is
   \param parent_fid the folder ID of the parent folder 
   \param foldername the name to look up
   \param fid the folder ID for the folder with the specified name (0 if not found)

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_fid_by_name(struct openchangedb_context *oc_ctx,
						      const char *username,
						      uint64_t parent_fid,
						      const char* foldername,
						      uint64_t *fid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!foldername, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	
	return oc_ctx->get_fid_by_name(oc_ctx, username, parent_fid, foldername, fid);
}

/**
   \details Retrieve the message ID associated with a given subject (normalized)

   \param oc_ctx pointer to the openchange DB context
   \param username mailbox name where the parent folder is
   \param parent_fid the folder ID of the parent folder 
   \param subject the normalized subject to look up
   \param mailboxstore whether the folder is under a mailbox or not
   \param mid the message ID for the message (0 if not found)

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_mid_by_subject(struct openchangedb_context *oc_ctx,
							 const char *username,
							 uint64_t parent_fid,
							 const char *subject,
							 bool mailboxstore,
							 uint64_t *mid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!subject, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mid, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_mid_by_subject(oc_ctx, username, parent_fid, subject,
					  mailboxstore, mid);
}

/**
   \details Delete a folder

   \param oc_ctx pointer to the openchange DB context
   \param username mailbox name where the folder is
   \param fid the record folder identifier to be deleted

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_delete_folder(struct openchangedb_context *oc_ctx,
						    const char *username,
						    uint64_t fid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	
	return oc_ctx->delete_folder(oc_ctx, username, fid);
}

/**
   \details Set the receive folder for a specific message class.

   \param oc_ctx pointer to the openchange DB context
   \param recipient pointer to the mailbox's username
   \param MessageClass message class (e.g. IPM.whatever) to set
   \param fid folder identifier for the recipient folder for the message class

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_set_ReceiveFolder(struct openchangedb_context *oc_ctx,
							const char *recipient,
							const char *MessageClass, uint64_t fid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!recipient, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MessageClass, MAPI_E_INVALID_PARAMETER, NULL);
	
	return oc_ctx->set_ReceiveFolder(oc_ctx, recipient, MessageClass, fid);
}

/**
   FIXME Not used anywhere, remove it?
   \details Get fid given a partial MAPIStoreURI

   \param oc_ctx pointer to the openchange DB context
   \param partialURI partial MAPIStoreURI to look for
   \param fid pointer on function result
   for the message class

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_fid_from_partial_uri(struct openchangedb_context *oc_ctx,
							       const char *partialURI, uint64_t *fid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!partialURI, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	
	return oc_ctx->get_fid_from_partial_uri(oc_ctx, partialURI, fid);
}

/**
   FIXME broken? mailboxDN not saved?
   \details Get users given a partial MAPIStoreURI

   \param parent_ctx memory context
   \param oc_ctx pointer to the openchange DB context
   \param partialURI partial MAPIStoreURI to look for
   \param count number of users found
   \param MAPIStoreURI MAPIStoreURIs of the users found from the partial
   MAPIStoreURI
   \param users name of the mailboxes found from the partial MAPIStoreURI

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_users_from_partial_uri(TALLOC_CTX *parent_ctx,
								 struct openchangedb_context *oc_ctx,
								 const char *partialURI,
								 uint32_t *count,
								 char ***MAPIStoreURI,
								 char ***users)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!partialURI, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!MAPIStoreURI, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!users, MAPI_E_INVALID_PARAMETER, NULL);
	
	return oc_ctx->get_users_from_partial_uri(parent_ctx, oc_ctx, partialURI,
						  count, MAPIStoreURI, users);

}

/**
   \details Create a mailbox in openchangedb

   \param oc_ctx pointer to the openchange DB context
   \param username the owner of the mailbox
   \param organization_name name of the organization of the user
   \param group_name name of the group where the organization of the user belongs
   \param fid The fid used for the mailbox
   \param display_name Name of the mailbox

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_create_mailbox(struct openchangedb_context *oc_ctx,
						     const char *username,
						     const char *organization_name,
						     const char *group_name,
						     uint64_t fid,
						     const char *display_name)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!organization_name, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!group_name, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!display_name, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->create_mailbox(oc_ctx, username, organization_name,
				      group_name, EMSMDBP_MAILBOX_ROOT, fid,
				      display_name);
}

/**
   \details Create a folder in openchangedb

   \param oc_ctx pointer to the openchange DB context
   \param username The name of the mailbox where the parent folder is
   \param parentFolderID the FID of the parent folder
   \param fid the FID of the folder to create
   \param changeNumber the change number
   \param MAPIStoreURI the mapistore URI to associate to this folder
   \param systemIdx the systemIdx value for the new folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_create_folder(struct openchangedb_context *oc_ctx,
						    const char *username,
						    uint64_t parentFolderID,
						    uint64_t fid,
						    uint64_t changeNumber,
						    const char *MAPIStoreURI,
						    int systemIdx)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!parentFolderID, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!changeNumber, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->create_folder(oc_ctx, username, parentFolderID, fid,
				     changeNumber, MAPIStoreURI, systemIdx);
}

/**
   \details Retrieve the number of messages within the specified folder

   \param oc_ctx pointer to the openchange DB context
   \param username Name of the mailbox where the folder is
   \param fid the folder identifier to use for the search
   \param RowCount pointer to the returned number of results
   \param fai whether we want to count fai messages or system messages

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_message_count(struct openchangedb_context *oc_ctx,
							const char *username,
							uint64_t fid,
							uint32_t *RowCount,
							bool fai)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!RowCount, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_message_count(oc_ctx, username, fid, RowCount, fai);
}

/**
   \details Retrieve the system idx associated with a folder record

   \param oc_ctx pointer to the openchange DB context
   \param username the name of the mailbox where the folder is
   \param fid the folder identifier to use for the search
   \param system_idx_p pointer to the returned value

   \return MAPI_E_SUCCESS on success, otherwise MAPI_E_NOT_FOUND
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_system_idx(struct openchangedb_context *oc_ctx,
						     const char *username,
						     uint64_t fid,
						     int *system_idx_p)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!system_idx_p, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_system_idx(oc_ctx, username, fid, system_idx_p);
}

_PUBLIC_ enum MAPISTATUS openchangedb_transaction_start(struct openchangedb_context *oc_ctx)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->transaction_start(oc_ctx);
}

_PUBLIC_ enum MAPISTATUS openchangedb_transaction_commit(struct openchangedb_context *oc_ctx)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->transaction_commit(oc_ctx);
}

/**
   \details Retrieve a new folder id for a public folder, only used on cases
   where the openchangedb backend uses folder ids per user. In that case the
   public folder ids will still be global, per organization

   \param oc_ctx pointer to the openchange DB context
   \param username The name of the current login user so we can now the current
   organization where the public folder will be created
   \param fid pointer to the returned value
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_new_public_folderID(struct openchangedb_context *oc_ctx,
							      const char *username,
							      uint64_t *fid)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fid, MAPI_E_INVALID_PARAMETER, NULL);

	return oc_ctx->get_new_public_folderID(oc_ctx, username, fid);
}

/**
   \details Whether the current openchangedb backend uses folder ids per user
   and the given folder id as parameter belongs to a public folder id (which
   have global folder ids in an organization)

   \param oc_ctx pointer to the openchange DB context
   \param fid folder id to identify if belongs to a either public or system
   folder
 */
_PUBLIC_ bool openchangedb_is_public_folder_id(struct openchangedb_context *oc_ctx,
					       uint64_t fid)
{
	if (!oc_ctx) {
		DEBUG(0, ("Bad parameters when calling openchangedb_is_public_folder_id\n"));
		return false;
	}
	return oc_ctx->is_public_folder_id(oc_ctx, fid);
}

/**
   \details Get the indexing url backend for the user given as parameter. The
   user represents a mailbox.
   By default a tdb backend file per user will be used in case an error is
   returned.

   \param oc_ctx pointer to the openchange DB context
   \param username Name of the mailbox
   \param indexing_url pointer to the result returned
 */
_PUBLIC_ enum MAPISTATUS openchangedb_get_indexing_url(struct openchangedb_context *oc_ctx,
						       const char *username,
						       const char **indexing_url)
{
	enum MAPISTATUS	retval;

	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!indexing_url, MAPI_E_INVALID_PARAMETER, NULL);

	retval = oc_ctx->get_indexing_url(oc_ctx, username, indexing_url);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);

	/* check indexing_url we are about to return */
	if (*indexing_url == NULL) {
		return MAPI_E_NOT_FOUND;
	} else if (!*indexing_url[0]) {
		DEBUG(3, ("[%s:%d]: Invalid empty indexing url for user %s\n",
			  __FUNCTION__, __LINE__, username));
		return MAPI_E_NOT_FOUND;
	}

	return MAPI_E_SUCCESS;
}

/**
   \details Set the current locale of the mailbox

   \param oc_ctx pointer to the openchange DB context
   \param username Name of the mailbox
   \param lcid language id

   \return Whether the locale has been changed or not
 */
_PUBLIC_ bool openchangedb_set_locale(struct openchangedb_context *oc_ctx,
				      const char *username, uint32_t lcid)
{
	if (!oc_ctx || !username) {
		DEBUG(0, ("Bad parameters when calling openchangedb_set_locale\n"));
		return false;
	}
	return oc_ctx->set_locale(oc_ctx, username, lcid);
}

/**
   \details Get a list of names depending of the locale given as parameter

   \param mem_ctx context memory where the return value will be allocated
   \param oc_ctx pointer to the openchange DB context
   \param locale something like en_UK or just en (in that case will be look
   for any en_* entries)
   \param type the table to look for values, it must be either "special_folders"
   or "folders"

   \return array of strings with i18n folders names
 */
_PUBLIC_ const char **openchangedb_get_folders_names(TALLOC_CTX *mem_ctx,
						     struct openchangedb_context *oc_ctx,
						     const char *locale,
						     const char *type)
{
	if (!oc_ctx || !locale || !type) {
		DEBUG(0, ("Bad parameters when calling openchangedb_get_folders_names\n"));
		return NULL;
	}
	if (strcmp("special_folders", type) != 0 && strcmp("folders", type) != 0) {
		DEBUG(0, ("Bad type parameter (%s) for openchangedb_get_folders_names\n", type));
		return NULL;
	}
	return oc_ctx->get_folders_names(mem_ctx, oc_ctx, locale, type);
}
