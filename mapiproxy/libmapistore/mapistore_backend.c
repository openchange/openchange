/*
   OpenChange Storage Abstraction Layer library

   OpenChange Project

   Note: init and load functions have been copied from
   samba4/source4/param/util.c initially wrote by Jelmer.

   Copyright (C) Jelmer Vernooij 2005-2007
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

#include <sys/types.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>

#include "mapistore.h"
#include "mapistore_errors.h"
#include "mapistore_private.h"
#include <dlinklist.h>

#include <samba_util.h>
#include <util/debug.h>

/**
   \file mapistore_backend.c

   \brief mapistore backends management API
 */


static struct mstore_backend {
	struct mapistore_backend	*backend;
} *backends = NULL;

int					num_backends;


/**
   \details Register mapistore backends

   \param backend pointer to the mapistore backend to register

   \return MAPISTORE_SUCCESS on success
 */
_PUBLIC_ enum mapistore_error mapistore_backend_register(const void *_backend)
{
	const struct mapistore_backend	*backend = _backend;
	uint32_t			i;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!backend, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend && backend && 
		    backend->backend.name && backends[i].backend->backend.name &&
		    !strcmp(backends[i].backend->backend.name, backend->backend.name)) {
			DEBUG(3, ("MAPISTORE backend '%s' already registered\n", backend->backend.name));
			return MAPISTORE_SUCCESS;
		}
	}

	backends = realloc_p(backends, struct mstore_backend, num_backends + 1);
	if (!backends) {
		smb_panic("out of memory in mapistore_backend_register");
	}

	backends[num_backends].backend = smb_xmemdup(backend, sizeof (*backend));
	backends[num_backends].backend->backend.name = smb_xstrdup(backend->backend.name);

	num_backends++;

	DEBUG(3, ("MAPISTORE backend '%s' registered\n", backend->backend.name));

	return MAPISTORE_SUCCESS;
}

/**
   \details Check if the specified backend is registered given its
   name.

   \param name backend's name to lookup

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_backend_registered(const char *name)
{
	int	i;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!name, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend && !strcmp(backends[i].backend->backend.name, name)) {
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}


/**
   \details Return the full path where mapistore backends are
   installed.

   \return Pointer to the full path where backends are installed.
 */
_PUBLIC_ const char *mapistore_backend_get_installdir(void)
{
	return MAPISTORE_BACKEND_INSTALLDIR;
}



/**
   \details Obtain the backend init function from a shared library
   file

   \param path full path to the backend shared library

   \return Pointer to the initialization function on success,
   otherwise NULL.
 */
static init_backend_fn load_backend(const char *path)
{
	void	*handle;
	void	*init_fn;

	handle = dlopen(path, RTLD_NOW);
	if (handle == NULL) {
		DEBUG(0, ("Unable to open %s: %s\n", path, dlerror()));
		return NULL;
	}

	init_fn = dlsym(handle, MAPISTORE_INIT_MODULE);

	if (init_fn == NULL) {
		DEBUG(0, ("Unable to find %s() in %s: %s\n",
			  MAPISTORE_INIT_MODULE, path, dlerror()));
		DEBUG(1, ("Loading mapistore backend '%s' failed\n", path));
		dlclose(handle);
		return NULL;
	}

	return (init_backend_fn) init_fn;
}


/**
   \details Load backends from specified directory

   \param mem_ctx pointer to the memory context
   \param path name of the backend's shared library folder

   \return allocated array of functions pointers to initialization
   functions on success, otherwise NULL.
 */
static init_backend_fn *load_backends(TALLOC_CTX *mem_ctx, const char *path)
{
	DIR		*dir;
	struct dirent	*entry;
	char		*filename;
	int		success = 0;
	init_backend_fn	*ret;

	ret = talloc_array(mem_ctx, init_backend_fn, 2);
	ret[0] = NULL;

	dir = opendir(path);
	if (dir == NULL) {
		talloc_free(ret);
		return NULL;
	}

	while ((entry = readdir(dir))) {
		if (ISDOT(entry->d_name) || ISDOTDOT(entry->d_name)) {
			continue;
		}
		
		filename = talloc_asprintf(mem_ctx, "%s/%s", path, entry->d_name);
		ret[success] = load_backend(filename);
		if (ret[success]) {
			ret = talloc_realloc(mem_ctx, ret, init_backend_fn, success + 2);
			success++;
			ret[success] = NULL;
		}

		talloc_free(filename);
	}

	closedir(dir);

	return ret;
}


/**
   \details Load the initialization functions from backends DSO

   \param mem_ctx pointer to the memory context
   \param path pointer to the backend's DSO folder

   \return allocated array of functions pointers to initialization
   functions on success, otherwise NULL.
 */
_PUBLIC_ init_backend_fn *mapistore_backend_load(TALLOC_CTX *mem_ctx, const char *path)
{
	if (!path) {
		path = mapistore_backend_get_installdir();
	}

	return load_backends(mem_ctx, path);
}


/**
   \details Run specified initialization functions.

   \param fns pointer to an array of mapistore backends initialization
   functions

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_backend_run_init(init_backend_fn *fns)
{
	int				i;
	enum mapistore_error		retval = MAPISTORE_SUCCESS;

	if (fns == NULL) {
		return MAPISTORE_ERR_NOT_FOUND;
	}

	for (i = 0; fns[i]; i++) {
		retval &= (bool)fns[i]();
		if (retval != MAPISTORE_SUCCESS) {
			return retval;
		}
	}

	return retval;
}


/**
   \details Initialize mapistore backends

   \param mem_ctx pointer to the memory context
   \param path pointer to folder where mapistore backends are
   installed

   \return MAPISTORE_SUCCESS on success, otherwise
   MAPISTORE_ERR_BACKEND_INIT
 */
enum mapistore_error mapistore_backend_init(TALLOC_CTX *mem_ctx, const char *path)
{
	init_backend_fn			*ret;
	enum mapistore_error		retval = MAPISTORE_SUCCESS;
	int				i;

	ret = mapistore_backend_load(mem_ctx, path);
	retval = mapistore_backend_run_init(ret);
	talloc_free(ret);

	retval = mapistore_python_load_and_run(mem_ctx, path);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend) {
			retval = backends[i].backend->backend.init(backends[i].backend->backend.name);
			if (retval != MAPISTORE_SUCCESS) {
				DEBUG(3, ("[!] MAPISTORE backend '%s' initialization failed\n", backends[i].backend->backend.name));
				return retval;
			} else {
				DEBUG(3, ("MAPISTORE backend '%s' loaded\n", backends[i].backend->backend.name));
			}
		}
	}

	return retval;
}

/**
   \details List backend names for given user

   \param mem_ctx pointer to the memory context the list of backends will hang off

   \return valid pointers to a list of strings with the backend names and the number of backends on success, otherwise NULL
 */
enum mapistore_error mapistore_backend_list_backend_names(TALLOC_CTX *mem_ctx, int *backend_countP, const char ***backend_namesP)
{
	const char			**backend_names;
	int 				i;

	MAPISTORE_RETVAL_IF(!backend_namesP, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	backend_names = talloc_array(mem_ctx, const char *, num_backends);
	for (i = 0; i < num_backends; i++) {
		backend_names[i] = backends[i].backend->backend.name;
	}

	*backend_countP = num_backends;
	*backend_namesP = backend_names;
	return MAPISTORE_SUCCESS;
}

/**
   \details List backend contexts for given user

   \param mem_ctx pointer to the memory context
   \param namespace the backend namespace
   \param uri the backend parameters which can be passed inline

   \return a valid backend_context pointer on success, otherwise NULL
 */
enum mapistore_error mapistore_backend_list_contexts(const char *username, struct indexing_context *ictx, TALLOC_CTX *mem_ctx, struct mapistore_contexts_list **contexts_listP)
{
	enum mapistore_error		retval;
	int				i;
	struct mapistore_contexts_list	*contexts_list = NULL, *current_contexts_list;

	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!contexts_listP, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	for (i = 0; i < num_backends; i++) {
		retval = backends[i].backend->backend.list_contexts(mem_ctx, backends[i].backend->backend.name, username, ictx, &current_contexts_list);
		if (retval != MAPISTORE_SUCCESS) {
			DEBUG(0, ("[WARN] list contexts for %s failed with %s\n",
				  backends[i].backend->backend.name,
				  mapistore_errstr(retval)));
			continue;
		}
		DLIST_CONCATENATE(contexts_list, current_contexts_list, void);
	}

	*contexts_listP = contexts_list;
	(void) talloc_reference(mem_ctx, contexts_list);

	return MAPISTORE_SUCCESS;
}

/**
   \details Create backend context

   \param mem_ctx pointer to the memory context
   \param conn_info pointer to the mapistore connection information
   \param ictx pointer to the indexing context
   \param namespace the backend namespace
   \param uri the backend parameters which can be passes inline
   \param fid the folder identifier of the folder
   \param context_p pointer on pointer to the backend_context to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_backend_create_context(TALLOC_CTX *mem_ctx,
						      struct mapistore_connection_info *conn_info,
						      struct indexing_context *ictx,
						      const char *namespace,
						      const char *uri,
						      uint64_t fid,
						      struct backend_context **context_p)
{
	struct backend_context		*context;
	enum mapistore_error		retval;
	bool				found = false;
	void				*backend_object = NULL;
	int				i;

	DEBUG(5, ("namespace is %s and backend_uri is '%s'\n", namespace, uri));

	context = talloc_zero(mem_ctx, struct backend_context);
	MAPISTORE_RETVAL_IF(!context, MAPISTORE_ERR_NO_MEMORY, NULL);

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend->backend.namespace && 
		    !strcmp(namespace, backends[i].backend->backend.namespace)) {
			found = true;
			retval = backends[i].backend->backend.create_context(context, backends[i].backend->backend.name, conn_info, ictx, uri, &backend_object);
			MAPISTORE_RETVAL_IF(retval, retval, context);

			break;
		}
	}

	if (found == false) {
		DEBUG(0, ("MAPISTORE: no backend with namespace '%s' is available\n", namespace));
		talloc_free(context);
		return MAPISTORE_ERR_NOT_FOUND;
	}

	context->backend_object = backend_object;
	context->backend = backends[i].backend;
	retval = context->backend->context.get_root_folder(context, backend_object,
							   fid, &context->root_folder_object);
	MAPISTORE_RETVAL_IF(retval, retval, context);

	context->ref_count = 1;
	context->uri = talloc_asprintf(context, "%s%s", namespace, uri);
	MAPISTORE_RETVAL_IF(!context->uri, MAPISTORE_ERR_NO_MEMORY, context);
	*context_p = context;

	return retval;
}


enum mapistore_error mapistore_backend_create_root_folder(const char *username, enum mapistore_context_role ctx_role, uint64_t fid, const char *name, TALLOC_CTX *mem_ctx, char **mapistore_urip)
{
	enum mapistore_error		retval = MAPISTORE_ERR_NOT_FOUND;
	int				i;

	for (i = 0; retval == MAPISTORE_ERR_NOT_FOUND && i < num_backends; i++) {
		retval = backends[i].backend->backend.create_root_folder(username, ctx_role, fid, name, mem_ctx, mapistore_urip);
	}

	return retval;
}

/**
   \details Increase the ref count associated to a given backend

   \param bctx pointer to the backend context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ enum mapistore_error mapistore_backend_add_ref_count(struct backend_context *bctx)
{
	if (!bctx) {
		return MAPISTORE_ERROR;
	}

	bctx->ref_count += 1;

	return MAPISTORE_SUCCESS;
}


/**
   \details Delete a context from the specified backend

   \param bctx pointer to the backend context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum mapistore_error mapistore_backend_delete_context(struct backend_context *bctx)
{
	bctx->ref_count -= 1;
	if (bctx->ref_count) {
		return MAPISTORE_ERR_REF_COUNT;
	}

	talloc_free(bctx);
	
	return MAPISTORE_SUCCESS;
}


/**
   \details find the context matching given context identifier

   \param backend_list_ctx pointer to the backend context list
   \param context_id the context identifier to search

   \return Pointer to the mapistore_backend context on success, otherwise NULL
 */
_PUBLIC_ struct backend_context *mapistore_backend_lookup(struct backend_context_list *backend_list_ctx,
							  uint32_t context_id)
{
	struct backend_context_list	*el;

	/* Sanity checks */
	if (!backend_list_ctx) return NULL;

	for (el = backend_list_ctx; el; el = el->next) {
		if (el->ctx && el->ctx->context_id == context_id) {
			return el->ctx;
		}
	}

	return NULL;
}

/**
   \details find the context matching given uri string

   \param backend_list_ctx pointer to the backend context list
   \param uri the uri string to search

   \return Pointer to the mapistore_backend context on success,
   otherwise NULL
 */
_PUBLIC_ struct backend_context *mapistore_backend_lookup_by_uri(struct backend_context_list *backend_list_ctx,
								 const char *uri)
{
	struct backend_context_list	*el;

	/* sanity checks */
	if (!backend_list_ctx) return NULL;
	if (!uri) return NULL;

	for (el = backend_list_ctx; el; el = el->next) {
		if (el->ctx && el->ctx->uri &&
		    !strcmp(el->ctx->uri, uri)) {
			return el->ctx;
		}
	}
	
	return NULL;
}

/**
   \details Return a pointer on backend functions given its name

   \param mem_ctx pointer to the memory context
   \param name the backend name to lookup

   \return Allocated pointer to the mapistore_backend context on success,
   otherwise NULL
 */
_PUBLIC_ struct backend_context *mapistore_backend_lookup_by_name(TALLOC_CTX *mem_ctx, const char *name)
{
	struct backend_context	*context = NULL;
	int			i;

	/* Sanity checks */
	if (!name) return NULL;

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend && !strcmp(backends[i].backend->backend.name, name)) {
			context = talloc_zero(mem_ctx, struct backend_context);
			context->backend = backends[i].backend;
			context->ref_count = 0;
			context->uri = NULL;

			return context;
		}
	}
	return NULL;
}


enum mapistore_error mapistore_backend_get_path(TALLOC_CTX *mem_ctx, struct backend_context *bctx, uint64_t fmid, char **path)
{
	enum mapistore_error	ret;
	char			*bpath = NULL;

	ret = bctx->backend->context.get_path(mem_ctx, bctx->backend_object, fmid, &bpath);

	if (ret == MAPISTORE_SUCCESS) {
		if (!bpath) {
			DEBUG(3, ("%s: Mapistore backend return SUCCESS, but path url is NULL\n", __location__));
			return MAPISTORE_ERR_INVALID_DATA;
		}
		*path = talloc_asprintf(mem_ctx, "%s%s", bctx->backend->backend.namespace, bpath);
	} else {
		*path = NULL;
	}

	return ret;
}

enum mapistore_error mapistore_backend_folder_open_folder(struct backend_context *bctx, void *folder, TALLOC_CTX *mem_ctx, uint64_t fid, void **child_folder)
{
	return bctx->backend->folder.open_folder(mem_ctx, folder, fid, child_folder);
}

enum mapistore_error mapistore_backend_folder_create_folder(struct backend_context *bctx, void *folder,
					   TALLOC_CTX *mem_ctx, uint64_t fid, struct SRow *aRow, void **child_folder)
{
	return bctx->backend->folder.create_folder(mem_ctx, folder, fid, aRow, child_folder);
}

enum mapistore_error mapistore_backend_folder_delete(struct backend_context *bctx, void *folder)
{
	return bctx->backend->folder.delete(folder);
}

enum mapistore_error mapistore_backend_folder_open_message(struct backend_context *bctx, void *folder,
					  TALLOC_CTX *mem_ctx, uint64_t mid, bool read_write, void **messagep)
{
	return bctx->backend->folder.open_message(mem_ctx, folder, mid, read_write, messagep);
}

enum mapistore_error mapistore_backend_folder_create_message(struct backend_context *bctx, void *folder, TALLOC_CTX *mem_ctx, uint64_t mid, uint8_t associated, void **messagep)
{
	return bctx->backend->folder.create_message(mem_ctx, folder, mid, associated, messagep);
}

enum mapistore_error mapistore_backend_folder_delete_message(struct backend_context *bctx, void *folder, uint64_t mid, uint8_t flags)
{
        return bctx->backend->folder.delete_message(folder, mid, flags);
}

enum mapistore_error mapistore_backend_folder_move_copy_messages(struct backend_context *bctx, void *target_folder, void *source_folder, TALLOC_CTX *mem_ctx, uint32_t mid_count, uint64_t *source_mids, uint64_t *target_mids, struct Binary_r **target_change_keys, uint8_t want_copy)
{
	return bctx->backend->folder.move_copy_messages(target_folder, source_folder, mem_ctx, mid_count, source_mids, target_mids, target_change_keys, want_copy);
}

enum mapistore_error mapistore_backend_folder_move_folder(struct backend_context *bctx, void *move_folder, void *target_folder, TALLOC_CTX *mem_ctx, const char *new_folder_name)
{
        return bctx->backend->folder.move_folder(move_folder, target_folder, mem_ctx, new_folder_name);
}

enum mapistore_error mapistore_backend_folder_copy_folder(struct backend_context *bctx, void *move_folder, void *target_folder, TALLOC_CTX *mem_ctx, bool recursive, const char *new_folder_name)
{
        return bctx->backend->folder.copy_folder(move_folder, target_folder, mem_ctx, recursive, new_folder_name);
}

enum mapistore_error mapistore_backend_folder_get_deleted_fmids(struct backend_context *bctx, void *folder, TALLOC_CTX *mem_ctx, enum mapistore_table_type table_type, uint64_t change_num, struct UI8Array_r **fmidsp, uint64_t *cnp)
{
        return bctx->backend->folder.get_deleted_fmids(folder, mem_ctx, table_type, change_num, fmidsp, cnp);
}

enum mapistore_error mapistore_backend_folder_get_child_count(struct backend_context *bctx, void *folder, enum mapistore_table_type table_type, uint32_t *RowCount)
{
	return bctx->backend->folder.get_child_count(folder, table_type, RowCount);
}

enum mapistore_error mapistore_backend_folder_get_child_fid_by_name(struct backend_context *bctx, void *folder, const char *name, uint64_t *fidp)
{
	enum mapistore_error		ret = MAPISTORE_SUCCESS;
	struct mapi_SRestriction	name_restriction;
	void				*table;
	uint8_t				status;
	TALLOC_CTX			*mem_ctx;
	enum MAPITAGS			col;
	struct mapistore_property_data	*data_pointers;
	uint32_t			row_count;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	
	if (mapistore_backend_folder_open_table(bctx, folder, mem_ctx, MAPISTORE_FOLDER_TABLE, 0, &table, &row_count)) {
		talloc_free(mem_ctx);
		return MAPISTORE_ERROR;
	}

	name_restriction.rt = RES_PROPERTY;
	name_restriction.res.resProperty.relop = RELOP_EQ;
	name_restriction.res.resProperty.ulPropTag = PR_DISPLAY_NAME_UNICODE;
	name_restriction.res.resProperty.lpProp.ulPropTag = name_restriction.res.resProperty.ulPropTag;
	name_restriction.res.resProperty.lpProp.value.lpszW = name;

	col = PR_FID;
	mapistore_backend_table_set_columns(bctx, table, 1, &col);
	mapistore_backend_table_set_restrictions(bctx, table, &name_restriction, &status);
	ret = mapistore_backend_table_get_row(bctx, table, mem_ctx, MAPISTORE_PREFILTERED_QUERY, 0, &data_pointers);
	if (!ret) {
		if (data_pointers[0].error) {
			ret = MAPISTORE_ERROR;
		}
		else {
			*fidp = *(uint64_t *) data_pointers[0].data;
		}
	}

	talloc_free(mem_ctx);

	return ret;
}

enum mapistore_error mapistore_backend_folder_open_table(struct backend_context *bctx, void *folder,
							 TALLOC_CTX *mem_ctx, enum mapistore_table_type table_type, uint32_t handle_id, void **table, uint32_t *row_count)
{
        return bctx->backend->folder.open_table(mem_ctx, folder, table_type, handle_id, table, row_count);
}

enum mapistore_error mapistore_backend_folder_modify_permissions(struct backend_context *bctx, void *folder,
						uint8_t flags, uint16_t pcount, struct PermissionData *permissions)
{
        return bctx->backend->folder.modify_permissions(folder, flags, pcount, permissions);
}

enum mapistore_error mapistore_backend_folder_preload_message_bodies(struct backend_context *bctx, void *folder, enum mapistore_table_type table_type, const struct UI8Array_r *mids)
{
        return bctx->backend->folder.preload_message_bodies(folder, table_type, mids);
}

enum mapistore_error mapistore_backend_message_get_message_data(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, struct mapistore_message **msg)
{
	return bctx->backend->message.get_message_data(mem_ctx, message, msg);
}

enum mapistore_error mapistore_backend_message_modify_recipients(struct backend_context *bctx, void *message, struct SPropTagArray *columns, uint16_t count, struct mapistore_message_recipient *recipients)
{
	return bctx->backend->message.modify_recipients(message, columns, count, recipients);
}

enum mapistore_error mapistore_backend_message_set_read_flag(struct backend_context *bctx, void *message, uint8_t flag)
{
	return bctx->backend->message.set_read_flag(message, flag);
}

enum mapistore_error mapistore_backend_message_save(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx)
{
	return bctx->backend->message.save(mem_ctx, message);
}

enum mapistore_error mapistore_backend_message_submit(struct backend_context *bctx, void *message, enum SubmitFlags flags)
{
	return bctx->backend->message.submit(message, flags);
}

enum mapistore_error mapistore_backend_message_open_attachment(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, uint32_t aid, void **attachment)
{
	return bctx->backend->message.open_attachment(mem_ctx, message, aid, attachment);
}

enum mapistore_error mapistore_backend_message_create_attachment(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, void **attachment, uint32_t *aid)
{
        return bctx->backend->message.create_attachment(message, mem_ctx, attachment, aid);
}

enum mapistore_error mapistore_backend_message_delete_attachment(struct backend_context *bctx, void *message, uint32_t aid)
{
	return bctx->backend->message.delete_attachment(message, aid);
}

enum mapistore_error mapistore_backend_message_get_attachment_table(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, void **table, uint32_t *row_count)
{
	return bctx->backend->message.get_attachment_table(mem_ctx, message, table, row_count);
}

enum mapistore_error mapistore_backend_message_attachment_open_embedded_message(struct backend_context *bctx, void *attachment, TALLOC_CTX *mem_ctx, void **embedded_message, uint64_t *mid, struct mapistore_message **msg)
{
        return bctx->backend->message.open_embedded_message(attachment, mem_ctx, embedded_message, mid, msg);
}

enum mapistore_error mapistore_backend_message_attachment_create_embedded_message(struct backend_context *bctx, void *attachment, TALLOC_CTX *mem_ctx, void **embedded_message, struct mapistore_message **msg)
{
        return bctx->backend->message.create_embedded_message(attachment, mem_ctx, embedded_message, msg);
}

enum mapistore_error mapistore_backend_table_get_available_properties(struct backend_context *bctx, void *table, TALLOC_CTX *mem_ctx, struct SPropTagArray **propertiesp)
{
        return bctx->backend->table.get_available_properties(table, mem_ctx, propertiesp);
}

enum mapistore_error mapistore_backend_table_set_columns(struct backend_context *bctx, void *table, uint16_t count, enum MAPITAGS *properties)
{
        return bctx->backend->table.set_columns(table, count, properties);
}

enum mapistore_error mapistore_backend_table_set_restrictions(struct backend_context *bctx, void *table, struct mapi_SRestriction *restrictions, uint8_t *table_status)
{
        return bctx->backend->table.set_restrictions(table, restrictions, table_status);
}

enum mapistore_error mapistore_backend_table_set_sort_order(struct backend_context *bctx, void *table, struct SSortOrderSet *sort_order, uint8_t *table_status)
{
        return bctx->backend->table.set_sort_order(table, sort_order, table_status);
}

enum mapistore_error mapistore_backend_table_get_row(struct backend_context *bctx, void *table, TALLOC_CTX *mem_ctx,
						     enum mapistore_query_type query_type, uint32_t rowid,
						     struct mapistore_property_data **data)
{
        return bctx->backend->table.get_row(mem_ctx, table, query_type, rowid, data);
}

enum mapistore_error mapistore_backend_table_get_row_count(struct backend_context *bctx, void *table, enum mapistore_query_type query_type, uint32_t *row_countp)
{
        return bctx->backend->table.get_row_count(table, query_type, row_countp);
}

enum mapistore_error mapistore_backend_table_handle_destructor(struct backend_context *bctx, void *table, uint32_t handle_id)
{
        return bctx->backend->table.handle_destructor(table, handle_id);
}

enum mapistore_error mapistore_backend_properties_get_available_properties(struct backend_context *bctx, void *object, TALLOC_CTX *mem_ctx, struct SPropTagArray **propertiesp)
{
        return bctx->backend->properties.get_available_properties(object, mem_ctx, propertiesp);
}

enum mapistore_error mapistore_backend_properties_get_properties(struct backend_context *bctx,
						void *object, TALLOC_CTX *mem_ctx,
						uint16_t count, enum MAPITAGS
						*properties,
						struct mapistore_property_data *data)
{
	return bctx->backend->properties.get_properties(mem_ctx, object, count, properties, data);
}

enum mapistore_error mapistore_backend_properties_set_properties(struct backend_context *bctx, void *object, struct SRow *aRow)
{
        return bctx->backend->properties.set_properties(object, aRow);
}

enum mapistore_error mapistore_backend_manager_generate_uri(struct backend_context *bctx, TALLOC_CTX *mem_ctx, 
					   const char *username, const char *folder, 
					   const char *message, const char *root_uri, char **uri)
{
	return bctx->backend->manager.generate_uri(mem_ctx, username, folder, message, root_uri, uri);
}
