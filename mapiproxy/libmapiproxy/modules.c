/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij 2011

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

#include "libmapi/libmapi.h"
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>

#define SAMBA_INIT_MODULE "samba_init_module"

static openchange_plugin_init_fn load_plugin(const char *path)
{
	void *handle;
	void *init_fn;
	char *error;

	handle = dlopen(path, RTLD_NOW);

	/* This call should reset any possible non-fatal errors that
	   occured since last call to dl* functions */
	error = dlerror();

	if (handle == NULL) {
		oc_log(OC_LOG_ERROR, "Error loading plugin '%s': %s", path, error ? error : "");
		return NULL;
	}

	init_fn = (openchange_plugin_init_fn)dlsym(handle, SAMBA_INIT_MODULE);

	if (init_fn == NULL) {
		oc_log(OC_LOG_ERROR, "Unable to find %s() in %s: %s",
			  SAMBA_INIT_MODULE, path, dlerror());
		dlclose(handle);
		return NULL;
	}

	return (openchange_plugin_init_fn)init_fn;
}

openchange_plugin_init_fn *load_openchange_plugins(TALLOC_CTX *mem_ctx, const char *subsystem)
{
	DIR *dir;
	struct dirent *entry;
	char *filename, *path;
	int success = 0;
	openchange_plugin_init_fn *ret = talloc_array(mem_ctx, openchange_plugin_init_fn, 2);

	ret[0] = NULL;

	path = talloc_asprintf(mem_ctx, "%s/%s", MODULESDIR, subsystem);
	if (path == NULL) {
		return NULL;
	}

	dir = opendir(path);
	if (dir == NULL) {
		talloc_free(path);
		talloc_free(ret);
		return NULL;
	}

	while((entry = readdir(dir))) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		filename = talloc_asprintf(mem_ctx, "%s/%s", path, entry->d_name);

		ret[success] = load_plugin(filename);
		if (ret[success]) {
			ret = talloc_realloc(mem_ctx, ret, openchange_plugin_init_fn, success+2);
			success++;
			ret[success] = NULL;
		}

		talloc_free(filename);
	}

	closedir(dir);
	talloc_free(path);

	return ret;
}
