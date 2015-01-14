/*
   OpenChange Server implementation

   OpenChangeDB message object implementation

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
   \file openchangedb_message.c

   \brief OpenChange Dispatcher database message routines
 */

#include <inttypes.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapistore/mapistore.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_backends.h"


/**
   \details Initialize and create a message object

   \param mem_ctx pointer to the memory context to use for allocation
   \param oc_ctx pointer to the openchangedb context
   \param username The name of the mailbox where the parent folder of the
   message is.
   \param messageID the identifier of the message to create
   \param folderID the identifier of the folder where the message is created
   \param message_object pointer on pointer to the message object to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_
enum MAPISTATUS openchangedb_message_create(TALLOC_CTX *mem_ctx,
					    struct openchangedb_context *oc_ctx,
					    const char *username,
					    uint64_t messageID,
					    uint64_t folderID,
					    bool fai,
					    void **message_object)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->message_create(mem_ctx, oc_ctx, username, messageID,
				      folderID, fai, message_object);
}

/**
   \details Save (commit) message in openchangedb database

   \param msg the message object
   \param SaveFlags flags associated to the save operation

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_
enum MAPISTATUS openchangedb_message_save(struct openchangedb_context *oc_ctx,
					  void *msg, uint8_t SaveFlags)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!msg, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->message_save(oc_ctx, msg, SaveFlags);
}

/**
   \details Initialize and open a message object

   \param mem_ctx pointer to the memory context to use for allocation
   \param oc_ctx pointer to the openchangedb context
   \param username the name of the mailbox where the parent folder of the
   message is.
   \param messageID the identifier of the message to open
   \param folderID the identifier of the folder where the message is stored
   \param message_object pointer on pointer to the message object to return
   \param msgp pointer on pointer to the mapistore message to return

   \return MAPI_E_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_
enum MAPISTATUS openchangedb_message_open(TALLOC_CTX *mem_ctx,
					  struct openchangedb_context *oc_ctx,
					  const char *username,
					  uint64_t messageID, uint64_t folderID,
					  void **message_object, void **msgp)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->message_open(mem_ctx, oc_ctx, username, messageID,
				    folderID, message_object, msgp);
}


/**
   \details Retrieve a property on a LDB message

   \param mem_ctx pointer to the memory context
   \param message_object the openchangedb message to retrieve data from
   \param proptag the MAPI property tag to lookup
   \param data pointer on pointer to the data to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
_PUBLIC_
enum MAPISTATUS openchangedb_message_get_property(TALLOC_CTX *mem_ctx,
						  struct openchangedb_context *oc_ctx,
						  void *message_object,
						  uint32_t proptag, void **data)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!data, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->message_get_property(mem_ctx, oc_ctx, message_object,
					    proptag, data);
}

/**
   \details Set a list of properties on a message

   \param mem_ctx pointer to the memory context
   \param message_object pointer to the openchangedb message object
   \param row pointer to the SRow structure holding the array of
   properties to set on the message

   \return MAPI_E_SUCCESS on success, otherwise MAPI errors.
 */
_PUBLIC_
enum MAPISTATUS openchangedb_message_set_properties(TALLOC_CTX *mem_ctx,
						    struct openchangedb_context *oc_ctx,
						    void *message_object,
						    struct SRow *row)
{
	OPENCHANGE_RETVAL_IF(!oc_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!row, MAPI_E_NOT_INITIALIZED, NULL);

	return oc_ctx->message_set_properties(mem_ctx, oc_ctx, message_object,
					      row);
}
