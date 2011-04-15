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
   \param parent emsmdbp object of the parent folder for this folder

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   uint64_t folderID,
							   struct emsmdbp_object *parent)
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
                          ret = mapistore_add_context(emsmdbp_ctx->mstore_ctx, mapistore_uri, object->object.folder->folderID, &context_id);
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
   \param parent emsmdbp object of the parent

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;
	bool			mapistore = false;
	int			ret;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER) return NULL;

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

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.table->mapistore = true;
		object->object.table->contextID = parent->object.folder->contextID;		
		ret = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, 
						      parent->object.folder->contextID);
	}

	return object;
}

_PUBLIC_ void **emsmdbp_object_table_get_row_props(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, uint32_t row_id, uint32_t **retvalsp)
{
        void **data_pointers;
        enum MAPISTATUS retval;
        uint32_t *retvals;
        struct emsmdbp_object_table *table;
        struct mapistore_property_data *properties;
        uint32_t i, num_props;
	char *table_filter;

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
						if (properties[i].data == NULL)
							retvals[i] = MAPI_E_NOT_FOUND;
						else
							talloc_steal(data_pointers, properties[i].data);
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
		table_filter = talloc_asprintf(NULL, "(&(PidTagParentFolderId=0x%.16"PRIx64")(PidTagFolderId=*))", table->folderID);

		/* Lookup for flagged property row */
                retval = MAPI_E_SUCCESS;
		for (i = 0; retval != MAPI_E_INVALID_OBJECT && i < num_props; i++) {
			retval = openchangedb_get_table_property(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->oc_ctx, 
								 emsmdbp_ctx->szDisplayName,
								 table_filter, table->properties[i], 
								 table->numerator, data_pointers + i);
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
		talloc_free(table_filter);
	}

        if (retvalsp) {
                *retvalsp = retvals;
	}

        return data_pointers;
}

_PUBLIC_ void emsmdbp_object_table_fill_row_blob(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
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
                                            lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
                                            0x0000000b, (const void *)&flagged,
                                            table_row, 0, 0, 0);
        }
        else {
                libmapiserver_push_property(mem_ctx,
                                            lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
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

                libmapiserver_push_property(mem_ctx, lpcfg_iconv_convenience(emsmdbp_ctx->lp_ctx),
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


/**
   \details Initialize a stream object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param property the stream property identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_stream_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   uint32_t property,
							   enum OpenStream_OpenModeFlags flags,
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
	object->object.stream->property = property;
	object->object.stream->flags = flags;

	mapistore = emsmdbp_is_mapistore(parent);
	if (mapistore == true) {
		object->object.stream->mapistore = true;
		object->object.stream->contextID = emsmdbp_get_contextID(parent);
		object->object.stream->fd = -1;
		object->object.stream->objectID = -1;
		object->object.stream->objectType = -1;
                if (parent->poc_api) {
                        object->object.stream->parent_poc_api = true;
                        object->object.stream->parent_poc_backend_object = parent->poc_backend_object;
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
		talloc_free(object);
		return NULL;
        }

	return object;
}
