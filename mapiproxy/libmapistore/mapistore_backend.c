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
#include <libmapi/dlinklist.h>

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
   \param pointer to the backends shared library folder

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


/**
   \details Create backend context

   \param mem_ctx pointer to the memory context
   \param namespace the backend namespace
   \param uri the backend parameters which can be passes inline

   \return a valid backend_context pointer on success, otherwise NULL
 */
struct backend_context *mapistore_backend_create_context(TALLOC_CTX *mem_ctx, const char *namespace, 
							 const char *uri)
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
			retval = backends[i].backend->create_context(mem_ctx, uri, &private_data);
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
	context->backend = backends[i].backend;
	context->private_data = private_data;
	talloc_steal(context, context->private_data);

	return context;
}


/**
   \details Delete a context from the specified backend

   \param bctx pointer to the backend context

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ int mapistore_backend_delete_context(struct backend_context *bctx)
{
	if (!bctx->backend->delete_context) return MAPISTORE_ERROR;

	return bctx->backend->delete_context(bctx->private_data);
}
