/*
   OpenChange Storage Abstraction Layer library
   MAPIStore FS / OCPF backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010-2011

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

#include "mapistore_fsocpf.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>

/**
   \details Initialize fsocpf mapistore backend

   \return MAPISTORE_SUCCESS on success
 */
static enum MAPISTORE_ERROR fsocpf_init(void)
{
	DEBUG(0, ("fsocpf backend initialized\n"));

	ocpf_init();
	
	return MAPISTORE_SUCCESS;
}


/**
   \details Allocate / initialize the fsocpf_context structure

   \param mem_ctx pointer to the memory context
   \param login_user the username used to authenticate
   \param username the username we want to impersonate
   \param uri pointer to the fsocpf path
   \param dir pointer to the DIR structure associated with the uri
 */
static struct fsocpf_context *fsocpf_context_init(TALLOC_CTX *mem_ctx,
						  const char *login_user,
						  const char *username,
						  const char *uri, 
						  DIR *dir)
{
	struct fsocpf_context	*fsocpf_ctx;

	fsocpf_ctx = talloc_zero(mem_ctx, struct fsocpf_context);
	if (!fsocpf_ctx) return NULL;

	fsocpf_ctx->private_data = NULL;
	fsocpf_ctx->login_user = talloc_strdup(fsocpf_ctx, login_user);
	fsocpf_ctx->username = talloc_strdup(fsocpf_ctx, username);
	fsocpf_ctx->uri = talloc_strdup(fsocpf_ctx, uri);
	fsocpf_ctx->dir = dir;
	fsocpf_ctx->folders = NULL;
	fsocpf_ctx->messages = NULL;

	return fsocpf_ctx;
}

static enum MAPISTORE_ERROR fsocpf_provision_namedprops(TALLOC_CTX *mem_ctx,
							char **ldif,
							enum MAPISTORE_NAMEDPROPS_PROVISION_TYPE *ntype)
{
	return MAPISTORE_ERR_NOT_IMPLEMENTED;
}

/**
   \details Generate a mapistore URI for root (system/special) folders

   \param private_data pointer to the private data holding the fsocpf_context structure
   \param index the folder index for which to create the mapistore URI
   \param username the username for which to create the mapistore URI
   \param mapistore_uri pointer on pointer to the string to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR fsocpf_create_mapistore_uri(TALLOC_CTX *mem_ctx,
							uint32_t index,
							const char *username,
							char **mapistore_uri)
{
	uint32_t			i;

	for (i = 0; dflt_folders[i].name; i++) {
		if (dflt_folders[i].index == index) {
			*mapistore_uri = talloc_asprintf(mem_ctx, "fsocpf://%s/%s/%s", mapistore_get_mapping_path(), username, dflt_folders[i].name);
			DEBUG(5, ("* [%s:%d][%s]: URI = %s\n", __FILE__, __LINE__, __FUNCTION__, *mapistore_uri));
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}


/**
   \details Allocate / initialize a fsocpf_folder_list element

   This essentially creates a node of the linked list, which will be added to the list
   by the caller

   \param mem_ctx pointer to the memory context
   \param uri pointer to the fsocpf path for the folder
   \param dir pointer to the DIR structure associated with the fid / uri
 */
static struct fsocpf_folder_list *fsocpf_folder_list_element_init(TALLOC_CTX *mem_ctx, 
								  const char *uri, 
								  DIR *dir)
{
	struct fsocpf_folder_list *el;

	el = talloc_zero(mem_ctx, struct fsocpf_folder_list);
	if (!el) return NULL;

	el->folder = talloc_zero((TALLOC_CTX *)el, struct fsocpf_folder);
	if (!el->folder) {
		talloc_free(el);
		return NULL;
	}

	el->folder->uri = talloc_strdup((TALLOC_CTX *)el, uri);
	el->folder->dir = dir;

	return el;
}

/**
   \details Search for the fsocpf_folder for a given folder URI

   \param fsocpf_ctx the fsocpf context
   \param uri the folder uri

   \return folder on success or NULL if the folder was not found
 */
static struct fsocpf_folder *fsocpf_find_folder(struct fsocpf_context *fsocpf_ctx,
						const char *uri)
{
	struct fsocpf_folder_list	*el;
	const char			*path = NULL;

	/* Sanity checks */
	if (!fsocpf_ctx) return NULL;
	if (!uri) return NULL;

	if (mapistore_strip_ns_from_uri(uri, &path) != MAPISTORE_SUCCESS) {
		/* assume its already stripped */
		path = uri;
	}
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "finding %s\n", path);
	for (el = fsocpf_ctx->folders; el; el = el->next) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "el->folder->uri: %s\n", el->folder->uri);
		if (el->folder && el->folder->uri && !strcmp(path, el->folder->uri)) {
			return el->folder;
		}
	}

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "%s not found\n", path);

	return NULL;
}


/**
   \details Allocate / initialize a fsocpf_message_list element

   This essentialy creates a node of the linked list, which will be
   added to the list by the caller

   \param mem_ctx pointer to the memory context
   \param folder_uri the folder uri for the message
   \param uri pointer to the fsocpf path for the message
   \param context_id the ocpf context identifier

   \return Pointer to allocated folder list on success, otherwise NULL
 */
static struct fsocpf_message_list *fsocpf_message_list_element_init(TALLOC_CTX *mem_ctx,
								    const char *folder_uri,
								    const char *uri,
								    uint32_t context_id)
{
	struct fsocpf_message_list	*el;

	/* Sanity checks */
	if (!folder_uri) return NULL;
	if (!uri) return NULL;
	if (!context_id) return NULL;

	el = talloc_zero(mem_ctx, struct fsocpf_message_list);
	if (!el) return NULL;

	el->message = talloc_zero((TALLOC_CTX *)el, struct fsocpf_message);
	if (!el->message) return NULL;

	el->message->folder_uri = talloc_strdup(el->message, uri);
	el->message->uri = talloc_strdup((TALLOC_CTX *)el->message, uri);
	el->message->ocpf_context_id = context_id;

	return el;
}


/**
   \details search for the fsocpf_message for a given message URI

   \param fsocpf_ctx the store context
   \param uri the message URI of the fsocpf_message to search for

   \return message on success, or NULL if the message was not found
 */
static struct fsocpf_message *fsocpf_find_message(struct fsocpf_context *fsocpf_ctx,
						  const char *uri)
{
	struct fsocpf_message_list	*el;

	/* Sanity checks */
	if (!fsocpf_ctx) return NULL;
	if (!uri) return NULL;

	for (el = fsocpf_ctx->messages; el; el = el->next) {
		if (el->message && el->message->uri && !strcmp(el->message->uri, uri)) {
			return el->message;
		}
	}

	return NULL;
}


/**
   \details search for the fsocpf_message_list for a given message URI

   \param fsocpf_ctx the store context
   \param uri the message URI of the fsocpf_message to search for

   \return point to message list on success, or NULL if the message was not found
 */
static struct fsocpf_message_list *fsocpf_find_message_list(struct fsocpf_context *fsocpf_ctx,
							    const char *uri)
{
	struct fsocpf_message_list	*el;

	/* Sanity checks */
	if (!fsocpf_ctx) return NULL;
	if (!fsocpf_ctx->messages) return NULL;
	if (!uri) return NULL;

	for (el = fsocpf_ctx->messages; el; el = el->next) {
		if (el->message && el->message->uri && !strcmp(el->message->uri, uri)) {
			return el;
		}
	}

	return NULL;
}


/**
   \details Create a connection context to the fsocpf backend

   \param mem_ctx pointer to the memory context
   \param login_user the username used for authentication
   \param username the username we want to impersonate
   \param uri pointer to the fsocpf path
   \param private_data pointer to the private backend context 
 */
static enum MAPISTORE_ERROR fsocpf_create_context(struct mapistore_backend_context *mstoredb_ctx, 
						  const char *login_user, 
						  const char *username,
						  const char *uri, 
						  void **private_data)
{
	TALLOC_CTX			*mem_ctx;
	DIR				*top_dir;
	DIR				*root_dir;
	char				*root_uri;
	char				*tmp;
	struct fsocpf_context		*fsocpf_ctx;
	struct fsocpf_folder_list	*el;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!mstoredb_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!login_user, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!private_data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Creating context for URI: %s\n", uri);

	mem_ctx = (TALLOC_CTX *) mstoredb_ctx;

	/* Step 1. Try to open root context directory (username based) */
	tmp = strstr(uri, username);
	MAPISTORE_RETVAL_IF(!tmp, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	root_uri = talloc_strndup(mem_ctx, uri, strlen(uri) - strlen(tmp) + strlen(username));
	MAPISTORE_RETVAL_IF(!root_uri, MAPISTORE_ERR_NO_MEMORY, NULL);

	root_dir = opendir(root_uri);
	if (!root_dir) {
		/* If it doesn't exist, try to create it */
		if (mkdir(root_uri, S_IRWXU) != 0) {
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to create mapistore root folder: %s (%s)\n",
					   root_uri, strerror(errno));
			talloc_free(root_uri);
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
	} else {
		closedir(root_dir);
	}
	talloc_free(root_uri);

	/* Step 1. Try to open context directory */
	top_dir = opendir(uri);
	if (!top_dir) {
		/* If it doesn't exist, try to create it */
		if (mkdir(uri, S_IRWXU) != 0 ) {
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to create fsocpf folder: %s\n", strerror(errno));
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		top_dir = opendir(uri);
		if (!top_dir) {
			MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unable to open folder: %s\n", strerror(errno));
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
	}

	/* Step 2. Allocate / Initialize the fsocpf context structure */
	fsocpf_ctx = fsocpf_context_init(mem_ctx, login_user, username, uri, top_dir);
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	/* Create the entry in the list for top mapistore folders */
	el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, uri, top_dir);
	MAPISTORE_RETVAL_IF(!el, MAPISTORE_ERR_NO_MEMORY, NULL);

	DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Folder added to the list: %s\n", el->folder->uri);

	/* Step 3. Store fsocpf context within the opaque private_data pointer */
	*private_data = (void *)fsocpf_ctx;

	DEBUG(0, ("%s has been opened\n", uri));
	{
		struct dirent *curdir;
		int j = 0;

		while ((curdir = readdir(fsocpf_ctx->dir)) != NULL) {
			DEBUG(0, ("%d: readdir: %s\n", j, curdir->d_name));
			j++;
		}
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a connection context from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum MAPISTORE_ERROR fsocpf_delete_context(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	if (!fsocpf_ctx) {
		return MAPISTORE_SUCCESS;
	}

	if (fsocpf_ctx->dir) {
		closedir(fsocpf_ctx->dir);
		fsocpf_ctx->dir = NULL;
	}

	talloc_free(fsocpf_ctx);
	fsocpf_ctx = NULL;

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete data associated to a given folder or message

   \param private_data pointer to the current fsocpf context
   \param uri the URI to lookup for release
   \param type the type of the element to release

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum MAPISTORE_ERROR fsocpf_release_record(void *private_data, const char *uri, uint8_t type)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message_list	*message;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message_list(fsocpf_ctx, uri);
		if (message && message->message) {
			if (message->message->ocpf_context_id) {
				ocpf_del_context(message->message->ocpf_context_id);
			}
			DLIST_REMOVE(fsocpf_ctx->messages, message);
			talloc_free(message);
		}
		break;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details return the mapistore path associated to a given message or
   folder ID

   \param private_data pointer to the current fsocpf context
   \param uri the folder/message URI to lookup
   \param type whether it is a folder or message
   \param path pointer on pointer to the path to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum MAPISTORE_ERROR fsocpf_get_path(void *private_data, const char *uri,
					    uint8_t type, char **path)
{
	struct fsocpf_folder	*folder;
	struct fsocpf_message	*message;
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	switch (type) {
	case MAPISTORE_FOLDER:
		folder = fsocpf_find_folder(fsocpf_ctx, uri);
		if (!folder) {
			DEBUG(0, ("folder doesn't exist ...\n"));
			*path = NULL;
			return MAPISTORE_ERROR;
		}
		DEBUG(0, ("folder->path is %s\n", folder->uri));
		*path = folder->uri;
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message(fsocpf_ctx, uri);
		if (!message) {
			DEBUG(0, ("message doesn't exist ...\n"));
			*path = NULL;
			return MAPISTORE_ERROR;
		}
		DEBUG(0, ("message->uri is %s\n", message->uri));
		*path = message->uri;
		break;
	default:
		DEBUG(0, ("[%s]: Invalid type %d\n", __FUNCTION__, type));
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}

static enum MAPISTORE_ERROR fsocpf_op_get_uri_by_name(void *private_data, 
						      const char *parent_uri, 
						      const char *foldername, 
						      char **uri)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder 	*folder;
	struct dirent		*curdir;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!parent_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!foldername, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Lookup the parent folder from its URI */
	folder = fsocpf_find_folder(fsocpf_ctx, parent_uri);
	if (!folder) {
		return MAPISTORE_ERROR;
	}

	/* Iterate over the contents of the parent folder, searching for a matching name */
	rewinddir(folder->dir);
	while ((curdir = readdir(folder->dir)) != NULL) {
		if ((curdir->d_type == DT_DIR) && (strcmp(curdir->d_name, foldername) == 0)) {
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "folder name %s found\n", foldername);
			*uri = talloc_asprintf((TALLOC_CTX *)fsocpf_ctx, "%s/%s", folder->uri, curdir->d_name);
			return MAPISTORE_SUCCESS;
		}
	}
	return MAPISTORE_ERR_NOT_FOUND;
}

/**
  \details Set the properties for a folder
  
  \param folder_uri the folder full patch
  \param aRow the properties to set on the folder
  
  \return MAPISTORE_SUCCESS on success, otherwise a MAPISTORE_ERROR value.
*/
/* FIXME: We can't use FID anymore in ocpf_write_init */
static enum MAPISTORE_ERROR fsocpf_set_folder_props(const char *folder_uri, struct SRow *aRow)
{
	TALLOC_CTX			*mem_ctx;
	struct mapi_SPropValue_array	mapi_lpProps;
	uint32_t			ocpf_context_id;
	char				*propfile;
	uint32_t			i;
	struct stat             	sb;

	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);
	
	/* Create the .properties file */
	propfile = talloc_asprintf(mem_ctx, "%s/.properties", folder_uri);
	if (stat(propfile, &sb) == -1) {
		ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_CREATE);
		/* Create the array of mapi properties */
		mapi_lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, aRow->cValues);
		mapi_lpProps.cValues = aRow->cValues;
		for (i = 0; i < aRow->cValues; i++) {
			cast_mapi_SPropValue((TALLOC_CTX *)mapi_lpProps.lpProps,
					      &(mapi_lpProps.lpProps[i]), &(aRow->lpProps[i]));
		}
	} else {
		uint32_t num_props = 0;
		struct SPropValue *original_props;
		int i = 0;
		ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_RDWR);
		/* get all the old folder properties */
		ocpf_parse(ocpf_context_id); /* TODO: check return type */
		ocpf_server_set_SPropValue(mem_ctx, ocpf_context_id);
		original_props = ocpf_get_SPropValue(ocpf_context_id, &num_props);
		/* add new and update existing properties */
		for (i = 0; i < aRow->cValues; ++i) {
			int j = 0;
			bool found = false;
			for (j = 0; j < num_props; ++j) {
				if (aRow->lpProps[i].ulPropTag == original_props[j].ulPropTag) {
					original_props[j].value = aRow->lpProps[i].value;
					found = true;
				}
			}
			if ( ! found) {
				num_props += 1;
				original_props = talloc_realloc(NULL, original_props, struct SPropValue, num_props);
				original_props[num_props -1].ulPropTag = aRow->lpProps[i].ulPropTag;
				original_props[num_props -1].value = aRow->lpProps[i].value;
			}
		}
		/* convert the output to the right format */
		mapi_lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, num_props);
		mapi_lpProps.cValues = num_props;
		for (i = 0; i < num_props; i++) {
			cast_mapi_SPropValue((TALLOC_CTX *)mapi_lpProps.lpProps,
					      &(mapi_lpProps.lpProps[i]), &(original_props[i]));
		}
	}

	/* ocpf_write_init(ocpf_context_id, fid); */
	ocpf_clear_props(ocpf_context_id);
	ocpf_write_init(ocpf_context_id, 0);
	ocpf_write_auto(ocpf_context_id, NULL, &mapi_lpProps);
	ocpf_write_commit(ocpf_context_id);
	ocpf_del_context(ocpf_context_id);
	
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}

/**
   \details Create a folder in the fsocpf backend
   
   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum MAPISTORE_ERROR fsocpf_op_mkdir(void *private_data,
					    const char *_parent_uri,
					    const char *folder_name,
					    const char *folder_desc,
					    enum FOLDER_TYPE folder_type,
					    char **folder_uri)
{
	enum MAPISTORE_ERROR		retval;
	struct SRow			aRow;
	TALLOC_CTX			*mem_ctx;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*folder;
	const char			*parent_uri;
	char				*newfolder;
	char				*dummy_uri;
	struct fsocpf_folder_list	*newel;
	DIR				*dir;
	int				ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!_parent_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!folder_name, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Step 0. Check if it already exists */
	retval = mapistore_strip_ns_from_uri(_parent_uri, &parent_uri);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	if (fsocpf_op_get_uri_by_name(private_data, parent_uri, folder_name, &dummy_uri) == MAPISTORE_SUCCESS) {
		/* already exists */
		talloc_free(dummy_uri);
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_INFO, "Cannot create folder '%s' in directory'%s', that name already exists\n", folder_name, parent_uri);
		return MAPISTORE_ERR_EXIST;
	}

	/* Step 1. Search for the parent fid */
	folder = fsocpf_find_folder(fsocpf_ctx, parent_uri);
	if (!folder) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Parent context not found for folder '%s'\n", parent_uri);
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);

	/* Step 2. Stringify fid and create directory */
	newfolder = talloc_asprintf(mem_ctx, "%s/%s", folder->uri, folder_name);
	ret = mkdir(newfolder, 0700);
	if (ret) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "mkdir failed: %s\n", strerror(errno));
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}
	dir = opendir(newfolder);
	
	/* Step 3. Add this folder to the list of ones we know about */
	newel = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, newfolder, dir);
	MAPISTORE_RETVAL_IF(!newel, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	DLIST_ADD_END(fsocpf_ctx->folders, newel, struct fsocpf_folder_list *);
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Element added to the list '%s'\n", folder_name);

	/* Step 4. Set the properties on the folder */

	if (folder_desc) {
		aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, 3);
		aRow.cValues = 2;
		aRow.lpProps[0].ulPropTag = PidTagDisplayName;
		aRow.lpProps[0].value.lpszW = folder_name;
		aRow.lpProps[1].ulPropTag = PidTagComment;
		aRow.lpProps[1].value.lpszW = folder_desc;

	} else {
		aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, 2);
		aRow.cValues = 1;
		aRow.lpProps[0].ulPropTag = PidTagDisplayName;
		aRow.lpProps[0].value.lpszW = folder_name;
	}

	retval = fsocpf_set_folder_props(newfolder, &aRow);

	/* Step 5. Generate the URI for the new created folder */
	*folder_uri = talloc_asprintf((TALLOC_CTX *)fsocpf_ctx, "fsocpf://%s", newfolder);

	talloc_free(aRow.lpProps);
	talloc_free(newfolder);
	talloc_free(mem_ctx);

	return retval;
}


/**
   \details Delete a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context
   \param parent_uri the URI for the parent of the folder to delete
   \param folder_uri the URI for the folder to delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
/* FIXME: there's no check to ensure parent_uri is the folder_uri parent ... */
static enum MAPISTORE_ERROR fsocpf_op_rmdir(void *private_data, const char *parent_uri, const char *folder_uri)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*parent;
	struct fsocpf_folder		*folder;
	const char 			*folder_path = NULL;
	char				*propertiespath;
	TALLOC_CTX			*mem_ctx;
	int				ret;
	struct fsocpf_folder_list	*el;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!parent_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mapistore_strip_ns_from_uri(folder_uri, &folder_path) != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_CRITICAL, "misformed folder_uri: %s\n", folder_uri);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_LOW, "Deleting %s from %s\n", folder_path, parent_uri);

	/* Step 1. Search for the parent fid */
	parent = fsocpf_find_folder(fsocpf_ctx, parent_uri);
	MAPISTORE_RETVAL_IF(!parent, MAPISTORE_ERR_NO_DIRECTORY, NULL);

	/* Step 2. Search for the folder element */
	folder = fsocpf_find_folder(fsocpf_ctx, folder_path);
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NO_DIRECTORY, NULL);

	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);

	/* Step 3. Remove .properties file */
	propertiespath = talloc_asprintf(mem_ctx, "%s/.properties", folder_path);
	ret = unlink(propertiespath);
	if (ret) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Unlink failed with error '%s'\n", strerror(errno));
	}

	/* Step 4. Delete directory */
	ret = rmdir(folder_path);
	if (ret) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "rmdir failed with error '%s'\n", strerror(errno));
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}
	for (el = fsocpf_ctx->folders; el; el = el->next) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "el->folder->uri: %s\n", el->folder->uri);
		if (el->folder && el->folder->uri && !strcmp(folder_path, el->folder->uri)) {
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "removing: %s\n", el->folder->uri);
			DLIST_REMOVE(fsocpf_ctx->folders, el);
		}
	}
	 
	return MAPISTORE_SUCCESS;
}


/**
   \details Open a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context
   \param parent_uri the parent folder URI
   \param folder_uri the URI of the folder to open

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
/* FIXME: curdir->d_name is NOT the full path */
static enum MAPISTORE_ERROR fsocpf_op_opendir(void *private_data, const char *parent_uri, const char *folder_uri)
{
	TALLOC_CTX			*mem_ctx;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*folder;
	struct fsocpf_folder_list	*el;
	struct fsocpf_folder_list	*newel;
	struct dirent			*curdir;
	DIR				*dir;
	const char			*folder_path = NULL;
	const char			*parent_folder_path = NULL;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!parent_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mapistore_strip_ns_from_uri(folder_uri, &folder_path) != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_CRITICAL, "misformed folder_uri: %s\n", folder_uri);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	if (mapistore_strip_ns_from_uri(parent_uri, &parent_folder_path) != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_CRITICAL, "misformed parent_uri: %s\n", folder_uri);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	/* Step 0. If fid equals top folder fid, it is already open */
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Have context uri %s, looking for %s\n", fsocpf_ctx->uri, folder_path);
	if (!strcmp(fsocpf_ctx->uri, folder_path)) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders) {
			el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fsocpf_ctx->uri, fsocpf_ctx->dir);
			MAPISTORE_RETVAL_IF(!el, MAPISTORE_ERR_NO_MEMORY, NULL);

			DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Folder added to the list '%s'\n", el->folder->uri);
		}

		folder = fsocpf_find_folder(fsocpf_ctx, folder_path);
		MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NO_DIRECTORY, NULL);

		return MAPISTORE_SUCCESS;
	} else {
		/* Step 1. Search for the parent fid */
		folder = fsocpf_find_folder(fsocpf_ctx, parent_folder_path);
		MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NO_DIRECTORY, NULL);
	}

	mem_ctx = talloc_named(NULL, 0, __FUNCTION__);

	/* Read the directory and search for the fid to open */
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Looking for '%s'\n", folder_path);
	rewinddir(folder->dir);
	errno = 0;
	{
		int i = 0;
		while ((curdir = readdir(folder->dir)) != NULL) {
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "[%d]: readdir: %s\n", i, curdir->d_name);
			i++;
			if (curdir->d_name && !strcmp(curdir->d_name, folder_path)) {
				dir = opendir(folder_path);
				MAPISTORE_RETVAL_IF(!dir, MAPISTORE_ERR_CONTEXT_FAILED, mem_ctx);

				MSTORE_DEBUG_INFO(MSTORE_LEVEL_PEDANTIC, "%s\n", "Folder found");

				newel = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, folder_path, dir);
				MAPISTORE_RETVAL_IF(!newel, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

				DLIST_ADD_END(fsocpf_ctx->folders, newel, struct fsocpf_folder_list *);
				MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Element added to the list: %s\n", folder_path);
			}
		}
	}

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_PEDANTIC, "errno = %d\n", errno);

	rewinddir(folder->dir);
	talloc_free(mem_ctx);
		
	return MAPISTORE_SUCCESS;
}


/**
   \details Close a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context
   \param folder_uri the URI of the folder to close

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum MAPISTORE_ERROR fsocpf_op_closedir(void *private_data, const char *folder_uri)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}
	/* TODO: this probably should be implemented */

	return MAPISTORE_SUCCESS;
}


/**
   \details Read directory content from the fsocpf backend

   \param private_data pointer to the current fsocpf context
   \param folder_uri pointer to the URI of the folder
   \param table_type the type of table to read
   \param RowCount pointer to the number of rows to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static enum MAPISTORE_ERROR fsocpf_op_readdir_count(void *private_data, 
						    const char *folder_uri,
						    enum MAPISTORE_TABLE_TYPE table_type,
						    uint32_t *RowCount)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*folder;
	struct fsocpf_folder_list	*el;
	struct dirent			*curdir;
	const char			*folder_path = NULL;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!RowCount, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mapistore_strip_ns_from_uri(folder_uri, &folder_path) != MAPISTORE_SUCCESS) {
		/* assume its already stripped */
		folder_path = folder_uri;
	}

	if (!strcmp(fsocpf_ctx->uri, folder_path)) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders) {
			el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fsocpf_ctx->uri, fsocpf_ctx->dir);
			MAPISTORE_RETVAL_IF(!el, MAPISTORE_ERR_NO_MEMORY, NULL);

			DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Element added to the list '%s'\n", el->folder->uri);
		}
	}

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder(fsocpf_ctx, folder_path);
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NO_DIRECTORY, NULL);

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		rewinddir(folder->dir);
		errno = 0;
		*RowCount = 0;
		while ((curdir = readdir(folder->dir)) != NULL) {
			if (curdir->d_name && curdir->d_type == DT_DIR &&
			    strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..")) {
				MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Adding folder entry to RowCount: '%s'\n", curdir->d_name);
				*RowCount += 1;
			}
		}
		break;
	case MAPISTORE_MESSAGE_TABLE:
		rewinddir(folder->dir);
		errno = 0;
		*RowCount = 0;
		while ((curdir = readdir(folder->dir)) != NULL) {
			if (curdir->d_name && curdir->d_type == DT_REG &&
			    strcmp(curdir->d_name, ".properties")) {
				MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Adding message entry to RowCount: '%s'\n", curdir->d_name);
				*RowCount += 1;
			}
		}
		break;
	default:
		break;
	}

	return MAPISTORE_SUCCESS;
}


/* FIXME: We don't use FID anymore, nor do backends should know about them */
static enum MAPISTORE_ERROR fsocpf_get_property_from_folder_table(struct fsocpf_folder *folder,
								  uint32_t pos,
								  enum MAPITAGS proptag,
								  void **data)
{
	int			ret;
	struct dirent		*curdir;
	uint32_t		counter = 0;
	char			*folderID = NULL;
	char			*propfile;
	uint32_t		cValues = 0;
	struct SPropValue	*lpProps;
	uint32_t		ocpf_context_id;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Set dir listing to current position */
	rewinddir(folder->dir);
	errno = 0;
	while ((curdir = readdir(folder->dir)) != NULL) {
		if (curdir->d_name && curdir->d_type == DT_DIR &&
		    strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..") &&
		    counter == pos) {
			folderID = talloc_strdup(folder, curdir->d_name);
			break;
		}
		if (strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..") && 
		    (curdir->d_type == DT_DIR)) {
			counter++;
		}
	}

	MAPISTORE_RETVAL_IF(!folderID, MAPISTORE_ERR_NOT_FOUND, NULL);

	/* Otherwise opens .properties file with ocpf for fid entry */
	propfile = talloc_asprintf(folder, "%s/%s/.properties", folder->uri, folderID);
	talloc_free(folderID);

	ret = ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_READ);
	talloc_free(propfile);

	/* process the file */
	ret = ocpf_parse(ocpf_context_id);

	ocpf_server_set_SPropValue(folder, ocpf_context_id);
	lpProps = ocpf_get_SPropValue(ocpf_context_id, &cValues);

	/* FIXME: We need to find a proper way to handle this (for all types) */
	talloc_steal(folder, lpProps);

	*data = (void *) get_SPropValue(lpProps, proptag);
	if (((proptag & 0xFFFF) == PT_STRING8) || ((proptag & 0xFFFF) == PT_UNICODE)) {
		/* Hack around PT_STRING8 and PT_UNICODE */
		if (*data == NULL && ((proptag & 0xFFFF) == PT_STRING8)) {
		  *data = (void *) get_SPropValue(lpProps, (enum MAPITAGS)(((int)proptag & 0xFFFF0000) + PT_UNICODE));
		} else if (*data == NULL && (proptag & 0xFFFF) == PT_UNICODE) {
		  *data = (void *) get_SPropValue(lpProps, (enum MAPITAGS)((int)(proptag & 0xFFFF0000) + PT_STRING8));
		}
		*data = talloc_strdup(folder, (char *)*data);
	}

	if (*data == NULL) {
		ret = ocpf_del_context(ocpf_context_id);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	ret = ocpf_del_context(ocpf_context_id);
	return MAPISTORE_SUCCESS;
}


static enum MAPISTORE_ERROR fsocpf_get_property_from_message_table(struct fsocpf_folder *folder,
								   uint32_t pos,
								   enum MAPITAGS proptag,
								   void **data)
{
	int			ret;
	struct dirent		*curdir;
	uint32_t		counter = 0;
	char			*messageID = NULL;
	char			*propfile;
	uint32_t		cValues = 0;
	struct SPropValue	*lpProps;
	uint32_t		ocpf_context_id;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Set dir listing to current position */
	rewinddir(folder->dir);
	errno = 0;
	while ((curdir = readdir(folder->dir)) != NULL) {
		if (curdir->d_name && curdir->d_type == DT_REG &&
		    strcmp(curdir->d_name, ".properties") && counter == pos) {
			messageID = talloc_strdup(folder, curdir->d_name);
			break;
		}
		if (strcmp(curdir->d_name, ".properties") && 
		    strcmp(curdir->d_name, ".") &&
		    strcmp(curdir->d_name, "..") &&
		    (curdir->d_type == DT_REG)) {
			counter++;
		}
	}

	MAPISTORE_RETVAL_IF(!messageID, MAPISTORE_ERR_NOT_FOUND, NULL);

	/* if fid, return folder fid */
	/* if (proptag == PR_FID) { */
	/* 	*data = (uint64_t *)&folder->fid; */
	/* 	return MAPISTORE_SUCCESS; */
	/*   } */

	/* If mid, return curdir->d_name */
	/* if (proptag == PR_MID) { */
	/* 	uint64_t	*mid; */

	/* 	mid = talloc_zero(folder, uint64_t); */
	/* 	*mid = strtoull(messageID, NULL, 16); */
	/* 	talloc_free(messageID); */
	/* 	*data = (uint64_t *)mid; */
	/* 	return MAPISTORE_SUCCESS; */
	/* } */

	/* Otherwise opens curdir->d_name file with ocpf */
	propfile = talloc_asprintf(folder, "%s/%s", folder->uri, messageID);
	talloc_free(messageID);

	ret = ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_READ);
	talloc_free(propfile);

	/* process the file */
	ret = ocpf_parse(ocpf_context_id);

	ocpf_server_set_SPropValue(folder, ocpf_context_id);
	lpProps = ocpf_get_SPropValue(ocpf_context_id, &cValues);

	/* FIXME: We need to find a proper way to handle this (for all types) */
	talloc_steal(folder, lpProps);

	*data = (void *) get_SPropValue(lpProps, proptag);
	if (((proptag & 0xFFFF) == PT_STRING8) || ((proptag & 0xFFFF) == PT_UNICODE)) {
		/* Hack around PT_STRING8 and PT_UNICODE */
		if (*data == NULL && ((proptag & 0xFFFF) == PT_STRING8)) {
		  *data = (void *) get_SPropValue(lpProps, (enum MAPITAGS)(((int)proptag & 0xFFFF0000) + PT_UNICODE));
		} else if (*data == NULL && (proptag & 0xFFFF) == PT_UNICODE) {
		  *data = (void *) get_SPropValue(lpProps, (enum MAPITAGS)(((int)proptag & 0xFFFF0000) + PT_STRING8));
		}
		*data = talloc_strdup(folder, (char *)*data);
	}

	if (*data == NULL) {
		ocpf_del_context(ocpf_context_id);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	ocpf_del_context(ocpf_context_id);
	return MAPISTORE_SUCCESS;	
}


static enum MAPISTORE_ERROR fsocpf_op_get_table_property(void *private_data,
							 const char *folder_uri,
							 enum MAPISTORE_TABLE_TYPE table_type,
							 uint32_t pos,
							 enum MAPITAGS proptag,
							 void **data)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder_list	*el;
	struct fsocpf_folder		*folder;
	enum MAPISTORE_ERROR		retval = MAPISTORE_SUCCESS;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!data, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (!strcmp(fsocpf_ctx->uri, folder_uri)) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders || !fsocpf_ctx->folders->folder) {
			el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fsocpf_ctx->uri, fsocpf_ctx->dir);
			MAPISTORE_RETVAL_IF(!el, MAPISTORE_ERR_NO_MEMORY, NULL);

			DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
			MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Element added to the list '%s'\n", el->folder->uri);
		}
	}

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder(fsocpf_ctx, folder_uri);
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NO_DIRECTORY, NULL);

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		retval = fsocpf_get_property_from_folder_table(folder, pos, proptag, data);
		break;
	case MAPISTORE_MESSAGE_TABLE:
		retval = fsocpf_get_property_from_message_table(folder, pos, proptag, data);
		break;
	default:
		break;
	}

	return retval;
}


static enum MAPISTORE_ERROR fsocpf_op_openmessage(void *private_data,
						  const char *folder_uri,
						  const char *message_uri,
						  struct mapistore_message *msg)
{
	int				ret;
	enum MAPISTATUS			retval;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message_list	*el;
	struct fsocpf_message		*message;
	struct fsocpf_folder		*folder;
	uint32_t			ocpf_context_id;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!message_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!msg, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Search for the mid fsocpf_message entry */
	message = fsocpf_find_message(fsocpf_ctx, message_uri);
	if (message) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Message already %s\n", "opened");
		msg->properties = talloc_zero(fsocpf_ctx, struct SRow);
		MAPISTORE_RETVAL_IF(!msg->properties, MAPISTORE_ERR_NO_MEMORY, NULL);

		retval = ocpf_get_recipients(message, message->ocpf_context_id, &msg->recipients);
		msg->properties->lpProps = ocpf_get_SPropValue(message->ocpf_context_id, 
							       &msg->properties->cValues);
		return MAPISTORE_SUCCESS;
	}

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder(fsocpf_ctx, folder_uri);
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NOT_FOUND, NULL);

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_INFO, "Message %s is stored within %s\n", message_uri, folder->uri);

	/* Trying to open and map the file with OCPF */
	ret = ocpf_new_context(message_uri, &ocpf_context_id, OCPF_FLAGS_READ);
	ret = ocpf_parse(ocpf_context_id);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	el = fsocpf_message_list_element_init((TALLOC_CTX *)fsocpf_ctx, folder_uri, message_uri, ocpf_context_id);
	MAPISTORE_RETVAL_IF(!el, MAPISTORE_ERR_NO_MEMORY, NULL);

	DLIST_ADD_END(fsocpf_ctx->messages, el, struct fsocpf_message_list *);
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Element added to the list '%s'\n", message_uri);

	/* Retrieve recipients from the message */
	retval = ocpf_get_recipients(el, ocpf_context_id, &msg->recipients);
	MAPISTORE_RETVAL_IF(retval, MAPISTORE_ERR_NOT_FOUND, NULL);

	/* Retrieve properties from the message */
	msg->properties = talloc_zero(el, struct SRow);
	MAPISTORE_RETVAL_IF(!msg->properties, MAPISTORE_ERR_NO_MEMORY, NULL);

	retval = ocpf_server_set_SPropValue(el, ocpf_context_id);
	msg->properties->lpProps = ocpf_get_SPropValue(ocpf_context_id, &msg->properties->cValues);

	return MAPISTORE_SUCCESS;
}


/* FIXME: the backend should be responsible for setting the message URI / unique ID */
/* When creating the message, the URI doesn't exist yet, this should be returned by the function */

/* Some backends will only be able to get the URI once they commit the
 * message, we should offer the possibility for backends to register
 * the indexing URI later and deal with a temporary URI until they
 * come to commit/save operations 
*/

static enum MAPISTORE_ERROR fsocpf_op_createmessage(void *private_data,
						    const char *folder_uri,
						    char **_message_uri,
						    bool *uri_register)
{
	int				ret;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message_list	*el;
	struct fsocpf_folder		*folder;
	char				*message_uri = NULL;
	uint32_t			ocpf_context_id;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!folder_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!message_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	
	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder(fsocpf_ctx, folder_uri);
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NOT_FOUND, NULL);

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Message %s will be created within %s\n", message_uri, folder->uri);
	
	ret = ocpf_new_context(message_uri, &ocpf_context_id, OCPF_FLAGS_CREATE);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_CONTEXT_FAILED, NULL);

	el = fsocpf_message_list_element_init((TALLOC_CTX *)fsocpf_ctx, folder_uri, message_uri, ocpf_context_id);
	MAPISTORE_RETVAL_IF(!el, MAPISTORE_ERR_NO_MEMORY, NULL);

	DLIST_ADD_END(fsocpf_ctx->messages, el, struct fsocpf_message_list *);
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "Element added to the list '%s'\n", message_uri);
	
	*_message_uri = message_uri;
	*uri_register = true;

	return MAPISTORE_SUCCESS;
}


/* FIXME: ocpf is using the fid to create the filename, we MUST offer a different mechanism
   OR we could let backends register the message on their own. To be decided */
static enum MAPISTORE_ERROR fsocpf_op_savechangesmessage(void *private_data,
							 const char *message_uri,
							 uint8_t flags)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message		*message;
	
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!message_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	message = fsocpf_find_message(fsocpf_ctx, message_uri);
	if (!message || !message->ocpf_context_id) {
		return MAPISTORE_ERR_NOT_FOUND;
	}

	ocpf_write_init(message->ocpf_context_id, 0);
	ocpf_write_commit(message->ocpf_context_id);

	return MAPISTORE_SUCCESS;
}

/* FIXME: See savechangesmessage comments */
static enum MAPISTORE_ERROR fsocpf_op_submitmessage(void *private_data,
						    const char *message_uri,
						    uint8_t flags)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message		*message;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!message_uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* This implementation is incorrect but should fit for immediate purposes */
	message = fsocpf_find_message(fsocpf_ctx, message_uri);

	ocpf_write_init(message->ocpf_context_id, 0);
	ocpf_write_commit(message->ocpf_context_id);

	return MAPISTORE_SUCCESS;
}


static char *fsocpf_get_recipients(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, uint8_t recipient_class)
{
	char		*recipient = NULL;
	uint32_t	i;

	for (i = 0; i < SRowSet->cRows; i++) {
		if (SRowSet->aRow[i].lpProps[0].value.l == recipient_class) {
			if (!recipient) {
				recipient = talloc_strdup(mem_ctx, SRowSet->aRow[i].lpProps[1].value.lpszA);
			} else {
				recipient = talloc_asprintf(recipient, "%s;%s", recipient, 
							    SRowSet->aRow[i].lpProps[1].value.lpszA);
			}
		}
	}

	return recipient;
}

static enum MAPISTORE_ERROR fsocpf_op_getprops(void *private_data, 
					       const char *uri,
					       uint8_t type, 
					       struct SPropTagArray *SPropTagArray,
					       struct SRow *aRow)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message	*message;
	uint32_t		cValues;
	struct SPropValue	*lpProps;
	struct SPropValue	lpProp;
	struct SRowSet		*SRowSet;
	uint32_t		i;
	uint32_t		j;
	char			*recip_str = NULL;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!SPropTagArray, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!aRow, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message(fsocpf_ctx, uri);
		ocpf_server_set_SPropValue(fsocpf_ctx, message->ocpf_context_id);
		lpProps = ocpf_get_SPropValue(message->ocpf_context_id, &cValues);
		ocpf_get_recipients(fsocpf_ctx, message->ocpf_context_id, &SRowSet);

		ocpf_dump(message->ocpf_context_id);
		for (i = 0; i != SPropTagArray->cValues; i++) {
			switch (SPropTagArray->aulPropTag[i]) {
			case PR_DISPLAY_TO:
			case PR_DISPLAY_TO_UNICODE:
				recip_str = fsocpf_get_recipients(fsocpf_ctx, SRowSet, OCPF_MAPI_TO);
				break;
			case PR_DISPLAY_CC:
			case PR_DISPLAY_CC_UNICODE:
				recip_str = fsocpf_get_recipients(fsocpf_ctx, SRowSet, OCPF_MAPI_CC);
				break;
			case PR_DISPLAY_BCC:
			case PR_DISPLAY_BCC_UNICODE:
				recip_str = fsocpf_get_recipients(fsocpf_ctx, SRowSet, OCPF_MAPI_BCC);
				break;
			default:
				for (j = 0; j != cValues; j++) {
					if (SPropTagArray->aulPropTag[i] == lpProps[j].ulPropTag)  {
						SRow_addprop(aRow, lpProps[j]);
					}
				}
			}

			if (recip_str) {
				lpProp.ulPropTag = SPropTagArray->aulPropTag[i];
				switch (SPropTagArray->aulPropTag[i] & 0xFFFF) {
				case PT_STRING8:
					lpProp.value.lpszA = talloc_strdup(aRow, recip_str);
					break;
				case PT_UNICODE:
					lpProp.value.lpszW = talloc_strdup(aRow, recip_str);
					break;
				}
				SRow_addprop(aRow, lpProp);
				talloc_free(recip_str);
				recip_str = NULL;
			}
		}
	}

	return MAPISTORE_SUCCESS;
}


static enum MAPISTORE_ERROR fsocpf_op_setprops(void *private_data,
					       const char *uri,
					       uint8_t type,
					       struct SRow *aRow)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *) private_data;
	struct fsocpf_folder	*folder;
	struct fsocpf_message	*message;
	uint32_t		i;
	const char		*path;
	enum MAPISTORE_ERROR	retval = MAPISTORE_SUCCESS;

	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, MSTORE_SINGLE_MSG, "");

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!fsocpf_ctx, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!aRow, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	if (mapistore_strip_ns_from_uri(uri, &path) != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_INFO(MSTORE_LEVEL_CRITICAL, "misformed uri: %s\n", uri);
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}
	MSTORE_DEBUG_INFO(MSTORE_LEVEL_DEBUG, "uri: %s, path: %s\n", uri, path);

	switch (type) {
	case MAPISTORE_FOLDER:
		folder = fsocpf_find_folder(fsocpf_ctx, path);
		MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_NOT_FOUND, NULL);
		retval = fsocpf_set_folder_props(folder->uri, aRow);
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message(fsocpf_ctx, path);
		MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_NOT_FOUND, NULL);
		for (i = 0; i < aRow->cValues; i++) {
			if (aRow->lpProps[i].ulPropTag == PidTagMessageClass) {
				ocpf_server_set_type(message->ocpf_context_id, aRow->lpProps[i].value.lpszW);
			}
			ocpf_server_add_SPropValue(message->ocpf_context_id, &aRow->lpProps[i]);
		}
		break;
	}

	return retval;
}

/* TODO: this looks like it probably does not handle soft deletion */
static enum MAPISTORE_ERROR fsocpf_op_deletemessage(void *private_data,
						    const char *message_uri,
						    enum MAPISTORE_DELETION_TYPE deletion_type)
{
	int				ret;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message		*message;
	
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	message = fsocpf_find_message(fsocpf_ctx, message_uri);
	MAPISTORE_RETVAL_IF(!message, MAPISTORE_ERR_NOT_FOUND, NULL);

	ret = unlink(message->uri);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERROR, NULL);

	return MAPISTORE_SUCCESS;
}

/**
   \details Entry point for mapistore FSOCPF backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum MAPISTORE_ERROR mapistore_init_backend(void)
{
	struct mapistore_backend	backend;
	enum MAPISTORE_ERROR		retval;

	/* Initialize backend with defaults */
	retval = mapistore_backend_init_defaults(&backend);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Fill in our name */
	backend.name = "fsocpf";
	backend.description = "mapistore filesystem + ocpf backend";
	backend.uri_namespace = "fsocpf://";

	/* Fill in all the operations */
	backend.init = fsocpf_init;
	backend.create_context = fsocpf_create_context;
	backend.delete_context = fsocpf_delete_context;
	backend.release_record = fsocpf_release_record;
	backend.get_path = fsocpf_get_path;
	backend.op_mkdir = fsocpf_op_mkdir;
	backend.op_rmdir = fsocpf_op_rmdir;
	backend.op_opendir = fsocpf_op_opendir;
	backend.op_closedir = fsocpf_op_closedir;
	backend.op_readdir_count = fsocpf_op_readdir_count;
	backend.op_get_table_property = fsocpf_op_get_table_property;
	backend.op_openmessage = fsocpf_op_openmessage;
	backend.op_createmessage = fsocpf_op_createmessage;
	backend.op_savechangesmessage = fsocpf_op_savechangesmessage;
	backend.op_submitmessage = fsocpf_op_submitmessage;
	backend.op_getprops = fsocpf_op_getprops;
	backend.op_get_uri_by_name = fsocpf_op_get_uri_by_name;
	backend.op_setprops = fsocpf_op_setprops;
	backend.op_deletemessage = fsocpf_op_deletemessage;

	/* Fill in admin operations on mapistore database/store */
	backend.op_db_create_uri = fsocpf_create_mapistore_uri;
	backend.op_db_provision_namedprops = fsocpf_provision_namedprops;

	/* Register ourselves with the MAPISTORE subsystem */
	retval = mapistore_backend_register(&backend);
	if (retval != MAPISTORE_SUCCESS) {
		MSTORE_DEBUG_ERROR(MSTORE_LEVEL_CRITICAL, "Failed to register the '%s' mapistore backend!\n", backend.name);
	}

	return retval;
}
