/*
   Memcached util functions

   OpenChange Project

   Copyright (C) Jesús García Sáez 2015

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

#include "oc_memcached.h"
#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>
#include "ccan/htable/htable.h"
#include "ccan/hash/hash.h"
#include <libmapi/oc_log.h>

#define DEFAULT_CONFIG "default"

/* Items stored on ht table */
struct memc_item {
	memcached_st	*memc;
	const char	*config_string;
};

/* Rehash function for ht table */
static size_t _ht_rehash(const void *e, void *unused)
{
	return hash_string(((struct memc_item *)e)->config_string);
}

/* Comparison function to get items from ht table */
static bool _ht_cmp(const void *e, void *string)
{
	return strcmp(((struct memc_item *)e)->config_string, (const char *)string) == 0;
}

/* This is a dictionary [config_string] -> [memcached_st *] (actually struct memc_item) */
static struct htable ht = HTABLE_INITIALIZER(ht, _ht_rehash, NULL);


static memcached_st *_new_connection(const char *config_string)
{
	memcached_st		*memc;
	memcached_server_st     *servers = NULL;
	memcached_return	rc;

	if (config_string) {
		memc = memcached(config_string, strlen(config_string));
		if (!memc) {
			OC_DEBUG(3, "[memcached] Failed to initialize memcached with config string `%s`",
				 config_string);
			return NULL;
		}
	} else {
		memc = (memcached_st *) memcached_create(NULL);
		if (!memc) {
			OC_DEBUG(3, "[memcached] Failed memcached_create");
			return NULL;
		}

		servers = memcached_server_list_append(servers, MSTORE_MEMC_DFLT_HOST, MSTORE_MEMC_DFLT_PORT, &rc);
		rc = memcached_server_push(memc, servers);
		if (rc != MEMCACHED_SUCCESS) {
			OC_DEBUG(3, "[memcached] Failed to add server to memcached list");
			return NULL;
		}
	}

	return memc;
}

static bool _save_connection(const char *config_string, memcached_st *memc)
{
	struct memc_item	*entry = NULL;

	/* This entries will never be deallocated */
	entry = talloc_zero(talloc_autofree_context(), struct memc_item);
	entry->config_string = talloc_strdup(entry, config_string);
	if (entry->config_string == NULL) {
		OC_DEBUG(3, "[memcached] Failed talloc_strdup: out of memory?");
		return false;
	}
	entry->memc = memc;

	/* Store the new connection in our table */
	if (!htable_add(&ht, hash_string(config_string), entry)) {
		OC_DEBUG(3, "[memcached] Error adding new memcached connection");
		return false;
	}
	OC_DEBUG(5, "[memcached] Stored new connection %"PRIu32, hash_string(config_string));
	return true;
}

static memcached_st *_get_connection(const char *config_string)
{
	struct memc_item	*entry = NULL;

	entry = htable_get(&ht, hash_string(config_string), _ht_cmp, config_string);
	if (!entry) return NULL;
	OC_DEBUG(5, "[memcached] Found connection, reusing it %"PRIu32, hash_string(config_string));
	return entry->memc;
}

/**
   \details Returns a memcached_st pointer to use it in all libmemcached
   functions.

   \param config_string Config string to initialize a memcached context,
   in case of being NULL it will default to MSTORE_MEMC_DFLT_HOST.
   For more info about format check online doc of libmemcached:

       http://docs.libmemcached.org/libmemcached_configuration.html

   \param shared if true internally this connections will be cached and
   reused between different calls (one connection per config_string).
   If false the connection returned won't be cached or returned to another
   caller.

 */
memcached_st *oc_memcached_new_connection(const char *config_string, bool shared)
{
	memcached_st	*memc;
	const char	*key;

	if (shared) {
		key = config_string ? config_string : DEFAULT_CONFIG;
		memc = _get_connection(key);

		if (!memc) {
			memc = _new_connection(config_string);

			if (memc && !_save_connection(key, memc)) {
				memcached_free(memc);
				return NULL;
			}
		}
	} else {
		memc = _new_connection(config_string);
	}

	return memc;
}

/**
   \details Close and free one memcached connection.

   \param memc The memcached_st object
   \param shared Whether this connection could be shared or not, this
   parameter must have the same value as when the connection was created

   \note Actually right now the connections if they are shared are not
   neither closed nor freed because we reuse them between users and operations.
 */
void oc_memcached_release_connection(memcached_st *memc, bool shared)
{
	if (!shared) {
		memcached_free(memc);
	}
	/* else: Do nothing, connection shared and could be reused */
}
