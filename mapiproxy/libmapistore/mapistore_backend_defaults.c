/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011-2012

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

#include "mapistore_errors.h"
#include "mapistore.h"
#include "mapistore_private.h"

/**
   \file mapistore_backend_defaults.c

   \brief Initialize default behavior for mapistore backends
 */


static enum mapistore_error mapistore_op_defaults_init(void)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));	
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_list_contexts(const char *owner, struct tdb_wrap *indexing,
								TALLOC_CTX *mem_ctx, 
								struct mapistore_contexts_list **contexts_listp)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_create_context(TALLOC_CTX *mem_ctx, 
								 struct mapistore_connection_info *conn_info,
								 struct tdb_wrap *indexing_ctx,
								 const char *uri, void **ctx)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_create_root_folder(const char *username, 
								     enum mapistore_context_role ctx_role, 
								     uint64_t fid, 
								     const char *name,
								     TALLOC_CTX *mem_ctx,
								     char **mapistore_urip)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_path(void *ctx_obj, TALLOC_CTX *mem_ctx,
							   uint64_t fmid, char **path)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_root_folder(void *backend_object,
								  TALLOC_CTX *mem_ctx,
								  uint64_t fid,
								  void **root_folder_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_open_folder(void *folder_object,
							      TALLOC_CTX *mem_ctx,
							      uint64_t fid,
							      void **child_folder_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_create_folder(void *folder_object,
								TALLOC_CTX *mem_ctx,
								uint64_t fid,
								struct SRow *aRow,
								void **child_folder_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_delete_folder(void *folder_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_open_message(void *folder_object,
							       TALLOC_CTX *mem_ctx,
							       uint64_t mid,
							       bool rw,
							       void **message_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_create_message(void *folder_object,
								 TALLOC_CTX *mem_ctx,
								 uint64_t mid,
								 uint8_t associated,
								 void **message_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_delete_message(void *folder_object,
								 uint64_t mid,
								 uint8_t flags)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_move_copy_messages(void *target_folder,
								     void *source_folder,
                                                                     TALLOC_CTX *mem_ctx,
								     uint32_t mid_count,
								     uint64_t *source_mids,
								     uint64_t *target_mids,
								     struct Binary_r **target_change_keys,
								     uint8_t want_copy)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_deleted_fmids(void *folder_object,
								    TALLOC_CTX *mem_ctx,
								    enum mapistore_table_type table_type,
								    uint64_t change_num,
								    struct UI8Array_r **fmidsp,
								    uint64_t *cnp)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_child_count(void *folder_object,
								  enum mapistore_table_type table_type,
								  uint32_t *RowCount)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_open_table(void *folder_object,
							     TALLOC_CTX *mem_ctx,
							     enum mapistore_table_type table_type,
							     uint32_t handle_id,
							     void **table_object,
							     uint32_t *row_count)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_modify_permissions(void *folder_object,
								     uint8_t flags,
								     uint16_t pcount,
								     struct PermissionData *permissions)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_message_data(void *message_object,
								   TALLOC_CTX *mem_ctx,
								   struct mapistore_message **msg)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_modify_recipients(void *message_object,
								    struct SPropTagArray *columns,
								    uint16_t count,
								    struct mapistore_message_recipient *recipients)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_set_read_flag(void *message_object, uint8_t flag)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_save(void *message_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_submit(void *message_object, enum SubmitFlags flags)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_open_attachment(void *message_object,
								  TALLOC_CTX *mem_ctx,
								  uint32_t aid,
								  void **attachment_object)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_create_attachment(void *message_object,
								    TALLOC_CTX *mem_ctx,
								    void **attachment_object,
								    uint32_t *aid)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_attachment_table(void *message_object,
								       TALLOC_CTX *mem_ctx,
								       void **table_object,
								       uint32_t *row_count)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_open_embedded_message(void *message_object,
									TALLOC_CTX *mem_ctx,
									void **embedded_message_object,
									uint64_t *mid,
									struct mapistore_message **msg)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_available_properties(void *x_object,
									   TALLOC_CTX *mem_ctx,
									   struct SPropTagArray **propertiesp)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_set_columns(void *table_object,
							      uint16_t count,
							      enum MAPITAGS *properties)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_set_restrictions(void *table_object,
								   struct mapi_SRestriction *restrictions,
								   uint8_t *table_status)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_set_sort_order(void *table_object,
								 struct SSortOrderSet *sort_order,
								 uint8_t *table_status)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_row(void *table_object,
							  TALLOC_CTX *mem_ctx,
							  enum mapistore_query_type query_type,
							  uint32_t rowid,
							  struct mapistore_property_data **data)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_row_count(void *table_object,
								enum mapistore_query_type query_type,
								uint32_t *row_countp)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_handle_destructor(void *table_object,
								    uint32_t handle_id)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_get_properties(void *x_object,
								 TALLOC_CTX *mem_ctx,
								 uint16_t count,
								 enum MAPITAGS *properties,
								 struct mapistore_property_data *data)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_set_properties(void *x_object,
								 struct SRow *aRow)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

static enum mapistore_error mapistore_op_defaults_generate_uri(TALLOC_CTX *mem_ctx,
							       const char *username,
							       const char *folder,
							       const char *message,
							       const char *root_uri,
							       char **uri)
{
	DEBUG(3, ("[%s:%d] MAPISTORE defaults - MAPISTORE_ERR_NOT_IMPLEMENTED\n", __FUNCTION__, __LINE__));
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

extern enum mapistore_error mapistore_backend_init_defaults(struct mapistore_backend *backend)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!backend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Backend operations */
	backend->backend.name = NULL;
	backend->backend.description = NULL;
	backend->backend.namespace = NULL;
	backend->backend.init = mapistore_op_defaults_init;
	backend->backend.list_contexts = mapistore_op_defaults_list_contexts;
	backend->backend.create_context = mapistore_op_defaults_create_context;
	backend->backend.create_root_folder = mapistore_op_defaults_create_root_folder;

	/* context operations */
	backend->context.get_path = mapistore_op_defaults_get_path;
	backend->context.get_root_folder = mapistore_op_defaults_get_root_folder;

	/* oxcfold semantics */
	backend->folder.open_folder = mapistore_op_defaults_open_folder;
	backend->folder.create_folder = mapistore_op_defaults_create_folder;
	backend->folder.delete = mapistore_op_defaults_delete_folder;
	backend->folder.open_message = mapistore_op_defaults_open_message;
	backend->folder.create_message = mapistore_op_defaults_create_message;
	backend->folder.delete_message = mapistore_op_defaults_delete_message;
	backend->folder.move_copy_messages = mapistore_op_defaults_move_copy_messages;
	backend->folder.get_deleted_fmids = mapistore_op_defaults_get_deleted_fmids;
	backend->folder.get_child_count = mapistore_op_defaults_get_child_count;
	backend->folder.open_table = mapistore_op_defaults_open_table;
	backend->folder.modify_permissions = mapistore_op_defaults_modify_permissions;

	/* oxcmsg operations */
	backend->message.get_message_data = mapistore_op_defaults_get_message_data;
	backend->message.modify_recipients = mapistore_op_defaults_modify_recipients;
	backend->message.set_read_flag = mapistore_op_defaults_set_read_flag;
	backend->message.save = mapistore_op_defaults_save;
	backend->message.submit = mapistore_op_defaults_submit;
	backend->message.open_attachment = mapistore_op_defaults_open_attachment;
	backend->message.create_attachment = mapistore_op_defaults_create_attachment;
	backend->message.get_attachment_table = mapistore_op_defaults_get_attachment_table;
	backend->message.open_embedded_message = mapistore_op_defaults_open_embedded_message;

	/* oxctabl operations */
	backend->table.get_available_properties = mapistore_op_defaults_get_available_properties;
	backend->table.set_columns = mapistore_op_defaults_set_columns;
	backend->table.set_restrictions = mapistore_op_defaults_set_restrictions;
	backend->table.set_sort_order = mapistore_op_defaults_set_sort_order;
	backend->table.get_row = mapistore_op_defaults_get_row;
	backend->table.get_row_count = mapistore_op_defaults_get_row_count;
	backend->table.handle_destructor = mapistore_op_defaults_handle_destructor;

	/* oxcprpt operations */
	backend->properties.get_available_properties = mapistore_op_defaults_get_available_properties;
	backend->properties.get_properties = mapistore_op_defaults_get_properties;
	backend->properties.set_properties = mapistore_op_defaults_set_properties;

	/* manager operations */
	backend->manager.generate_uri = mapistore_op_defaults_generate_uri;

	return MAPISTORE_SUCCESS;
}
