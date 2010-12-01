/*
   OpenChange Storage Abstraction Layer library
   MAPIStore FS / OCPF backend

   OpenChange Project

   Copyright (C) Julien Kerihuel 2010

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
static int fsocpf_init(void)
{
	DEBUG(0, ("fsocpf backend initialized\n"));

	ocpf_init();
	
	return MAPISTORE_SUCCESS;
}

/**
   \details Allocate / initialize the fsocpf_context structure

   \param mem_ctx pointer to the memory context
   \param uri pointer to the fsocpf path
   \param dir pointer to the DIR structure associated with the uri
 */
static struct fsocpf_context *fsocpf_context_init(TALLOC_CTX *mem_ctx, 
						  const char *uri, 
						  DIR *dir)
{
	struct fsocpf_context	*fsocpf_ctx;

	fsocpf_ctx = talloc_zero(mem_ctx, struct fsocpf_context);
	fsocpf_ctx->private_data = NULL;
	fsocpf_ctx->uri = talloc_strdup(fsocpf_ctx, uri);
	fsocpf_ctx->dir = dir;
	fsocpf_ctx->folders = NULL;
	fsocpf_ctx->messages = NULL;

	return fsocpf_ctx;
}

/**
   \details Allocate / initialize a fsocpf_folder_list element

   This essentially creates a node of the linked list, which will be added to the list
   by the caller

   \param mem_ctx pointer to the memory context
   \param fid the folder id for the folder
   \param uri pointer to the fsocpf path for the folder
   \param dir pointer to the DIR structure associated with the fid / uri
 */
static struct fsocpf_folder_list *fsocpf_folder_list_element_init(TALLOC_CTX *mem_ctx, 
								  uint64_t fid, 
								  const char *uri, 
								  DIR *dir)
{
	struct fsocpf_folder_list *el;

	el = talloc_zero(mem_ctx, struct fsocpf_folder_list);
	el->folder = talloc_zero((TALLOC_CTX *)el, struct fsocpf_folder);
	el->folder->fid = fid;
	el->folder->path = talloc_strdup((TALLOC_CTX *)el, uri);
	el->folder->dir = dir;

	return el;
}


/**
  \details search for the fsocpf_folder for a given folder ID

  \param fsocpf_ctx the store context
  \param fid the folder ID of the fsocpf_folder to search for

  \return folder on success, or NULL if the folder was not found
*/
static struct fsocpf_folder *fsocpf_find_folder_by_fid(struct fsocpf_context *fsocpf_ctx, 
						       uint64_t fid)
{
	struct fsocpf_folder_list	*el;

	for (el = fsocpf_ctx->folders; el; el = el->next) {
		if (el->folder && el->folder->fid == fid) {
			return el->folder;
		}
	}
	return NULL;
}


/**
   \details Allocate / initialize a fsocpf_message_list element

   This essentialy creates a node of the linked list, which will be
   added to the list by the caller

   \param mem_ctx pointer to the memory context
   \param fid the folder id for the message
   \param mid the message id for the message
   \param uri pointer to the fsocpf path for the message
   \param context_id the ocpf context identifier
 */
static struct fsocpf_message_list *fsocpf_message_list_element_init(TALLOC_CTX *mem_ctx,
								    uint64_t fid,
								    uint64_t mid,
								    const char *uri,
								    uint32_t context_id)
{
	struct fsocpf_message_list	*el;

	el = talloc_zero(mem_ctx, struct fsocpf_message_list);
	el->message = talloc_zero((TALLOC_CTX *)el, struct fsocpf_message);
	el->message->fid = fid;
	el->message->mid = mid;
	el->message->path = talloc_strdup((TALLOC_CTX *)el, uri);
	el->message->ocpf_context_id = context_id;

	return el;
}


/**
   \details search for the fsocpf_message for a given message ID

   \param fsocpf_ctx the store context
   \param mid the message ID of the fsocpf_message to search for

   \return message on success, or NULL if the message was not found
 */
static struct fsocpf_message *fsocpf_find_message_by_mid(struct fsocpf_context *fsocpf_ctx,
							 uint64_t mid)
{
	struct fsocpf_message_list	*el;

	for (el = fsocpf_ctx->messages; el; el = el->next) {
		if (el->message && el->message->mid == mid) {
			return el->message;
		}
	}
	return NULL;
}


/**
   \details search for the fsocpf_message_list for a given message ID

   \param fsocpf_ctx the store context
   \param mid the message ID of the fsocpf_message to search for

   \return point to message list on success, or NULL if the message was not found
 */
static struct fsocpf_message_list *fsocpf_find_message_list_by_mid(struct fsocpf_context *fsocpf_ctx,
								   uint64_t mid)
{
	struct fsocpf_message_list	*el;

	if (!fsocpf_ctx || !fsocpf_ctx->messages) {
		return NULL;
	}

	for (el = fsocpf_ctx->messages; el; el = el->next) {
		if (el->message && el->message->mid == mid) {
			return el;
		}
	}
	return NULL;
}


/**
   \details Create a connection context to the fsocpf backend

   \param mem_ctx pointer to the memory context
   \param uri pointer to the fsocpf path
   \param private_data pointer to the private backend context 
 */
static int fsocpf_create_context(TALLOC_CTX *mem_ctx, const char *uri, void **private_data)
{
	DIR				*top_dir;
	struct fsocpf_context		*fsocpf_ctx;
	struct fsocpf_folder_list	*el;
	int				len;
	int				i;

	DEBUG(0, ("[%s:%d]\n", __FUNCTION__, __LINE__));
	DEBUG(4, ("[%s:%d]: fsocpf uri: %s\n", __FUNCTION__, __LINE__, uri));

	/* Step 1. Try to open context directory */
	top_dir = opendir(uri);
	if (!top_dir) {
		/* If it doesn't exist, try to create it */
		if (mkdir(uri, S_IRWXU) != 0 ) {
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
		top_dir = opendir(uri);
		if (!top_dir) {
			return MAPISTORE_ERR_CONTEXT_FAILED;
		}
	}

	/* Step 2. Allocate / Initialize the fsocpf context structure */
	fsocpf_ctx = fsocpf_context_init(mem_ctx, uri, top_dir);

	/* FIXME: Retrieve the fid from the URI */
	len = strlen(uri);
	for (i = len; i > 0; i--) {
		if (uri[i] == '/' && i != len) {
			char *tmp;

			tmp = talloc_strdup(mem_ctx, uri + i + 1);
			fsocpf_ctx->fid = strtoull(tmp, NULL, 16);
			talloc_free(tmp);
			break;
		}
	}

	/* Create the entry in the list for top mapistore folders */
	el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fsocpf_ctx->fid, uri, top_dir);
	DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
	DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", el->folder->fid));

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
static int fsocpf_delete_context(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

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

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_release_record(void *private_data, uint64_t fmid, uint8_t type)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message_list	*message;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		return MAPISTORE_SUCCESS;
	}

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message_list_by_mid(fsocpf_ctx, fmid);
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
   \param fmid the folder/message ID to lookup
   \param type whether it is a folder or message
   \param path pointer on pointer to the path to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static int fsocpf_get_path(void *private_data, uint64_t fmid,
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
		folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fmid);
		if (!folder) {
			DEBUG(0, ("folder doesn't exist ...\n"));
			*path = NULL;
			return MAPISTORE_ERROR;
		}
		DEBUG(0, ("folder->path is %s\n", folder->path));
		*path = folder->path;
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message_by_mid(fsocpf_ctx, fmid);
		if (!message) {
			DEBUG(0, ("message doesn't exist ...\n"));
			*path = NULL;
			return MAPISTORE_ERROR;
		}
		DEBUG(0, ("message->path is %s\n", message->path));
		*path = message->path;
		break;
	default:
		DEBUG(0, ("[%s]: Invalid type %d\n", __FUNCTION__, type));
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}

static int fsocpf_op_get_fid_by_name(void *private_data, uint64_t parent_fid, const char* foldername, uint64_t *fid)
{
	TALLOC_CTX		*mem_ctx;
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder 	*folder;
	uint32_t		ocpf_context_id;
	struct dirent		*curdir;
	char			*propfile;
	struct SPropValue	*lpProps;
	uint32_t		cValues = 0;
	int			ret;
	int			i;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	/* Step 1. Search for the parent folder by fid */
	folder = fsocpf_find_folder_by_fid(fsocpf_ctx, parent_fid);
	if (!folder) {
		return MAPISTORE_ERROR;
	}
	
	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_get_fid_by_name");

	/* Step 2. Iterate over the contents of the parent folder, searching for a matching name */
	rewinddir(folder->dir);
	while ((curdir = readdir(folder->dir)) != NULL) {
		if ((curdir->d_type == DT_DIR) && (strncmp(curdir->d_name, "0x", 2) == 0)) {
			// open the .properties file for this sub-directory
			propfile = talloc_asprintf(mem_ctx, "%s/%s/.properties",
						   folder->path, curdir->d_name);
			DEBUG(6, ("propfile: %s\n", propfile));
			ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_READ);

			/* process the file */
			ret = ocpf_parse(ocpf_context_id);
			DEBUG(6, ("ocpf_parse (%d) = %d\n", ocpf_context_id, ret));
			talloc_free(propfile);
	
			ocpf_server_set_SPropValue(mem_ctx, ocpf_context_id);
			lpProps = ocpf_get_SPropValue(ocpf_context_id, &cValues);
			for (i = 0; i < cValues; ++i) {
				if (lpProps && lpProps[i].ulPropTag == PR_DISPLAY_NAME) {
					const char * this_folder_display_name = get_SPropValue_data(&(lpProps[i]));
					DEBUG(6, ("looking at %s found in %s\n", this_folder_display_name, curdir->d_name));
					if (strcmp(this_folder_display_name, foldername) == 0) {
						DEBUG(4, ("folder name %s found in %s\n", this_folder_display_name, curdir->d_name));
						talloc_free(mem_ctx);
						ocpf_del_context(ocpf_context_id);
						*fid = strtoul(curdir->d_name, NULL, 16);
						return MAPISTORE_SUCCESS;
					}
				}
			}
			ocpf_del_context(ocpf_context_id);
		}
	}
	talloc_free(mem_ctx);
	return MAPISTORE_ERR_NOT_FOUND;
}

/**
  \details Set the properties for a folder
  
  \param path the path to the folder
  \param fid the folder ID of the folder
  \param aRow the properties to set on the folder
*/
static void fsocpf_set_folder_props(const char *path, uint64_t fid, struct SRow *aRow)
{
	TALLOC_CTX			*mem_ctx;
	struct mapi_SPropValue_array	mapi_lpProps;
	uint32_t			ocpf_context_id;
	char				*propfile;
	uint32_t			i;

	mem_ctx = talloc_named(NULL, 0, "fsocpf_set_folder_props");
	
	/* Create the array of mapi properties */
	mapi_lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, aRow->cValues);
	mapi_lpProps.cValues = aRow->cValues;
	mapidump_SRow(aRow, "[+]");
	for (i = 0; i < aRow->cValues; i++) {
		cast_mapi_SPropValue((TALLOC_CTX *)mapi_lpProps.lpProps,
				     &(mapi_lpProps.lpProps[i]), &(aRow->lpProps[i]));
	}

	/* Create the .properties file */
	propfile = talloc_asprintf(mem_ctx, "%s/.properties", path);

	ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_CREATE);

	ocpf_write_init(ocpf_context_id, fid);
	DEBUG(0, ("Writing %s\n", propfile));
	ocpf_write_auto(ocpf_context_id, NULL, &mapi_lpProps);
	ocpf_write_commit(ocpf_context_id);
	ocpf_del_context(ocpf_context_id);
	
	talloc_free(mem_ctx);
}

/**
   \details Create a folder in the fsocpf backend
   
   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_mkdir(void *private_data, uint64_t parent_fid, uint64_t fid,
			   struct SRow *aRow)
{
	TALLOC_CTX			*mem_ctx;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*folder;
	char				*newfolder;
	const char			*new_folder_name = NULL;
	uint64_t			dummy_fid;
	struct fsocpf_folder_list	*newel;
	DIR				*dir;
	int				ret;
	uint32_t			i;


	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		DEBUG(0, ("No fsocpf context found :-(\n"));
		return MAPISTORE_ERROR;
	}

	/* Step 0. Check if it already exists */
	for (i = 0; i < aRow->cValues; ++i) {
		if (aRow->lpProps[i].ulPropTag == PR_DISPLAY_NAME) {
			new_folder_name = aRow->lpProps[i].value.lpszA;
		}
	}
	if (fsocpf_op_get_fid_by_name(private_data, parent_fid, new_folder_name, &dummy_fid) == MAPISTORE_SUCCESS) {
		/* already exists */
		return MAPISTORE_ERR_EXIST;
	}

	/* Step 1. Search for the parent fid */
	folder = fsocpf_find_folder_by_fid(fsocpf_ctx, parent_fid);

	if (! folder) {
		DEBUG(0, ("parent context for folder 0x%.16"PRIx64" not found\n", parent_fid));
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_mkdir");

	/* Step 2. Stringify fid and create directory */
	newfolder = talloc_asprintf(mem_ctx, "%s/0x%.16"PRIx64, folder->path, fid);
	DEBUG(0, ("newfolder = %s\n", newfolder));
	ret = mkdir(newfolder, 0700);
	if (ret) {
		DEBUG(0, ("mkdir failed with ret = %d\n", ret));
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}
	dir = opendir(newfolder);
	
	/* add this folder to the list of ones we know about */
	newel = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, newfolder, dir);
	DLIST_ADD_END(fsocpf_ctx->folders, newel, struct fsocpf_folder_list *);
	DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", fid));

	fsocpf_set_folder_props(newfolder, fid, aRow);
	
	talloc_free(mem_ctx);

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context
   \param parent_fid the FID for the parent of the folder to delete
   \param fid the FID for the folder to delete

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_rmdir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder	*parent;
	char			*folderpath;
	char			*propertiespath;
	TALLOC_CTX		*mem_ctx;
	int			ret;
	
	if (!fsocpf_ctx) {
		DEBUG(0, ("No fsocpf context found for op_rmdir :-(\n"));
		return MAPISTORE_ERROR;
	}
	DEBUG(4, ("FSOCPF would delete FID 0x%"PRIx64" from 0x%"PRIx64"\n", fid, parent_fid));

	/* Step 1. Search for the parent fid */
	parent = fsocpf_find_folder_by_fid(fsocpf_ctx, parent_fid);

	if (! parent) {
		DEBUG(0, ("parent context for folder 0x%.16"PRIx64" not found\n", parent_fid));
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_mkdir");

	/* Step 2. Stringify fid */
	folderpath = talloc_asprintf(mem_ctx, "%s/0x%.16"PRIx64, parent->path, fid);
	DEBUG(5, ("folder to delete = %s\n", folderpath));

	/* Step 3. Remove .properties file */
	propertiespath = talloc_asprintf(mem_ctx, "%s/.properties", folderpath);
	ret = unlink(propertiespath);
	if (ret) {
		DEBUG(0, ("unlink failed with ret = %d (%s)\n", ret, strerror(errno)));
		/* this could happen if we have no .properties file, so lets still try to delete */
	}

	/* Step 4. Delete directory */
	ret = rmdir(folderpath);
	if (ret) {
		DEBUG(0, ("rmdir failed with ret = %d (%s)\n", ret, strerror(errno)));
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Open a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context
   \param parent_fid the parent folder identifier
   \param fid the identifier of the colder to open

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_opendir(void *private_data, uint64_t parent_fid, uint64_t fid)
{
	TALLOC_CTX			*mem_ctx;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*folder;
	struct fsocpf_folder_list	*el;
	struct fsocpf_folder_list	*newel;
	struct dirent			*curdir;
	char				*searchdir;
	char				*newpath;
	DIR				*dir;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	/* Step 0. If fid equals top folder fid, it is already open */
	if (fsocpf_ctx->fid == fid) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders) {
			el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, fsocpf_ctx->uri, fsocpf_ctx->dir);
			DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
			DEBUG(0, ("Element added to the list 0x%16"PRIx64"\n", el->folder->fid));
		}

		folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fid);

		return (! folder) ? MAPISTORE_ERR_NO_DIRECTORY : MAPISTORE_SUCCESS;
	} else {
		/* Step 1. Search for the parent fid */
		folder = fsocpf_find_folder_by_fid(fsocpf_ctx, parent_fid);
		if (! folder) {
			return MAPISTORE_ERR_NO_DIRECTORY;
		}
	}

	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_opendir");

	/* Step 2. stringify fid */
	searchdir = talloc_asprintf(mem_ctx, "0x%.16"PRIx64, fid);
	DEBUG(0, ("Looking for %s\n", searchdir));

	/* Read the directory and search for the fid to open */
	rewinddir(folder->dir);
	errno = 0;
	{
		int i = 0;
		while ((curdir = readdir(folder->dir)) != NULL) {
			DEBUG(0, ("%d: readdir: %s\n", i, curdir->d_name));
			i++;
			if (curdir->d_name && !strcmp(curdir->d_name, searchdir)) {

				newpath = talloc_asprintf(mem_ctx, "%s/0x%.16"PRIx64, folder->path, fid);
				dir = opendir(newpath);
				if (!dir) {
					talloc_free(mem_ctx);
					return MAPISTORE_ERR_CONTEXT_FAILED;
				}
				DEBUG(0, ("FOUND\n"));

				newel = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, newpath, dir);
				DLIST_ADD_END(fsocpf_ctx->folders, newel, struct fsocpf_folder_list *);
				DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", fid));
			}
		}
	}

	DEBUG(0, ("errno = %d\n", errno));

	rewinddir(folder->dir);
	talloc_free(mem_ctx);
		
	return MAPISTORE_SUCCESS;
}


/**
   \details Close a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_closedir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	return MAPISTORE_SUCCESS;
}


/**
   \details Read directory content from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_readdir_count(void *private_data, 
				   uint64_t fid,
				   uint8_t table_type,
				   uint32_t *RowCount)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder		*folder;
	struct dirent			*curdir;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx || !RowCount) {
		return MAPISTORE_ERROR;
	}

	if (fsocpf_ctx->fid == fid) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders) {
			struct fsocpf_folder_list *el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, fsocpf_ctx->uri, fsocpf_ctx->dir);
			DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
			DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", el->folder->fid));
		}
	}

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fid);
	if (! folder) {
		*RowCount = 0;
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		rewinddir(folder->dir);
		errno = 0;
		*RowCount = 0;
		while ((curdir = readdir(folder->dir)) != NULL) {
			if (curdir->d_name && curdir->d_type == DT_DIR &&
			    strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..")) {
				DEBUG(0, ("Adding %s to the RowCount\n", curdir->d_name));
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
				DEBUG(0, ("Adding %s to the RowCount\n", curdir->d_name));
				*RowCount += 1;
			}
		}
		break;
	default:
		break;
	}

	return MAPISTORE_SUCCESS;
}


static int fsocpf_get_property_from_folder_table(struct fsocpf_folder *folder,
						 uint32_t pos,
						 uint32_t proptag,
						 void **data)
{
	int			ret;
	struct dirent		*curdir;
	uint32_t		counter = 0;
	char			*folderID;
	char			*propfile;
	uint32_t		cValues = 0;
	struct SPropValue	*lpProps;
	uint32_t		ocpf_context_id;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

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

	if (!curdir) {
		DEBUG(0, ("curdir not found\n"));
		*data = NULL;
		return MAPI_E_NOT_FOUND;
	}

	/* If fid, return folder->fid */
	if (proptag == PR_FID) {
		uint64_t	*fid;

		fid = talloc_zero(folder, uint64_t);
		*fid = strtoull(folderID, NULL, 16);
		talloc_free(folderID);
		*data = (uint64_t *)fid;
		return MAPI_E_SUCCESS;
	} 

	/* Otherwise opens .properties file with ocpf for fid entry */
	propfile = talloc_asprintf(folder, "%s/%s/.properties", folder->path, folderID);
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
			*data = (void *) get_SPropValue(lpProps, (proptag & 0xFFFF0000) + PT_UNICODE);
		} else if (*data == NULL && (proptag & 0xFFFF) == PT_UNICODE) {
			*data = (void *) get_SPropValue(lpProps, (proptag & 0xFFFF0000) + PT_STRING8);
		}
		*data = talloc_strdup(folder, (char *)*data);
	}

	if (*data == NULL) {
		ret = ocpf_del_context(ocpf_context_id);
		return MAPI_E_NOT_FOUND;
	}

	ret = ocpf_del_context(ocpf_context_id);
	return MAPI_E_SUCCESS;
}


static int fsocpf_get_property_from_message_table(struct fsocpf_folder *folder,
						  uint32_t pos,
						  uint32_t proptag,
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

	DEBUG(5, ("[%s:%d\n]", __FUNCTION__, __LINE__));

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

	if (!messageID) {
		*data = NULL;
		return MAPI_E_NOT_FOUND;
	}

	/* if fid, return folder fid */
	if (proptag == PR_FID) {
		*data = (uint64_t *)&folder->fid;
		return MAPI_E_SUCCESS;
	  }

	/* If mid, return curdir->d_name */
	if (proptag == PR_MID) {
		uint64_t	*mid;

		mid = talloc_zero(folder, uint64_t);
		*mid = strtoull(messageID, NULL, 16);
		talloc_free(messageID);
		*data = (uint64_t *)mid;
		return MAPI_E_SUCCESS;
	}

	/* Otherwise opens curdir->d_name file with ocpf */
	propfile = talloc_asprintf(folder, "%s/%s", folder->path, messageID);
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
			*data = (void *) get_SPropValue(lpProps, (proptag & 0xFFFF0000) + PT_UNICODE);
		} else if (*data == NULL && (proptag & 0xFFFF) == PT_UNICODE) {
			*data = (void *) get_SPropValue(lpProps, (proptag & 0xFFFF0000) + PT_STRING8);
		}
		*data = talloc_strdup(folder, (char *)*data);
	}

	if (*data == NULL) {
		ocpf_del_context(ocpf_context_id);
		return MAPI_E_NOT_FOUND;
	}

	ocpf_del_context(ocpf_context_id);
	return MAPI_E_SUCCESS;	
}


static int fsocpf_op_get_table_property(void *private_data,
					uint64_t fid,
					uint8_t table_type,
					uint32_t pos,
					uint32_t proptag,
					void **data)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_folder_list	*el;
	struct fsocpf_folder		*folder;
	int				retval = MAPI_E_SUCCESS;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx || !data) {
		return MAPISTORE_ERROR;
	}

	if (fsocpf_ctx->fid == fid) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders || !fsocpf_ctx->folders->folder) {
			el = fsocpf_folder_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, fsocpf_ctx->uri, fsocpf_ctx->dir);
			DLIST_ADD_END(fsocpf_ctx->folders, el, struct fsocpf_folder_list *);
			DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", el->folder->fid));
		}
	}

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fid);
	if (! folder) {
		*data = NULL;
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

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


static int fsocpf_op_openmessage(void *private_data,
				 uint64_t fid,
				 uint64_t mid,
				 struct mapistore_message *msg)
{
	TALLOC_CTX			*mem_ctx;
	int				ret;
	enum MAPISTATUS			retval;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message_list	*el;
	struct fsocpf_message		*message;
	struct fsocpf_folder		*folder;
	uint32_t			ocpf_context_id;
	char				*propfile;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	/* Search for the mid fsocpf_message entry */
	message = fsocpf_find_message_by_mid(fsocpf_ctx, mid);
	if (message) {
		DEBUG(0, ("Message was already opened\n"));
		msg->properties = talloc_zero(fsocpf_ctx, struct SRow);
		msg->recipients = ocpf_get_recipients(message, message->ocpf_context_id);
		msg->properties->lpProps = ocpf_get_SPropValue(message->ocpf_context_id, 
							       &msg->properties->cValues);
		return MAPI_E_SUCCESS;
	}

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fid);
	if (!folder) {
		DEBUG(0, ("fsocpf_op_openmessage: folder not found\n"));
		return MAPISTORE_ERR_NOT_FOUND;
	}

	DEBUG(0, ("Message: 0x%.16"PRIx64" is inside %s\n", mid, folder->path));

	/* Trying to open and map the file with OCPF */
	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_openmessage");
	propfile = talloc_asprintf(mem_ctx, "%s/0x%.16"PRIx64, folder->path, mid);

	ret = ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_READ);
	ret = ocpf_parse(ocpf_context_id);

	if (!ret) {
		el = fsocpf_message_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, mid, 
						      propfile, ocpf_context_id);
		DLIST_ADD_END(fsocpf_ctx->messages, el, struct fsocpf_message_list *);
		DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", mid));

		/* Retrieve recipients from the message */
		msg->recipients = ocpf_get_recipients(el, ocpf_context_id);

		/* Retrieve properties from the message */
		msg->properties = talloc_zero(el, struct SRow);
		retval = ocpf_server_set_SPropValue(el, ocpf_context_id);
		msg->properties->lpProps = ocpf_get_SPropValue(ocpf_context_id, &msg->properties->cValues);
	} else {
		DEBUG(0, ("An error occured while processing %s\n", propfile));
		talloc_free(propfile);
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	talloc_free(propfile);
	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}


static int fsocpf_op_createmessage(void *private_data,
				   uint64_t fid,
				   uint64_t mid)
{
	TALLOC_CTX			*mem_ctx;
	int				ret;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message_list	*el;
	struct fsocpf_folder		*folder;
	uint32_t			ocpf_context_id;
	char				*propfile;
	
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	/* Search for the fid fsocpf_folder entry */
	folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fid);
	if (!folder) {
		DEBUG(0, ("fsocpf_op_createmessage: folder not found\n"));
		return MAPISTORE_ERR_NOT_FOUND;
	}

	DEBUG(0, ("Message: 0x%.16"PRIx64" will be created inside %s\n", mid, folder->path));
	
	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_createmessage");
	propfile = talloc_asprintf(mem_ctx, "%s/0x%.16"PRIx64, folder->path, mid);

	ret = ocpf_new_context(propfile, &ocpf_context_id, OCPF_FLAGS_CREATE);
	if (!ret) {
		el = fsocpf_message_list_element_init((TALLOC_CTX *)fsocpf_ctx, fid, mid,
						      propfile, ocpf_context_id);
		DLIST_ADD_END(fsocpf_ctx->messages, el, struct fsocpf_message_list *);
		DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", mid));
	} else {
		DEBUG(0, ("An error occured while creating %s\n", propfile));
		talloc_free(propfile);
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_CONTEXT_FAILED;
	}

	talloc_free(propfile);
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


static int fsocpf_op_savechangesmessage(void *private_data,
					uint64_t mid,
					uint8_t flags)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message		*message;
	
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	message = fsocpf_find_message_by_mid(fsocpf_ctx, mid);
	if (!message || !message->ocpf_context_id) {
		return MAPISTORE_ERR_NOT_FOUND;
	}
	ocpf_write_init(message->ocpf_context_id, message->fid);
	ocpf_write_commit(message->ocpf_context_id);

	return MAPISTORE_SUCCESS;
}


static int fsocpf_op_submitmessage(void *private_data,
				   uint64_t mid,
				   uint8_t flags)
{
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message		*message;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	/* This implementation is incorrect but should fit for immediate purposes */
	message = fsocpf_find_message_by_mid(fsocpf_ctx, mid);
	ocpf_write_init(message->ocpf_context_id, message->fid);
	ocpf_write_commit(message->ocpf_context_id);

	return MAPISTORE_SUCCESS;
}


static char *fsocpf_get_recipients(TALLOC_CTX *mem_ctx, struct SRowSet *SRowSet, uint8_t class)
{
	char		*recipient = NULL;
	uint32_t	i;

	for (i = 0; i < SRowSet->cRows; i++) {
		if (SRowSet->aRow[i].lpProps[0].value.l == class) {
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

static int fsocpf_op_getprops(void *private_data, 
			      uint64_t fmid, 
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


	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (type) {
	case MAPISTORE_FOLDER:
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message_by_mid(fsocpf_ctx, fmid);
		ocpf_server_set_SPropValue(fsocpf_ctx, message->ocpf_context_id);
		lpProps = ocpf_get_SPropValue(message->ocpf_context_id, &cValues);
		SRowSet = ocpf_get_recipients(fsocpf_ctx, message->ocpf_context_id);

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

	return MAPI_E_SUCCESS;
}


static int fsocpf_op_setprops(void *private_data,
			      uint64_t fmid,
			      uint8_t type,
			      struct SRow *aRow)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *) private_data;
	struct fsocpf_folder	*folder;
	struct fsocpf_message	*message;
	int			i;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	switch (type) {
	case MAPISTORE_FOLDER:
		folder = fsocpf_find_folder_by_fid(fsocpf_ctx, fmid);
		if (!folder) {
			return MAPISTORE_ERR_NOT_FOUND;
		}
		fsocpf_set_folder_props(folder->path, folder->fid, aRow);
		break;
	case MAPISTORE_MESSAGE:
		message = fsocpf_find_message_by_mid(fsocpf_ctx, fmid);
		if (!message || !message->ocpf_context_id) {
			return MAPISTORE_ERR_NOT_FOUND;
		}
		for (i = 0; i < aRow->cValues; i++) {
			if (aRow->lpProps[i].ulPropTag == PR_MESSAGE_CLASS) {
				ocpf_server_set_type(message->ocpf_context_id, aRow->lpProps[i].value.lpszA);
			} else if (aRow->lpProps[i].ulPropTag == PR_MESSAGE_CLASS_UNICODE) {
				ocpf_server_set_type(message->ocpf_context_id, aRow->lpProps[i].value.lpszW);
			}
			ocpf_server_add_SPropValue(message->ocpf_context_id, &aRow->lpProps[i]);
		}
		break;
	}

	return MAPISTORE_SUCCESS;
}

static int fsocpf_op_deletemessage(void *private_data,
				   uint64_t mid,
				   uint8_t flags)
{
	int res;
	struct fsocpf_context		*fsocpf_ctx = (struct fsocpf_context *)private_data;
	struct fsocpf_message		*message;
	
	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	message = fsocpf_find_message_by_mid(fsocpf_ctx, mid);

	if (!message || !message->path) {
		return MAPISTORE_ERR_NOT_FOUND;
	}

	res = unlink(message->path);

	if (res == 0) {
		return MAPISTORE_SUCCESS;
	} else {
		DEBUG(1, ("%s, could not unlink: %s\n", __FUNCTION__, strerror(errno)));
		return MAPISTORE_ERROR;
	}
}

static int fsocpf_op_get_folders_list(void *private_data,
                                      uint64_t fmid,
                                      struct indexing_folders_list **folders_list)
{
        struct indexing_folders_list *flist;
        /* char *folder, *tmp_uri, *uri, *substr; */
	int				ret;
        char *path;
        struct fsocpf_message *message;
        struct fsocpf_folder *folder;
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

        flist = talloc_zero(fsocpf_ctx, struct indexing_folders_list);
        flist->folderID = talloc_array(flist, uint64_t, 64);
        flist->count = 1;
        flist->folderID[0] = fsocpf_ctx->fid;
        *folders_list = flist;

        message = fsocpf_find_message_by_mid (fsocpf_ctx, fmid);
        if (message) {
                path = message->path;
        }
        else {
                folder = fsocpf_find_folder_by_fid (fsocpf_ctx, fmid);
                if (folder) {
                        path = message->path;
                } 
                else {
                        return MAPISTORE_ERR_NOT_FOUND;
                }
        }

        path = path + strlen(fsocpf_ctx->uri) + 1;

        return MAPISTORE_SUCCESS;
}

/**
   \details Entry point for mapistore FSOCPF backend

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
int mapistore_init_backend(void)
{
	struct mapistore_backend	backend;
	int				ret;

	/* Fill in our name */
	backend.name = "fsocpf";
	backend.description = "mapistore filesystem + ocpf backend";
	backend.namespace = "fsocpf://";

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
	backend.op_get_fid_by_name = fsocpf_op_get_fid_by_name;
	backend.op_setprops = fsocpf_op_setprops;
	backend.op_deletemessage = fsocpf_op_deletemessage;
	backend.op_get_folders_list = fsocpf_op_get_folders_list;

	/* Register ourselves with the MAPISTORE subsystem */
	ret = mapistore_backend_register(&backend);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("Failed to register the '%s' mapistore backend!\n", backend.name));
		return ret;
	}

	return MAPISTORE_SUCCESS;
}
