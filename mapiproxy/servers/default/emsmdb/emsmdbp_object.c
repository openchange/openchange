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
		return object->object.folder->mapistore;
	case EMSMDBP_OBJECT_TABLE:
		return object->object.table->mapistore;
	case EMSMDBP_OBJECT_MESSAGE:
		return object->object.message->mapistore;
	case EMSMDBP_OBJECT_STREAM:
		return object->object.stream->mapistore;
	case EMSMDBP_OBJECT_ATTACHMENT:
		return object->object.attachment->mapistore;
	default:
		return false;
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
		return  object->object.mailbox->mailboxstore;
	case EMSMDBP_OBJECT_FOLDER:
		return object->object.folder->mailboxstore;
	default:
		break;
	}
	
	/* We should never hit this case */
	return true;
}


/**
   \details Return the contextID associated to a handle

   \param object pointer to the emsmdp object

   \return contextID value on success, otherwise -1
 */
uint32_t emsmdbp_get_contextID(struct emsmdbp_object *object)
{
	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		return -1;
	case EMSMDBP_OBJECT_FOLDER:
		return object->object.folder->contextID;
	case EMSMDBP_OBJECT_MESSAGE:
		return object->object.message->contextID;
	case EMSMDBP_OBJECT_STREAM:
		return object->object.stream->contextID;
	case EMSMDBP_OBJECT_ATTACHMENT:
		return object->object.attachment->contextID;
	case EMSMDBP_OBJECT_SUBSCRIPTION:
		return object->object.subscription->contextID;
	default:
		return -1;
	}

	return -1;
}


/**
   \details Retrieve the folder handle matching given fid

   \param handles_ctx pointer to the handles context
   \param fid folder identifier to lookup

   \return pointer to valid mapi_handles structure on success, otherwise NULL
 */
struct mapi_handles *emsmdbp_object_get_folder_handle_by_fid(struct mapi_handles_context *handles_ctx,
							     uint64_t fid)
{
	struct mapi_handles	*handle;
	struct emsmdbp_object	*object;
	void			*data;

	for (handle = handles_ctx->handles; handle; handle = handle->next) {
		mapi_handles_get_private_data(handle, &data);
		if (data) {
			object = (struct emsmdbp_object *) data;
			if (object->type == EMSMDBP_OBJECT_FOLDER && object->object.folder->folderID == fid) {
				return handle;
			}
		}
	}

	return NULL;
}

_PUBLIC_ int emsmdbp_object_stream_commit(struct emsmdbp_object *stream_object)
{
	int rc;
        void *poc_backend_object;
	struct emsmdbp_object_stream *stream;
        void *stream_data;
        uint8_t *utf8_buffer;
        struct Binary_r *binary_data;
        struct SRow aRow;
        uint32_t string_size;
	uint16_t propType;

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
			string_size = strlen_m_ext((char *) stream->stream.buffer.data, CH_UTF16LE, CH_UTF8) / 2;
			utf8_buffer = talloc_array(aRow.lpProps, uint8_t, string_size + 1);
			memset(utf8_buffer, 0, string_size);
			convert_string(CH_UTF16LE, CH_UTF8,
				       stream->stream.buffer.data, stream->stream.buffer.length,
				       utf8_buffer, string_size,
				       false);
			stream_data = utf8_buffer;
		}
		set_SPropValue_proptag(aRow.lpProps, stream->property, stream_data);
		if (stream->parent_poc_api) {
			poc_backend_object = stream->parent_poc_backend_object;
			rc = mapistore_pocop_set_properties(stream_object->mstore_ctx,
							    stream->contextID, poc_backend_object, &aRow);
		}
		else {
			mapistore_setprops(stream_object->mstore_ctx, stream->contextID, stream->objectID, 
					   stream->objectType, &aRow);
		}
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
	int			ret;

	if (!data) return -1;
	
	DEBUG(4, ("[%s:%d]: emsmdbp %s object released\n", __FUNCTION__, __LINE__,
		  emsmdbp_getstr_type(object)));

	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		ret = mapistore_del_context(object->mstore_ctx, object->object.folder->contextID);
                if (object->poc_api) {
                                mapistore_pocop_release(object->mstore_ctx, object->object.folder->contextID,
                                                        object->poc_backend_object);
                }
		DEBUG(4, ("[%s:%d] mapistore folder context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
	case EMSMDBP_OBJECT_MESSAGE:
		ret = mapistore_release_record(object->mstore_ctx, object->object.message->contextID,
					       object->object.message->messageID, MAPISTORE_MESSAGE);
                if (object->poc_api) {
                                mapistore_pocop_release(object->mstore_ctx, object->object.message->contextID,
                                                        object->poc_backend_object);
                }
		ret = mapistore_del_context(object->mstore_ctx, object->object.message->contextID);
		DEBUG(4, ("[%s:%d] mapistore message context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
	case EMSMDBP_OBJECT_TABLE:
                if (object->poc_api) {
                                mapistore_pocop_release(object->mstore_ctx, object->object.table->contextID,
                                                        object->poc_backend_object);
                }
                if (object->object.table->subscription_list) {
                        DEBUG(5, ("  list object: removing subscription from context list\n"));
                        DLIST_REMOVE(object->mstore_ctx->subscriptions, object->object.table->subscription_list);
                }
		ret = mapistore_del_context(object->mstore_ctx, object->object.message->contextID);
		DEBUG(4, ("[%s:%d] mapistore message context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
	case EMSMDBP_OBJECT_STREAM:
		emsmdbp_object_stream_commit(object);
                if (object->poc_api) {
                                mapistore_pocop_release(object->mstore_ctx, object->object.message->contextID,
                                                        object->poc_backend_object);
                }
		ret = mapistore_del_context(object->mstore_ctx, object->object.stream->contextID);
		DEBUG(4, ("[%s:%d] mapistore stream context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
	case EMSMDBP_OBJECT_ATTACHMENT:
                if (object->poc_api) {
                                mapistore_pocop_release(object->mstore_ctx, object->object.attachment->contextID,
                                                        object->poc_backend_object);
                }
		ret = mapistore_del_context(object->mstore_ctx, object->object.attachment->contextID);
		DEBUG(4, ("[%s:%d] mapistore stream context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
        case EMSMDBP_OBJECT_SUBSCRIPTION:
                if (object->object.subscription->subscription_list) {
                        DEBUG(5, ("  subscription object: removing subscription from context list\n"));
                        DLIST_REMOVE(object->mstore_ctx->subscriptions, object->object.subscription->subscription_list);
                }
		ret = mapistore_del_context(object->mstore_ctx, object->object.folder->contextID);
		DEBUG(4, ("[%s:%d] mapistore subscription context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
        case EMSMDBP_OBJECT_SYNCCONTEXT:
        case EMSMDBP_OBJECT_FTCONTEXT:
		break;
	default:
		DEBUG(4, ("[%s:%d] destroying unhandled object type: %d\n", __FUNCTION__, __LINE__, object->type));
		break;
	}

	talloc_free(object);

	return 0;
}

/**
   \details Initialize an emsmdbp_object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx)
{
	struct emsmdbp_object	*object = NULL;

	object = talloc_zero(mem_ctx, struct emsmdbp_object);
	if (!object) return NULL;

	talloc_set_destructor((void *)object, (int (*)(void *))emsmdbp_object_destructor);

	object->type = EMSMDBP_OBJECT_UNDEF;
	object->emsmdbp_ctx = emsmdbp_ctx;
	object->mstore_ctx = emsmdbp_ctx->mstore_ctx;
	object->object.mailbox = NULL;
	object->object.folder = NULL;
	object->object.message = NULL;
	object->object.stream = NULL;
	object->private_data = NULL;

        /* proof of concept */
	object->poc_api = false;
	object->poc_backend_object = NULL;

	object->stream_data = NULL;

	return object;
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

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
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

		if (res->count == 1) {
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
							   struct emsmdbp_object *parent)
{
	enum MAPISTATUS				retval;
	struct emsmdbp_object			*object;
	struct mapistore_connection_info	*conn_info;
	char					*mapistore_uri = NULL;
	uint32_t				context_id;
	int					ret;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.folder = talloc_zero(object, struct emsmdbp_object_folder);
	if (!object->object.folder) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FOLDER;
	object->object.folder->contextID = -1;
	object->object.folder->folderID = folderID;
	object->object.folder->mapistore_root = false;
	object->object.folder->mailboxstore = emsmdbp_is_mailboxstore(parent);

	/* system folders acting as containers don't have
	 * mapistore_uri attributes (Mailbox, IPM Subtree) 
	 */
	retval = openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx,
					       object->object.folder->folderID, 
					       &mapistore_uri,
                                               object->object.folder->mailboxstore);

	if (retval == MAPI_E_SUCCESS) {
		if (!mapistore_uri) {
			DEBUG(0, ("This is not a mapistore container (folderID = %.16lx)\n", folderID));
			object->object.folder->mapistore = false;
		} else {
			DEBUG(0, ("This is a mapistore container: uri = %s\n", mapistore_uri));
			object->object.folder->mapistore = true;
			object->object.folder->mapistore_root = true;
			ret = mapistore_search_context_by_uri(emsmdbp_ctx->mstore_ctx, mapistore_uri, 
							      &context_id);
			if (ret == MAPISTORE_SUCCESS) {
				ret = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, context_id);
			} else {
				conn_info = talloc_zero(emsmdbp_ctx, struct mapistore_connection_info);
				conn_info->mstore_ctx = emsmdbp_ctx->mstore_ctx;
				conn_info->oc_ctx = emsmdbp_ctx->oc_ctx;
				conn_info->username = emsmdbp_ctx->username;
				openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, &conn_info->repl_id, &conn_info->replica_guid);
				conn_info->indexing = mapistore_indexing_get_tdb_wrap(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username);

				ret = mapistore_add_context(emsmdbp_ctx->mstore_ctx, conn_info, mapistore_uri, object->object.folder->folderID, &context_id);
				DEBUG(0, ("context id: %d (%s)\n", context_id, mapistore_uri));
				if (ret != MAPISTORE_SUCCESS) {
					talloc_free(object);
					return NULL;
				}
				ret = mapistore_add_context_indexing(emsmdbp_ctx->mstore_ctx, 
								     emsmdbp_ctx->username,
								     context_id);
				ret = mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx,
									context_id, folderID);
			}
			object->object.folder->contextID = context_id;
		}
	} else {
		if (emsmdbp_is_mapistore(parent)) {
			object->object.folder->mapistore = true;
			object->object.folder->contextID = emsmdbp_get_contextID(parent);
			ret = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, 
							      object->object.folder->contextID);
		} else {
			object->object.folder->mapistore = false;
		}
	}

	return object;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_folder_open(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent, uint64_t folderID)
{
	struct emsmdbp_object	*folder_object = NULL;
	int			retval;
	char			*path;
	struct backend_context	*bctx;
	void			*local_ctx;
	bool			force_mapistore = false;

	local_ctx = talloc_zero(NULL, void);

	retval = openchangedb_get_mapistoreURI(local_ctx, emsmdbp_ctx->oc_ctx, folderID, &path, true);

	/* TODO: OpenFolder might attempt to open a folder at the mailbox root level.. */
	if (retval == MAPI_E_SUCCESS) {
		/* system/special folder */
		DEBUG(0, ("Opening system/special folder\n"));
	}
	else {
		/* handled by mapistore */
		DEBUG(0, ("Opening Generic folder\n"));
		
		bctx = mapistore_find_container_backend(emsmdbp_ctx->mstore_ctx, folderID);
		if (bctx) {
			retval = mapistore_opendir(emsmdbp_ctx->mstore_ctx, bctx->context_id, folderID);
			if (retval) {
				goto end;
			}
			force_mapistore = true;
		}
		else {
			goto end;
		}
	}

	folder_object = emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, folderID, parent);
	if (force_mapistore) {
		folder_object->object.folder->mapistore = true;
	}
end:
	talloc_free(local_ctx);

	return folder_object;
}

int emsmdbp_folder_get_folder_count(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *folder, uint32_t *row_countp)
{
	int retval;

	if (emsmdbp_is_mapistore(folder)) {
		retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx,
						    folder->object.folder->contextID,
						    folder->object.folder->folderID,
						    row_countp);
	}
	else {
		retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, folder->object.folder->folderID, row_countp);
	}

	return retval;
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
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER && parent->type != EMSMDBP_OBJECT_MESSAGE) return NULL;

	/* Initialize table object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;
	
	object->object.table = talloc_zero(object, struct emsmdbp_object_table);
	if (!object->object.table) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_TABLE;
	object->object.table->folderID = parent->object.folder->folderID;
	object->object.table->prop_count = 0;
	object->object.table->properties = NULL;
	object->object.table->numerator = 0;
	object->object.table->denominator = 0;
	object->object.table->ulType = 0;
	object->object.table->mapistore = false;
	object->object.table->contextID = -1;
	object->object.table->subscription_list = NULL;
	object->object.table->mapistore_root = false;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.table->mapistore = true;
		if (parent->type == EMSMDBP_OBJECT_FOLDER) {
			object->object.table->mapistore_root = parent->object.folder->mapistore_root;
		}
		object->object.table->contextID = parent->object.folder->contextID;		
	}

	return object;
}

_PUBLIC_ int emsmdbp_object_table_get_available_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, struct SPropTagArray **propertiesp)
{
	int retval;
	struct emsmdbp_object_table *table;
	struct SPropTagArray *properties;

	if (!table_object->type == EMSMDBP_OBJECT_TABLE)
		return MAPISTORE_ERROR;

	table = table_object->object.table;
	if (table->mapistore) {
		if (table_object->poc_api) {
			retval = mapistore_pocop_get_available_table_properties(emsmdbp_ctx->mstore_ctx, table->contextID, table_object->poc_backend_object, propertiesp);
		}
		else {
			retval = mapistore_get_available_table_properties(emsmdbp_ctx->mstore_ctx, table->contextID, table->ulType, propertiesp);
		}
		if (retval == MAPISTORE_SUCCESS) {
			talloc_steal(mem_ctx, *propertiesp);
			talloc_steal(mem_ctx, (*propertiesp)->aulPropTag);
		}
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
        void **data_pointers;
        enum MAPISTATUS retval;
        uint32_t *retvals;
        struct emsmdbp_object_table *table;
        struct emsmdbp_object *rowFolder;
        struct mapistore_property_data *properties;
        uint32_t i, num_props, *obj_count;
	uint64_t *rowFolderID;
	uint8_t *has_subobj;
	char *table_filter;
	void *odb_ctx;

        table = table_object->object.table;
        num_props = table_object->object.table->prop_count;

        data_pointers = talloc_array(table_object, void *, num_props);
        memset(data_pointers, 0, sizeof(void *) * num_props);
        retvals = talloc_array(table_object, uint32_t, num_props);
        memset(retvals, 0, sizeof(uint32_t) * num_props);

	if (emsmdbp_is_mapistore(table_object)) {
		if (table_object->poc_api) {
			properties = talloc_array(NULL, struct mapistore_property_data, num_props);
			memset(properties, 0, sizeof(struct mapistore_property_data) * num_props);
			retval = mapistore_pocop_get_table_row(emsmdbp_ctx->mstore_ctx, table->contextID,
							       table_object->poc_backend_object,
							       MAPISTORE_PREFILTERED_QUERY, row_id, properties);
			if (retval == MAPI_E_SUCCESS) {
				for (i = 0; i < num_props; i++) {
					data_pointers[i] = properties[i].data;
                                        
					if (properties[i].error) {
						if (properties[i].error == MAPISTORE_ERR_NOT_FOUND)
							retvals[i] = MAPI_E_NOT_FOUND;
						else if (properties[i].error == MAPISTORE_ERR_NO_MEMORY)
							retvals[i] = MAPI_E_NOT_ENOUGH_MEMORY;
						else {
							DEBUG (4, ("%s: unknown mapistore error: %.8x", __PRETTY_FUNCTION__, properties[i].error));
						}
					}
					else {
						if (properties[i].data == NULL) {
							retvals[i] = MAPI_E_NOT_FOUND;
						}
						else {
							(void) talloc_reference(data_pointers, properties[i].data);
						}
					}
				}
			}
			else {
				DEBUG(5, ("%s: invalid object (likely due to a restriction)\n", __location__));
				talloc_free(retvals);
				retvals = NULL;
				talloc_free(data_pointers);
				data_pointers = NULL;
			}
			talloc_free(properties);
		}
		else {
			retval = MAPI_E_SUCCESS;
			for (i = 0; retval != MAPI_E_INVALID_OBJECT && i < num_props; i++) {
				retval = mapistore_get_table_property(emsmdbp_ctx->mstore_ctx, table->contextID,
								      table->ulType,
								      MAPISTORE_PREFILTERED_QUERY,
								      table->folderID, 
								      table->properties[i],
								      row_id, data_pointers + i);
				if (retval == MAPI_E_INVALID_OBJECT) {
					DEBUG(5, ("%s: invalid object (likely due to a restriction)\n", __location__));
					talloc_free(retvals);
					retvals = NULL;
					talloc_free(data_pointers);
					data_pointers = NULL;
				}
				else {
					retvals[i] = retval;
				}
			}
		}
	}
	else {
		odb_ctx = talloc_zero(NULL, void);

		table_filter = talloc_asprintf(odb_ctx, "(&(PidTagParentFolderId=%"PRId64")(PidTagFolderId=*))", table->folderID);
		retval = openchangedb_get_table_property(odb_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
							 table_filter, PR_FID, row_id, (void **) &rowFolderID);
		/* it's a hack to pass a table object as parent here... */
		rowFolder = emsmdbp_object_folder_init(odb_ctx, emsmdbp_ctx, *rowFolderID, table_object);

		/* Lookup for flagged property row */
                retval = MAPI_E_SUCCESS;
		for (i = 0; retval != MAPI_E_INVALID_OBJECT && i < num_props; i++) {
			if (table->properties[i] == PR_CONTENT_COUNT) {
				/* a hack to avoid fetching dynamic fields from openchange.ldb */
				obj_count = talloc_zero(data_pointers, uint32_t);
				retval = mapistore_get_message_count(emsmdbp_ctx->mstore_ctx, rowFolder->object.folder->contextID, rowFolder->object.folder->folderID,
								     MAPISTORE_MESSAGE_TABLE, obj_count);
				data_pointers[i] = obj_count;
			}
			else if (table->properties[i] == PR_ASSOC_CONTENT_COUNT) {
				obj_count = talloc_zero(data_pointers, uint32_t);
				retval = mapistore_get_message_count(emsmdbp_ctx->mstore_ctx, rowFolder->object.folder->contextID, rowFolder->object.folder->folderID,
								     MAPISTORE_FAI_TABLE, obj_count);
				data_pointers[i] = obj_count;
			}
			else if (table->properties[i] == PR_FOLDER_CHILD_COUNT) {
				obj_count = talloc_zero(data_pointers, uint32_t);
				retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx, rowFolder->object.folder->contextID, rowFolder->object.folder->folderID, obj_count);
				data_pointers[i] = obj_count;
			}
			else if (table->properties[i] == PR_SUBFOLDERS) {
				obj_count = talloc_zero(NULL, uint32_t);
				retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx, rowFolder->object.folder->contextID, rowFolder->object.folder->folderID, obj_count);
				has_subobj = talloc_zero(data_pointers, uint8_t);
				*has_subobj = (*obj_count > 0) ? 1 : 0;
				data_pointers[i] = has_subobj;
				talloc_free(obj_count);
			}
			else if (table->properties[i] == PR_CONTENT_UNREAD || table->properties[i] == PR_DELETED_COUNT_TOTAL) {
				/* TODO: temporary */
				obj_count = talloc_zero(NULL, uint32_t);
				data_pointers[i] = obj_count;
				retval = MAPI_E_SUCCESS;
			}
			else {
				retval = openchangedb_get_table_property(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->oc_ctx, 
									 emsmdbp_ctx->username,
									 table_filter, table->properties[i], 
									 row_id, data_pointers + i);
			}
			/* DEBUG(5, ("  %.8x: %d", table->properties[j], retval)); */
			if (retval == MAPI_E_INVALID_OBJECT) {
				DEBUG(5, ("%s: invalid object in non-mapistore folder, count set to 0\n", __location__));
				talloc_free(retvals);
				retvals = NULL;
				talloc_free(data_pointers);
				data_pointers = NULL;
			}
			else {
				retvals[i] = retval;
			}
		}
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
	bool			mapistore = false;
	int			ret;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER) return NULL;

	/* Initialize message object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.message = talloc_zero(object, struct emsmdbp_object_message);
	if (!object->object.message) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_MESSAGE;
	object->object.message->folderID = parent->object.folder->folderID;
	object->object.message->messageID = messageID;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.message->mapistore = true;
		object->object.message->contextID = parent->object.folder->contextID;		
		ret = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, 
						      parent->object.folder->contextID);
	}

	return object;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_open(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_object, uint64_t folderID, uint64_t messageID, struct mapistore_message **msgp)
{
	struct emsmdbp_object *folder_object, *message_object;
	struct mapistore_message *msg;
	bool mapistore;

	if (!parent_object) return NULL;

	if (parent_object->type == EMSMDBP_OBJECT_FOLDER && parent_object->object.folder->folderID == folderID) {
		folder_object = parent_object;
	}
	else {
		folder_object = emsmdbp_object_folder_open(mem_ctx, emsmdbp_ctx, parent_object, folderID);
	}
	if (!folder_object) return NULL;

	mapistore = emsmdbp_is_mapistore(folder_object);
	switch (mapistore) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
		/* mapistore implementation goes here */
		message_object = emsmdbp_object_message_init(mem_ctx, emsmdbp_ctx, messageID, folder_object);
		msg = talloc_zero(message_object, struct mapistore_message);
		if (mapistore_openmessage(emsmdbp_ctx->mstore_ctx, folder_object->object.folder->contextID,
					  folderID, messageID, msg) == 0) {
			(void) talloc_reference(message_object, folder_object);
			*msgp = msg;
		}
		else {
			talloc_free(message_object);
			message_object = NULL;
		}
	}

	return message_object;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_open_attachment_table(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *message_object)
{
	struct emsmdbp_object	*table_object;
	void			*backend_attachment_table;
	uint32_t		row_count, contextID;
	int			retval;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!message_object || message_object->type != EMSMDBP_OBJECT_MESSAGE) return NULL;

	switch (emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		DEBUG(0, ("Not implemented yet - shouldn't occur\n"));
		break;
	case true:
                contextID = message_object->object.message->contextID;
                retval = mapistore_pocop_get_attachment_table(emsmdbp_ctx->mstore_ctx, contextID,
							      message_object->object.message->messageID,
                                                              &backend_attachment_table,
                                                              &row_count);
                if (retval) return NULL;

		table_object = emsmdbp_object_table_init(mem_ctx, emsmdbp_ctx, message_object);
		if (table_object) {
			table_object->poc_api = true;
			table_object->poc_backend_object = backend_attachment_table;
			table_object->object.table->denominator = row_count;
			table_object->object.table->ulType = EMSMDBP_TABLE_ATTACHMENT_TYPE;
			table_object->object.table->contextID = contextID;
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
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.stream = talloc_zero(object, struct emsmdbp_object_stream);
	if (!object->object.stream) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_STREAM;
	object->object.stream->property = 0;
	object->object.stream->needs_commit = false;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.stream->mapistore = true;
		object->object.stream->contextID = emsmdbp_get_contextID(parent);
		object->object.stream->objectID = -1;
		object->object.stream->objectType = -1;
                if (parent->poc_api) {
                        object->object.stream->parent_poc_api = true;
                        object->object.stream->parent_poc_backend_object = parent->poc_backend_object;
                }
		object->object.stream->stream.buffer.data = NULL;
		object->object.stream->stream.buffer.length = 0;
		object->object.stream->stream.position = 0;
	}

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
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.attachment = talloc_zero(object, struct emsmdbp_object_attachment);
	if (!object->object.attachment) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_ATTACHMENT;
	object->object.attachment->messageID = messageID;
	object->object.attachment->attachmentID = -1;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.attachment->mapistore = true;
		object->object.attachment->contextID = emsmdbp_get_contextID(parent);
	}

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
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.subscription = talloc_zero(object, struct emsmdbp_object_subscription);
	if (!object->object.subscription) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_SUBSCRIPTION;
        object->object.subscription->subscription_list = NULL;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.subscription->mapistore = true;
		object->object.subscription->contextID = emsmdbp_get_contextID(parent);
	}
        else {
                DEBUG(0, ("Subscriptions only supported on mapistore objects.\n"));
        }

	return object;
}

_PUBLIC_ int emsmdbp_object_get_available_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray **propertiesp)
{
	uint32_t contextID;
	uint8_t table_type;
	int retval;

	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		contextID = object->object.folder->contextID;
		table_type = MAPISTORE_FOLDER_TABLE;
		break;
	case EMSMDBP_OBJECT_MESSAGE:
		contextID = object->object.message->contextID;
		/* FIXME: the assumption here is wrong because we don't test the nature of the message */
		table_type = MAPISTORE_MESSAGE_TABLE;
		break;
	case EMSMDBP_OBJECT_ATTACHMENT:
		contextID = object->object.attachment->contextID;
		table_type = MAPISTORE_ATTACHMENT_TABLE;
		break;
	default:
		abort();
	}

	if (emsmdbp_is_mapistore(object)) {
		if (object->poc_api) {
			retval = mapistore_pocop_get_available_properties(emsmdbp_ctx->mstore_ctx, contextID, object->poc_backend_object, propertiesp);
		}
		else {
			retval = mapistore_get_available_table_properties(emsmdbp_ctx->mstore_ctx, contextID, table_type, propertiesp);
		}
		if (retval == MAPISTORE_SUCCESS) {
			talloc_steal(mem_ctx, *propertiesp);
			talloc_steal(mem_ctx, (*propertiesp)->aulPropTag);
		}
	}
	else {
		retval = MAPISTORE_ERROR;
		DEBUG(5, ("only mapistore is supported at this time\n"));
		abort();
	}

	return retval;
}

static void emsmdbp_object_get_properties_systemspecialfolder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object_folder	*folder;
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
                        retval = mapistore_get_message_count(emsmdbp_ctx->mstore_ctx, folder->contextID, folder->folderID,
                                                             MAPISTORE_MESSAGE_TABLE, obj_count);
                        data_pointers[i] = obj_count;
                }
                else if (properties->aulPropTag[i] == PR_ASSOC_CONTENT_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
                        retval = mapistore_get_message_count(emsmdbp_ctx->mstore_ctx, folder->contextID, folder->folderID,
                                                             MAPISTORE_FAI_TABLE, obj_count);
                        data_pointers[i] = obj_count;
                }
                else if (properties->aulPropTag[i] == PR_FOLDER_CHILD_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
                        retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx, folder->contextID, folder->folderID, obj_count);
                        data_pointers[i] = obj_count;
                }
		else if (properties->aulPropTag[i] == PR_SUBFOLDERS) {
			obj_count = talloc_zero(NULL, uint32_t);
			retval = mapistore_get_folder_count(emsmdbp_ctx->mstore_ctx, folder->contextID, folder->folderID, obj_count);
			has_subobj = talloc_zero(data_pointers, uint8_t);
			*has_subobj = (*obj_count > 0) ? 1 : 0;
			data_pointers[i] = has_subobj;
			talloc_free(obj_count);
		}
		else if (properties->aulPropTag[i] == PR_CONTENT_UNREAD || properties->aulPropTag[i] == PR_DELETED_COUNT_TOTAL) {
			/* TODO: temporary hack */
			obj_count = talloc_zero(NULL, uint32_t);
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
			if (openchangedb_lookup_folder_property(emsmdbp_ctx->oc_ctx, properties->aulPropTag[i], 
								folder->folderID)) {
				retval = MAPI_E_NOT_FOUND;
			}
			else {
				retval = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx, 
									  emsmdbp_ctx->username, properties->aulPropTag[i],
									  folder->folderID, data_pointers + i);
			}
                }
		retvals[i] = retval;
        }
}

static void emsmdbp_object_get_properties_mailbox(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
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
}

static void emsmdbp_object_get_properties_mapistore(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t		contextID = -1;
	uint64_t		fmid = 0;
	struct SRow             *aRow;
	struct mapistore_property_data  *prop_data;
	int			i;
	uint8_t			type;
	uint16_t		propType;

	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		contextID = object->object.folder->contextID;
		if (!object->poc_api) {
			fmid = object->object.folder->folderID;
			type = MAPISTORE_FOLDER;
		}
		break;
	case EMSMDBP_OBJECT_MESSAGE:
		contextID = object->object.message->contextID;
		if (!object->poc_api) {
			fmid = object->object.message->messageID;
			type = MAPISTORE_MESSAGE;
		}
		break;
	case EMSMDBP_OBJECT_ATTACHMENT:
		contextID = object->object.attachment->contextID;
		break;
	default:
		break;
	}

	if (contextID != -1) {
                if (object->poc_api) {
                        prop_data = talloc_array(NULL, struct mapistore_property_data, properties->cValues);
                        memset(prop_data, 0, sizeof(struct mapistore_property_data) * properties->cValues);

                        mapistore_pocop_get_properties(emsmdbp_ctx->mstore_ctx, contextID,
                                                       object->poc_backend_object,
                                                       properties->cValues,
                                                       properties->aulPropTag,
                                                       prop_data);
                        for (i = 0; i < properties->cValues; i++) {
                                if (prop_data[i].error) {
                                        if (prop_data[i].error == MAPISTORE_ERR_NOT_FOUND) {
                                                retvals[i] = MAPI_E_NOT_FOUND;
					}
                                        else {
                                                retvals[i] = MAPI_E_NO_SUPPORT;
                                                DEBUG (4, ("%s: unknown mapistore error: %.8x", __PRETTY_FUNCTION__, prop_data[i].error));
                                        }
                                }
                                else {
                                        if (prop_data[i].data == NULL) {
                                                retvals[i] = MAPI_E_NOT_FOUND;
					}
                                        else {
						data_pointers[i] = prop_data[i].data;
                                                talloc_steal(data_pointers, prop_data[i].data);
					}
                                }
                        }
                        talloc_free(prop_data);
                }
                else {
			aRow = talloc_zero(NULL, struct SRow);
                        if (mapistore_getprops(emsmdbp_ctx->mstore_ctx, contextID, fmid, type, properties, aRow) == MAPISTORE_SUCCESS) {
				talloc_steal(data_pointers, aRow->lpProps);
				for (i = 0; i < properties->cValues; i++) {
					propType = aRow->lpProps[i].ulPropTag & 0xffff;
					if (propType == PT_ERROR) {
						retvals[i] = aRow->lpProps[i].value.err;
					}
					else {
						if (((propType == PT_STRING8
						      || propType == PT_UNICODE)
						     && aRow->lpProps[i].value.lpszW == NULL)
						    || (propType == PT_BINARY
							&& aRow->lpProps[i].value.bin.lpb == NULL)) {
							retvals[i] = MAPI_E_NOT_FOUND;
						}
						else {
							data_pointers[i] = (void *) get_SPropValue_data(&aRow->lpProps[i]);
						}
					}
				}
			}
			else {
				*data_pointers = NULL;
			}
			talloc_free(aRow);
                }
	} else {
		memset(retvals, MAPI_E_INVALID_OBJECT, sizeof(enum MAPITAGS) * properties->cValues);
	}
}

_PUBLIC_ void **emsmdbp_object_get_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, enum MAPISTATUS **retvalsp)
{
        void **data_pointers;
        enum MAPISTATUS *retvals;
	bool mapistore;

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
		emsmdbp_object_get_properties_systemspecialfolder(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
	} else {
		mapistore = emsmdbp_is_mapistore(object);
		/* Nasty hack */
		if (!object) {
			mapistore = true;
		}

		switch (mapistore) {
		case false:
			switch (object->type) {
			case EMSMDBP_OBJECT_MAILBOX:
				emsmdbp_object_get_properties_mailbox(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			case EMSMDBP_OBJECT_FOLDER:
				emsmdbp_object_get_properties_systemspecialfolder(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			default:
				break;
			}
			break;
		case true:
			/* folder or messages handled by mapistore */
			emsmdbp_object_get_properties_mapistore(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
			break;
		}
	}

	if (retvalsp) {
		*retvalsp = retvals;
	}

        return data_pointers;
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
	uint16_t prop_type;
	struct emsmdbp_stream_data *stream_data;

	stream_data = talloc_zero(mem_ctx, struct emsmdbp_stream_data);
        stream_data->prop_tag = prop_tag;
	prop_type = prop_tag & 0xffff;
	if (prop_type == PT_STRING8) {
		stream_data->data.length = strlen(value) + 1;
		stream_data->data.data = value;
                (void) talloc_reference(stream_data, stream_data->data.data);
	}
	else if (prop_type == PT_UNICODE) {
		stream_data->data.length = strlen_m_ext((char *) value, CH_UTF8, CH_UTF16LE) * 2 + 2;
		stream_data->data.data = talloc_array(stream_data, uint8_t, stream_data->data.length);
		convert_string(CH_UTF8, CH_UTF16LE,
			       value, strlen(value),
			       stream_data->data.data, stream_data->data.length,
			       false);
		memset(stream_data->data.data + stream_data->data.length - 2, 0, 2 * sizeof(uint8_t));
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
								struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*synccontext_object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
	if (parent->type != EMSMDBP_OBJECT_FOLDER) return NULL;

	synccontext_object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!synccontext_object) return NULL;

	synccontext_object->object.synccontext = talloc_zero(synccontext_object, struct emsmdbp_object_synccontext);
	if (!synccontext_object->object.synccontext) {
		talloc_free(synccontext_object);
		return NULL;
	}

	synccontext_object->type = EMSMDBP_OBJECT_SYNCCONTEXT;

	synccontext_object->object.synccontext->folder = parent;
	(void) talloc_reference(synccontext_object->object.synccontext, parent);
        synccontext_object->object.synccontext->state_property = 0;
        synccontext_object->object.synccontext->state_stream.buffer.length = 0;
        synccontext_object->object.synccontext->state_stream.buffer.data = talloc_zero(synccontext_object->object.synccontext, uint8_t);
        synccontext_object->object.synccontext->stream.buffer.length = 0;
        synccontext_object->object.synccontext->stream.buffer.data = NULL;

	synccontext_object->object.synccontext->cnset_seen = talloc_zero(emsmdbp_ctx, struct idset);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, NULL, &synccontext_object->object.synccontext->cnset_seen->guid);
	synccontext_object->object.synccontext->cnset_seen->ranges = talloc_zero(synccontext_object->object.synccontext->cnset_seen, struct globset_range);
	synccontext_object->object.synccontext->cnset_seen->range_count = 1;
	synccontext_object->object.synccontext->cnset_seen->ranges->next = NULL;
	synccontext_object->object.synccontext->cnset_seen->ranges->prev = synccontext_object->object.synccontext->cnset_seen->ranges;
	synccontext_object->object.synccontext->cnset_seen->ranges->low = 0xffffffffffffffff;
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

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.ftcontext = talloc_zero(object, struct emsmdbp_object_ftcontext);
	if (!object->object.ftcontext) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FTCONTEXT;

	return object;
}
