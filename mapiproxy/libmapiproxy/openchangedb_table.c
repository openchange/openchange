/*
   OpenChange Server implementation

   OpenChangeDB table object implementation

   Copyright (C) Julien Kerihuel 2011
   Copyright (C) Jesús García Sáez 2014

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

/**
   \file openchangedb_table.c

   \brief OpenChange Dispatcher database table routines
 */

#include <inttypes.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_backends.h"

/**
   /details Initialize an openchangedb table

   \param mem_ctx pointer to the memory context to use for allocation
   \param username The name of the mailbox where the folder is
   \param table_type the type of table this object represents
   \param folderID the identifier of the folder this table represents
   \param table_object pointer on pointer to the table object to return

   \return MAPI_E_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_table_init(TALLOC_CTX *mem_ctx,
						 struct openchangedb_context *self,
						 const char *username,
						 uint8_t table_type,
						 uint64_t folderID,
						 void **table_object)
{
	MAPI_RETVAL_IF(!self, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!table_object, MAPI_E_NOT_INITIALIZED, NULL);

	return self->table_init(mem_ctx, self, username, table_type, folderID,
				table_object);
}


/**
   \details Set sort order to specified openchangedb table object

   \param table_object pointer to the table object
   \param lpSortCriteria pointer to the sort order to save

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_table_set_sort_order(struct openchangedb_context *self,
							   void *table_object,
							   struct SSortOrderSet *lpSortCriteria)
{
	MAPI_RETVAL_IF(!self, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!table_object, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!lpSortCriteria, MAPI_E_INVALID_PARAMETER, NULL);

	return self->table_set_sort_order(self, table_object, lpSortCriteria);
}


_PUBLIC_ enum MAPISTATUS openchangedb_table_set_restrictions(struct openchangedb_context *self,
							     void *table_object,
							     struct mapi_SRestriction *res)
{
	MAPI_RETVAL_IF(!self, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!table_object, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!res, MAPI_E_INVALID_PARAMETER, NULL);

	return self->table_set_restrictions(self, table_object, res);
}

_PUBLIC_ enum MAPISTATUS openchangedb_table_get_property(TALLOC_CTX *mem_ctx,
							 struct openchangedb_context *self,
							 void *table_object,
							 enum MAPITAGS proptag,
							 uint32_t pos,
							 bool live_filtered,
							 void **data)
{
	OPENCHANGE_RETVAL_IF(!self, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!table_object, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!data, MAPI_E_NOT_INITIALIZED, NULL);

	return self->table_get_property(mem_ctx, self, table_object, proptag, pos, live_filtered, data);
}
