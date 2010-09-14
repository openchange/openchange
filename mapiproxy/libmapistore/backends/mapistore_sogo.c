/*
   OpenChange Storage Abstraction Layer library
   SOGo backend

   OpenChange Project

   Copyright (C)

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

#include "mapistore_sogo.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>

/**
   \details Initialize sogo mapistore backend

   \return MAPISTORE_SUCCESS on success
 */
static int sogo_init(void)
{
	DEBUG(0, ("sogo backend initialized\n"));

	return MAPISTORE_SUCCESS;
}

/**
   \details Create a connection context to the sogo backend

   \param mem_ctx pointer to the memory context
   \param uri pointer to the sogo path
   \param private_data pointer to the private backend context 
 */
static int sogo_create_context(TALLOC_CTX *mem_ctx, const char *uri, void **private_data)
{
	DEBUG(0, ("[%s:%d]\n", __FUNCTION__, __LINE__));
	DEBUG(4, ("[%s:%d]: sogo uri: %s\n", __FUNCTION__, __LINE__, uri));

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a connection context from the sogo backend

   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_delete_context(void *private_data)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete data associated to a given folder or message

   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_release_record(void *private_data, uint64_t fmid, uint8_t type)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		break;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details return the mapistore path associated to a given message or
   folder ID

   \param private_data pointer to the current sogo context
   \param fmid the folder/message ID to lookup
   \param type whether it is a folder or message
   \param path pointer on pointer to the path to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static int sogo_get_path(void *private_data, uint64_t fmid,
			   uint8_t type, char **path)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		break;
	}

	return MAPISTORE_SUCCESS;
}

static int sogo_op_get_fid_by_name(void *private_data, uint64_t parent_fid, const char* foldername, uint64_t *fid)
{
	return MAPISTORE_ERR_NOT_FOUND;
}


/**
   \details Create a folder in the sogo backend
   
   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_op_mkdir(void *private_data, uint64_t parent_fid, uint64_t fid,
			   struct SRow *aRow)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a folder from the sogo backend

   \param private_data pointer to the current sogo context
   \param parent_fid the FID for the parent of the folder to delete
   \param fid the FID for the folder to delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_op_rmdir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	return MAPISTORE_SUCCESS;
}


/**
   \details Open a folder from the sogo backend

   \param private_data pointer to the current sogo context
   \param parent_fid the parent folder identifier
   \param fid the identifier of the colder to open

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_op_opendir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPISTORE_SUCCESS;
}


/**
   \details Close a folder from the sogo backend

   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_op_closedir(void *private_data)
{
	return MAPISTORE_SUCCESS;
}


/**
   \details Read directory content from the sogo backend

   \param private_data pointer to the current sogo context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int sogo_op_readdir_count(void *private_data, 
				   uint64_t fid,
				   uint8_t table_type,
				   uint32_t *RowCount)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		break;
	case MAPISTORE_MESSAGE_TABLE:
		break;
	default:
		break;
	}

	return MAPI_E_SUCCESS;
}


static int sogo_op_get_table_property(void *private_data,
					uint64_t fid,
					uint8_t table_type,
					uint32_t pos,
					uint32_t proptag,
					void **data)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		break;
	case MAPISTORE_MESSAGE_TABLE:
		break;
	default:
		break;
	}

	return MAPI_E_SUCCESS;
}


static int sogo_op_openmessage(void *private_data,
				 uint64_t fid,
				 uint64_t mid,
				 struct mapistore_message *msg)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPI_E_SUCCESS;
}


static int sogo_op_createmessage(void *private_data,
				   uint64_t fid,
				   uint64_t mid)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPI_E_SUCCESS;
}


static int sogo_op_savechangesmessage(void *private_data,
					uint64_t mid,
					uint8_t flags)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPI_E_SUCCESS;
}


static int sogo_op_submitmessage(void *private_data,
				   uint64_t mid,
				   uint8_t flags)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPI_E_SUCCESS;
}


static int sogo_op_getprops(void *private_data, 
			    uint64_t fmid, 
			    uint8_t type, 
			    struct SPropTagArray *SPropTagArray,
			    struct SRow *aRow)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		break;
	}

	return MAPI_E_SUCCESS;
}


static int sogo_op_setprops(void *private_data,
			      uint64_t fmid,
			      uint8_t type,
			      struct SRow *aRow)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		break;
	}

	return MAPI_E_SUCCESS;
}

static int sogo_op_deletemessage(void *private_data,
				   uint64_t mid,
				   uint8_t flags)
{
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	return MAPI_E_SUCCESS;
}

/**
   \details Entry point for mapistore SOGO backend

   \return MAPI_E_SUCCESS on success, otherwise MAPISTORE error
 */
int mapistore_init_backend(void)
{
	struct mapistore_backend	backend;
	int				ret;

	/* Fill in our name */
	backend.name = "sogo";
	backend.description = "mapistore sogo backend";
	backend.namespace = "sogo://";

	/* Fill in all the operations */
	backend.init = sogo_init;
	backend.create_context = sogo_create_context;
	backend.delete_context = sogo_delete_context;
	backend.release_record = sogo_release_record;
	backend.get_path = sogo_get_path;
	backend.op_mkdir = sogo_op_mkdir;
	backend.op_rmdir = sogo_op_rmdir;
	backend.op_opendir = sogo_op_opendir;
	backend.op_closedir = sogo_op_closedir;
	backend.op_readdir_count = sogo_op_readdir_count;
	backend.op_get_table_property = sogo_op_get_table_property;
	backend.op_openmessage = sogo_op_openmessage;
	backend.op_createmessage = sogo_op_createmessage;
	backend.op_savechangesmessage = sogo_op_savechangesmessage;
	backend.op_submitmessage = sogo_op_submitmessage;
	backend.op_getprops = sogo_op_getprops;
	backend.op_get_fid_by_name = sogo_op_get_fid_by_name;
	backend.op_setprops = sogo_op_setprops;
	backend.op_deletemessage = sogo_op_deletemessage;

	/* Register ourselves with the MAPISTORE subsystem */
	ret = mapistore_backend_register(&backend);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("Failed to register the '%s' mapistore backend!\n", backend.name));
		return ret;
	}

	return MAPISTORE_SUCCESS;
}
