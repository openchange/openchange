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
	
	return MAPISTORE_SUCCESS;
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
	struct folder_list_context	*el;
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
	fsocpf_ctx = talloc_zero(mem_ctx, struct fsocpf_context);
	fsocpf_ctx->private_data = NULL;
	fsocpf_ctx->uri = talloc_strdup(fsocpf_ctx, uri);
	fsocpf_ctx->dir = top_dir;
	fsocpf_ctx->folders = talloc_zero(fsocpf_ctx, struct folder_list_context);

	/* FIXME: Retrieve the fid from the URI */
	len = strlen(uri);
	for (i = strlen(uri); i > 0; i--) {
		if (uri[i] == '/' && i != strlen(uri)) {
			char *tmp;

			tmp = talloc_strdup(mem_ctx, uri + i + 1);
			fsocpf_ctx->fid = strtoull(tmp, NULL, 16);
			talloc_free(tmp);
			break;
		}
	}

	/* Create the entry in the list for top mapistore folders */
	el = talloc_zero((TALLOC_CTX *)fsocpf_ctx->folders, struct folder_list_context);
	el->ctx = talloc_zero((TALLOC_CTX *)el, struct folder_list);
	el->ctx->fid = fsocpf_ctx->fid;
	el->ctx->path = talloc_strdup(fsocpf_ctx, uri);
	el->ctx->dir = top_dir;
	DLIST_ADD_END(fsocpf_ctx->folders, el, struct folder_list_context *);
	DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", el->ctx->fid));

	/* Step 3. Store fsocpf context within the opaque private_data pointer */
	*private_data = (void *)fsocpf_ctx;

	DEBUG(0, ("%s has been opened\n", uri));
	{
		struct dirent *curdir;
		int i = 0;

		while ((curdir = readdir(fsocpf_ctx->dir)) != NULL) {
			DEBUG(0, ("%d: readdir: %s\n", i, curdir->d_name));
			i++;
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
	}
	talloc_free(fsocpf_ctx);

	return MAPISTORE_SUCCESS;
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
	struct folder_list_context	*el;
	struct mapi_SPropValue_array	mapi_lpProps;
	bool				found = false;
	char				*newfolder;
	char				*propfile;
	int				ret;
	int				i;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		DEBUG(0, ("No fsocpf context found :-(\n"));
		return MAPISTORE_ERROR;
	}

	/* Step 1. Search for the parent fid */
	for (el = fsocpf_ctx->folders; el; el = el->next) {
		if (el->ctx && el->ctx->fid == parent_fid) {
			found = true;
			break;
		}
	}
	if (found == false) {
		DEBUG(0, ("parent context for folder 0x%.16llx not found\n", parent_fid));
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_mkdir");

	/* Step 2. Stringify fid and create directory */
	newfolder = talloc_asprintf(mem_ctx, "%s/0x%.16"PRIx64, el->ctx->path, fid);
	DEBUG(0, ("newfolder = %s\n", newfolder));
	ret = mkdir(newfolder, 0700);
	if (ret) {
		DEBUG(0, ("mkdir failed with ret = %d\n", ret));
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}

	/* Step 3. Create the array of mapi properties */
	mapi_lpProps.lpProps = talloc_array(mem_ctx, struct mapi_SPropValue, aRow->cValues);
	mapi_lpProps.cValues = aRow->cValues;
	mapidump_SRow(aRow, "[+]");
	for (i = 0; i < aRow->cValues; i++) {
		cast_mapi_SPropValue(&(mapi_lpProps.lpProps[i]), &(aRow->lpProps[i]));
	}

	/* Step 3. Create the .properties file */
	ocpf_init();
	propfile = talloc_asprintf(mem_ctx, "%s/.properties", newfolder);
	talloc_free(newfolder);

	ocpf_write_init(propfile, fid);
	DEBUG(0, ("Writing %s\n", propfile));
	ocpf_write_auto(NULL, &mapi_lpProps);
	ocpf_write_commit();
	ocpf_release();

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a folder from the fsocpf backend

   \param private_data pointer to the current fsocpf context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
static int fsocpf_op_rmdir(void *private_data)
{
	struct fsocpf_context	*fsocpf_ctx = (struct fsocpf_context *)private_data;

	if (!fsocpf_ctx) {
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
	struct folder_list_context	*el;
	bool				found = false;
	struct dirent			*curdir;
	char				*searchdir;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx) {
		return MAPISTORE_ERROR;
	}

	/* Step 0. If fid equals top folder fid, it is already open */
	if (fsocpf_ctx->fid == fid) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders->ctx) {
			el = talloc_zero((TALLOC_CTX *)fsocpf_ctx->folders, struct folder_list_context);
			el->ctx = talloc_zero((TALLOC_CTX *)el, struct folder_list);
			el->ctx->fid = fid;
			el->ctx->path = talloc_strdup(el, fsocpf_ctx->uri);
			el->ctx->dir = fsocpf_ctx->dir;
			DLIST_ADD_END(fsocpf_ctx->folders, el, struct folder_list_context *);
			DEBUG(0, ("Element added to the list 0x%16"PRIx64"\n", el->ctx->fid));
			goto test;
		}
		DEBUG(0, ("OK returning success now\n"));
		return MAPISTORE_SUCCESS;
	}

test:
	/* Step 1. Search for the parent fid */
	for (el = fsocpf_ctx->folders; el; el = el->next) {
		/* if (el->ctx && el->ctx->fid == parent_fid) { */
		if (el->ctx && el->ctx->fid == fid) {
			found = true;
			break;
		}
	}

	if (found == false) {
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	mem_ctx = talloc_named(NULL, 0, "fsocpf_op_opendir");

	/* Step 2. stringify fid */
	searchdir = talloc_asprintf(mem_ctx, "0x%.16"PRIx64, fid);
	DEBUG(0, ("Looking for %s\n", searchdir));

	/* Read the directory and search for the fid to open */
	rewinddir(el->ctx->dir);
	errno = 0;
	{
		int i = 0;
		while ((curdir = readdir(el->ctx->dir)) != NULL) {
			DEBUG(0, ("%d: readdir: %s\n", i, curdir->d_name));
			i++;
			if (curdir->d_name && !strcmp(curdir->d_name, searchdir)) {
				DEBUG(0, ("FOUND\n"));
			}
		}
	}

	DEBUG(0, ("errno = %d\n", errno));

	rewinddir(el->ctx->dir);
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
	struct folder_list_context	*el;
	bool				found = false;
	struct dirent			*curdir;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx || !RowCount) {
		return MAPISTORE_ERROR;
	}

	if (fsocpf_ctx->fid == fid) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders->ctx) {
			el = talloc_zero((TALLOC_CTX *)fsocpf_ctx->folders, struct folder_list_context);
			el->ctx = talloc_zero((TALLOC_CTX *)el, struct folder_list);
			el->ctx->fid = fid;
			el->ctx->path = talloc_strdup(el, fsocpf_ctx->uri);
			el->ctx->dir = fsocpf_ctx->dir;
			DLIST_ADD_END(fsocpf_ctx->folders, el, struct folder_list_context *);
			DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", el->ctx->fid));
		}
	}

	/* Search for the fid folder_list entry */
	for (el = fsocpf_ctx->folders; el; el = el->next) {
		if (el->ctx && el->ctx->fid == fid) {
			found = true;
			break;
		}
	}
	if (found == false) {
		*RowCount = 0;
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		rewinddir(el->ctx->dir);
		errno = 0;
		*RowCount = 0;
		while ((curdir = readdir(el->ctx->dir)) != NULL) {
			if (curdir->d_name && curdir->d_type == DT_DIR &&
			    strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..")) {
				DEBUG(0, ("Adding %s to the RowCount\n", curdir->d_name));
				*RowCount += 1;
			}
		}
		break;
	case MAPISTORE_MESSAGE_TABLE:
		DEBUG(0, ("Not implemented yet\n"));
		break;
	default:
		break;
	}

	return MAPISTORE_SUCCESS;
}


static int fsocpf_get_property_from_folder_table(struct folder_list *ctx,
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

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	/* Set dir listing to current position */
	rewinddir(ctx->dir);
	errno = 0;
	while ((curdir = readdir(ctx->dir)) != NULL) {
		if (curdir->d_name && curdir->d_type == DT_DIR &&
		    strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..") &&
		    counter == pos) {
			folderID = talloc_strdup(ctx, curdir->d_name);
			break;
		}
		if (strcmp(curdir->d_name, ".") && strcmp(curdir->d_name, "..")) {
			counter++;
		}
	}

	if (!curdir) {
		talloc_free(folderID);
		*data = NULL;
		return MAPI_E_NOT_FOUND;
	}

	/* If fid, return ctx->fid */
	if (proptag == PR_FID) {
		uint64_t	*fid;

		fid = talloc_zero(ctx, uint64_t);
		*fid = strtoull(folderID, NULL, 16);
		talloc_free(folderID);
		*data = (uint64_t *)fid;
		return MAPI_E_SUCCESS;
	} 

	/* Otherwise opens .properties file with ocpf for fid entry */
	ocpf_init();
	propfile = talloc_asprintf(ctx, "%s/%s/.properties", ctx->path, folderID);
	talloc_free(folderID);

	/* process the file */
	ret = ocpf_parse(propfile);
	fflush(0);
	talloc_free(propfile);
	ocpf_dump();

	ocpf_set_SPropValue_array(ctx);
	lpProps = ocpf_get_SPropValue(&cValues);

	/* FIXME: We need to find a proper way to handle this (for all types) */
	talloc_steal(ctx, lpProps);

	*data = (void *) get_SPropValue(lpProps, proptag);
	if (((proptag & 0xFFFF) == PT_STRING8) || ((proptag & 0xFFFF) == PT_UNICODE)) {
		/* Hack around PT_STRING8 and PT_UNICODE */
		if (*data == NULL && ((proptag & 0xFFFF) == PT_STRING8)) {
			*data = (void *) get_SPropValue(lpProps, (proptag & 0xFFFF0000) + PT_UNICODE);
		} else if (*data == NULL && (proptag & 0xFFFF) == PT_UNICODE) {
			*data = (void *) get_SPropValue(lpProps, (proptag & 0xFFFF0000) + PT_STRING8);
		}
		*data = talloc_strdup(ctx, (char *)*data);
	}

	if (*data == NULL) {
		ocpf_release();
		return MAPI_E_NOT_FOUND;
	}

	ocpf_release();	
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
	struct folder_list_context	*el;
	bool				found = false;
	int				retval = MAPI_E_SUCCESS;

	DEBUG(5, ("[%s:%d]\n", __FUNCTION__, __LINE__));

	if (!fsocpf_ctx || !data) {
		return MAPISTORE_ERROR;
	}

	if (fsocpf_ctx->fid == fid) {
		/* If we access it for the first time, just add an entry to the folder list */
		if (!fsocpf_ctx->folders->ctx) {
			el = talloc_zero((TALLOC_CTX *)fsocpf_ctx->folders, struct folder_list_context);
			el->ctx = talloc_zero((TALLOC_CTX *)el, struct folder_list);
			el->ctx->fid = fid;
			el->ctx->path = talloc_strdup(el, fsocpf_ctx->uri);
			el->ctx->dir = fsocpf_ctx->dir;
			DLIST_ADD_END(fsocpf_ctx->folders, el, struct folder_list_context *);
			DEBUG(0, ("Element added to the list 0x%.16"PRIx64"\n", el->ctx->fid));			
		}
	}

	/* Search for the fid folder_list entry */
	for (el = fsocpf_ctx->folders; el; el = el->next) {
		if (el->ctx && el->ctx->fid == fid) {
			found = true;
			break;
		}
	}
	if (found == false) {
		*data = NULL;
		return MAPISTORE_ERR_NO_DIRECTORY;
	}

	switch (table_type) {
	case MAPISTORE_FOLDER_TABLE:
		retval = fsocpf_get_property_from_folder_table(el->ctx, pos, proptag, data);
		break;
	case MAPISTORE_MESSAGE_TABLE:
		DEBUG(0, ("Not implemented yet\n"));
		break;
	default:
		break;
	}

	return retval;
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
	backend.op_mkdir = fsocpf_op_mkdir;
	backend.op_rmdir = fsocpf_op_rmdir;
	backend.op_opendir = fsocpf_op_opendir;
	backend.op_closedir = fsocpf_op_closedir;
	backend.op_readdir_count = fsocpf_op_readdir_count;
	backend.op_get_table_property = fsocpf_op_get_table_property;

	/* Register ourselves with the MAPISTORE subsystem */
	ret = mapistore_backend_register(&backend);
	if (ret != MAPISTORE_SUCCESS) {
		DEBUG(0, ("Failed to register the '%s' mapistore backend!\n", backend.name));
		return ret;
	}

	return MAPISTORE_SUCCESS;
}
