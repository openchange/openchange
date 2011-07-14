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

#include <util.h>
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

   \param _backend pointer to the mapistore backend to register

   \return MAPISTORE_SUCCESS on success
 */
_PUBLIC_ extern int mapistore_backend_register(const void *_backend)
{
	const struct mapistore_backend	*backend = _backend;
	uint32_t			i;

	/* Sanity checks */
	if (!backend) {
		return MAPISTORE_ERR_INVALID_PARAMETER;
	}

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend && backend && 
		    backend->name && backends[i].backend->name &&
		    !strcmp(backends[i].backend->name, backend->name)) {
			DEBUG(3, ("MAPISTORE backend '%s' already registered\n", backend->name));
			return MAPISTORE_SUCCESS;
		}
	}

	backends = realloc_p(backends, struct mstore_backend, num_backends + 1);
	if (!backends) {
		smb_panic("out of memory in mapistore_backend_register");
	}

	backends[num_backends].backend = smb_xmemdup(backend, sizeof (*backend));
	backends[num_backends].backend->name = smb_xstrdup(backend->name);

	num_backends++;

	DEBUG(3, ("MAPISTORE backend '%s' registered\n", backend->name));

	return MAPISTORE_SUCCESS;
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

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapistore_backend_run_init(init_backend_fn *fns)
{
	int				i;
	bool				ret = true;

	if (fns == NULL) {
		return true;
	}

	for (i = 0; fns[i]; i++) {
		ret &= (bool)fns[i]();
	}

	return ret;
}


/**
   \details Initialize mapistore backends

   \param mem_ctx pointer to the memory context
   \param path pointer to folder where mapistore backends are
   installed

   \return MAPISTORE_SUCCESS on success, otherwise
   MAPISTORE_ERR_BACKEND_INIT
 */
int mapistore_backend_init(TALLOC_CTX *mem_ctx, const char *path)
{
	init_backend_fn			*ret;
	bool				status;
	int				retval;
	int				i;

	ret = mapistore_backend_load(mem_ctx, path);
	status = mapistore_backend_run_init(ret);
	talloc_free(ret);

	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend) {
			DEBUG(3, ("MAPISTORE backend '%s' loaded\n", backends[i].backend->name));
			retval = backends[i].backend->init();
		}
	}

	return (status != true) ? MAPISTORE_SUCCESS : MAPISTORE_ERR_BACKEND_INIT;
}

static int delete_context(void *data)
{
	struct backend_context		*context = (struct backend_context *) data;

	context->backend->delete_context(context->private_data);

	return 0;
}

/**
   \details Create backend context

   \param mem_ctx pointer to the memory context
   \param namespace the backend namespace
   \param uri the backend parameters which can be passes inline

   \return a valid backend_context pointer on success, otherwise NULL
 */
struct backend_context *mapistore_backend_create_context(TALLOC_CTX *mem_ctx, struct mapistore_connection_info *conn_info, const char *namespace, 
							 const char *uri, uint64_t fid)
{
	struct backend_context		*context;
	int				retval;
	bool				found = false;
	void				*private_data = NULL;
	int				i;

	DEBUG(0, ("namespace is %s and backend_uri is '%s'\n", namespace, uri));
	for (i = 0; i < num_backends; i++) {
		if (backends[i].backend->namespace && 
		    !strcmp(namespace, backends[i].backend->namespace)) {
			found = true;
			retval = backends[i].backend->create_context(mem_ctx, conn_info, uri, fid, &private_data);
			if (retval != MAPISTORE_SUCCESS) {
				return NULL;
			}

			break;
		}
	}
	if (found == false) {
		DEBUG(0, ("MAPISTORE: no backend with namespace '%s' is available\n", namespace));
		return NULL;
	}

	context = talloc_zero(mem_ctx, struct backend_context);
	talloc_set_destructor((void *)context, (int (*)(void *))delete_context);
	context->backend = backends[i].backend;
	context->private_data = private_data;
	context->ref_count = 0;
	context->uri = talloc_strdup(context, uri);
	talloc_steal(context, private_data);

	return context;
}

/**
   \details Increase the ref count associated to a given backend

   \param bctx pointer to the backend context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE_ERROR
 */
_PUBLIC_ int mapistore_backend_add_ref_count(struct backend_context *bctx)
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
_PUBLIC_ int mapistore_backend_delete_context(struct backend_context *bctx)
{
	int	ret;

	if (!bctx->backend->delete_context) return MAPISTORE_ERROR;

	if (bctx->indexing) {
		mapistore_indexing_del_ref_count(bctx->indexing);
	}

	if (bctx->ref_count) {
		bctx->ref_count -= 1;
		return MAPISTORE_ERR_REF_COUNT;
	}

	ret = bctx->backend->delete_context(bctx->private_data);
	talloc_set_destructor((void *)bctx, NULL);
	
	return ret;
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

int mapistore_get_path(struct backend_context *bctx, TALLOC_CTX *mem_ctx, uint64_t fmid, uint8_t type, char **path)
{
	int	ret;
	char	*bpath = NULL;

	ret = bctx->backend->get_path(bctx->private_data, mem_ctx, fmid, type, &bpath);

	if (!ret) {
		*path = talloc_asprintf(bctx, "%s%s", bctx->backend->namespace, bpath);
	} else {
		*path = NULL;
	}

	return ret;
}

int mapistore_backend_opendir(struct backend_context *bctx, uint64_t fid)
{
	return bctx->backend->op_opendir(bctx->private_data, fid);
}


int mapistore_backend_mkdir(struct backend_context *bctx, 
			    uint64_t parent_fid, 
			    uint64_t fid,
			    struct SRow *aRow)
{
	return bctx->backend->op_mkdir(bctx->private_data, parent_fid, fid, aRow);
}

int mapistore_backend_rmdir(struct backend_context *bctx,
			    uint64_t parent_fid,
			    uint64_t fid)
{
	return bctx->backend->op_rmdir(bctx->private_data, parent_fid, fid);
}

int mapistore_backend_readdir_count(struct backend_context *bctx, uint64_t fid, uint8_t table_type, 
				    uint32_t *RowCount)
{
	int		ret;

	ret = bctx->backend->op_readdir_count(bctx->private_data, fid, table_type, RowCount);

	return ret;
}


int mapistore_backend_openmessage(struct backend_context *bctx, TALLOC_CTX *mem_ctx, uint64_t parent_fid, uint64_t mid, 
				  void **messagep, struct mapistore_message **msg)
{
	return bctx->backend->op_openmessage(bctx->private_data, mem_ctx, parent_fid, mid, messagep, msg);
}


int mapistore_backend_createmessage(struct backend_context *bctx, TALLOC_CTX *mem_ctx, uint64_t parent_fid, uint64_t mid, uint8_t associated, void **messagep)
{
	return bctx->backend->op_createmessage(bctx->private_data, mem_ctx, parent_fid, mid, associated, messagep);
}


int mapistore_backend_getprops(struct backend_context *bctx, TALLOC_CTX *mem_ctx, uint64_t fmid, uint8_t type, 
			       struct SPropTagArray *SPropTagArray, struct SRow *aRow)
{
	return bctx->backend->op_getprops(bctx->private_data, mem_ctx, fmid, type, SPropTagArray, aRow);
}

int mapistore_backend_setprops(struct backend_context *bctx, uint64_t fmid, uint8_t type, struct SRow *aRow)
{
	return bctx->backend->op_setprops(bctx->private_data, fmid, type, aRow);
}

int mapistore_backend_deletemessage(struct backend_context *bctx, uint64_t fid, uint64_t mid, uint8_t flags)
{
        return bctx->backend->op_deletemessage(bctx->private_data, fid, mid, flags);
}

/* proof of concept */
int mapistore_backend_pocop_open_table(struct backend_context *bctx, TALLOC_CTX *mem_ctx, uint64_t fid, uint8_t table_type,
                                       uint32_t handle_id, void **table, uint32_t *row_count)
{
        return bctx->backend->folder.open_table(bctx->private_data, mem_ctx, fid, table_type, handle_id, table, row_count);
}

int mapistore_backend_pocop_message_modify_recipients(struct backend_context *bctx, void *message, struct ModifyRecipientRow *row, uint16_t count)
{
	return bctx->backend->message.modify_recipients(message, row, count);
}

int mapistore_backend_pocop_message_save(struct backend_context *bctx, void *message)
{
	return bctx->backend->message.save(message);
}

int mapistore_backend_pocop_message_submit(struct backend_context *bctx, void *message, enum SubmitFlags flags)
{
	return bctx->backend->message.submit(message, flags);
}

int mapistore_backend_pocop_get_attachment_table(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, void **table, uint32_t *row_count)
{
        return bctx->backend->message.get_attachment_table(message, mem_ctx, table, row_count);
}

int mapistore_backend_pocop_get_attachment(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, uint32_t aid, void **attachment)
{
        return bctx->backend->message.get_attachment(message, mem_ctx, aid, attachment);
}

int mapistore_backend_pocop_create_attachment(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, void **attachment, uint32_t *aid)
{
        return bctx->backend->message.create_attachment(message, mem_ctx, attachment, aid);
}

int mapistore_backend_pocop_open_embedded_message(struct backend_context *bctx, void *message, TALLOC_CTX *mem_ctx, void **embedded_message, uint64_t *mid, struct mapistore_message **msg)
{
        return bctx->backend->message.open_embedded_message(message, mem_ctx, embedded_message, mid, msg);
}

int mapistore_backend_pocop_get_available_table_properties(struct backend_context *bctx, void *table, TALLOC_CTX *mem_ctx, struct SPropTagArray **propertiesp)
{
        return bctx->backend->table.get_available_properties(table, mem_ctx, propertiesp);
}

int mapistore_backend_pocop_set_table_columns(struct backend_context *bctx, void *table,
                                              uint16_t count, enum MAPITAGS *properties)
{
        return bctx->backend->table.set_columns(table, count, properties);
}

int mapistore_backend_pocop_set_table_restrictions(struct backend_context *bctx, void *table,
                                                   struct mapi_SRestriction *restrictions, uint8_t *table_status)
{
        return bctx->backend->table.set_restrictions(table, restrictions, table_status);
}

int mapistore_backend_pocop_set_table_sort_order(struct backend_context *bctx, void *table,
                                                 struct SSortOrderSet *sort_order, uint8_t *table_status)
{
        return bctx->backend->table.set_sort_order(table, sort_order, table_status);
}

int mapistore_backend_pocop_get_table_row(struct backend_context *bctx, void *table, TALLOC_CTX *mem_ctx,
                                          enum table_query_type query_type, uint32_t rowid,
                                          struct mapistore_property_data **data)
{
        return bctx->backend->table.get_row(table, mem_ctx, query_type, rowid, data);
}

int mapistore_backend_pocop_get_available_properties(struct backend_context *bctx, void *object, TALLOC_CTX *mem_ctx, struct SPropTagArray **propertiesp)
{
        return bctx->backend->properties.get_available_properties(object, mem_ctx, propertiesp);
}

int mapistore_backend_pocop_get_properties(struct backend_context *bctx,
                                           void *object, TALLOC_CTX *mem_ctx,
                                           uint16_t count, enum MAPITAGS
                                           *properties,
                                           struct mapistore_property_data *data)
{
        return bctx->backend->properties.get_properties(object, mem_ctx, count, properties, data);
}

int mapistore_backend_pocop_set_properties(struct backend_context *bctx, void *object, struct SRow *aRow)
{
        return bctx->backend->properties.set_properties(object, aRow);
}
