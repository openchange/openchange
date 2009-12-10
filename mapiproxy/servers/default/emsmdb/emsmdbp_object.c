/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

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

/**
   \file emsmdbp_object.c

   \brief Server-side specific objects init/release routines
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "dcesrv_exchange_emsmdb.h"

static const char *emsmdbp_getstr_type(struct emsmdbp_object *object)
{
	switch (object->type) {
	case EMSMDBP_OBJECT_UNDEF:
		return "undefined";
	case EMSMDBP_OBJECT_MAILBOX:
		return "mailbox";
	case EMSMDBP_OBJECT_FOLDER:
		return "folder";
	case EMSMDBP_OBJECT_MESSAGE:
		return "message";
	case EMSMDBP_OBJECT_TABLE:
		return "table";
	default:
		return "unknown";
	}
}


/**
   \details talloc destructor fo emsmdbp_objects

   \param data generic pointer on data

   \return 0 on success, otherwise -1
 */
static int emsmdbp_object_destructor(void *data)
{
	struct emsmdbp_object	*object = (struct emsmdbp_object *) data;
	int			ret;

	if (!data) return -1;
	
	DEBUG(4, ("[%s:%d]: emsmdbp %s object released\n", __FUNCTION__, __LINE__,
		  emsmdbp_getstr_type(object)));

	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		ret = mapistore_del_context(object->mstore_ctx, object->object.folder->contextID);
		DEBUG(4, ("[%s:%d] mapistore folder context retval = %d\n", __FUNCTION__, __LINE__, ret));
		break;
	default:
		break;
	}

	talloc_free(object);

	return 0;
}

/**
   \details Initialize an emsmdbp_object

   \param mem_ctx pointer to the memory context

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx)
{
	struct emsmdbp_object	*object = NULL;

	object = talloc_zero(mem_ctx, struct emsmdbp_object);
	if (!object) return NULL;

	talloc_set_destructor((void *)object, (int (*)(void *))emsmdbp_object_destructor);

	object->type = EMSMDBP_OBJECT_UNDEF;
	object->mstore_ctx = emsmdbp_ctx->mstore_ctx;
	object->object.mailbox = NULL;
	object->object.folder = NULL;
	object->private_data = NULL;

	return object;
}


/**
   \details Initialize a mailbox object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the Logon MAPI request

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_mailbox_init(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    struct EcDoRpc_MAPI_REQ *request)
{
	struct emsmdbp_object		*object;
	const char			*displayName;
	const char * const		recipient_attrs[] = { "*", NULL };
	int				ret;
	struct ldb_result		*res = NULL;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!request) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	/* Initialize the mailbox object */
	object->object.mailbox = talloc_zero(object, struct emsmdbp_object_mailbox);
	if (!object->object.mailbox) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_MAILBOX;
	object->object.mailbox->owner_Name = NULL;
	object->object.mailbox->owner_EssDN = NULL;
	object->object.mailbox->szUserDN = NULL;
	object->object.mailbox->folderID = 0x0;

	object->object.mailbox->owner_EssDN = talloc_strdup(object->object.mailbox, request->u.mapi_Logon.EssDN);
	object->object.mailbox->szUserDN = talloc_strdup(object->object.mailbox, emsmdbp_ctx->szUserDN);

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, mem_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "legacyExchangeDN=%s", 
			 object->object.mailbox->owner_EssDN);

	if (res->count == 1) {
		displayName = ldb_msg_find_attr_as_string(res->msgs[0], "displayName", NULL);
		if (displayName) {
			object->object.mailbox->owner_Name = talloc_strdup(object->object.mailbox, displayName);

			/* Retrieve Mailbox folder identifier */
			openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_Name,
							0x1, &object->object.mailbox->folderID);
		}
	}

	talloc_free(res);

	return object;
}


/**
   \details Initialize a folder object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the OpenFolder MAPI request
   \param parent pointer to the parent MAPI handle

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   struct EcDoRpc_MAPI_REQ *request,
							   struct mapi_handles *parent)
{
	enum MAPISTATUS			retval;
	struct emsmdbp_object		*object;
	int				mailboxfolder;
	char				*mapistore_uri = NULL;
	uint32_t			context_id;
	int				ret;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!request) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;

	object->object.folder = talloc_zero(object, struct emsmdbp_object_folder);
	if (!object->object.folder) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FOLDER;
	object->object.folder->contextID = -1;
	object->object.folder->folderID = request->u.mapi_OpenFolder.folder_id;
	
	retval = mapi_handles_get_systemfolder(parent, &mailboxfolder);
	object->object.folder->IsSystemFolder = (!mailboxfolder) ? true : false;

	if (object->object.folder->IsSystemFolder == false) {
		/* Retrieve the systemfolder value */
		object->object.folder->systemfolder = -1;
		/* assign mapistore context from parent */
	} else {
		object->object.folder->systemfolder = 1;
		/* mapistore backend initialization goes here */
		retval = openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx,
						       object->object.folder->folderID, &mapistore_uri);
		if (retval == MAPI_E_SUCCESS) {
			ret = mapistore_add_context(emsmdbp_ctx->mstore_ctx, mapistore_uri, &context_id);
			if (ret != MAPISTORE_SUCCESS) {
				talloc_free(object);
				return NULL;
			}
			object->object.folder->contextID = context_id;
		} else {
			talloc_free(object);
			return NULL;
		}
	}

	return object;
}


/**
   \details Initialize a table object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param parent pointer to the parent MAPI handle

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct mapi_handles *parent)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*object;
	struct emsmdbp_object	*folder;
	void			*data = NULL;
	int			mailboxfolder;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	/* Retrieve parent object */
	retval = mapi_handles_get_private_data(parent, &data);
	if (retval) return NULL;
	folder = (struct emsmdbp_object *) data;

	/* Initialize table object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx);
	if (!object) return NULL;
	
	object->object.table = talloc_zero(object, struct emsmdbp_object_table);
	if (!object->object.table) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_TABLE;
	object->object.table->folderID = folder->object.folder->folderID;
	object->object.table->prop_count = 0;
	object->object.table->properties = NULL;
	object->object.table->numerator = 0;
	object->object.table->denominator = 0;

	retval = mapi_handles_get_systemfolder(parent, &mailboxfolder);
	object->object.table->IsSystemTable = (!mailboxfolder) ? true : false;

	/* FIXME: missing mapistore folders support */

	return object;
}
