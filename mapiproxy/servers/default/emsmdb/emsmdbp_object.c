/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009

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
   \file emsmdbp_object.c

   \brief Server-side specific objects init/release routines
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"

#include "dcesrv_exchange_emsmdb.h"

/* a private struct used to map array for properties with MV_FLAG set in a type-agnostic way */
struct DataArray_r {
	uint32_t cValues;
	const void *values;
};

const char *emsmdbp_getstr_type(struct emsmdbp_object *object)
{
	switch (object->type) {
	case EMSMDBP_OBJECT_UNDEF:
		return "undefined";
	case EMSMDBP_OBJECT_MAILBOX:
		return "mailbox";
	case EMSMDBP_OBJECT_FOLDER:
		return "folder";
	case EMSMDBP_OBJECT_MESSAGE:
		return "message";
	case EMSMDBP_OBJECT_TABLE:
		return "table";
	case EMSMDBP_OBJECT_STREAM:
		return "stream";
	case EMSMDBP_OBJECT_ATTACHMENT:
		return "attachment";
	case EMSMDBP_OBJECT_SUBSCRIPTION:
		return "subscription";
	case EMSMDBP_OBJECT_SYNCCONTEXT:
		return "synccontext";
	case EMSMDBP_OBJECT_FTCONTEXT:
		return "ftcontext";
	default:
		return "unknown";
	}
}

/**
   \details Convenient function to determine whether specified
   object is using mapistore or not

   \param object pointer to the emsmdp object

   \return true if parent is using mapistore, otherwise false
 */
bool emsmdbp_is_mapistore(struct emsmdbp_object *object)
{
	/* Sanity checks - probably pointless */
	if (!object) {
		return false;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		return false;
	case EMSMDBP_OBJECT_FOLDER:
		if (object->object.folder->mapistore_root) {
			return true;
		}
	default:
		if (object->parent_object) {
			return emsmdbp_is_mapistore(object->parent_object);
		}
	}

	return false;
}

/**
   \details Convenient function to determine whether specified
   mapi_handles refers to object within mailbox or public folders
   store.

   \param object pointer to the emsmdp object

   \return true if parent is within mailbox store, otherwise false
 */
bool emsmdbp_is_mailboxstore(struct emsmdbp_object *object)
{
	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		return object->object.mailbox->mailboxstore;
	default:
		if (object->parent_object) {
			return emsmdbp_is_mailboxstore(object->parent_object);
		}
	}
	
	/* We should never hit this case */
	return false;
}


/**
   \details Return the contextID associated to a handle

   \param object pointer to the emsmdp object

   \return contextID value on success, otherwise -1
 */
_PUBLIC_ uint32_t emsmdbp_get_contextID(struct emsmdbp_object *object)
{
	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		return -1;
	case EMSMDBP_OBJECT_FOLDER:
		if (object->object.folder->mapistore_root) {
			return object->object.folder->contextID;
		}
	default:
		if (object->parent_object) {
			return emsmdbp_get_contextID(object->parent_object);
		}
	}

	return -1;
}

_PUBLIC_ enum MAPISTATUS emsmdbp_object_get_fid_by_name(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_folder, const char *name, uint64_t *fidp)
{
	uint64_t	folderID;

	if (!emsmdbp_ctx) return MAPI_E_CALL_FAILED;
	if (!parent_folder) return MAPI_E_CALL_FAILED;
	if (!name) return MAPI_E_CALL_FAILED;
	if (!fidp) return MAPI_E_CALL_FAILED;

	if (parent_folder->type == EMSMDBP_OBJECT_FOLDER) {
		folderID = parent_folder->object.folder->folderID;
	}
	else if (parent_folder->type == EMSMDBP_OBJECT_MAILBOX) {
		folderID = parent_folder->object.mailbox->folderID;
	}
	else {
		return MAPI_E_CALL_FAILED;
	}

	if (emsmdbp_is_mapistore(parent_folder)) {
		if (mapistore_folder_get_child_fid_by_name(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent_folder), parent_folder->backend_object, name, fidp)) {
			return MAPI_E_NOT_FOUND;
		}

		return MAPI_E_SUCCESS;
	}
	else {
		return openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, folderID, name, fidp);
	}
}

_PUBLIC_ enum MAPISTATUS emsmdbp_object_create_folder(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_folder, TALLOC_CTX *mem_ctx, uint64_t fid, struct SRow *rowp, struct emsmdbp_object **new_folderp)
{
	uint64_t			parentFolderID;
	uint64_t			testFolderID;
	char				*parentDN;
	char				*dn;
	char				*mailboxDN;
	struct ldb_dn			*basedn;
	struct ldb_message		*msg;
	struct SPropValue		*value;
	NTTIME				nt_time;
	int				retval;
	TALLOC_CTX			*local_mem_ctx;
	struct emsmdbp_object		*new_folder;
	uint32_t			contextID;
	char				*path;

	/* Sanity checks */

	if (!emsmdbp_ctx) return MAPI_E_CALL_FAILED;
	if (!parent_folder) return MAPI_E_CALL_FAILED;
	if (!rowp) return MAPI_E_CALL_FAILED;

	new_folder = emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, fid, parent_folder);
	if (emsmdbp_is_mapistore(parent_folder)) {
		retval = mapistore_folder_create_folder(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent_folder), parent_folder->backend_object, new_folder, fid, rowp, &new_folder->backend_object);
		if (retval == MAPISTORE_ERR_EXIST) {
			/* folder with this name already exists */
			DEBUG(4, ("emsmdbp_object: CreateFolder Duplicate Folder error\n"));
			talloc_free(new_folder);
			return MAPI_E_COLLISION;
		}
		OPENCHANGE_RETVAL_IF(retval, MAPI_E_CALL_FAILED, new_folder);
	}
	else {
		parentFolderID = parent_folder->object.folder->folderID;
		value = get_SPropValue_SRow(rowp, PR_DISPLAY_NAME_UNICODE);
		if (!value) {
			value = get_SPropValue_SRow(rowp, PR_DISPLAY_NAME);
		}
		if (!value) {
			talloc_free(new_folder);
			return MAPI_E_INVALID_PARAMETER;
		}
		if (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, parentFolderID,
						 value->value.lpszW, &testFolderID) == MAPI_E_SUCCESS) {
			/* this folder already exists */
			DEBUG(4, ("emsmdbp_object: CreateFolder Duplicate Folder error\n"));
			talloc_free(new_folder);
			return MAPI_E_COLLISION;
		}

		/* This part should be moved into openchangedb.c */
		local_mem_ctx = talloc_zero(NULL, void);
		retval = openchangedb_get_distinguishedName(local_mem_ctx, emsmdbp_ctx->oc_ctx, parentFolderID, &parentDN);
		retval = openchangedb_get_mailboxDN(local_mem_ctx, emsmdbp_ctx->oc_ctx, parentFolderID, &mailboxDN);
		dn = talloc_asprintf(local_mem_ctx, "CN=%"PRIu64",%s", fid, parentDN);
		basedn = ldb_dn_new(local_mem_ctx, emsmdbp_ctx->oc_ctx, dn);
		talloc_free(dn);
		if (!ldb_dn_validate(basedn)) {
			talloc_free(local_mem_ctx);
			talloc_free(new_folder);
			return MAPI_E_BAD_VALUE;
		}
	
		msg = ldb_msg_new(local_mem_ctx);
		msg->dn = ldb_dn_copy(local_mem_ctx, basedn);
		ldb_msg_add_string(msg, "objectClass", "systemfolder");
		ldb_msg_add_fmt(msg, "cn", "%"PRIu64, fid);
		ldb_msg_add_string(msg, "PidTagContentUnreadCount", "0");
		ldb_msg_add_string(msg, "PidTagContentCount", "0");
		ldb_msg_add_string(msg, "PidTagContainerClass", "IPF.Note");
		ldb_msg_add_string(msg, "PidTagAttributeHidden", "0");
		ldb_msg_add_string(msg, "PidTagAttributeSystem", "0");
		ldb_msg_add_string(msg, "PidTagAttributeReadOnly", "0");
		ldb_msg_add_string(msg, "PidTagAccess", "63");
		ldb_msg_add_string(msg, "PidTagRights", "2043");
		ldb_msg_add_fmt(msg, "PidTagFolderType", "1");
		ldb_msg_add_fmt(msg, "PidTagParentFolderId", "%"PRIu64, parentFolderID);
		ldb_msg_add_fmt(msg, "PidTagFolderId", "%"PRIu64, fid);
		ldb_msg_add_fmt(msg, "mailboxDN", mailboxDN);
		path = talloc_asprintf(local_mem_ctx, "sogo://%s:%s@fallback/0x%.16"PRIx64, emsmdbp_ctx->username, emsmdbp_ctx->username, fid);
		ldb_msg_add_fmt(msg, "MAPIStoreURI", path);
		ldb_msg_add_string(msg, "PidTagSubFolders", "FALSE");

		value = get_SPropValue_SRow(rowp, PR_LAST_MODIFICATION_TIME);
		if (value) {
			nt_time = ((NTTIME) value->value.ft.dwHighDateTime << 32
				   | value->value.ft.dwLowDateTime);
		}
		else {
			unix_to_nt_time(&nt_time, time(NULL));
		}
		value = get_SPropValue_SRow(rowp, PR_CHANGE_NUM);
		if (value) {
			ldb_msg_add_fmt(msg, "PidTagChangeNumber", "%"PRIu64, value->value.d);
		}
		else {
			DEBUG(0, (__location__": PR_CHANGE_NUM *must* be present\n"));
			abort();
		}
		ldb_msg_add_fmt(msg, "PidTagCreationTime", "%"PRIu64, nt_time);
		ldb_msg_add_fmt(msg, "PidTagNTSDModificationTime", "%"PRIu64, nt_time);
		ldb_msg_add_string(msg, "FolderType", "1");
		ldb_msg_add_fmt(msg, "distinguishedName", "%s", ldb_dn_get_linearized(msg->dn));

		msg->elements[0].flags = LDB_FLAG_MOD_ADD;
		ldb_add(emsmdbp_ctx->oc_ctx, msg);

		openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, fid, rowp);

		/* Created top folders are always using a mapistore backend */
		retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, path, new_folder->object.folder->folderID, &contextID, &new_folder->backend_object);
		if (retval != MAPISTORE_SUCCESS) {
			abort();
		}
		mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, contextID, fid);
		new_folder->object.folder->mapistore_root = true;
		new_folder->object.folder->contextID = contextID;

		talloc_free(local_mem_ctx);
	}
	*new_folderp = new_folder;

	return MAPI_E_SUCCESS;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_open_folder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent, uint64_t fid)
{
	struct emsmdbp_object			*folder_object;
	int					retval;
	char					*path;
	uint32_t				contextID;
	void					*local_ctx;

	folder_object = emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, fid, parent);
	if (emsmdbp_is_mapistore(parent)) {
		DEBUG(0, ("%s: opening child mapistore folder\n", __FUNCTION__));
		retval = mapistore_folder_open_folder(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent), parent->backend_object, folder_object, fid, &folder_object->backend_object);
		if (retval != MAPISTORE_SUCCESS) {
			talloc_free(folder_object);
			folder_object = NULL;
		}
	}
	else {
		local_ctx = talloc_zero(NULL, void);
	
		retval = openchangedb_get_mapistoreURI(local_ctx, emsmdbp_ctx->oc_ctx, fid, &path, true);
		if (retval == MAPI_E_SUCCESS && path) {
			folder_object->object.folder->mapistore_root = true;
			/* system/special folder */
			DEBUG(0, ("%s: opening base mapistore folder\n", __FUNCTION__));

			retval = mapistore_search_context_by_uri(emsmdbp_ctx->mstore_ctx, path, &contextID, &folder_object->backend_object);
			if (retval == MAPISTORE_SUCCESS) {
				retval = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, contextID);
			} else {
				retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, path, folder_object->object.folder->folderID, &contextID, &folder_object->backend_object);
				if (retval != MAPISTORE_SUCCESS) {
					abort();
				}
				mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, contextID, fid);
			}
			folder_object->object.folder->contextID = contextID;
			/* (void) talloc_reference(folder_object, folder_object->backend_object); */
		}
		else {
			DEBUG(0, ("%s: opening openchangedb folder\n", __FUNCTION__));
		}
		talloc_free(local_ctx);
	}

	return folder_object;
}

_PUBLIC_ int emsmdbp_get_uri_from_fid(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, uint64_t fid, char **urip)
{
	enum MAPISTATUS	retval;
	bool		soft_deleted;

	retval = openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx, fid, urip, true); /* FIXME: always mailboxstore */
	if (retval == MAPI_E_SUCCESS) {
		return MAPISTORE_SUCCESS;
	}
	return mapistore_indexing_record_get_uri(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, mem_ctx, fid, urip, &soft_deleted);
}

_PUBLIC_ int emsmdbp_get_fid_from_uri(struct emsmdbp_context *emsmdbp_ctx, const char *uri, uint64_t *fidp)
{
	int	ret;
	bool	soft_deleted;

	ret = openchangedb_get_fid(emsmdbp_ctx->oc_ctx, uri, fidp);
	if (ret != MAPI_E_SUCCESS) {
		ret = mapistore_indexing_record_get_fmid(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, uri, false, fidp, &soft_deleted);
	}

	return ret;
}

static char *emsmdbp_compute_parent_uri(TALLOC_CTX *mem_ctx, char *uri)
{
	char *parent_uri, *slash, *lastchar;
	int len;

	if (!uri) return NULL;

	parent_uri = talloc_strdup(mem_ctx, uri);
	len = strlen(parent_uri);
	lastchar = parent_uri + len - 1;
	if (*lastchar == '/') {
		*lastchar = 0;
	}
	slash = strrchr(parent_uri, '/');
	if (slash) {
		*(slash + 1) = 0;
	}
	else {
		talloc_free(parent_uri);
		parent_uri = NULL;
	}

	return parent_uri;
}

static int emsmdbp_get_parent_fid(struct emsmdbp_context *emsmdbp_ctx, uint64_t fid, uint64_t *parent_fidp)
{
	TALLOC_CTX	*mem_ctx;
	int		retval = MAPISTORE_SUCCESS;
	bool		soft_deleted;
	char		*uri, *parent_uri;

	mem_ctx = talloc_zero(NULL, void);
	retval = openchangedb_get_parent_fid(emsmdbp_ctx->oc_ctx, fid, parent_fidp, true);
	if (retval == MAPISTORE_SUCCESS) {
		goto end;
	}
	retval = openchangedb_get_parent_fid(emsmdbp_ctx->oc_ctx, fid, parent_fidp, false);
	if (retval == MAPISTORE_SUCCESS) {
		goto end;
	}

	retval = mapistore_indexing_record_get_uri(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, mem_ctx, fid, &uri, &soft_deleted);
	if (retval == MAPISTORE_SUCCESS) {
		parent_uri = emsmdbp_compute_parent_uri(mem_ctx, uri);
		if (parent_uri) {
			retval = emsmdbp_get_fid_from_uri(emsmdbp_ctx, parent_uri, parent_fidp);
		}
		else {
			retval = MAPISTORE_ERR_NOT_FOUND;
		}
	}

end:
	talloc_free(mem_ctx);

	return retval;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_open_folder_by_fid(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *context_object, uint64_t fid)
{
	uint64_t		parent_fid;
	int			retval;
	struct emsmdbp_object	*parent_object;
	
	if ((context_object->type == EMSMDBP_OBJECT_MAILBOX
	     && fid == context_object->object.mailbox->folderID)
	    || (context_object->type == EMSMDBP_OBJECT_FOLDER
		&& fid == context_object->object.folder->folderID)) {
		return context_object;
	}

	retval = emsmdbp_get_parent_fid(emsmdbp_ctx, fid, &parent_fid);
	if (retval == MAPISTORE_SUCCESS) {
		if (parent_fid) {
			parent_object = emsmdbp_object_open_folder_by_fid(mem_ctx, emsmdbp_ctx, context_object, parent_fid);
			if (parent_object) {
				return emsmdbp_object_open_folder(mem_ctx, emsmdbp_ctx, parent_object, fid);
			}
		}
		else {
			return emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, fid, NULL);
		}
	}

	return NULL;
}

_PUBLIC_ int emsmdbp_object_stream_commit(struct emsmdbp_object *stream_object)
{
	int				rc;
	struct emsmdbp_object_stream	*stream;
        void				*stream_data;
        uint8_t				*utf8_buffer;
        struct Binary_r			*binary_data;
        struct SRow			aRow;
	size_t				converted_size;
	uint16_t			propType;

	if (!stream_object || stream_object->type != EMSMDBP_OBJECT_STREAM) return MAPISTORE_ERROR;

	stream = stream_object->object.stream;

	rc = MAPISTORE_SUCCESS;
	if (stream->needs_commit) {
		stream->needs_commit = false;
		aRow.cValues = 1;
		aRow.lpProps = talloc_zero(NULL, struct SPropValue);

		propType = stream->property & 0xffff;
		if (propType == PT_BINARY) {
			binary_data = talloc(aRow.lpProps, struct Binary_r);
			binary_data->cb = stream->stream.buffer.length;
			binary_data->lpb = stream->stream.buffer.data;
			stream_data = binary_data;
		}
		else if (propType == PT_STRING8) {
			stream_data = stream->stream.buffer.data;
		}
		else {
			/* PT_UNICODE */
			utf8_buffer = talloc_array(aRow.lpProps, uint8_t, stream->stream.buffer.length + 2);
			convert_string(CH_UTF16LE, CH_UTF8,
				       stream->stream.buffer.data, stream->stream.buffer.length,
				       utf8_buffer, stream->stream.buffer.length, &converted_size);
			utf8_buffer[converted_size] = 0;
			stream_data = utf8_buffer;
		}
		set_SPropValue_proptag(aRow.lpProps, stream->property, stream_data);

		emsmdbp_object_set_properties(stream_object->emsmdbp_ctx, stream_object->parent_object, &aRow);
		talloc_free(aRow.lpProps);
	}

	return rc;
}

/**
   \details talloc destructor for emsmdbp_objects

   \param data generic pointer on data

   \return 0 on success, otherwise -1
 */
static int emsmdbp_object_destructor(void *data)
{
	struct emsmdbp_object	*object = (struct emsmdbp_object *) data;
	int			ret = MAPISTORE_SUCCESS;
	uint32_t		contextID;

	if (!data) return -1;
	if (!emsmdbp_is_mapistore(object)) goto nomapistore;

	DEBUG(4, ("[%s:%d]: emsmdbp %s object released\n", __FUNCTION__, __LINE__,
		  emsmdbp_getstr_type(object)));

	contextID = emsmdbp_get_contextID(object);
	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		if (object->object.folder->mapistore_root) {
			ret = mapistore_del_context(object->emsmdbp_ctx->mstore_ctx, contextID);
		}
		DEBUG(4, ("[%s:%d] mapistore folder context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
	case EMSMDBP_OBJECT_TABLE:
		if (emsmdbp_is_mapistore(object) && object->object.table->handle > 0) {
			mapistore_table_handle_destructor(object->emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, object->object.table->handle);
		}
                if (object->object.table->subscription_list) {
                        DLIST_REMOVE(object->emsmdbp_ctx->mstore_ctx->subscriptions, object->object.table->subscription_list);
			talloc_free(object->object.table->subscription_list);
			/* talloc_unlink(object->emsmdbp_ctx, object->object.table->subscription_list); */
                }
		break;
	case EMSMDBP_OBJECT_STREAM:
		emsmdbp_object_stream_commit(object);
		break;
        case EMSMDBP_OBJECT_SUBSCRIPTION:
                if (object->object.subscription->subscription_list) {
                        DLIST_REMOVE(object->emsmdbp_ctx->mstore_ctx->subscriptions, object->object.subscription->subscription_list);
			talloc_free(object->object.subscription->subscription_list);
                }
		break;
	}
	
nomapistore:
	talloc_unlink(object, object->parent_object);

	return 0;
}

/**
   \details Initialize an emsmdbp_object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_object)
{
	struct emsmdbp_object	*object = NULL;

	object = talloc_zero(mem_ctx, struct emsmdbp_object);
	if (!object) return NULL;

	talloc_set_destructor((void *)object, (int (*)(void *))emsmdbp_object_destructor);

	object->type = EMSMDBP_OBJECT_UNDEF;
	object->emsmdbp_ctx = emsmdbp_ctx;
	object->object.mailbox = NULL;
	object->object.folder = NULL;
	object->object.message = NULL;
	object->object.stream = NULL;
	object->backend_object = NULL;
	object->parent_object = parent_object;
	(void) talloc_reference(object, parent_object);

	object->stream_data = NULL;

	return object;
}

static int emsmdbp_copy_properties(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *dest_object, struct SPropTagArray *excluded_tags)
{
	TALLOC_CTX		*mem_ctx;
	bool			*properties_exclusion;
	struct SPropTagArray	*properties, *needed_properties;
        void                    **data_pointers;
        enum MAPISTATUS         *retvals = NULL;
	struct SRow		*aRow;
	struct SPropValue	newValue;
	uint32_t		i;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	if (emsmdbp_object_get_available_properties(mem_ctx, emsmdbp_ctx, source_object, &properties) == MAPISTORE_ERROR) {
		DEBUG(0, ("["__location__"] - mapistore support not implemented yet - shouldn't occur\n"));
		talloc_free(mem_ctx);
		return MAPI_E_NO_SUPPORT;
	}

	/* 1. Exclusions */
	properties_exclusion = talloc_array(mem_ctx, bool, 65536);
	memset(properties_exclusion, 0, 65536 * sizeof(bool));

	/* 1a. Explicit exclusions */
	properties_exclusion[PR_ROW_TYPE >> 16] = true;
	properties_exclusion[PR_INSTANCE_KEY >> 16] = true;
	properties_exclusion[PR_INSTANCE_NUM >> 16] = true;
	properties_exclusion[PR_INST_ID >> 16] = true;
	properties_exclusion[PR_FID >> 16] = true;
	properties_exclusion[PR_MID >> 16] = true;
	properties_exclusion[PR_SOURCE_KEY >> 16] = true;
	properties_exclusion[PR_PARENT_SOURCE_KEY >> 16] = true;
	properties_exclusion[PR_PARENT_FID >> 16] = true;

	/* 1b. Request exclusions */
	if (excluded_tags != NULL) {
		for (i = 0; i < excluded_tags->cValues; i++) {
			properties_exclusion[excluded_tags->aulPropTag[i] >> 16] = true;
		}
	}

	needed_properties = talloc_zero(mem_ctx, struct SPropTagArray);
	needed_properties->aulPropTag = talloc_zero(needed_properties, void);
	for (i = 0; i < properties->cValues; i++) {
		if (!properties_exclusion[properties->aulPropTag[i] >> 16]) {
			SPropTagArray_add(mem_ctx, needed_properties, properties->aulPropTag[i]);
		}
	}

	data_pointers = emsmdbp_object_get_properties(mem_ctx, emsmdbp_ctx, source_object, needed_properties, &retvals);
	if (data_pointers) {
		aRow = talloc_zero(mem_ctx, struct SRow);
		for (i = 0; i < needed_properties->cValues; i++) {
			if (retvals[i] == MAPI_E_SUCCESS) {
				/* _PUBLIC_ enum MAPISTATUS SRow_addprop(struct SRow *aRow, struct SPropValue spropvalue) */
				set_SPropValue_proptag(&newValue, needed_properties->aulPropTag[i], data_pointers[i]);
				SRow_addprop(aRow, newValue);
			}
		}
		if (emsmdbp_object_set_properties(emsmdbp_ctx, dest_object, aRow) != MAPISTORE_SUCCESS) {
			talloc_free(mem_ctx);
			return MAPI_E_NO_SUPPORT;
		}
	}
	else {
		talloc_free(mem_ctx);
		return MAPI_E_NO_SUPPORT;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/* FIXME: this function is already present in oxcmsg... */
struct emsmdbp_prop_index {
	uint32_t display_name; /* PR_DISPLAY_NAME_UNICODE or PR_7BIT_DISPLAY_NAME_UNICODE or PR_RECIPIENT_DISPLAY_NAME_UNICODE */
	uint32_t email_address; /* PR_EMAIL_ADDRESS_UNICODE or PR_SMTP_ADDRESS_UNICODE */
};

static inline void emsmdbp_fill_prop_index(struct emsmdbp_prop_index *prop_index, struct SPropTagArray *properties)
{
	if (SPropTagArray_find(*properties, PR_DISPLAY_NAME_UNICODE, &prop_index->display_name) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_7BIT_DISPLAY_NAME_UNICODE, &prop_index->display_name) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_RECIPIENT_DISPLAY_NAME_UNICODE, &prop_index->display_name) == MAPI_E_NOT_FOUND) {
		prop_index->display_name = (uint32_t) -1;
	}
	if (SPropTagArray_find(*properties, PR_EMAIL_ADDRESS_UNICODE, &prop_index->email_address) == MAPI_E_NOT_FOUND
	    && SPropTagArray_find(*properties, PR_SMTP_ADDRESS_UNICODE, &prop_index->email_address) == MAPI_E_NOT_FOUND) {
		prop_index->email_address = (uint32_t) -1;
	}
}

static inline int emsmdbp_copy_message_recipients_mapistore(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *dest_object)
{
	TALLOC_CTX			*mem_ctx;
	struct mapistore_message	*msg_data;
	uint32_t			contextID, i;
	struct emsmdbp_prop_index	prop_index;
	struct SPropTagArray		*new_columns;
	void				**new_data;

	if (!emsmdbp_is_mapistore(source_object) || !emsmdbp_is_mapistore(dest_object)) {
		/* we silently fail for non-mapistore messages */
		return MAPI_E_SUCCESS;
	}

	/* Fetch data from source message */
	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	contextID = emsmdbp_get_contextID(source_object);
	mapistore_message_get_message_data(emsmdbp_ctx->mstore_ctx, contextID, source_object->backend_object, mem_ctx, &msg_data);

	/* By convention, we pass PR_DISPLAY_NAME_UNICODE and PR_EMAIL_ADDRESS_UNICODE to the backend, so we prepend them to each values array */
	if (msg_data->recipients_count > 0
	    && (msg_data->columns->cValues < 2 || msg_data->columns->aulPropTag[0] != PR_DISPLAY_NAME_UNICODE || msg_data->columns->aulPropTag[1] != PR_EMAIL_ADDRESS_UNICODE)) {
		emsmdbp_fill_prop_index(&prop_index, msg_data->columns);

		new_columns = talloc_zero(mem_ctx, struct SPropTagArray);
		new_columns->cValues = msg_data->columns->cValues + 2;
		new_columns->aulPropTag = talloc_array(new_columns, enum MAPITAGS, new_columns->cValues);
		memcpy(new_columns->aulPropTag + 2, msg_data->columns->aulPropTag, sizeof(enum MAPITAGS) * msg_data->columns->cValues);
		new_columns->aulPropTag[0] = PR_DISPLAY_NAME_UNICODE;
		new_columns->aulPropTag[1] = PR_EMAIL_ADDRESS_UNICODE;

		for (i = 0; i < msg_data->recipients_count; i++) {
			new_data = talloc_array(mem_ctx, void *, new_columns->cValues);
			memcpy(new_data + 2, msg_data->recipients[i].data, sizeof(void *) * msg_data->columns->cValues);
			if (prop_index.display_name != (uint32_t) -1) {
				new_data[0] = msg_data->recipients[i].data[prop_index.display_name];
			}
			else {
				new_data[0] = NULL;
			}
			if (prop_index.email_address != (uint32_t) -1) {
				new_data[1] = msg_data->recipients[i].data[prop_index.email_address];
			}
			else {
				new_data[1] = NULL;
			}
			msg_data->recipients[i].data = new_data;
		}
		msg_data->columns = new_columns;

		/* Copy data into dest message */
		mapistore_message_modify_recipients(emsmdbp_ctx->mstore_ctx, contextID, dest_object->backend_object, msg_data->columns, msg_data->recipients_count, msg_data->recipients);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static inline int emsmdbp_copy_message_attachments_mapistore(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *dest_object)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		i, count, contextID, dest_num;
	void			**data_pointers;
	uint32_t		*retvals, *attach_nums;
	struct emsmdbp_object	*table_object, *source_attach, *dest_attach;
	enum MAPITAGS		column;
	int			ret;

	if (!emsmdbp_is_mapistore(source_object) || !emsmdbp_is_mapistore(dest_object)) {
		/* we silently fail for non-mapistore messages */
		return MAPI_E_SUCCESS;
	}

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);

	/* we fetch the attachment nums */
	table_object = emsmdbp_object_message_open_attachment_table(mem_ctx, emsmdbp_ctx, source_object);
	if (!table_object) {
		talloc_free(mem_ctx);
		return MAPI_E_NOT_FOUND;
	}

	column = PR_ATTACH_NUM;
	table_object->object.table->prop_count = 1;
	table_object->object.table->properties = &column;

	contextID = emsmdbp_get_contextID(table_object);
	mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, 1, &column);

	count = table_object->object.table->denominator;
	attach_nums = talloc_array(mem_ctx, uint32_t, count);
	for (i = 0; i < table_object->object.table->denominator; i++) {
		data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, table_object, i, &retvals);
		if (!data_pointers) {
			talloc_free(mem_ctx);
			return MAPI_E_CALL_FAILED;
		}
		if (retvals[0] != MAPISTORE_SUCCESS) {
			talloc_free(mem_ctx);
			DEBUG(5, ("cannot copy attachments without PR_ATTACH_NUM\n"));
			return MAPI_E_CALL_FAILED;
		}
		attach_nums[i] = *(uint32_t *) data_pointers[0];
	}

	/* we open each attachment manually and copy their props to created dest attachments */
	for (i = 0; i < count; i++) {
		source_attach = emsmdbp_object_attachment_init(mem_ctx, emsmdbp_ctx, source_object->object.message->messageID, source_object);
		if (!source_attach
		    || mapistore_message_open_attachment(emsmdbp_ctx->mstore_ctx, contextID, source_object->backend_object, source_attach, attach_nums[i], &source_attach->backend_object)) {
			talloc_free(mem_ctx);
			return MAPI_E_CALL_FAILED;
		}

		dest_attach = emsmdbp_object_attachment_init(mem_ctx, emsmdbp_ctx, dest_object->object.message->messageID, dest_object);
		if (!dest_attach
		    || mapistore_message_create_attachment(emsmdbp_ctx->mstore_ctx, contextID, dest_object->backend_object, dest_attach, &dest_attach->backend_object, &dest_num)) {
			talloc_free(mem_ctx);
			return MAPI_E_CALL_FAILED;
		}

		ret = emsmdbp_copy_properties(emsmdbp_ctx, source_attach, dest_attach, NULL);
		if (ret != MAPI_E_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Copy properties from an object to another object

   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param source_object pointer to the source object
   \param target_object pointer to the target object
   \param excluded_properties pointer to a SPropTagArray listing properties that must not be copied
   \param deep_copy indicates whether subobjects must be copied

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ int emsmdbp_object_copy_properties(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *target_object, struct SPropTagArray *excluded_properties, bool deep_copy)
{
	int ret;

	if (!(source_object->type == EMSMDBP_OBJECT_FOLDER
	      || source_object->type == EMSMDBP_OBJECT_MAILBOX
	      || source_object->type == EMSMDBP_OBJECT_MESSAGE
	      || source_object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		DEBUG(0, (__location__": object must be EMSMDBP_OBJECT_FOLDER, EMSMDBP_OBJECT_MAILBOX, EMSMDBP_OBJECT_MESSAGE or EMSMDBP_OBJECT_ATTACHMENT (type =  %d)\n", source_object->type));
		ret = MAPI_E_NO_SUPPORT;
                goto end;
        }
	if (target_object->type != source_object->type) {
		DEBUG(0, ("source and destination objects type must match (type =  %d)\n", target_object->type));
		ret = MAPI_E_NO_SUPPORT;
                goto end;
        }

	/* copy properties (common to all object types) */
	ret = emsmdbp_copy_properties(emsmdbp_ctx, source_object, target_object, excluded_properties);
	if (ret != MAPI_E_SUCCESS) {
                goto end;
	}

	/* type specific ops */
	switch (source_object->type) {
	case EMSMDBP_OBJECT_MESSAGE:
		if (emsmdbp_is_mapistore(source_object) && emsmdbp_is_mapistore(target_object)) {
			ret = emsmdbp_copy_message_recipients_mapistore(emsmdbp_ctx, source_object, target_object);
			if (ret != MAPI_E_SUCCESS) {
				goto end;
			}
			if (deep_copy) {
				ret = emsmdbp_copy_message_attachments_mapistore(emsmdbp_ctx, source_object, target_object);
				if (ret != MAPI_E_SUCCESS) {
					goto end;
				}
			}
		}
		else {
			DEBUG(0, ("Cannot copy recipients or attachments to or from non-mapistore messages\n"));
		}
		break;
	default:
		if (deep_copy) {
			DEBUG(0, ("Cannot deep copy non-message objects\n"));
		}
	}

end:

	return ret;
}

/**
   \details Initialize a mailbox object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the Logon MAPI request
   \param mailboxstore boolean which specifies whether the mailbox
   object is a PF store or a private mailbox store

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_mailbox_init(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    struct EcDoRpc_MAPI_REQ *request,
							    bool mailboxstore)
{
	struct emsmdbp_object		*object;
	const char			*displayName, *cn;
	const char * const		recipient_attrs[] = { "*", NULL };
	int				ret;
	struct ldb_result		*res = NULL;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!request) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, NULL);
	if (!object) return NULL;

	/* Initialize the mailbox object */
	object->object.mailbox = talloc_zero(object, struct emsmdbp_object_mailbox);
	if (!object->object.mailbox) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_MAILBOX;
	object->object.mailbox->owner_Name = NULL;
	object->object.mailbox->owner_EssDN = NULL;
	object->object.mailbox->szUserDN = NULL;
	object->object.mailbox->folderID = 0x0;
	object->object.mailbox->mailboxstore = mailboxstore;

	if (mailboxstore == true) {
		object->object.mailbox->owner_EssDN = talloc_strdup(object->object.mailbox, 
								    request->u.mapi_Logon.EssDN);
		ret = ldb_search(emsmdbp_ctx->samdb_ctx, mem_ctx, &res,
				 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
				 LDB_SCOPE_SUBTREE, recipient_attrs, "legacyExchangeDN=%s", 
				 object->object.mailbox->owner_EssDN);
		if (!ret && res->count == 1) {
			cn = ldb_msg_find_attr_as_string(res->msgs[0], "cn", NULL);
			if (cn) {
				object->object.mailbox->owner_username = talloc_strdup(object->object.mailbox,  cn);

				/* Retrieve Mailbox folder identifier */
				openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username,
								0x1, &object->object.mailbox->folderID);
			}
			displayName = ldb_msg_find_attr_as_string(res->msgs[0], "displayName", NULL);
			if (displayName) {
				object->object.mailbox->owner_Name = talloc_strdup(object->object.mailbox, 
										   displayName);
			}
		}
	} else {
		/* Retrieve Public folder identifier */
		openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, 0x1, &object->object.mailbox->folderID);
	}

	object->object.mailbox->szUserDN = talloc_strdup(object->object.mailbox, emsmdbp_ctx->szUserDN);

	talloc_free(res);

	return object;
}


/**
   \details Initialize a folder object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param folderID the folder identifier
   \param parent emsmdbp object of the parent folder for this folder

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   uint64_t folderID,
							   struct emsmdbp_object *parent_object)
{
	struct emsmdbp_object			*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent_object);
	if (!object) return NULL;

	object->object.folder = talloc_zero(object, struct emsmdbp_object_folder);
	if (!object->object.folder) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FOLDER;
	object->object.folder->folderID = folderID;
	object->object.folder->mapistore_root = false;
	object->object.folder->contextID = (uint32_t) -1;
 
	return object;
}

int emsmdbp_folder_get_folder_count(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *folder, uint32_t *row_countp)
{
	int		retval;
	uint64_t	folderID;

	if (emsmdbp_is_mapistore(folder)) {
		retval = mapistore_folder_get_folder_count(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(folder),
							   folder->backend_object, row_countp);
	}
	else {
		if (folder->type == EMSMDBP_OBJECT_FOLDER) {
			folderID = folder->object.folder->folderID;
		}
		else if (folder->type == EMSMDBP_OBJECT_MAILBOX) {
			folderID = folder->object.folder->folderID;
		}
		else {
			DEBUG(5, ("unsupported object type\n"));
			return MAPISTORE_ERROR;
		}
		retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, folderID, row_countp);
	}

	return retval;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_folder_open_table(TALLOC_CTX *mem_ctx, struct emsmdbp_object *parent_object, uint32_t table_type, uint32_t handle_id)
{
	struct emsmdbp_object	*table_object;
	uint8_t			mstore_type;
	int			ret;

	if (!(parent_object->type == EMSMDBP_OBJECT_FOLDER || parent_object->type == EMSMDBP_OBJECT_MAILBOX)) {
		DEBUG(0, (__location__": parent_object must be EMSMDBP_OBJECT_FOLDER or EMSMDBP_OBJECT_MAILBOX (type =  %d)\n", parent_object->type));
		return NULL;
	}

	table_object = emsmdbp_object_table_init(mem_ctx, parent_object->emsmdbp_ctx, parent_object);
	if (table_object) {
		table_object->object.table->handle = handle_id;
		table_object->object.table->ulType = table_type;
		if (emsmdbp_is_mapistore(parent_object)) {
			switch (table_type) {
			case EMSMDBP_TABLE_MESSAGE_TYPE:
				mstore_type = MAPISTORE_MESSAGE_TABLE;
				break;
			case EMSMDBP_TABLE_FAI_TYPE:
				mstore_type = MAPISTORE_FAI_TABLE;
				break;
			case EMSMDBP_TABLE_FOLDER_TYPE:
				mstore_type = MAPISTORE_FOLDER_TABLE;
				break;
			case EMSMDBP_TABLE_PERMISSIONS_TYPE:
				mstore_type = MAPISTORE_PERMISSIONS_TABLE;
				break;
			default:
				DEBUG(5, ("Unhandled table type for folders: %d\n", table_type));
				abort();
			}

			ret = mapistore_folder_open_table(parent_object->emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent_object), parent_object->backend_object, table_object, mstore_type, handle_id, &table_object->backend_object, &table_object->object.table->denominator);
			if (ret != MAPISTORE_SUCCESS) {
				talloc_free(table_object);
				table_object = NULL;
			}
		}
		else {
			if (table_type == EMSMDBP_TABLE_FOLDER_TYPE) {
				emsmdbp_folder_get_folder_count(parent_object->emsmdbp_ctx, parent_object, &table_object->object.table->denominator);
			}
			else {
				/* Non-mapistore message tables are always empty */
				table_object->object.table->denominator = 0;
			}
		}
	}

	return table_object;	
}

/**
   \details Initialize a table object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param parent emsmdbp object of the parent

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER && parent->type != EMSMDBP_OBJECT_MAILBOX && parent->type != EMSMDBP_OBJECT_MESSAGE) return NULL;

	/* Initialize table object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;
	
	object->object.table = talloc_zero(object, struct emsmdbp_object_table);
	if (!object->object.table) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_TABLE;
	object->object.table->prop_count = 0;
	object->object.table->properties = NULL;
	object->object.table->numerator = 0;
	object->object.table->denominator = 0;
	object->object.table->ulType = 0;
	object->object.table->restricted = false;
	object->object.table->subscription_list = NULL;

	return object;
}

_PUBLIC_ int emsmdbp_object_table_get_available_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, struct SPropTagArray **propertiesp)
{
	int				retval;
	struct SPropTagArray		*properties;
	uint32_t			contextID;

	if (!table_object->type == EMSMDBP_OBJECT_TABLE) {
		return MAPISTORE_ERROR;
	}

	if (emsmdbp_is_mapistore(table_object)) {
		contextID = emsmdbp_get_contextID(table_object);
		retval = mapistore_table_get_available_properties(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, mem_ctx, propertiesp);
	}
	else {
		properties = talloc_zero(mem_ctx, struct SPropTagArray);
		properties->aulPropTag = talloc_zero(properties, enum MAPITAGS);
		/* TODO: this list might not be complete */
		SPropTagArray_add(properties, properties, PR_FID);
		SPropTagArray_add(properties, properties, PR_PARENT_FID);
		SPropTagArray_add(properties, properties, PR_DISPLAY_NAME_UNICODE);
		SPropTagArray_add(properties, properties, PR_COMMENT_UNICODE);
		SPropTagArray_add(properties, properties, PR_ACCESS);
		SPropTagArray_add(properties, properties, PR_CREATION_TIME);
		SPropTagArray_add(properties, properties, PR_NTSD_MODIFICATION_TIME);
		SPropTagArray_add(properties, properties, PR_LAST_MODIFICATION_TIME);
		SPropTagArray_add(properties, properties, PR_ADDITIONAL_REN_ENTRYIDS);
		SPropTagArray_add(properties, properties, PR_ADDITIONAL_REN_ENTRYIDS_EX);
		SPropTagArray_add(properties, properties, PR_CREATOR_SID);
		SPropTagArray_add(properties, properties, PR_LAST_MODIFIER_SID);
		SPropTagArray_add(properties, properties, PR_ATTR_HIDDEN);
		SPropTagArray_add(properties, properties, PR_ATTR_SYSTEM);
		SPropTagArray_add(properties, properties, PR_ATTR_READONLY);
		SPropTagArray_add(properties, properties, PR_EXTENDED_ACL_DATA);
		SPropTagArray_add(properties, properties, PR_CONTAINER_CLASS_UNICODE);
		SPropTagArray_add(properties, properties, PR_MESSAGE_CLASS_UNICODE);
		SPropTagArray_add(properties, properties, PR_RIGHTS);
		SPropTagArray_add(properties, properties, PR_CONTENT_COUNT);
		SPropTagArray_add(properties, properties, PR_ASSOC_CONTENT_COUNT);
		SPropTagArray_add(properties, properties, PR_SUBFOLDERS);
		SPropTagArray_add(properties, properties, PR_MAPPING_SIGNATURE);
		SPropTagArray_add(properties, properties, PR_USER_ENTRYID);
		SPropTagArray_add(properties, properties, PR_MAILBOX_OWNER_ENTRYID);
		SPropTagArray_add(properties, properties, PR_MAILBOX_OWNER_NAME_UNICODE);
		SPropTagArray_add(properties, properties, PR_IPM_APPOINTMENT_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_CONTACT_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_JOURNAL_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_NOTE_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_TASK_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_DRAFTS_ENTRYID);
		SPropTagArray_add(properties, properties, PR_REMINDERS_ONLINE_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_PUBLIC_FOLDERS_ENTRYID);
		SPropTagArray_add(properties, properties, PR_FOLDER_XVIEWINFO_E);
		SPropTagArray_add(properties, properties, PR_FOLDER_VIEWLIST);
		SPropTagArray_add(properties, properties, PR_FREEBUSY_ENTRYIDS);
		*propertiesp = properties;

		retval = MAPISTORE_SUCCESS;
	}

	return retval;
}

_PUBLIC_ void **emsmdbp_object_table_get_row_props(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, uint32_t row_id, uint32_t **retvalsp)
{
        void				**data_pointers;
        enum MAPISTATUS			retval;
        uint32_t			*retvals;
	struct emsmdbp_object		*subfolder;
        struct emsmdbp_object_table	*table;
        struct mapistore_property_data	*properties;
        uint32_t			contextID, i, num_props, *obj_count;
	uint64_t			*rowFolderID;
	uint64_t			folderID;
	uint8_t				*has_subobj;
	char				*table_filter;
	void				*odb_ctx;
	struct Binary_r			*binr;

        table = table_object->object.table;
        num_props = table_object->object.table->prop_count;

        data_pointers = talloc_array(mem_ctx, void *, num_props);
        memset(data_pointers, 0, sizeof(void *) * num_props);
        retvals = talloc_array(mem_ctx, uint32_t, num_props);
        memset(retvals, 0, sizeof(uint32_t) * num_props);

	contextID = emsmdbp_get_contextID(table_object);
	if (emsmdbp_is_mapistore(table_object)) {
		retval = mapistore_table_get_row(emsmdbp_ctx->mstore_ctx, contextID,
						 table_object->backend_object, data_pointers,
						 MAPISTORE_PREFILTERED_QUERY, row_id, &properties);
		if (retval == MAPI_E_SUCCESS) {
			for (i = 0; i < num_props; i++) {
				data_pointers[i] = properties[i].data;
                                        
				if (properties[i].error) {
					if (properties[i].error == MAPISTORE_ERR_NOT_FOUND)
						retvals[i] = MAPI_E_NOT_FOUND;
					else if (properties[i].error == MAPISTORE_ERR_NO_MEMORY)
						retvals[i] = MAPI_E_NOT_ENOUGH_MEMORY;
					else {
						retvals[i] = MAPI_E_NO_SUPPORT;
						DEBUG (4, ("%s: unknown mapistore error: %.8x\n", __PRETTY_FUNCTION__, properties[i].error));
					}
				}
				else {
					if (properties[i].data == NULL) {
						retvals[i] = MAPI_E_NOT_FOUND;
					}
				}
			}
		}
		else {
			DEBUG(5, ("%s: invalid object (likely due to a restriction)\n", __location__));
			talloc_free(retvals);
			talloc_free(data_pointers);
			return NULL;
		}
	}
	else {
		if (table_object->parent_object->type == EMSMDBP_OBJECT_FOLDER) {
			folderID = table_object->parent_object->object.folder->folderID;
		}
		else if (table_object->parent_object->type == EMSMDBP_OBJECT_MAILBOX) {
			folderID = table_object->parent_object->object.mailbox->folderID;
		}
		else {
			DEBUG(5, ("%s: non-poc tables can only be client of folder objects\n", __location__));
			talloc_free(retvals);
			talloc_free(data_pointers);
			return NULL;
		}

		odb_ctx = talloc_zero(NULL, void);

		table_filter = talloc_asprintf(odb_ctx, "(&(PidTagParentFolderId=%"PRIu64")(PidTagFolderId=*))", folderID);
		retval = openchangedb_get_table_property(odb_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
							 table_filter, PR_FID, row_id, (void **) &rowFolderID);
		if (retval == MAPI_E_INVALID_OBJECT) {
			talloc_free(retvals);
			talloc_free(data_pointers);
			return NULL;
		}

		subfolder = emsmdbp_object_open_folder(NULL, table_object->parent_object->emsmdbp_ctx, table_object->parent_object, *(uint64_t *)rowFolderID);

		/* Lookup for flagged property row */
                retval = MAPI_E_SUCCESS;
		for (i = 0; retval != MAPI_E_INVALID_OBJECT && i < num_props; i++) {
			if (table->properties[i] == PR_CONTENT_COUNT) {
				/* a hack to avoid fetching dynamic fields from openchange.ldb */
				obj_count = talloc_zero(data_pointers, uint32_t);
				retval = mapistore_folder_get_message_count(emsmdbp_ctx->mstore_ctx, contextID, subfolder,
									    MAPISTORE_MESSAGE_TABLE, obj_count);
				data_pointers[i] = obj_count;
			}
			else if (table->properties[i] == PR_ASSOC_CONTENT_COUNT) {
				obj_count = talloc_zero(data_pointers, uint32_t);
				retval = mapistore_folder_get_message_count(emsmdbp_ctx->mstore_ctx, contextID, subfolder,
									    MAPISTORE_FAI_TABLE, obj_count);
				data_pointers[i] = obj_count;
			}
			else if (table->properties[i] == PR_FOLDER_CHILD_COUNT) {
				obj_count = talloc_zero(data_pointers, uint32_t);
				retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, subfolder, obj_count);
				data_pointers[i] = obj_count;
			}
			else if (table->properties[i] == PR_SOURCE_KEY) {
				emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, subfolder->object.folder->folderID, &binr);
				data_pointers[i] = binr;
			}
			else if (table->properties[i] == PR_SUBFOLDERS) {
				obj_count = talloc_zero(NULL, uint32_t);
				retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, subfolder, obj_count);
				has_subobj = talloc_zero(data_pointers, uint8_t);
				*has_subobj = (*obj_count > 0) ? 1 : 0;
				data_pointers[i] = has_subobj;
				talloc_free(obj_count);
			}
			else if (table->properties[i] == PR_CONTENT_UNREAD || table->properties[i] == PR_DELETED_COUNT_TOTAL) {
				/* TODO: temporary */
				obj_count = talloc_zero(data_pointers, uint32_t);
				data_pointers[i] = obj_count;
				retval = MAPI_E_SUCCESS;
			}
			else {
				retval = openchangedb_get_table_property(data_pointers, emsmdbp_ctx->oc_ctx, 
									 emsmdbp_ctx->username,
									 table_filter, table->properties[i], 
									 row_id, data_pointers + i);
			}
			/* DEBUG(5, ("  %.8x: %d", table->properties[j], retval)); */
			if (retval == MAPI_E_INVALID_OBJECT) {
				DEBUG(5, ("%s: invalid object in non-mapistore folder, count set to 0\n", __location__));
				talloc_free(retvals);
				talloc_free(data_pointers);
				talloc_free(subfolder);
				return NULL;
			}
			else {
				retvals[i] = retval;
			}
		}

		talloc_free(subfolder);
		talloc_free(odb_ctx);
	}

        if (retvalsp) {
                *retvalsp = retvals;
	}

        return data_pointers;
}

_PUBLIC_ void emsmdbp_fill_table_row_blob(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
					  DATA_BLOB *table_row, uint16_t num_props,
					  enum MAPITAGS *properties,
					  void **data_pointers, uint32_t *retvals)
{
        uint16_t i;
        uint8_t flagged;
        enum MAPITAGS property;
        void *data;
        uint32_t retval;

        flagged = 0;

        for (i = 0; !flagged && i < num_props; i++) {
                if (retvals[i] != MAPI_E_SUCCESS) {
                        flagged = 1;
                }
        }

        if (flagged) {
                libmapiserver_push_property(mem_ctx,
                                            0x0000000b, (const void *)&flagged,
                                            table_row, 0, 0, 0);
        }
        else {
                libmapiserver_push_property(mem_ctx,
                                            0x00000000, (const void *)&flagged,
                                            table_row, 0, 1, 0);
        }

        for (i = 0; i < num_props; i++) {
                property = properties[i];
                retval = retvals[i];
                if (retval != MAPI_E_SUCCESS) {
                        property = (property & 0xFFFF0000) + PT_ERROR;
                        data = &retval;
                }
                else {
                        data = data_pointers[i];
                }

                libmapiserver_push_property(mem_ctx,
                                            property, data, table_row,
                                            flagged?PT_ERROR:0, flagged, 0);
        }
}

/**
   \details Initialize a message object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param messageID the message identifier
   \param parent emsmdbp object of the parent

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_init(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    uint64_t messageID,
							    struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER) {
		DEBUG(5, ("expecting EMSMDBP_OBJECT_FOLDER as type of parent object\n"));
		return NULL;
	}

	/* Initialize message object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.message = talloc_zero(object, struct emsmdbp_object_message);
	if (!object->object.message) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_MESSAGE;
	object->object.message->messageID = messageID;

	return object;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_open(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_object, uint64_t folderID, uint64_t messageID, struct mapistore_message **msgp)
{
	struct emsmdbp_object *folder_object, *message_object = NULL;
	uint32_t contextID;
	bool mapistore;
	TALLOC_CTX *local_mem_ctx;

	if (!parent_object) return NULL;

	local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	folder_object = emsmdbp_object_open_folder_by_fid(local_mem_ctx, emsmdbp_ctx, parent_object, folderID);
	if (!folder_object)  {
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(folder_object);
	switch (mapistore) {
	case false:
		/* system/special folder */
		DEBUG(0, ("[%s] not implemented yet - shouldn't occur\n", __location__));
		break;
	case true:
		/* mapistore implementation goes here */
		message_object = emsmdbp_object_message_init(mem_ctx, emsmdbp_ctx, messageID, folder_object);
		contextID = emsmdbp_get_contextID(folder_object);
		if (mapistore_folder_open_message(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, message_object, messageID, &message_object->backend_object) != MAPISTORE_SUCCESS
		    || mapistore_message_get_message_data(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object, mem_ctx, msgp) != MAPISTORE_SUCCESS) {
			talloc_free(message_object);
			message_object = NULL;
		}
	}

end:
	talloc_free(local_mem_ctx);

	return message_object;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_open_attachment_table(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *message_object)
{
	struct emsmdbp_object	*table_object;
	uint32_t		contextID;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!message_object || message_object->type != EMSMDBP_OBJECT_MESSAGE) return NULL;

	switch (emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		DEBUG(0, ("[%s] not implemented yet - shouldn't occur\n", __location__));
		table_object = NULL;
		break;
	case true:
                contextID = emsmdbp_get_contextID(message_object);

		table_object = emsmdbp_object_table_init(mem_ctx, emsmdbp_ctx, message_object);
		if (table_object) {
			table_object->object.table->ulType = EMSMDBP_TABLE_ATTACHMENT_TYPE;
			mapistore_message_get_attachment_table(emsmdbp_ctx->mstore_ctx, contextID,
							       message_object->backend_object,
							       table_object, &table_object->backend_object,
							       &table_object->object.table->denominator);
		}
        }

	return table_object;
}

/**
   \details Initialize a stream object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param property the stream property identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_stream_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.stream = talloc_zero(object, struct emsmdbp_object_stream);
	if (!object->object.stream) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_STREAM;
	object->object.stream->property = 0;
	object->object.stream->needs_commit = false;
	object->object.stream->stream.buffer.data = NULL;
	object->object.stream->stream.buffer.length = 0;
	object->object.stream->stream.position = 0;

	return object;
}


/**
   \details Initialize a attachment object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent 
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_attachment_init(TALLOC_CTX *mem_ctx,
                                                               struct emsmdbp_context *emsmdbp_ctx,
                                                               uint64_t messageID,
                                                               struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.attachment = talloc_zero(object, struct emsmdbp_object_attachment);
	if (!object->object.attachment) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_ATTACHMENT;
	object->object.attachment->attachmentID = -1;

	return object;
}

/**
   \details Initialize a notification subscription object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param whole_store whether the subscription applies to the specified change on the entire store or stricly on the specified folder/message
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_subscription_init(TALLOC_CTX *mem_ctx,
                                                                 struct emsmdbp_context *emsmdbp_ctx,
                                                                 struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.subscription = talloc_zero(object, struct emsmdbp_object_subscription);
	if (!object->object.subscription) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_SUBSCRIPTION;
        object->object.subscription->subscription_list = NULL;

	return object;
}

_PUBLIC_ int emsmdbp_object_get_available_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray **propertiesp)
{
	uint32_t contextID;

	if (!(object->type == EMSMDBP_OBJECT_FOLDER
	      || object->type == EMSMDBP_OBJECT_MAILBOX
	      || object->type == EMSMDBP_OBJECT_MESSAGE
	      || object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		DEBUG(0, (__location__": object must be EMSMDBP_OBJECT_FOLDER, EMSMDBP_OBJECT_MAILBOX, EMSMDBP_OBJECT_MESSAGE or EMSMDBP_OBJECT_ATTACHMENT (type =  %d)\n", object->type));
		return MAPISTORE_ERROR;
        }

	if (!emsmdbp_is_mapistore(object)) {
		DEBUG(5, (__location__": only mapistore is supported at this time\n"));
		return MAPISTORE_ERROR;
	}

	contextID = emsmdbp_get_contextID(object);

	return mapistore_properties_get_available_properties(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, mem_ctx, propertiesp);
}

static int emsmdbp_object_get_properties_systemspecialfolder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object_folder	*folder;
	int				i;
        uint32_t                        *obj_count;
	uint8_t				*has_subobj;
	time_t				unix_time;
	NTTIME				nt_time;
	struct FILETIME			*ft;
	struct Binary_r			*binr;

	folder = (struct emsmdbp_object_folder *) object->object.folder;
        for (i = 0; i < properties->cValues; i++) {
                if (properties->aulPropTag[i] == PR_FOLDER_CHILD_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
			retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, object->object.folder->folderID, obj_count);
			data_pointers[i] = obj_count;
                }
		else if (properties->aulPropTag[i] == PR_SUBFOLDERS) {
			obj_count = talloc_zero(NULL, uint32_t);
			retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, object->object.folder->folderID, obj_count);
			has_subobj = talloc_zero(data_pointers, uint8_t);
			*has_subobj = (*obj_count > 0) ? 1 : 0;
			data_pointers[i] = has_subobj;
			talloc_free(obj_count);
		}
		else if (properties->aulPropTag[i] == PR_SOURCE_KEY) {
			emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, object->object.folder->folderID, &binr);
			data_pointers[i] = binr;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PR_CONTENT_COUNT
			 || properties->aulPropTag[i] == PR_ASSOC_CONTENT_COUNT
			 || properties->aulPropTag[i] == PR_CONTENT_UNREAD
			 || properties->aulPropTag[i] == PR_DELETED_COUNT_TOTAL) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
			data_pointers[i] = obj_count;
			retval = MAPI_E_SUCCESS;
                }
		else if (properties->aulPropTag[i] == PR_LOCAL_COMMIT_TIME_MAX) {
			/* TODO: temporary hack */
			unix_time = time(NULL) & 0xffffff00;
			unix_to_nt_time(&nt_time, unix_time);
			ft = talloc_zero(data_pointers, struct FILETIME);
			ft->dwLowDateTime = (nt_time & 0xffffffff);
			ft->dwHighDateTime = nt_time >> 32;
			data_pointers[i] = ft;
			retval = MAPI_E_SUCCESS;
		}
                else {
			retval = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx, 
								  emsmdbp_ctx->username, properties->aulPropTag[i],
								  folder->folderID, data_pointers + i);
                }
		retvals[i] = retval;
        }

	return MAPISTORE_SUCCESS;
}

static int emsmdbp_object_get_properties_mapistore_root(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object_folder	*folder;
	struct Binary_r			*binr;
	int				i;
        uint32_t                        *obj_count;
	uint8_t				*has_subobj;
	time_t				unix_time;
	NTTIME				nt_time;
	struct FILETIME			*ft;

	folder = (struct emsmdbp_object_folder *) object->object.folder;
        for (i = 0; i < properties->cValues; i++) {
                if (properties->aulPropTag[i] == PR_CONTENT_COUNT) {
                        /* a hack to avoid fetching dynamic fields from openchange.ldb */
                        obj_count = talloc_zero(data_pointers, uint32_t);
                        retval = mapistore_folder_get_message_count(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object,
								    MAPISTORE_MESSAGE_TABLE, obj_count);
			if (!retval) {
				data_pointers[i] = obj_count;
			}
                }
		else if (properties->aulPropTag[i] == PR_SOURCE_KEY) {
			emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, object->object.folder->folderID, &binr);
			data_pointers[i] = binr;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PR_FOLDER_TYPE) {
			obj_count = talloc_zero(data_pointers, uint32_t);
			*obj_count = FOLDER_GENERIC;
			data_pointers[i] = obj_count;
			retval = MAPI_E_SUCCESS;
		}
                else if (properties->aulPropTag[i] == PR_ASSOC_CONTENT_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
                        retval = mapistore_folder_get_message_count(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object,
								    MAPISTORE_FAI_TABLE, obj_count);
			if (!retval) {
				data_pointers[i] = obj_count;
			}
                }
                else if (properties->aulPropTag[i] == PR_FOLDER_CHILD_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
                        retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, object, obj_count);
			if (!retval) {
				data_pointers[i] = obj_count;
			}
                }
		else if (properties->aulPropTag[i] == PR_SUBFOLDERS) {
			obj_count = talloc_zero(NULL, uint32_t);
			retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, object, obj_count);
			if (!retval) {
				has_subobj = talloc_zero(data_pointers, uint8_t);
				*has_subobj = (*obj_count > 0) ? 1 : 0;
				data_pointers[i] = has_subobj;
			}
			talloc_free(obj_count);
		}
		else if (properties->aulPropTag[i] == PR_CONTENT_UNREAD || properties->aulPropTag[i] == PR_DELETED_COUNT_TOTAL) {
			/* TODO: temporary hack */
			obj_count = talloc_zero(data_pointers, uint32_t);
			*obj_count = 0;
			data_pointers[i] = obj_count;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PR_LOCAL_COMMIT_TIME_MAX) {
			/* TODO: temporary hack */
			unix_time = time(NULL) & 0xffffff00;
			unix_to_nt_time(&nt_time, unix_time);
			ft = talloc_zero(data_pointers, struct FILETIME);
			ft->dwLowDateTime = (nt_time & 0xffffffff);
			ft->dwHighDateTime = nt_time >> 32;
			data_pointers[i] = ft;
			retval = MAPI_E_SUCCESS;
		}
                else {
			retval = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx, 
								  emsmdbp_ctx->username, properties->aulPropTag[i],
								  folder->folderID, data_pointers + i);
                }
		retvals[i] = retval;
        }

	return MAPISTORE_SUCCESS;
}

static int emsmdbp_object_get_properties_mailbox(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t			i;
	struct SBinary_short		*bin;

	for (i = 0; i < properties->cValues; i++) {
		switch (properties->aulPropTag[i]) {
		case PR_MAPPING_SIGNATURE:
		case PR_IPM_PUBLIC_FOLDERS_ENTRYID:
			retvals[i] = MAPI_E_NO_ACCESS;
			break;
		case PR_USER_ENTRYID:
			bin = talloc_zero(data_pointers, struct SBinary_short);
			retvals[i] = entryid_set_AB_EntryID(data_pointers, object->object.mailbox->szUserDN, bin);
			data_pointers[i] = bin;
			break;
		case PR_MAILBOX_OWNER_ENTRYID:
			if (object->object.mailbox->mailboxstore == false) {
				retvals[i] = MAPI_E_NO_ACCESS;
			} else {
				bin = talloc_zero(data_pointers, struct SBinary_short);
				retvals[i] = entryid_set_AB_EntryID(data_pointers, object->object.mailbox->owner_EssDN,
								    bin);
				data_pointers[i] = bin;
			}
			break;
		case PR_MAILBOX_OWNER_NAME:
		case PR_MAILBOX_OWNER_NAME_UNICODE:
			if (object->object.mailbox->mailboxstore == false) {
				retvals[i] = MAPI_E_NO_ACCESS;
			} else {
				retvals[i] = MAPI_E_SUCCESS;
				data_pointers[i] = talloc_strdup(data_pointers, object->object.mailbox->owner_Name);
			}
			break;
		default:
			retvals[i] = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx,
								      emsmdbp_ctx->username, properties->aulPropTag[i],
								      object->object.mailbox->folderID, data_pointers + i);
		}
	}

	return MAPISTORE_SUCCESS;
}

static int emsmdbp_object_get_properties_mapistore(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t		contextID = -1;
	struct mapistore_property_data  *prop_data;
	int			i, ret;

	contextID = emsmdbp_get_contextID(object);
	prop_data = talloc_array(NULL, struct mapistore_property_data, properties->cValues);
	memset(prop_data, 0, sizeof(struct mapistore_property_data) * properties->cValues);

	ret = mapistore_properties_get_properties(emsmdbp_ctx->mstore_ctx, contextID,
						  object->backend_object,
						  prop_data,
						  properties->cValues,
						  properties->aulPropTag,
						  prop_data);
	if (ret == MAPISTORE_SUCCESS) {
		for (i = 0; i < properties->cValues; i++) {
			if (prop_data[i].error) {
				if (prop_data[i].error == MAPISTORE_ERR_NOT_FOUND) {
					retvals[i] = MAPI_E_NOT_FOUND;
				}
				else {
					retvals[i] = MAPI_E_NO_SUPPORT;
					DEBUG (4, ("%s: unknown mapistore error: %.8x\n", __PRETTY_FUNCTION__, prop_data[i].error));
				}
			}
			else {
				if (prop_data[i].data == NULL) {
					retvals[i] = MAPI_E_NOT_FOUND;
				}
				else {
					data_pointers[i] = prop_data[i].data;
					(void) talloc_reference(data_pointers, prop_data[i].data);
				}
			}
		}
	}
	talloc_free(prop_data);

	return ret;
}

_PUBLIC_ void **emsmdbp_object_get_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, enum MAPISTATUS **retvalsp)
{
        void		**data_pointers;
        enum MAPISTATUS	*retvals;
	bool		mapistore;
	int		retval;

        data_pointers = talloc_array(mem_ctx, void *, properties->cValues);
        memset(data_pointers, 0, sizeof(void *) * properties->cValues);

        retvals = talloc_array(mem_ctx, enum MAPISTATUS, properties->cValues);
        memset(retvals, 0, sizeof(enum MAPISTATUS) * properties->cValues);

	/* Temporary hack: If this is a mapistore root container
	 * (e.g. Inbox, Calendar etc.), directly stored under
	 * IPM.Subtree, then fetch properties from openchange
	 * dispatcher db, not mapistore */
	if (object && object->type == EMSMDBP_OBJECT_FOLDER &&
	    object->object.folder->mapistore_root == true) {
		retval = emsmdbp_object_get_properties_mapistore_root(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
	} else {
		mapistore = emsmdbp_is_mapistore(object);
		/* Nasty hack */
		if (!object) {
			DEBUG(5, ("[%s] what's that hack!??\n", __location__));
			mapistore = true;
		}

		switch (mapistore) {
		case false:
			switch (object->type) {
			case EMSMDBP_OBJECT_MAILBOX:
				retval = emsmdbp_object_get_properties_mailbox(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			case EMSMDBP_OBJECT_FOLDER:
				retval = emsmdbp_object_get_properties_systemspecialfolder(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			default:
				retval = MAPISTORE_ERROR;
				break;
			}
			break;
		case true:
			/* folder or messages handled by mapistore */
			retval = emsmdbp_object_get_properties_mapistore(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
			break;
		}
	}

	if (retvalsp) {
		*retvalsp = retvals;
	}

	if (retval) {
		talloc_free(data_pointers);
		data_pointers = NULL;
	}

        return data_pointers;
}

/* TODO: handling of "property problems" */
_PUBLIC_ int emsmdbp_object_set_properties(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SRow *rowp)
{
	uint32_t contextID;
	bool mapistore;

	/* Sanity checks */
	if (!emsmdbp_ctx) return MAPI_E_CALL_FAILED;
	if (!object) return MAPI_E_CALL_FAILED;
	if (!rowp) return MAPI_E_CALL_FAILED;

	if (!(object->type == EMSMDBP_OBJECT_FOLDER
	      || object->type == EMSMDBP_OBJECT_MAILBOX
	      || object->type == EMSMDBP_OBJECT_MESSAGE
	      || object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		DEBUG(0, (__location__": object must be EMSMDBP_OBJECT_FOLDER, EMSMDBP_OBJECT_MAILBOX, EMSMDBP_OBJECT_MESSAGE or EMSMDBP_OBJECT_ATTACHMENT (type =  %d)\n", object->type));
		return MAPI_E_NO_SUPPORT;
        }

	/* Temporary hack: If this is a mapistore root container
	 * (e.g. Inbox, Calendar etc.), directly stored under
	 * IPM.Subtree, then set properties from openchange
	 * dispatcher db, not mapistore */
	if (object->type == EMSMDBP_OBJECT_FOLDER
	    && object->object.folder->mapistore_root == true) {
		openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, object->object.folder->folderID, rowp);
	}
	else {
		contextID = emsmdbp_get_contextID(object);
		mapistore = emsmdbp_is_mapistore(object);
		switch (mapistore) {
		case false:
			if (object->type == EMSMDBP_OBJECT_FOLDER) {
				openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, object->object.folder->folderID, rowp);
			}
			else if (object->type == EMSMDBP_OBJECT_MAILBOX) {
				openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, object->object.mailbox->folderID, rowp);
			}
			else {
				DEBUG(0, ("Setting properties on openchangedb not implemented yet for non-folder object type\n"));
				return MAPI_E_NO_SUPPORT;
			}
			break;
		case true:
			mapistore_properties_set_properties(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, rowp);
			break;
		}
	}

	return MAPI_E_SUCCESS;
}

_PUBLIC_ void emsmdbp_fill_row_blob(TALLOC_CTX *mem_ctx,
				    struct emsmdbp_context *emsmdbp_ctx,
				    uint8_t *layout,
				    DATA_BLOB *property_row,
				    struct SPropTagArray *properties,
				    void **data_pointers,
				    enum MAPISTATUS *retvals,
				    bool *untyped_status)
{
        uint16_t i;
        uint8_t flagged;
        enum MAPITAGS property;
        void *data;
        uint32_t retval;

        flagged = 0;
        for (i = 0; !flagged && i < properties->cValues; i++) {
                if (retvals[i] != MAPI_E_SUCCESS || untyped_status[i] || !data_pointers[i]) {
                        flagged = 1;
                }
        }
	*layout = flagged;

        for (i = 0; i < properties->cValues; i++) {
                retval = retvals[i];
                if (retval != MAPI_E_SUCCESS) {
                        property = (properties->aulPropTag[i] & 0xFFFF0000) + PT_ERROR;
                        data = &retval;
                }
                else {
                        property = properties->aulPropTag[i];
                        data = data_pointers[i];
                }
                libmapiserver_push_property(mem_ctx,
                                            property, data, property_row,
                                            flagged ? PT_ERROR : 0, flagged, untyped_status[i]);
        }
}

_PUBLIC_ struct emsmdbp_stream_data *emsmdbp_stream_data_from_value(TALLOC_CTX *mem_ctx, enum MAPITAGS prop_tag, void *value)
{
	uint16_t			prop_type;
	struct emsmdbp_stream_data	*stream_data;
	size_t				converted_size;

	stream_data = talloc_zero(mem_ctx, struct emsmdbp_stream_data);
        stream_data->prop_tag = prop_tag;
	prop_type = prop_tag & 0xffff;
	if (prop_type == PT_STRING8) {
		stream_data->data.length = strlen(value) + 1;
		stream_data->data.data = value;
                (void) talloc_reference(stream_data, stream_data->data.data);
	}
	else if (prop_type == PT_UNICODE) {
		stream_data->data.length = strlen_m_ext((char *) value, CH_UTF8, CH_UTF16LE) * 2;
		stream_data->data.data = talloc_array(stream_data, uint8_t, stream_data->data.length + 2);
		convert_string(CH_UTF8, CH_UTF16LE,
			       value, strlen(value),
			       stream_data->data.data, stream_data->data.length,
			       &converted_size);
		memset(stream_data->data.data + stream_data->data.length, 0, 2 * sizeof(uint8_t));
	}
	else if (prop_type == PT_BINARY) {
		stream_data->data.length = ((struct Binary_r *) value)->cb;
		stream_data->data.data = ((struct Binary_r *) value)->lpb;
                (void) talloc_reference(stream_data, stream_data->data.data);
	}
	else {
		talloc_free(stream_data);
		return NULL;
	}

	return stream_data;
}

_PUBLIC_ DATA_BLOB emsmdbp_stream_read_buffer(struct emsmdbp_stream *stream, uint32_t length)
{
	DATA_BLOB buffer;
	uint32_t real_length;

	real_length = length;
	if (real_length + stream->position > stream->buffer.length) {
		real_length = stream->buffer.length - stream->position;
	}
	buffer.length = real_length;
	buffer.data = stream->buffer.data + stream->position;
	stream->position += real_length;

	return buffer;
}

_PUBLIC_ void emsmdbp_stream_write_buffer(TALLOC_CTX *mem_ctx, struct emsmdbp_stream *stream, DATA_BLOB new_buffer)
{
	uint32_t new_position, old_length;
	uint8_t *old_data;

	new_position = stream->position + new_buffer.length;
	if (new_position >= stream->buffer.length) {
		old_length = stream->buffer.length;
		stream->buffer.length = new_position;
		if (stream->buffer.data) {
			old_data = stream->buffer.data;
			stream->buffer.data = talloc_realloc(mem_ctx, stream->buffer.data, uint8_t, stream->buffer.length);
			if (!stream->buffer.data) {
				DEBUG(5, ("WARNING: [bug] lost buffer pointer (data = NULL)\n"));
				stream->buffer.data = talloc_array(mem_ctx, uint8_t, stream->buffer.length);
				memcpy(stream->buffer.data, old_data, old_length);
			}
		}
		else {
			stream->buffer.data = talloc_array(mem_ctx, uint8_t, stream->buffer.length);
		}
	}

	memcpy(stream->buffer.data + stream->position, new_buffer.data, new_buffer.length);
	stream->position = new_position;
}

_PUBLIC_ struct emsmdbp_stream_data *emsmdbp_object_get_stream_data(struct emsmdbp_object *object, enum MAPITAGS prop_tag)
{
        struct emsmdbp_stream_data *current_data;

	for (current_data = object->stream_data; current_data; current_data = current_data->next) {
		if (current_data->prop_tag == prop_tag) {
			DEBUG(5, ("[%s]: found data for tag %.8x\n", __FUNCTION__, prop_tag));
			return current_data;
		}
	}

	return NULL;
}

/**
   \details Initialize a synccontext object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param whole_store whether the subscription applies to the specified change on the entire store or stricly on the specified folder/message
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_synccontext_init(TALLOC_CTX *mem_ctx,
								struct emsmdbp_context *emsmdbp_ctx,
								struct emsmdbp_object *parent_object)
{
	struct emsmdbp_object	*synccontext_object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent_object) return NULL;
	if (!(parent_object->type == EMSMDBP_OBJECT_FOLDER || parent_object->type == EMSMDBP_OBJECT_MAILBOX)) {
		DEBUG(0, (__location__": parent_object must be EMSMDBP_OBJECT_FOLDER or EMSMDBP_OBJECT_MAILBOX (type =  %d)\n", parent_object->type));
		return NULL;
        }

	synccontext_object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent_object);
	if (!synccontext_object) return NULL;

	synccontext_object->object.synccontext = talloc_zero(synccontext_object, struct emsmdbp_object_synccontext);
	if (!synccontext_object->object.synccontext) {
		talloc_free(synccontext_object);
		return NULL;
	}

	synccontext_object->type = EMSMDBP_OBJECT_SYNCCONTEXT;

	(void) talloc_reference(synccontext_object->object.synccontext, parent_object);
        synccontext_object->object.synccontext->state_property = 0;
        synccontext_object->object.synccontext->state_stream.buffer.length = 0;
        synccontext_object->object.synccontext->state_stream.buffer.data = talloc_zero(synccontext_object->object.synccontext, uint8_t);
        synccontext_object->object.synccontext->stream.buffer.length = 0;
        synccontext_object->object.synccontext->stream.buffer.data = NULL;

	synccontext_object->object.synccontext->cnset_seen = talloc_zero(emsmdbp_ctx, struct idset);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, NULL, &synccontext_object->object.synccontext->cnset_seen->repl.guid);
	synccontext_object->object.synccontext->cnset_seen->ranges = talloc_zero(synccontext_object->object.synccontext->cnset_seen, struct globset_range);
	synccontext_object->object.synccontext->cnset_seen->range_count = 1;
	synccontext_object->object.synccontext->cnset_seen->ranges->next = NULL;
	synccontext_object->object.synccontext->cnset_seen->ranges->prev = synccontext_object->object.synccontext->cnset_seen->ranges;
	synccontext_object->object.synccontext->cnset_seen->ranges->low = 0xffffffffffffffffLL;
	synccontext_object->object.synccontext->cnset_seen->ranges->high = 0x0;

        /* synccontext_object->object.synccontext->property_tags.cValues = 0; */
        /* synccontext_object->object.synccontext->property_tags.aulPropTag = NULL; */

	return synccontext_object;
}

/**
   \details Initialize a ftcontext object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param whole_store whether the subscription applies to the specified change on the entire store or stricly on the specified folder/message
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_ftcontext_init(TALLOC_CTX *mem_ctx,
							      struct emsmdbp_context *emsmdbp_ctx,
							      struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.ftcontext = talloc_zero(object, struct emsmdbp_object_ftcontext);
	if (!object->object.ftcontext) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FTCONTEXT;

	return object;
}
