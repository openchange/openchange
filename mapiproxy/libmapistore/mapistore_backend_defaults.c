/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Copyright (C) Julien Kerihuel 2011

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
#include "mapistore_backend.h"
#include "mapistore_common.h"

/**
   \file mapistore_backend_defaults.c

   \brief Initialize default behavior for mapistore backends
 */


static enum MAPISTORE_ERROR mapistore_op_defaults_init(void)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_create_context(struct mapistore_backend_context *mstoredb_ctx, 
								 const char *login_user, const char *username,
								 const char *uri, void **private_data)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_delete_context(void *private_data)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_release_record(void *private_data, uint64_t fmid, uint8_t type)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_get_path(void *private_data, uint64_t fmid, uint8_t type, char **path)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_mkdir(void *private_data, uint64_t parent_fid, 
							uint64_t fid, struct SRow *aRow)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_rmdir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_opendir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_closedir(void *private_data)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_readdir_count(void *private_data, uint64_t fid,
								enum MAPISTORE_TABLE_TYPE table_type,
								uint32_t *RowCount)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_get_table_property(void *private_data, uint64_t fid,
								     enum MAPISTORE_TABLE_TYPE table_type, 
								     uint32_t pos,
								     enum MAPITAGS proptag,
								     void **data)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_openmessage(void *private_data, uint64_t fid,
							      uint64_t mid,
							      struct mapistore_message *msg)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_createmessage(void *private_data, uint64_t fid,
								uint64_t mid)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_savechangesmessage(void *private_data, uint64_t mid,
								     uint8_t flags)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_submitmessage(void *private_data, uint64_t mid, uint8_t flags)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_deletemessage(void *private_data, uint64_t mid, 
								enum MAPISTORE_DELETION_TYPE deletion_type)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_get_fid_by_name(void *private_data, uint64_t parent_fid,
								  const char *foldername, uint64_t *fid)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_getprops(void *private_data, uint64_t fmid, uint8_t type,
							   struct SPropTagArray *SPropTagArray, struct SRow *aRow)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_setprops(void *private_data, uint64_t fmid, uint8_t type,
							   struct SRow *aRow)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_db_create_uri(TALLOC_CTX *mem_ctx, uint32_t index, const char *username,
								char **mapistore_uri)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_provision_namedprops(TALLOC_CTX *mem_ctx, char **ldif,
								       enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE *ntype)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


static enum MAPISTORE_ERROR mapistore_op_defaults_db_mkdir(void *private_data, enum MAPISTORE_DFLT_FOLDERS system_idx,
							   const char *mapistore_uri, const char *folder_name)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}


extern enum MAPISTORE_ERROR mapistore_backend_init_defaults(struct mapistore_backend *backend)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!backend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Backend information */
	backend->name = NULL;
	backend->description = NULL;
	backend->uri_namespace = NULL;

	/* Backend semantics */
	backend->init = mapistore_op_defaults_init;
	backend->create_context = mapistore_op_defaults_create_context;
	backend->delete_context = mapistore_op_defaults_delete_context;
	backend->release_record = mapistore_op_defaults_release_record;
	backend->get_path = mapistore_op_defaults_get_path;

	/* Folder semantics */
	backend->op_mkdir = mapistore_op_defaults_mkdir;
	backend->op_rmdir = mapistore_op_defaults_rmdir;
	backend->op_opendir = mapistore_op_defaults_opendir;
	backend->op_closedir = mapistore_op_defaults_closedir;
	backend->op_readdir_count = mapistore_op_defaults_readdir_count;
	backend->op_get_table_property = mapistore_op_defaults_get_table_property;
	backend->op_get_fid_by_name = mapistore_op_defaults_get_fid_by_name;

	/* Message semantics */
	backend->op_openmessage = mapistore_op_defaults_openmessage;
	backend->op_createmessage = mapistore_op_defaults_createmessage;
	backend->op_savechangesmessage = mapistore_op_defaults_savechangesmessage;
	backend->op_submitmessage = mapistore_op_defaults_submitmessage;
	backend->op_deletemessage = mapistore_op_defaults_deletemessage;

	
	/* Common semantics */
	backend->op_getprops = mapistore_op_defaults_getprops;
	backend->op_setprops = mapistore_op_defaults_setprops;


	/* MAPIStoreDB/Store semantics */
	backend->op_db_create_uri = mapistore_op_defaults_db_create_uri;
	backend->op_db_provision_namedprops = mapistore_op_defaults_provision_namedprops;
	backend->op_db_mkdir = mapistore_op_defaults_db_mkdir;

	return MAPISTORE_SUCCESS;
}
