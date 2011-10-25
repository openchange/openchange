/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2011

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

/**
   /details Initialize an openchangedb table

   \param mem_ctx pointer to the memory context to use for allocation
   \param table_type the type of table this object represents
   \param folderID the identifier of the folder this table represents
   \param table_object pointer on pointer to the table object to return

   \return MAPI_E_SUCCESS on success, otherwise MAPISTORE error
 */
_PUBLIC_ enum MAPISTATUS openchangedb_message_open(TALLOC_CTX *mem_ctx, void *ldb_ctx,
						   uint64_t messageID, uint64_t folderID, 
						   void **message_object, void **msgp)
{
	struct mapistore_message	*mmsg;
	struct openchangedb_message	*msg;
	const char * const		attrs[] = { "*", NULL };
	int				ret;
	char				*ldb_filter;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!ldb_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!msgp, MAPI_E_NOT_INITIALIZED, NULL);

	msg = talloc_zero(mem_ctx, struct openchangedb_message);
	if (!msg) {
		return MAPI_E_NOT_ENOUGH_MEMORY;
	}
	printf("openchangedb_message_open: folderID=%"PRIu64" messageID=%"PRIu64"\n", folderID, messageID);
	msg->folderID = folderID;
	msg->messageID = messageID;

	/* Open the message and load results */
	ldb_filter = talloc_asprintf(mem_ctx, "(&(PidTagParentFolderId=%"PRIu64")(PidTagMessageId=%"PRIu64"))", folderID, messageID);
	ret = ldb_search(ldb_ctx, (TALLOC_CTX *)msg, &msg->res, ldb_get_default_basedn(ldb_ctx),
			 LDB_SCOPE_SUBTREE, attrs, ldb_filter, NULL);
	printf("We have found: %d messages for ldb_filter = %s\n", msg->res->count, ldb_filter);
	talloc_free(ldb_filter);
	OPENCHANGE_RETVAL_IF(ret != LDB_SUCCESS || !msg->res->count, MAPI_E_NOT_FOUND, msg);

	mmsg = talloc_zero(mem_ctx, struct mapistore_message);
	mmsg->subject_prefix = NULL;
	mmsg->normalized_subject = ldb_msg_find_attr_as_string(msg->res->msgs[0], "PidTagNormalizedSubject", NULL);
	mmsg->columns = NULL;
	mmsg->recipients_count = 0;
	mmsg->recipients = NULL;

	*message_object = (void *)msg;
	*msgp = (void *)mmsg;

	return MAPI_E_SUCCESS;
}


_PUBLIC_ enum MAPISTATUS openchangedb_message_get_property(TALLOC_CTX *mem_ctx, void *message_object, 
							   uint32_t proptag, void **data)
{
	struct openchangedb_message	*msg;
	const char			*PidTagAttr = NULL;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!message_object, MAPI_E_NOT_INITIALIZED, NULL);
	OPENCHANGE_RETVAL_IF(!data, MAPI_E_NOT_INITIALIZED, NULL);

	msg = (struct openchangedb_message *)message_object;
	OPENCHANGE_RETVAL_IF(!msg->res, MAPI_E_NOT_INITIALIZED, NULL);

	/* Turn property into PidTagAttr */
	PidTagAttr = (char *) openchangedb_property_get_attribute(proptag);

	/* Ensure the element exists */
	OPENCHANGE_RETVAL_IF(!ldb_msg_find_element(msg->res->msgs[0], PidTagAttr), MAPI_E_NOT_FOUND, NULL);

	/* Retrieve data */
	*data = openchangedb_get_property_data(mem_ctx, msg->res, 0, proptag, PidTagAttr);
	OPENCHANGE_RETVAL_IF(*data != NULL, MAPI_E_SUCCESS, NULL);

	return MAPI_E_NOT_FOUND;
}
