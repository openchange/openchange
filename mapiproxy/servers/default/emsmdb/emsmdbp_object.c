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
#include "dcesrv_exchange_emsmdb.h"

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
	default:
		return "unknown";
	}
}


/**
   \details Convenient function to determine whether specified
   mapi_handles refers to object using mapistore or not

   \param handles pointer to the MAPI handle to lookup

   \return true if parent is using mapistore, otherwise false
 */
bool emsmdbp_is_mapistore(struct mapi_handles *handles)
{
	void			*data;
	struct emsmdbp_object	*object;

	/* Sanity checks - probably pointless */
	if (!handles) {
		return false;
	}

	mapi_handles_get_private_data(handles, &data);
	object = (struct emsmdbp_object *)data;

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

   \param handles pointer to the MAPI handle to lookup

   \return true if parent is within mailbox store, otherwise false
 */
bool emsmdbp_is_mailboxstore(struct mapi_handles *handles)
{
	void			*data;
	struct emsmdbp_object	*object;

	/* Sanity checks - irrelevant */

	mapi_handles_get_private_data(handles, &data);
	object = (struct emsmdbp_object *)data;

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

   \param handles pointer to the MAPI handle to lookup

   \return contextID value on success, otherwise -1
 */
uint32_t emsmdbp_get_contextID(struct mapi_handles *handles)
{
	void			*data;
	struct emsmdbp_object	*object;

	mapi_handles_get_private_data(handles, &data);
	object = (struct emsmdbp_object *) data;

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

#warning 'emsmdbp_commit_stream' is a tmp function to be moved somewhere...
static int emsmdbp_commit_stream(struct mapistore_context *mstore_ctx, struct emsmdbp_object_stream *stream)
{
	int rc;
        void *poc_backend_object;
        void *stream_data;
        uint8_t *stream_buffer, *utf8_buffer;
        struct Binary_r *binary_data;
        struct SRow aRow;
        off_t stream_size;

	rc = MAPISTORE_SUCCESS;
	if (stream->fd > -1) {
		if ((stream->flags & OpenStream_Create)) {
                        stream_size = lseek(stream->fd, 0, SEEK_END);
			lseek(stream->fd, 0, SEEK_SET);
                        if (stream->parent_poc_api) {
                                aRow.cValues = 1;
                                aRow.lpProps = talloc_zero(NULL, struct SPropValue);

                                stream_buffer = talloc_array(aRow.lpProps, uint8_t, stream_size + 1);
                                *(stream_buffer + stream_size) = 0;
                                stream_size = read(stream->fd, stream_buffer, stream_size);

                                if ((stream->property & PT_BINARY) == PT_BINARY) {
                                        binary_data = talloc(aRow.lpProps, struct Binary_r);
                                        binary_data->cb = stream_size;
                                        binary_data->lpb = stream_buffer;
                                        stream_data = binary_data;
                                }
                                else {
                                        utf8_buffer = talloc_array(stream_buffer, uint8_t, stream_size);
                                        memset(utf8_buffer, 0, stream_size);
                                        size_t convert_string(charset_t from, charset_t to,
                                                              void const *src, size_t srclen, 
                                                              void *dest, size_t destlen, bool allow_badcharcnv);

                                        convert_string(CH_UTF16BE, CH_UTF8,
                                                       stream_buffer, stream_size,
                                                       utf8_buffer, stream_size,
                                                       false);
                                        DEBUG(4, ("%s: no unicode conversion performed yet\n", __PRETTY_FUNCTION__));
                                        stream_data = utf8_buffer;
                                }
                                set_SPropValue_proptag (aRow.lpProps, stream->property, stream_data);
                                poc_backend_object = stream->parent_poc_backend_object;
                                rc =  mapistore_pocop_set_properties(mstore_ctx,
                                                                     stream->contextID, poc_backend_object, &aRow);
                                talloc_free(aRow.lpProps);
                        }
                        else {
                                rc = mapistore_set_property_from_fd(mstore_ctx,
                                                                    stream->contextID,
                                                                    stream->objectID,
                                                                    stream->objectType,
                                                                    stream->property,
                                                                    stream->fd);
                        }
		}
		close (stream->fd);
		stream->fd = -1;
	}
	else {
		rc = MAPISTORE_ERROR;
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
		ret = emsmdbp_commit_stream(object->mstore_ctx, object->object.stream);
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
	object->mstore_ctx = emsmdbp_ctx->mstore_ctx;
	object->object.mailbox = NULL;
	object->object.folder = NULL;
	object->object.message = NULL;
	object->object.stream = NULL;
	object->private_data = NULL;

        /* proof of concept */
	object->poc_api = false;
	object->poc_backend_object = NULL;

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
	const char			*displayName;
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
			displayName = ldb_msg_find_attr_as_string(res->msgs[0], "displayName", NULL);
			if (displayName) {
				object->object.mailbox->owner_Name = talloc_strdup(object->object.mailbox, 
										   displayName);

				/* Retrieve Mailbox folder identifier */
				openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, 
								object->object.mailbox->owner_Name,
								0x1, &object->object.mailbox->folderID);
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
   \param parent handle to the parent folder for this folder

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   uint64_t folderID,
							   struct mapi_handles *parent)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object		*object;
	char				*mapistore_uri = NULL;
	uint32_t			context_id;
	int				ret;

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
					       &mapistore_uri, object->object.folder->mailboxstore);

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
				ret = mapistore_add_context(emsmdbp_ctx->mstore_ctx, mapistore_uri, &context_id);
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


/**
   \details Initialize a table object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param parent pointer to the parent MAPI handle

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct mapi_handles *parent)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object;
	struct emsmdbp_object	*folder;
	void			*data = NULL;
	bool			mapistore = false;
	int			ret;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	/* Retrieve parent object */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) return NULL;
	folder = (struct emsmdbp_object *) data;

	/* Initialize table object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;
	
	object->object.table = talloc_zero(object, struct emsmdbp_object_table);
	if (!object->object.table) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_TABLE;
	object->object.table->folderID = folder->object.folder->folderID;
	object->object.table->prop_count = 0;
	object->object.table->properties = NULL;
	object->object.table->numerator = 0;
	object->object.table->denominator = 0;
	object->object.table->ulType = 0;
	object->object.table->mapistore = false;
	object->object.table->contextID = -1;
	object->object.table->subscription_list = NULL;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.table->mapistore = true;
		object->object.table->contextID = folder->object.folder->contextID;		
		ret = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, 
						      folder->object.folder->contextID);
	}

	return object;
}


/**
   \details Initialize a message object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param messageID the message identifier
   \param parent pointer to the parent MAPI handle

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_init(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    uint64_t messageID,
							    struct mapi_handles *parent)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object;
	struct emsmdbp_object	*folder;
	void			*data = NULL;
	bool			mapistore = false;
	int			ret;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	/* Retrieve parent object */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) return NULL;
	folder = (struct emsmdbp_object *) data;

	/* Initialize message object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.message = talloc_zero(object, struct emsmdbp_object_message);
	if (!object->object.message) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_MESSAGE;
	object->object.message->folderID = folder->object.folder->folderID;
	object->object.message->messageID = messageID;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.message->mapistore = true;
		object->object.message->contextID = folder->object.folder->contextID;		
		ret = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, 
						      folder->object.folder->contextID);
	} 

	return object;	
}


/**
   \details Initialize a stream object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param property the stream property identifier
   \param parent pointer to the parent MAPI handle
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_stream_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   uint32_t property,
							   enum OpenStream_OpenModeFlags flags,
							   struct mapi_handles *parent)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object, *parent_object;
	void			*data;
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	/* Retrieve parent object */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) return NULL;
        parent_object = data;
	
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.stream = talloc_zero(object, struct emsmdbp_object_stream);
	if (!object->object.stream) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_STREAM;
	object->object.stream->property = property;
	object->object.stream->flags = flags;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.stream->mapistore = true;
		object->object.stream->contextID = emsmdbp_get_contextID(parent);
		object->object.stream->fd = -1;
		object->object.stream->objectID = -1;
		object->object.stream->objectType = -1;
                if (parent_object->poc_api) {
                        object->object.stream->parent_poc_api = true;
                        object->object.stream->parent_poc_backend_object = parent_object->poc_backend_object;
                }
	}

	return object;
}


/**
   \details Initialize a attachment object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent pointer to the parent MAPI handle
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_attachment_init(TALLOC_CTX *mem_ctx,
                                                               struct emsmdbp_context *emsmdbp_ctx,
                                                               uint64_t messageID,
                                                               struct mapi_handles *parent)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object;
	void			*data;
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	/* Retrieve parent object */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) return NULL;
	
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
   \param parent pointer to the parent MAPI handle
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_subscription_init(TALLOC_CTX *mem_ctx,
                                                                 struct emsmdbp_context *emsmdbp_ctx,
                                                                 struct mapi_handles *parent)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object;
	void			*data;
	bool			mapistore = false;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	/* Retrieve parent object */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) return NULL;
	
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
		object->object.subscription->contextID = -1;
        }

	return object;
}
