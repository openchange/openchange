/*
   OpenChange Server implementation

   MAPI logon map API implementation

   Copyright (C) Enrique J. Hernandez 2016

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
   \file mapi_logon.c

   \brief API for MAPI logon map as defined in [MS-OXCROPS] Section 3.1.4.2
   inside a RPC session
 */

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "libmapiproxy.h"
#include "../util/ccan/htable/htable.h"
#include "../util/ccan/hash/hash.h"

/* Rehash function for ht table */
static size_t _ht_rehash(const void* e, void* unused)
{
	return hash(&(((struct logon_table_value*)e)->logon_id), 1, 0);
}

/* Comparison function to get items from ht table */
static bool _ht_cmp(const void* e, void *id)
{
	if (!id) {
		return false;
	}
	return (((struct logon_table_value*) e)->logon_id == *(uint8_t*)id);
}

/**
   \details Initialise MAPI logon context

   \param mem_ctx pointer to the memory context

   \return Allocated MAPI logon context on success, otherwise NULL
 */
_PUBLIC_ struct mapi_logon_context* mapi_logon_init(TALLOC_CTX *mem_ctx)
{
	struct mapi_logon_context *logon_ctx;

	/* Step 1. Initialise the context */
	logon_ctx = talloc_zero(mem_ctx, struct mapi_logon_context);
	if (!logon_ctx) {
		return NULL;
	}

	/* Step 2. Initialise the hash table */
	logon_ctx->logon_table = talloc_zero(logon_ctx, struct htable);
	if (!logon_ctx->logon_table) {
		return NULL;
	}
	htable_init(logon_ctx->logon_table, _ht_rehash, NULL);

	return logon_ctx;
}

/**
   \details Release MAPI handles context

   \param handles_ctx pointer to the MAPI handles context

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_logon_release(struct mapi_logon_context *logon_ctx)
{
	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!logon_ctx, MAPI_E_NOT_INITIALIZED, NULL);

	htable_clear(logon_ctx->logon_table);
	talloc_free(logon_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Search for the logon_id in Logon Table

   \param logon_ctx pointer to the MAPI logon context
   \param logon_id the logon identifier
   \param username pointer to the username associated with that logon_id if success

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_logon_search(struct mapi_logon_context *logon_ctx,
					   uint8_t logon_id, const char **username_p)
{
	struct logon_table_value *entry;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!logon_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username_p, MAPI_E_INVALID_PARAMETER, NULL);

	entry = (struct logon_table_value *)htable_get(logon_ctx->logon_table, hash(&logon_id, 1, 0), _ht_cmp, &logon_id);
	if (entry) {
		*username_p = entry->username;
		return MAPI_E_SUCCESS;
	}

	return MAPI_E_NOT_FOUND;
}

/**
   \details Set the map between a logon_id and a username

   \param logon_ctx pointer to the MAPI logon context
   \param logon_id the logon id to set
   \param username the associated username with that logon

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS mapi_logon_set(struct mapi_logon_context *logon_ctx,
					uint8_t logon_id, const char *username)
{
	bool retval;
	struct logon_table_value *new_entry, *old_entry;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!logon_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	new_entry = talloc_zero(logon_ctx, struct logon_table_value);
	OPENCHANGE_RETVAL_IF(!new_entry, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	new_entry->username = talloc_strdup(new_entry, username);
	OPENCHANGE_RETVAL_IF(!new_entry->username, MAPI_E_NOT_ENOUGH_MEMORY, new_entry);

	new_entry->logon_id = logon_id;

	old_entry = (struct logon_table_value *)htable_get(logon_ctx->logon_table,
							   hash(&logon_id, 1, 0), _ht_cmp,
							   &logon_id);
	if (old_entry) {
		retval = htable_del(logon_ctx->logon_table, hash(&logon_id, 1, 0),
				    old_entry);
		if (retval) {
			OC_DEBUG(5, "Reusing logon_id: %u", logon_id);
		}
	}

	retval = htable_add(logon_ctx->logon_table, hash(&logon_id, 1, 0), new_entry);
	OPENCHANGE_RETVAL_IF(retval == false, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	return MAPI_E_SUCCESS;
}
