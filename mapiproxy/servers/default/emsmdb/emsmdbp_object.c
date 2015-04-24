/*
   OpenChange Server implementation

   EMSMDBP: EMSMDB Provider implementation

   Copyright (C) Julien Kerihuel 2009-2015
   Copyright (C) Enrique J. Hernández 2015

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

#include <ctype.h>
#include <time.h>

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/libmapiproxy/fault_util.h"
#include "mapiproxy/libmapiserver/libmapiserver.h"
#include "mapiproxy/libmapistore/mapistore_nameid.h"
#include "libmapi/property_tags.h"
#include "libmapi/property_altnames.h"

#include "dcesrv_exchange_emsmdb.h"

const char *emsmdbp_getstr_type(struct emsmdbp_object *object)
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
	case EMSMDBP_OBJECT_STREAM:
		return "stream";
	case EMSMDBP_OBJECT_ATTACHMENT:
		return "attachment";
	case EMSMDBP_OBJECT_SUBSCRIPTION:
		return "subscription";
	case EMSMDBP_OBJECT_SYNCCONTEXT:
		return "synccontext";
	case EMSMDBP_OBJECT_FTCONTEXT:
		return "ftcontext";
	default:
		return "unknown";
	}
}

/**
   \details Convenient function to determine whether specified
   object is using mapistore or not

   \param object pointer to the emsmdp object

   \return true if parent is using mapistore, otherwise false
 */
bool emsmdbp_is_mapistore(struct emsmdbp_object *object)
{
	/* Sanity checks - probably pointless */
	if (!object) {
		return false;
	}

	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		return false;
	case EMSMDBP_OBJECT_FOLDER:
		if (object->object.folder->mapistore_root) {
			return true;
		}
	default:
		if (object->parent_object) {
			return emsmdbp_is_mapistore(object->parent_object);
		}
	}

	return false;
}

static struct emsmdbp_object *emsmdbp_get_mailbox(struct emsmdbp_object *object)
{
	if (object->type == EMSMDBP_OBJECT_MAILBOX) {
		return object;
	}

	return emsmdbp_get_mailbox(object->parent_object);
}

/**
   \details Convenient function to determine whether specified
   mapi_handles refers to object within mailbox or public folders
   store.

   \param object pointer to the emsmdp object

   \return true if parent is within mailbox store, otherwise false
 */
bool emsmdbp_is_mailboxstore(struct emsmdbp_object *object)
{
	struct emsmdbp_object *mailbox = emsmdbp_get_mailbox(object);

	return mailbox->object.mailbox->mailboxstore;
}

/**
   \details Convenience function to determine the owner of an object

   \param object pointer to the emsmdp object

   \return true if parent is within mailbox store, otherwise false
 */
char *emsmdbp_get_owner(struct emsmdbp_object *object)
{
	struct emsmdbp_object *mailbox;

	mailbox = emsmdbp_get_mailbox(object);

	return mailbox->object.mailbox->owner_username;
}


/**
   \details Return the contextID associated to a handle

   \param object pointer to the emsmdp object

   \return contextID value on success, otherwise -1
 */
_PUBLIC_ uint32_t emsmdbp_get_contextID(struct emsmdbp_object *object)
{
	switch (object->type) {
	case EMSMDBP_OBJECT_MAILBOX:
		return -1;
	case EMSMDBP_OBJECT_FOLDER:
		if (object->object.folder->mapistore_root) {
			return object->object.folder->contextID;
		} else if (object->parent_object) {
			return emsmdbp_get_contextID(object->parent_object);
		}
		break;
	default:
		if (object->parent_object) {
			return emsmdbp_get_contextID(object->parent_object);
		}
	}

	return -1;
}

_PUBLIC_ enum mapistore_error emsmdbp_object_get_fid_by_name(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_folder, const char *name, uint64_t *fidp)
{
	uint64_t	folderID;

	if (!emsmdbp_ctx) return MAPISTORE_ERROR;
	if (!parent_folder) return MAPISTORE_ERROR;
	if (!name) return MAPISTORE_ERROR;
	if (!fidp) return MAPISTORE_ERROR;

	if (parent_folder->type == EMSMDBP_OBJECT_FOLDER) {
		folderID = parent_folder->object.folder->folderID;
	}
	else if (parent_folder->type == EMSMDBP_OBJECT_MAILBOX) {
		folderID = parent_folder->object.mailbox->folderID;
	}
	else {
		return MAPISTORE_ERROR;
	}

	if (emsmdbp_is_mapistore(parent_folder)) {
		if (mapistore_folder_get_child_fid_by_name(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent_folder), parent_folder->backend_object, name, fidp)) {
			return MAPISTORE_ERR_NOT_FOUND;
		}

		return MAPISTORE_SUCCESS;
	}
	else {
		struct emsmdbp_object *mailbox_object = emsmdbp_get_mailbox(parent_folder);
		if (mailbox_object == NULL) {
			OC_DEBUG(0, "Failed to find mailbox object for parent_folder.\n");
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}

		return openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, mailbox_object->object.mailbox->owner_username, folderID, name, fidp);
	}
}

static enum mapistore_context_role emsmdbp_container_class_to_role(const char *container_class)
{
	enum mapistore_context_role	i, role = MAPISTORE_FALLBACK_ROLE;
	static const char		**container_classes = NULL;
	bool				found = false;

	if (!container_classes) {
		container_classes = talloc_array(NULL, const char *, MAPISTORE_MAX_ROLES);
		for (i = MAPISTORE_MAIL_ROLE; i < MAPISTORE_MAX_ROLES; i++) {
			container_classes[i] = "IPF.Note";
		}
		container_classes[MAPISTORE_CALENDAR_ROLE] = "IPF.Appointment";
		container_classes[MAPISTORE_CONTACTS_ROLE] = "IPF.Contact";
		container_classes[MAPISTORE_TASKS_ROLE] = "IPF.Task";
		container_classes[MAPISTORE_NOTES_ROLE] = "IPF.StickyNote";
		container_classes[MAPISTORE_JOURNAL_ROLE] = "IPF.Journal";
		container_classes[MAPISTORE_FALLBACK_ROLE] = "";
	}

	if (container_class) {
		for (i = 0; !found && i < MAPISTORE_MAX_ROLES; i++) {
			if (strcmp(container_class, container_classes[i]) == 0) {
				role = i;
				found = true;
			}
		}
	}

	return role;
}

static enum mapistore_error emsmdbp_object_folder_commit_creation(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *new_folder, bool force_container_class)
{
	enum mapistore_error		ret = MAPISTORE_SUCCESS;
	enum MAPISTATUS			retval;
	struct SPropValue		*value;
	char				*mapistore_uri, *owner;
	enum mapistore_context_role	role;
	TALLOC_CTX			*mem_ctx;
	uint64_t			parent_fid, fid;
	uint32_t			context_id;

	if (!new_folder->object.folder->postponed_props) {
		return ret;
	}

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	value = get_SPropValue_SRow(new_folder->object.folder->postponed_props, PR_CONTAINER_CLASS_UNICODE);
	if (!value) {
		/* Sometimes Outlook does pass non-unicode values. */
		value = get_SPropValue_SRow(new_folder->object.folder->postponed_props, PR_CONTAINER_CLASS);
	}
	if (value) {
		role = emsmdbp_container_class_to_role(value->value.lpszW);
	}
	else if (force_container_class) {
		OC_DEBUG(5, "forcing folder backend role to 'fallback'\n");
		role = MAPISTORE_FALLBACK_ROLE;
	}
	else {
		OC_DEBUG(5, "container class not set yet\n");
		goto end;
	}

	value = get_SPropValue_SRow(new_folder->object.folder->postponed_props, PR_DISPLAY_NAME_UNICODE);
	if (!value) {
		value = get_SPropValue_SRow(new_folder->object.folder->postponed_props, PR_DISPLAY_NAME);
	}
	if (!value) {
		OC_DEBUG(5, "display name not set yet\n");
		goto end;
	}

	fid = new_folder->object.folder->folderID;
	owner = emsmdbp_get_owner(new_folder);

	ret = mapistore_create_root_folder(owner, role, fid, value->value.lpszW, mem_ctx, &mapistore_uri);
	if (ret != MAPISTORE_SUCCESS) {
		goto end;
	}

	ret = mapistore_add_context(emsmdbp_ctx->mstore_ctx, owner, mapistore_uri, fid, &context_id, &new_folder->backend_object);
	if (ret != MAPISTORE_SUCCESS) {
		OC_PANIC(false,
			 ("mapistore_add_context() failed with 0x%.8x, mapistore_uri = [%s].\n",
			  ret, mapistore_uri));
		goto end;
	}

	new_folder->object.folder->contextID = context_id;

	if (new_folder->parent_object->type == EMSMDBP_OBJECT_MAILBOX) {
		parent_fid = new_folder->parent_object->object.mailbox->folderID;
	}
	else { /* EMSMDBP_OBJECT_FOLDER */
		parent_fid = new_folder->parent_object->object.folder->folderID;
	}

	value = get_SPropValue_SRow(new_folder->object.folder->postponed_props, PidTagChangeNumber);
	retval = openchangedb_create_folder(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, parent_fid, fid, value->value.d, mapistore_uri, -1);
	if (retval != MAPI_E_SUCCESS) {
		if (retval == MAPI_E_COLLISION) {
			ret = MAPISTORE_ERR_EXIST;
		}
		else {
			ret = MAPISTORE_ERR_NOT_FOUND;
		}
		OC_DEBUG(0, "openchangedb folder creation failed: 0x%.8x\n", retval);
		goto end;
	}

	new_folder->object.folder->contextID = context_id;

	openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fid, new_folder->object.folder->postponed_props);
	mapistore_indexing_record_add_fmid_for_uri(emsmdbp_ctx->mstore_ctx, context_id, owner, fid,
						   mapistore_uri);
	mapistore_properties_set_properties(emsmdbp_ctx->mstore_ctx, context_id, new_folder->backend_object, new_folder->object.folder->postponed_props);

	talloc_unlink(new_folder, new_folder->object.folder->postponed_props);
	new_folder->object.folder->postponed_props = NULL;

	OC_DEBUG(5, "new mapistore context created at uri: %s\n", mapistore_uri);

end:
	talloc_free(mem_ctx);

	return ret;
}

_PUBLIC_ enum MAPISTATUS emsmdbp_object_create_folder(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_folder, TALLOC_CTX *mem_ctx, uint64_t fid, struct SRow *rowp, struct emsmdbp_object **new_folderp)
{
	uint64_t			parentFolderID, testFolderID;
	struct SPropValue		*value;
	enum mapistore_error		retval;
	struct emsmdbp_object		*new_folder;
	struct SRow			*postponed_props;

	/* Sanity checks */
	MAPI_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!parent_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!rowp, MAPI_E_INVALID_PARAMETER, NULL);

	new_folder = emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, fid, parent_folder);
	if (emsmdbp_is_mapistore(parent_folder)) {
		retval = mapistore_folder_create_folder(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent_folder), parent_folder->backend_object, new_folder, fid, rowp, &new_folder->backend_object);
		if (retval != MAPISTORE_SUCCESS) {
			talloc_free(new_folder);
			return mapistore_error_to_mapi(retval);
		}
	}
	else {
		parentFolderID = parent_folder->object.folder->folderID;
		value = get_SPropValue_SRow(rowp, PR_DISPLAY_NAME_UNICODE);
		if (!value) {
			value = get_SPropValue_SRow(rowp, PR_DISPLAY_NAME);
		}
		if (!value) {
			talloc_free(new_folder);
			return MAPI_E_INVALID_PARAMETER;
		}
		if (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, parentFolderID,
						 value->value.lpszW, &testFolderID) == MAPI_E_SUCCESS) {
			/* this folder already exists */
			OC_DEBUG(4, "emsmdbp_object: CreateFolder Duplicate Folder error\n");
			talloc_free(new_folder);
			return MAPI_E_COLLISION;
		}

		value = get_SPropValue_SRow(rowp, PidTagChangeNumber);
		if (value) {
			postponed_props = talloc_zero(new_folder, struct SRow);
			postponed_props->cValues = rowp->cValues;
			postponed_props->lpProps = talloc_array(postponed_props, struct SPropValue, rowp->cValues);
			mapi_copy_spropvalues(postponed_props->lpProps, rowp->lpProps, postponed_props->lpProps, rowp->cValues);
			new_folder->object.folder->postponed_props = postponed_props;
			new_folder->object.folder->mapistore_root = true;

			retval = emsmdbp_object_folder_commit_creation(emsmdbp_ctx, new_folder, false);
			if (retval != MAPISTORE_SUCCESS) {
				talloc_free(new_folder);
				return mapistore_error_to_mapi(retval);
			}
		}
		else {
			OC_PANIC(true, ("PidTagChangeNumber *must* be present\n"));
		}
	}
	*new_folderp = new_folder;

	return MAPI_E_SUCCESS;
}

_PUBLIC_ enum mapistore_error emsmdbp_object_open_folder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent, uint64_t fid, struct emsmdbp_object **folder_object_p)
{
	struct emsmdbp_object			*folder_object, *mailbox_object;
	enum mapistore_error			retval;
	enum MAPISTATUS				ret;
	char					*path;
	char					*owner;
	uint32_t				contextID;
	uint64_t				parent_fid, oc_parent_fid;
	void					*local_ctx;

	folder_object = emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, fid, parent);
	if (emsmdbp_is_mapistore(parent)) {
		OC_DEBUG(5, "opening child mapistore folder\n");
		retval = mapistore_folder_open_folder(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent), parent->backend_object, folder_object, fid, &folder_object->backend_object);
		if (retval != MAPISTORE_SUCCESS) {
			talloc_free(folder_object);
			return retval;
		}
	}
	else {
		local_ctx = talloc_zero(NULL, void);
	
		mailbox_object = emsmdbp_get_mailbox(parent);
		if (mailbox_object == NULL) {
			OC_DEBUG(0, "Failed to find mailbox object for parent passed.\n");
			return MAPISTORE_ERR_INVALID_PARAMETER;
		}

		retval = openchangedb_get_mapistoreURI(local_ctx, emsmdbp_ctx->oc_ctx, mailbox_object->object.mailbox->owner_username, fid, &path, true);
		if (retval == MAPISTORE_SUCCESS && path) {
			folder_object->object.folder->mapistore_root = true;
			/* system/special folder */
			OC_DEBUG(5, "opening base mapistore folder\n");

			retval = mapistore_search_context_by_uri(emsmdbp_ctx->mstore_ctx, path, &contextID, &folder_object->backend_object);
			if (retval == MAPISTORE_SUCCESS) {
				retval = mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, contextID);
			} else {
				owner = emsmdbp_get_owner(folder_object);
				retval = mapistore_add_context(emsmdbp_ctx->mstore_ctx, owner, path, folder_object->object.folder->folderID, &contextID, &folder_object->backend_object);
				if (retval != MAPISTORE_SUCCESS) {
					talloc_free(local_ctx);
					talloc_free(folder_object);
					return retval;
				}
				mapistore_indexing_record_add_fid(emsmdbp_ctx->mstore_ctx, contextID, owner, fid);
			}
			folder_object->object.folder->contextID = contextID;
			/* (void) talloc_reference(folder_object, folder_object->backend_object); */
		}
		else {
			switch (parent->type) {
			case EMSMDBP_OBJECT_MAILBOX:
				parent_fid = parent->object.mailbox->folderID;
				break;
			case EMSMDBP_OBJECT_FOLDER:
				parent_fid = parent->object.folder->folderID;
				break;
			default:
				OC_PANIC(true, ("Trying to open folder with parent not a Mailbox or Folder. Parent type: %d\n",
						parent->type));
				break;
			}
			ret = openchangedb_get_parent_fid(emsmdbp_ctx->oc_ctx, mailbox_object->object.mailbox->owner_username, fid, &oc_parent_fid, mailbox_object->object.mailbox->mailboxstore);
			if (ret != MAPI_E_SUCCESS) {
				OC_DEBUG(0, "folder %.16"PRIx64" or %.16"PRIx64" does not exist\n", parent_fid, fid);
				talloc_free(local_ctx);
				talloc_free(folder_object);
				return MAPISTORE_ERR_NOT_FOUND;
			}
			if (oc_parent_fid != parent_fid) {
				OC_DEBUG(0, "parent folder mismatch: expected %.16"PRIx64" but got %.16"PRIx64"\n", parent_fid, oc_parent_fid);
				talloc_free(local_ctx);
				talloc_free(folder_object);
				return MAPISTORE_ERR_NOT_FOUND;
			}
			OC_DEBUG(5, "opening openchangedb folder\n");
		}
		talloc_free(local_ctx);
	}

	*folder_object_p = folder_object;

	return MAPISTORE_SUCCESS;
}

_PUBLIC_ int emsmdbp_get_uri_from_fid(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, uint64_t fid, char **urip)
{
	enum MAPISTATUS	retval;
	bool		soft_deleted;

	retval = openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fid, urip, true); /* FIXME: always mailboxstore */
	if (retval == MAPI_E_SUCCESS) {
		return MAPISTORE_SUCCESS;
	}
	return mapistore_indexing_record_get_uri(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, mem_ctx, fid, urip, &soft_deleted);
}

_PUBLIC_ int emsmdbp_get_fid_from_uri(struct emsmdbp_context *emsmdbp_ctx, const char *uri, uint64_t *fidp)
{
	int	ret;
	bool	soft_deleted;

	ret = openchangedb_get_fid(emsmdbp_ctx->oc_ctx, uri, fidp);
	if (ret != MAPI_E_SUCCESS) {
		ret = (int) mapistore_indexing_record_get_fmid(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, uri, false, fidp, &soft_deleted);
	}

	return ret;
}

static char *emsmdbp_compute_parent_uri(TALLOC_CTX *mem_ctx, char *uri)
{
	char *parent_uri, *slash, *lastchar;
	int len;

	if (!uri) return NULL;

	parent_uri = talloc_strdup(mem_ctx, uri);
	len = strlen(parent_uri);
	lastchar = parent_uri + len - 1;
	if (*lastchar == '/') {
		*lastchar = 0;
	}
	slash = strrchr(parent_uri, '/');
	if (slash) {
		*(slash + 1) = 0;
	}
	else {
		talloc_free(parent_uri);
		parent_uri = NULL;
	}

	return parent_uri;
}

static enum MAPISTATUS emsmdbp_get_parent_fid(struct emsmdbp_context *emsmdbp_ctx,
					      struct emsmdbp_object *mailbox_object,
					      uint64_t fid, uint64_t *parent_fidp)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error		ret;
	enum MAPISTATUS			retval;
	bool				soft_deleted;
	char				*uri, *parent_uri;
	struct emsmdbp_object_mailbox	*mailbox;

	OPENCHANGE_RETVAL_IF(!mailbox_object, MAPI_E_INVALID_PARAMETER, NULL);
	mailbox = mailbox_object->object.mailbox;

	mem_ctx = talloc_zero(NULL, void);
	retval = openchangedb_get_parent_fid(emsmdbp_ctx->oc_ctx, mailbox->owner_username, fid, parent_fidp, true);
	if (retval == MAPI_E_SUCCESS) {
		goto end;
	}
	retval = openchangedb_get_parent_fid(emsmdbp_ctx->oc_ctx, mailbox->owner_username, fid, parent_fidp, false);
	if (retval == MAPI_E_SUCCESS) {
		goto end;
	}

	ret = mapistore_indexing_record_get_uri(emsmdbp_ctx->mstore_ctx, mailbox->owner_username, mem_ctx, fid, &uri, &soft_deleted);
	if (ret == MAPISTORE_SUCCESS) {
		parent_uri = emsmdbp_compute_parent_uri(mem_ctx, uri);
		if (parent_uri) {
			ret = emsmdbp_get_fid_from_uri(emsmdbp_ctx, parent_uri, parent_fidp);
		}
		else {
			ret = MAPISTORE_ERR_NOT_FOUND;
		}
	}
	retval = mapistore_error_to_mapi(ret);

end:
	talloc_free(mem_ctx);

	return retval;
}

/**
   \details Return the folder object associated to specified folder identified

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdbp context
   \param context_object pointer to current context object
   \param fid pointer to the Folder Identifier to lookup
   \param [out] folder_object_p location to store emsmdbp object on success

   \return MAPISTATUS error code
 */

_PUBLIC_ enum MAPISTATUS emsmdbp_object_open_folder_by_fid(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   struct emsmdbp_object *context_object,
							   uint64_t fid,
							   struct emsmdbp_object **folder_object_p)
{
	uint64_t		parent_fid;
	enum mapistore_error	ret;
	enum MAPISTATUS		retval;
	struct emsmdbp_object   *mailbox_object;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!context_object, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!folder_object_p, MAPI_E_INVALID_PARAMETER, NULL);

	if ((context_object->type == EMSMDBP_OBJECT_MAILBOX
	     && fid == context_object->object.mailbox->folderID)
	    || (context_object->type == EMSMDBP_OBJECT_FOLDER
		&& fid == context_object->object.folder->folderID)) {
		*folder_object_p = context_object;
		return MAPI_E_SUCCESS;
	}

	mailbox_object = emsmdbp_get_mailbox(context_object);
	if (fid == mailbox_object->object.mailbox->folderID) {
		*folder_object_p = mailbox_object;
		return MAPI_E_SUCCESS;
	}

	retval = emsmdbp_get_parent_fid(emsmdbp_ctx, mailbox_object, fid, &parent_fid);
	if (retval == MAPI_E_SUCCESS) {
		if (parent_fid) {
			struct emsmdbp_object	*parent_object = NULL;

			retval = emsmdbp_object_open_folder_by_fid(mem_ctx, emsmdbp_ctx, context_object, parent_fid, &parent_object);
			if (retval != MAPI_E_SUCCESS) {
				return retval;
			}
			ret = emsmdbp_object_open_folder(mem_ctx, emsmdbp_ctx, parent_object, fid, folder_object_p);
			return mapistore_error_to_mapi(ret);
		}
		else {
			*folder_object_p = emsmdbp_object_folder_init(mem_ctx, emsmdbp_ctx, fid, NULL);
			return MAPI_E_SUCCESS;
		}
	}

	return retval;
}

_PUBLIC_ int emsmdbp_object_stream_commit(struct emsmdbp_object *stream_object)
{
	int				rc;
	struct emsmdbp_object_stream	*stream;
        void				*stream_data;
        uint8_t				*utf8_buffer;
        struct Binary_r			*binary_data;
        struct SRow			aRow;
	size_t				converted_size;
	uint16_t			propType;

	if (!stream_object || stream_object->type != EMSMDBP_OBJECT_STREAM) return MAPISTORE_ERROR;

	stream = stream_object->object.stream;

	rc = MAPISTORE_SUCCESS;
	if (stream->needs_commit) {
		stream->needs_commit = false;
		aRow.cValues = 1;
		aRow.lpProps = talloc_zero(NULL, struct SPropValue);

		propType = stream->property & 0xffff;
		if (propType == PT_BINARY) {
			binary_data = talloc(aRow.lpProps, struct Binary_r);
			binary_data->cb = stream->stream.buffer.length;
			binary_data->lpb = stream->stream.buffer.data;
			stream_data = binary_data;
		}
		else if (propType == PT_STRING8) {
			stream_data = stream->stream.buffer.data;
		}
		else {
			/* PT_UNICODE */
			utf8_buffer = talloc_array(aRow.lpProps, uint8_t, stream->stream.buffer.length + 2);
			convert_string(CH_UTF16LE, CH_UTF8,
				       stream->stream.buffer.data, stream->stream.buffer.length,
				       utf8_buffer, stream->stream.buffer.length, &converted_size);
			utf8_buffer[converted_size] = 0;
			stream_data = utf8_buffer;
		}
		set_SPropValue_proptag(aRow.lpProps, stream->property, stream_data);

		emsmdbp_object_set_properties(stream_object->emsmdbp_ctx, stream_object->parent_object, &aRow);
		talloc_free(aRow.lpProps);
	}

	return rc;
}

/**
   \details talloc destructor for emsmdbp_objects

   \param data generic pointer on data

   \return 0 on success, otherwise -1
 */
static int emsmdbp_object_destructor(void *data)
{
	struct emsmdbp_object	*object = (struct emsmdbp_object *) data;
	enum mapistore_error	mretval;
	int			ret = MAPISTORE_SUCCESS;
	uint32_t		contextID;
	unsigned int		missing_objects;
	struct timeval		request_end, request_delta;

	if (!data) return -1;
	if (!emsmdbp_is_mapistore(object)) goto nomapistore;

	OC_DEBUG(4, "emsmdbp %s object released\n", emsmdbp_getstr_type(object));

	contextID = emsmdbp_get_contextID(object);
	switch (object->type) {
	case EMSMDBP_OBJECT_FOLDER:
		if (object->object.folder->mapistore_root) {
			ret = mapistore_del_context(object->emsmdbp_ctx->mstore_ctx, contextID);
		}
		OC_DEBUG(4, "mapistore folder context retval = %d\n", ret);
		break;
	case EMSMDBP_OBJECT_TABLE:
		if (emsmdbp_is_mapistore(object) && object->backend_object && object->object.table->handle > 0) {
			mapistore_table_handle_destructor(object->emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object),
							  object->backend_object, object->object.table->handle);
		}
		if (object->object.table->handle && (object->object.table->subscription == true)) {
			mretval = mapistore_notification_subscription_delete_by_handle(object->emsmdbp_ctx->mstore_ctx,
										       object->emsmdbp_ctx->session_uuid,
										       object->object.table->handle);
			if (mretval != MAPISTORE_SUCCESS) {
				OC_DEBUG(0, "Unable to delete table notification subscription with handle=0x%x",
					 object->object.table->handle);
			}
		}
		break;
	case EMSMDBP_OBJECT_STREAM:
		emsmdbp_object_stream_commit(object);
		break;
	case EMSMDBP_OBJECT_SUBSCRIPTION:
		mretval = mapistore_notification_subscription_delete_by_handle(object->emsmdbp_ctx->mstore_ctx,
								     object->emsmdbp_ctx->session_uuid,
								     object->object.subscription->handle);
		if (mretval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "Unable to delete notification subscription with handle=0x%x",
				 object->object.subscription->handle);
		}
		break;
	case EMSMDBP_OBJECT_SYNCCONTEXT:
		gettimeofday(&request_end, NULL);
		request_delta.tv_sec = request_end.tv_sec - object->object.synccontext->request_start.tv_sec;
		if (request_end.tv_usec < object->object.synccontext->request_start.tv_usec) {
			request_delta.tv_sec--;
			request_delta.tv_usec = 1000000 + request_end.tv_usec - object->object.synccontext->request_start.tv_usec;
		}
		else {
			request_delta.tv_usec = request_end.tv_usec - object->object.synccontext->request_start.tv_usec;
		}
		missing_objects = (object->object.synccontext->total_objects - object->object.synccontext->skipped_objects - object->object.synccontext->sent_objects);
		OC_DEBUG(5, "free synccontext: sent: %u, skipped: %u, total: %u -> missing: %u\n", object->object.synccontext->sent_objects, object->object.synccontext->skipped_objects, object->object.synccontext->total_objects, missing_objects);
		OC_DEBUG(5, "  time taken for transmitting entire data: %lu.%.6lu\n", request_delta.tv_sec, request_delta.tv_usec);
		break;
	case EMSMDBP_OBJECT_UNDEF:
	case EMSMDBP_OBJECT_MAILBOX:
	case EMSMDBP_OBJECT_MESSAGE:
	case EMSMDBP_OBJECT_ATTACHMENT:
	case EMSMDBP_OBJECT_FTCONTEXT:
		break;
	}
	
nomapistore:
	talloc_unlink(object, object->parent_object);

	return 0;
}

/**
   \details Initialize an emsmdbp_object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_init(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_object)
{
	struct emsmdbp_object	*object = NULL;

	object = talloc_zero(mem_ctx, struct emsmdbp_object);
	if (!object) return NULL;

	talloc_set_destructor((void *)object, (int (*)(void *))emsmdbp_object_destructor);

	object->type = EMSMDBP_OBJECT_UNDEF;
	object->emsmdbp_ctx = emsmdbp_ctx;
	object->object.mailbox = NULL;
	object->object.folder = NULL;
	object->object.message = NULL;
	object->object.stream = NULL;
	object->object.subscription = NULL;
	object->backend_object = NULL;
	object->parent_object = parent_object;
	(void) talloc_reference(object, parent_object);

	object->stream_data = NULL;

	return object;
}

static int emsmdbp_copy_properties(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *dest_object, struct SPropTagArray *excluded_tags)
{
	TALLOC_CTX		*mem_ctx;
	bool			properties_exclusion[65536];
	struct SPropTagArray	*properties, *needed_properties;
        void                    **data_pointers;
        enum MAPISTATUS         *retvals = NULL;
	struct SRow		*aRow;
	struct SPropValue	newValue;
	uint32_t		i;

	mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	if (emsmdbp_object_get_available_properties(mem_ctx, emsmdbp_ctx, source_object, &properties) == MAPISTORE_ERROR) {
		OC_DEBUG(0, "mapistore support not implemented yet - shouldn't occur\n");
		talloc_free(mem_ctx);
		return MAPI_E_NO_SUPPORT;
	}

	/* 1. Exclusions */
	memset(properties_exclusion, 0, 65536 * sizeof(bool));

	/* 1a. Explicit exclusions */
	properties_exclusion[(uint16_t) (PidTagRowType >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagInstanceKey >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagInstanceNum >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagInstID >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagFolderId >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagMid >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagSourceKey >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagParentSourceKey >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagParentFolderId >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagChangeNumber >> 16)] = true;
	properties_exclusion[(uint16_t) (PidTagChangeKey >> 16)] = true;	
	properties_exclusion[(uint16_t) (PidTagPredecessorChangeList >> 16)] = true;	

	/* 1b. Request exclusions */
	if (excluded_tags != NULL) {
		for (i = 0; i < excluded_tags->cValues; i++) {
			properties_exclusion[(uint16_t) (excluded_tags->aulPropTag[i] >> 16)] = true;
		}
	}

	needed_properties = talloc_zero(mem_ctx, struct SPropTagArray);
	needed_properties->aulPropTag = talloc_zero(needed_properties, void);
	for (i = 0; i < properties->cValues; i++) {
		if (!properties_exclusion[(uint16_t) (properties->aulPropTag[i] >> 16)]) {
			SPropTagArray_add(mem_ctx, needed_properties, properties->aulPropTag[i]);
		}
	}

	data_pointers = emsmdbp_object_get_properties(mem_ctx, emsmdbp_ctx, source_object, needed_properties, &retvals);
	if (data_pointers) {
		aRow = talloc_zero(mem_ctx, struct SRow);
		for (i = 0; i < needed_properties->cValues; i++) {
			if (retvals[i] == MAPI_E_SUCCESS) {
				/* _PUBLIC_ enum MAPISTATUS SRow_addprop(struct SRow *aRow, struct SPropValue spropvalue) */
				set_SPropValue_proptag(&newValue, needed_properties->aulPropTag[i], data_pointers[i]);
				SRow_addprop(aRow, newValue);
			}
		}
		if (emsmdbp_object_set_properties(emsmdbp_ctx, dest_object, aRow) != MAPISTORE_SUCCESS) {
			talloc_free(mem_ctx);
			return MAPI_E_NO_SUPPORT;
		}
	}
	else {
		talloc_free(mem_ctx);
		return MAPI_E_NO_SUPPORT;
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static inline int emsmdbp_copy_message_recipients_mapistore(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *dest_object)
{
	TALLOC_CTX			*mem_ctx;
	enum mapistore_error		ret;
	struct mapistore_message	*msg_data;
	uint32_t			contextID, i;
	int				email_address_idx, display_name_idx;
	struct SPropTagArray		*new_columns;
	void				**new_data;

	if (!emsmdbp_is_mapistore(source_object) || !emsmdbp_is_mapistore(dest_object)) {
		/* we silently fail for non-mapistore messages */
		return MAPI_E_SUCCESS;
	}

	/* Fetch data from source message */
	mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	contextID = emsmdbp_get_contextID(source_object);
	ret = mapistore_message_get_message_data(emsmdbp_ctx->mstore_ctx, contextID, source_object->backend_object, mem_ctx, &msg_data);
	OPENCHANGE_RETVAL_IF(ret != MAPISTORE_SUCCESS, mapistore_error_to_mapi(ret), mem_ctx);

	/* By convention, we pass PR_DISPLAY_NAME_UNICODE and PR_EMAIL_ADDRESS_UNICODE to the backend, so we prepend them to each values array */
	if (msg_data->recipients_count > 0
	    && (msg_data->columns->cValues < 2 || msg_data->columns->aulPropTag[0] != PR_DISPLAY_NAME_UNICODE || msg_data->columns->aulPropTag[1] != PR_EMAIL_ADDRESS_UNICODE)) {
		new_columns = talloc_zero(mem_ctx, struct SPropTagArray);
		OPENCHANGE_RETVAL_IF(!new_columns, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
		new_columns->cValues = msg_data->columns->cValues + 2;
		new_columns->aulPropTag = talloc_array(new_columns, enum MAPITAGS, new_columns->cValues);
		OPENCHANGE_RETVAL_IF(!new_columns->aulPropTag, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
		memcpy(new_columns->aulPropTag + 2, msg_data->columns->aulPropTag, sizeof(enum MAPITAGS) * msg_data->columns->cValues);
		new_columns->aulPropTag[0] = PR_DISPLAY_NAME_UNICODE;
		new_columns->aulPropTag[1] = PR_EMAIL_ADDRESS_UNICODE;

		email_address_idx = get_email_address_index_SPropTagArray(msg_data->columns);
		display_name_idx = get_display_name_index_SPropTagArray(msg_data->columns);

		for (i = 0; i < msg_data->recipients_count; i++) {
			new_data = talloc_array(mem_ctx, void *, new_columns->cValues);
			OPENCHANGE_RETVAL_IF(!new_data, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
			memcpy(new_data + 2, msg_data->recipients[i].data, sizeof(void *) * msg_data->columns->cValues);
			if (display_name_idx != -1) {
				new_data[0] = msg_data->recipients[i].data[display_name_idx];
			}
			else {
				new_data[0] = NULL;
			}
			if (email_address_idx != -1) {
				new_data[1] = msg_data->recipients[i].data[email_address_idx];
			}
			else {
				new_data[1] = NULL;
			}
			msg_data->recipients[i].data = new_data;
		}
		msg_data->columns = new_columns;

		/* Copy data into dest message */
		ret = mapistore_message_modify_recipients(emsmdbp_ctx->mstore_ctx, contextID, dest_object->backend_object, msg_data->columns, msg_data->recipients_count, msg_data->recipients);
		OPENCHANGE_RETVAL_IF(ret != MAPISTORE_SUCCESS, mapistore_error_to_mapi(ret), mem_ctx);
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

static inline int emsmdbp_copy_message_attachments_mapistore(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *dest_object)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		i, count, contextID, dest_num;
	void			**data_pointers;
	enum MAPISTATUS		*retvals;
	uint32_t		*attach_nums;
	struct emsmdbp_object	*table_object, *source_attach, *dest_attach;
	enum MAPITAGS		column;
	int			ret;

	if (!emsmdbp_is_mapistore(source_object) || !emsmdbp_is_mapistore(dest_object)) {
		/* we silently fail for non-mapistore messages */
		return MAPI_E_SUCCESS;
	}

	mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* we fetch the attachment nums */
	table_object = emsmdbp_object_message_open_attachment_table(mem_ctx, emsmdbp_ctx, source_object);
	if (!table_object) {
		talloc_free(mem_ctx);
		return MAPI_E_NOT_FOUND;
	}

	column = PR_ATTACH_NUM;
	table_object->object.table->prop_count = 1;
	table_object->object.table->properties = &column;

	contextID = emsmdbp_get_contextID(table_object);
	mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, 1, &column);

	count = table_object->object.table->denominator;
	attach_nums = talloc_array(mem_ctx, uint32_t, count);
	for (i = 0; i < table_object->object.table->denominator; i++) {
		data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, table_object, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
		if (!data_pointers) {
			talloc_free(mem_ctx);
			return MAPISTORE_ERROR;
		}
		if (retvals[0] != MAPI_E_SUCCESS) {
			talloc_free(mem_ctx);
			OC_DEBUG(5, "cannot copy attachments without PR_ATTACH_NUM\n");
			return MAPISTORE_ERROR;
		}
		attach_nums[i] = *(uint32_t *) data_pointers[0];
	}

	/* we open each attachment manually and copy their props to created dest attachments */
	for (i = 0; i < count; i++) {
		source_attach = emsmdbp_object_attachment_init(mem_ctx, emsmdbp_ctx, source_object->object.message->messageID, source_object);
		if (!source_attach
		    || mapistore_message_open_attachment(emsmdbp_ctx->mstore_ctx, contextID, source_object->backend_object, source_attach, attach_nums[i], &source_attach->backend_object)) {
			talloc_free(mem_ctx);
			return MAPISTORE_ERROR;
		}

		dest_attach = emsmdbp_object_attachment_init(mem_ctx, emsmdbp_ctx, dest_object->object.message->messageID, dest_object);
		if (!dest_attach
		    || mapistore_message_create_attachment(emsmdbp_ctx->mstore_ctx, contextID, dest_object->backend_object, dest_attach, &dest_attach->backend_object, &dest_num)) {
			talloc_free(mem_ctx);
			return MAPISTORE_ERROR;
		}

		ret = emsmdbp_copy_properties(emsmdbp_ctx, source_attach, dest_attach, NULL);
		if (ret != MAPI_E_SUCCESS) {
			talloc_free(mem_ctx);
			return ret;
		}
	}

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Copy properties from an object to another object

   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param source_object pointer to the source object
   \param target_object pointer to the target object
   \param excluded_properties pointer to a SPropTagArray listing properties that must not be copied
   \param deep_copy indicates whether subobjects must be copied

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ int emsmdbp_object_copy_properties(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *source_object, struct emsmdbp_object *target_object, struct SPropTagArray *excluded_properties, bool deep_copy)
{
	int ret;

	if (!(source_object->type == EMSMDBP_OBJECT_FOLDER
	      || source_object->type == EMSMDBP_OBJECT_MAILBOX
	      || source_object->type == EMSMDBP_OBJECT_MESSAGE
	      || source_object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		OC_DEBUG(0, "object must be EMSMDBP_OBJECT_FOLDER, EMSMDBP_OBJECT_MAILBOX, EMSMDBP_OBJECT_MESSAGE or EMSMDBP_OBJECT_ATTACHMENT (type =  %d)\n", source_object->type);
		ret = MAPI_E_NO_SUPPORT;
		goto end;
	}
	if (target_object->type != source_object->type) {
		OC_DEBUG(0, "source and destination objects type must match (type =  %d)\n", target_object->type);
		ret = MAPI_E_NO_SUPPORT;
		goto end;
	}

	/* copy properties (common to all object types) */
	ret = emsmdbp_copy_properties(emsmdbp_ctx, source_object, target_object, excluded_properties);
	if (ret != MAPI_E_SUCCESS) {
                goto end;
	}

	/* type specific ops */
	switch (source_object->type) {
	case EMSMDBP_OBJECT_MESSAGE:
		if (emsmdbp_is_mapistore(source_object) && emsmdbp_is_mapistore(target_object)) {
			ret = emsmdbp_copy_message_recipients_mapistore(emsmdbp_ctx, source_object, target_object);
			if (ret != MAPI_E_SUCCESS) {
				goto end;
			}
			if (deep_copy) {
				ret = emsmdbp_copy_message_attachments_mapistore(emsmdbp_ctx, source_object, target_object);
				if (ret != MAPI_E_SUCCESS) {
					goto end;
				}
			}
		}
		else {
			OC_DEBUG(0, "Cannot copy recipients or attachments to or from non-mapistore messages\n");
		}
		break;
	default:
		if (deep_copy) {
			OC_DEBUG(0, "Cannot deep copy non-message objects\n");
		}
	}

end:

	return ret;
}

/**
   \details Initialize a mailbox object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param request pointer to the Logon MAPI request
   \param mailboxstore boolean which specifies whether the mailbox
   object is a PF store or a private mailbox store

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_mailbox_init(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    const char *essDN,
							    bool mailboxstore)
{
	struct emsmdbp_object		*object;
	const char			*displayName, *accountName;
	const char * const		recipient_attrs[] = { "*", NULL };
	int				ret;
	struct ldb_result		*res = NULL;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!essDN) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, NULL);
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
	object->object.mailbox->mailboxstore = mailboxstore;

	object->object.mailbox->owner_EssDN = talloc_strdup(object->object.mailbox, essDN);
	ret = ldb_search(emsmdbp_ctx->samdb_ctx, mem_ctx, &res,
			 ldb_get_default_basedn(emsmdbp_ctx->samdb_ctx),
			 LDB_SCOPE_SUBTREE, recipient_attrs, "legacyExchangeDN=%s",
			 ldb_binary_encode_string(mem_ctx, object->object.mailbox->owner_EssDN));
	if (!ret && res->count == 1) {
		accountName = ldb_msg_find_attr_as_string(res->msgs[0], "sAMAccountName", NULL);
		if (accountName) {
			object->object.mailbox->owner_username = talloc_strdup(object->object.mailbox, accountName);

		}
		displayName = ldb_msg_find_attr_as_string(res->msgs[0], "displayName", NULL);
		if (displayName) {
			object->object.mailbox->owner_Name = talloc_strdup(object->object.mailbox,
									   displayName);
		}
	}

	if (mailboxstore == true) {
		/* Retrieve Mailbox folder identifier */
		openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username,
						0x1, &object->object.mailbox->folderID);
	} else {
		/* Retrieve Public folder identifier */
		openchangedb_get_PublicFolderID(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, EMSMDBP_PF_ROOT, &object->object.mailbox->folderID);
	}

	object->object.mailbox->szUserDN = talloc_strdup(object->object.mailbox, emsmdbp_ctx->szUserDN);

	talloc_free(res);

	return object;
}


/**
   \details Initialize a folder object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param folderID the folder identifier
   \param parent emsmdbp object of the parent folder for this folder

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_folder_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   uint64_t folderID,
							   struct emsmdbp_object *parent_object)
{
	struct emsmdbp_object			*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent_object);
	if (!object) return NULL;

	object->object.folder = talloc_zero(object, struct emsmdbp_object_folder);
	if (!object->object.folder) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FOLDER;
	object->object.folder->folderID = folderID;
	object->object.folder->mapistore_root = false;
	object->object.folder->contextID = (uint32_t) -1;
 
	return object;
}

enum MAPISTATUS emsmdbp_folder_get_folder_count(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *folder, uint32_t *row_countp)
{
	uint64_t		folderID;
	enum MAPISTATUS		retval;
	enum mapistore_error	ret;

	if (emsmdbp_is_mapistore(folder)) {
		ret = mapistore_folder_get_child_count(emsmdbp_ctx->mstore_ctx,
								emsmdbp_get_contextID(folder),
								folder->backend_object,
								MAPISTORE_FOLDER_TABLE, row_countp);
		retval = mapistore_error_to_mapi(ret);
	}
	else {
		if (folder->type == EMSMDBP_OBJECT_FOLDER) {
			folderID = folder->object.folder->folderID;
		}
		else if (folder->type == EMSMDBP_OBJECT_MAILBOX) {
			folderID = folder->object.folder->folderID;
		}
		else {
			OC_DEBUG(5, "unsupported object type\n");
			return MAPI_E_INVALID_OBJECT;
		}
		printf("emsmdbp_folder_get_folder_count: folderID = %"PRIu64"\n", folderID);
		retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, folderID, row_countp);
	}

	return retval;
}


/**
   \details Return the full number of folders within specified
   folder's hierarchy

   \param emsmdbp_ctx pointer to the emsmdbp context
   \param folder pointer to the emsmdb folder to start the count from
   \param row_countp pointer on the number of folders to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI_ERROR
 */
enum MAPISTATUS emsmdbp_folder_get_recursive_folder_count(struct emsmdbp_context *emsmdbp_ctx,
							  struct emsmdbp_object *folder,
							  uint32_t *row_countp)
{
	enum MAPISTATUS		retval = MAPI_E_SUCCESS;
	struct emsmdbp_object	*table_object;
	uint32_t		count = 0;
	struct mapi_handles	*rec = NULL;
	uint32_t		i;
	struct SPropTagArray	*SPropTagArray = NULL;
	uint32_t		handle = 0;

	retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, folder, &count);
	if ((retval == MAPI_E_SUCCESS) && count) {
		*row_countp += count;

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
		OPENCHANGE_RETVAL_IF(retval, retval, NULL);
		table_object = emsmdbp_folder_open_table(rec, folder, MAPISTORE_FOLDER_TABLE, rec->handle);
		if (!table_object) {
			mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
			retval = MAPI_E_INVALID_OBJECT;
			goto end;
		}

		table_object->object.table->prop_count = 1;
		table_object->object.table->properties = talloc_array(table_object, enum MAPITAGS, 1);
		if (!table_object->object.table->properties) {
			mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
			talloc_free(table_object);
			retval = MAPI_E_INVALID_OBJECT;
			goto end;
		}
		table_object->object.table->properties[0] = PidTagFolderId;

		SPropTagArray = set_SPropTagArray(table_object, 1, PidTagFolderId);
		mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(table_object),
					    table_object->backend_object, SPropTagArray->cValues,
					    SPropTagArray->aulPropTag);
		for (i = 0; i < count; i++) {
			void		**data_pointers = NULL;
			enum MAPISTATUS	*retvals = NULL;

			data_pointers = emsmdbp_object_table_get_row_props(table_object, emsmdbp_ctx, table_object,
									   i, MAPISTORE_PREFILTERED_QUERY, &retvals);

			if (data_pointers) {
				struct emsmdbp_object	*sfolder = NULL;

				retval = emsmdbp_object_open_folder_by_fid(table_object, emsmdbp_ctx, folder,
									   *((uint64_t *)data_pointers[0]), &sfolder);
				if (retval == MAPI_E_SUCCESS) {
					uint32_t scount = 0;

					retval = emsmdbp_folder_get_recursive_folder_count(emsmdbp_ctx, sfolder, &scount);
					*row_countp += scount;
					talloc_free(sfolder);
				}
				talloc_free(data_pointers);
				talloc_free(retvals);
			}
		}
		talloc_free(table_object->object.table->properties);
		talloc_free(SPropTagArray);
		talloc_free(table_object);
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
	}
end:
	return retval;
}


static inline enum MAPISTATUS emsmdbp_object_move_folder_to_mapistore_root(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *folder, struct emsmdbp_object *parent_folder, const char *new_name)
{
	uint64_t		parent_fid, fid, test_folder_id, change_number;
	enum MAPISTATUS		retval;
	char			*mapistore_uri;
	TALLOC_CTX		*mem_ctx;


	/* Sanity checks */
	MAPI_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!parent_folder, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!new_name, MAPI_E_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);
	MAPI_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	if (parent_folder->type == EMSMDBP_OBJECT_MAILBOX) {
		parent_fid = parent_folder->object.mailbox->folderID;
	} else { /* EMSMDBP_OBJECT_FOLDER */
		parent_fid = parent_folder->object.folder->folderID;
	}

	if (openchangedb_get_fid_by_name(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, parent_fid,
					 new_name, &test_folder_id) == MAPI_E_SUCCESS) {
		OC_DEBUG(4, "emsmdbp_object: move_folder_to_mapistore_root duplicate folder error\n");
		retval = MAPI_E_COLLISION;
		goto end;
	}

	fid = folder->object.folder->folderID;

	retval = emsmdbp_get_uri_from_fid(mem_ctx, emsmdbp_ctx, fid, &mapistore_uri);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(0, "Cannot locate folder id: %"PRIu64" on indexing database\n", fid);
		goto end;
	}

	retval = openchangedb_get_new_changeNumber(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, &change_number);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(0, "Cannot get a new change number: %s\n", mapi_get_errstr(retval));
		goto end;
	}

	retval = openchangedb_create_folder(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username,
					    parent_fid, fid, change_number,
					    mapistore_uri, -1);
	if (retval != MAPI_E_SUCCESS) {
		OC_DEBUG(0, "OpenChangeDB folder creation failed: 0x%.8x\n", retval);
		goto end;
	}

	/* Set folder properties is done in the upper layer by
	   oxcfxics in cached mode. This is a temporary hack until we
	   implement copy properties from MAPIStore to OpenChange
	   DB. */

	folder->object.folder->mapistore_root = true;

	OC_DEBUG(5, "New MAPIStore root folder moved at URI: %s\n", mapistore_uri);
	retval = MAPI_E_SUCCESS;

end:
	talloc_free(mem_ctx);
	return retval;
}

_PUBLIC_ enum mapistore_error emsmdbp_folder_move_folder(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *move_folder, struct emsmdbp_object *target_folder, TALLOC_CTX *mem_ctx, const char *new_name)
{
	enum mapistore_error	ret;
	enum MAPISTATUS		retval;
	uint32_t		contextID;
	int			system_idx;
	bool			is_top_of_IS, is_special;

	/* TODO: we should provide the ability to perform this operation between non-mapistore objects or between mapistore and non-mapistore objects */
	if (!emsmdbp_is_mapistore(move_folder)) {
		return MAPISTORE_ERR_DENIED;
	}

	if (!emsmdbp_is_mapistore(target_folder)) {
		/* target is the "Top of Information Store" ? */
		retval = openchangedb_get_system_idx(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, target_folder->object.folder->folderID, &system_idx);
		if (retval != MAPI_E_SUCCESS) {
			return MAPISTORE_ERROR;
		}
		is_top_of_IS = (system_idx == EMSMDBP_TOP_INFORMATION_STORE);
		if (!is_top_of_IS) {
			return MAPISTORE_ERR_DENIED;
		}
	}
	else {
		is_top_of_IS = false;
	}

	/* we check whether the folder is a special folder that cannot be moved */
	if (move_folder->object.folder->mapistore_root) {
		retval = openchangedb_get_system_idx(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, move_folder->object.folder->folderID, &system_idx);
		if (retval != MAPI_E_SUCCESS) {
			return MAPISTORE_ERROR;
		}
		is_special = (system_idx != -1);
		if (is_special) {
			return MAPISTORE_ERR_DENIED;
		}
	}

	contextID = emsmdbp_get_contextID(move_folder);
	if (is_top_of_IS) {
		/* We move it in MAPIStore backend and then we create
		   the required properties for a MAPIStore root
		   folder. This is required as indexing database is
		   updated in mapistore layer */
		ret = mapistore_folder_move_folder(emsmdbp_ctx->mstore_ctx, contextID, move_folder->backend_object, NULL, mem_ctx, new_name);
		if (ret == MAPISTORE_SUCCESS) {
			retval = emsmdbp_object_move_folder_to_mapistore_root(emsmdbp_ctx, move_folder, target_folder, new_name);
			if (retval != MAPI_E_SUCCESS) {
				OC_DEBUG(5, "Move folder to MAPIStore root failed: %s\n", mapi_get_errstr(retval));
				return MAPISTORE_ERROR;
			}
		}
	} else {
		ret = mapistore_folder_move_folder(emsmdbp_ctx->mstore_ctx, contextID, move_folder->backend_object, target_folder->backend_object, mem_ctx, new_name);
		if (move_folder->object.folder->mapistore_root) {
			retval = openchangedb_delete_folder(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, move_folder->object.folder->folderID);
			if (retval != MAPI_E_SUCCESS) {
				OC_DEBUG(0, "an error occurred during the deletion of the folder entry in the openchange db: %d\n", retval);
			}
		}
	}

	return ret;
}

/**
   \details  Delete the fmids from a folder in the indexing database.

   \param mstore_ctx pointer to the mapistore context
   \param context_id the context identifier
   \param username the owner of the folder to delete its entries
   \param fid the folder identifier
   \param deleted_fmid the array of child fmids from the folder
   \param deleted_fmid_count the number of deleted_fmids
   \param flags the delete flags. See emsmdbp_folder_delete for details.

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error.
*/
_PUBLIC_ enum mapistore_error emsmdbp_folder_delete_indexing_records(struct mapistore_context *mstore_ctx, uint32_t context_id, char *username, uint64_t fid, uint64_t *deleted_fmids, uint32_t deleted_fmids_count, uint8_t flags)
{
        enum mapistore_error    ret;
        uint8_t                 delete_type_flag;
        uint32_t                i;

        delete_type_flag = (flags & DELETE_HARD_DELETE) ? MAPISTORE_PERMANENT_DELETE : MAPISTORE_SOFT_DELETE;
        ret = mapistore_indexing_record_del_fid(mstore_ctx, context_id, username, fid, delete_type_flag);
        MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);

        for (i = 0; i < deleted_fmids_count; i++) {
                ret = mapistore_indexing_record_del_fid(mstore_ctx, context_id, username,
                                                        deleted_fmids[i], delete_type_flag);
                MAPISTORE_RETVAL_IF(ret != MAPISTORE_SUCCESS, ret, NULL);
        }

        return MAPISTORE_SUCCESS;
}

/**
   \details Delete a folder.

   \param emsmdbp_ctx pointer to the emsmdbp context
   \param parent_folder the parent folder
   \param fid the folder identifier to delete from parent_folder
   \param flags the delete flags.

   Possible values for flags are:
   -# DEL_MESSAGES Delete all the messages in the folder
   -# DEL_FOLDERS Delete the subfolder and all of its subfolders
   -# DELETE_HARD_DELETE Hard delete the folder

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error.
*/
_PUBLIC_ enum mapistore_error emsmdbp_folder_delete(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_folder, uint64_t fid, uint8_t flags)
{
	enum mapistore_error	ret;
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	bool			mailboxstore;
	uint32_t		context_id, deleted_fmids_count;
	void			*subfolder;
	char			*mapistoreURL;
	uint64_t		*deleted_fmids;

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	mailboxstore = emsmdbp_is_mailboxstore(parent_folder);
	if (emsmdbp_is_mapistore(parent_folder)) {	/* fid is not a mapistore root */
		OC_DEBUG(0, "Deleting mapistore folder\n");
		/* handled by mapistore */
		context_id = emsmdbp_get_contextID(parent_folder);

		ret = mapistore_folder_open_folder(emsmdbp_ctx->mstore_ctx, context_id, parent_folder->backend_object, mem_ctx, fid, &subfolder);
		if (ret != MAPISTORE_SUCCESS) {
			goto end;
		}

		ret = mapistore_folder_delete(emsmdbp_ctx->mstore_ctx, context_id, subfolder, flags, mem_ctx, &deleted_fmids, &deleted_fmids_count);
		if (ret != MAPISTORE_SUCCESS) {
			goto end;
		}

		/* Update indexing entries */
		ret = emsmdbp_folder_delete_indexing_records(emsmdbp_ctx->mstore_ctx, context_id,
							     emsmdbp_get_owner(parent_folder),
							     fid, deleted_fmids, deleted_fmids_count,
							     flags);
		if (ret != MAPISTORE_SUCCESS) {
			goto end;
		}
	}
	else {
		retval = openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fid, &mapistoreURL, mailboxstore);
		if (retval != MAPI_E_SUCCESS) {
			ret = MAPISTORE_ERR_NOT_FOUND;
			goto end;
		}

		if (mapistoreURL) {	/* fid is mapistore root */
			ret = mapistore_search_context_by_uri(emsmdbp_ctx->mstore_ctx, mapistoreURL, &context_id, &subfolder);
			if (ret == MAPISTORE_SUCCESS) {
				mapistore_add_context_ref_count(emsmdbp_ctx->mstore_ctx, context_id);
			} else {
				ret = mapistore_add_context(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, mapistoreURL, fid, &context_id, &subfolder);
				if (ret != MAPISTORE_SUCCESS) {
					goto end;
				}
			}

			ret = mapistore_folder_delete(emsmdbp_ctx->mstore_ctx, context_id,
						      subfolder, flags, mem_ctx, &deleted_fmids,
						      &deleted_fmids_count);
			if (ret != MAPISTORE_SUCCESS) {
				goto end;
			}

			mapistore_del_context(emsmdbp_ctx->mstore_ctx, context_id);

			/* Update indexing entries */
			ret = emsmdbp_folder_delete_indexing_records(emsmdbp_ctx->mstore_ctx, context_id,
								     emsmdbp_get_owner(parent_folder),
								     fid, deleted_fmids, deleted_fmids_count,
								     flags);
			if (ret != MAPISTORE_SUCCESS) {
				goto end;
			}
		}

		retval = openchangedb_delete_folder(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, fid);
		if (retval != MAPI_E_SUCCESS) {
			ret = MAPISTORE_ERR_NOT_FOUND;
			goto end;
		}
	}

	ret = MAPISTORE_SUCCESS;

end:
	talloc_free(mem_ctx);

	return ret;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_folder_open_table(TALLOC_CTX *mem_ctx, 
							  struct emsmdbp_object *parent_object, 
							  uint32_t table_type, uint32_t handle_id)
{
	struct emsmdbp_object	*table_object;
	uint64_t		folderID;
	uint8_t			mstore_type;
	int			ret;

	if (!(parent_object->type != EMSMDBP_OBJECT_FOLDER || parent_object->type != EMSMDBP_OBJECT_MAILBOX)) {
		OC_DEBUG(0, "parent_object must be EMSMDBP_OBJECT_FOLDER or EMSMDBP_OBJECT_MAILBOX (type =  %d)\n", parent_object->type);
		return NULL;
	}

	if (parent_object->type == EMSMDBP_OBJECT_FOLDER && parent_object->object.folder->postponed_props) {
		ret = emsmdbp_object_folder_commit_creation(parent_object->emsmdbp_ctx, parent_object, true);
		if (ret != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "folder_commit_creatin failed with error: 0x%.8X", ret);
			return NULL;
		}
	}

	table_object = emsmdbp_object_table_init(mem_ctx, parent_object->emsmdbp_ctx, parent_object);
	if (table_object) {
		table_object->object.table->handle = handle_id;
		table_object->object.table->ulType = table_type;
		if (emsmdbp_is_mapistore(parent_object)) {
			switch (table_type) {
			case MAPISTORE_MESSAGE_TABLE:
				mstore_type = MAPISTORE_MESSAGE_TABLE;
				break;
			case MAPISTORE_FAI_TABLE:
				mstore_type = MAPISTORE_FAI_TABLE;
				break;
			case MAPISTORE_FOLDER_TABLE:
				mstore_type = MAPISTORE_FOLDER_TABLE;
				break;
			case MAPISTORE_PERMISSIONS_TABLE:
				mstore_type = MAPISTORE_PERMISSIONS_TABLE;
				break;
			default:
				OC_PANIC(false, ("Unhandled table type for folders: %d\n", table_type));
				talloc_free(table_object);
				return NULL;
			}

			ret = mapistore_folder_open_table(parent_object->emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(parent_object), parent_object->backend_object, table_object, mstore_type, handle_id, &table_object->backend_object, &table_object->object.table->denominator);
			if (ret != MAPISTORE_SUCCESS) {
				talloc_free(table_object);
				table_object = NULL;
			}
		}
		else {
			if (table_type == MAPISTORE_FOLDER_TABLE) {
				/* this gets data both for openchangedb and mapistore: needs improvement */
				emsmdbp_folder_get_folder_count(parent_object->emsmdbp_ctx, parent_object, &table_object->object.table->denominator);
			}
			else {
				/* Retrieve folder ID */
				switch (parent_object->type) {
				case EMSMDBP_OBJECT_FOLDER:
					folderID = parent_object->object.folder->folderID;
					break;
				case EMSMDBP_OBJECT_MAILBOX:
					folderID = parent_object->object.mailbox->folderID;
					break;
				default:
					OC_DEBUG(5, "Unsupported object type");
					table_object->object.table->denominator = 0;
					return table_object;
				}

				/* Non-mapistore message tables */
				switch (table_type) {
				case MAPISTORE_MESSAGE_TABLE:
					openchangedb_get_message_count(parent_object->emsmdbp_ctx->oc_ctx,
								       parent_object->emsmdbp_ctx->username,
								       folderID,
								       &table_object->object.table->denominator,
								       false);
					break;
				case MAPISTORE_FAI_TABLE:
					openchangedb_get_message_count(parent_object->emsmdbp_ctx->oc_ctx,
								       parent_object->emsmdbp_ctx->username,
								       folderID,
								       &table_object->object.table->denominator,
								       true);
					break;
				default:
					table_object->object.table->denominator = 0;
					OC_PANIC(false, ("Unhandled openchangedb table type for folders: %d\n", table_type));
					return table_object;
				}
			}
			if (!emsmdbp_is_mapistore(parent_object)) {
				/* Retrieve folder ID */
				switch (parent_object->type) {
				case EMSMDBP_OBJECT_FOLDER:
					folderID = parent_object->object.folder->folderID;
					break;
				case EMSMDBP_OBJECT_MAILBOX:
					folderID = parent_object->object.mailbox->folderID;
					break;
				default:
					OC_DEBUG(5, "Unsupported object type");
					table_object->object.table->denominator = 0;
					return table_object;
				}
				OC_DEBUG(5, "Initializing openchangedb table\n");
				openchangedb_table_init((TALLOC_CTX *)table_object, parent_object->emsmdbp_ctx->oc_ctx, parent_object->emsmdbp_ctx->username, table_type, folderID, &table_object->backend_object);
			}
		}
	}

	return table_object;	
}

/**
   \details Initialize a table object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param parent emsmdbp object of the parent

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_table_init(TALLOC_CTX *mem_ctx,
							  struct emsmdbp_context *emsmdbp_ctx,
							  struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER && parent->type != EMSMDBP_OBJECT_MAILBOX && parent->type != EMSMDBP_OBJECT_MESSAGE) return NULL;

	/* Initialize table object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;
	
	object->object.table = talloc_zero(object, struct emsmdbp_object_table);
	if (!object->object.table) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_TABLE;
	object->object.table->prop_count = 0;
	object->object.table->properties = NULL;
	object->object.table->numerator = 0;
	object->object.table->denominator = 0;
	object->object.table->ulType = 0;
	object->object.table->restricted = false;
	object->object.table->flags = 0;
	object->object.table->subscription = false;

	return object;
}

_PUBLIC_ int emsmdbp_object_table_get_available_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, struct SPropTagArray **propertiesp)
{
	int				retval;
	struct SPropTagArray		*properties;
	uint32_t			contextID;

	if (table_object->type != EMSMDBP_OBJECT_TABLE) {
		return MAPISTORE_ERROR;
	}

	if (emsmdbp_is_mapistore(table_object)) {
		contextID = emsmdbp_get_contextID(table_object);
		retval = mapistore_table_get_available_properties(emsmdbp_ctx->mstore_ctx, contextID, table_object->backend_object, mem_ctx, propertiesp);
	}
	else {
		properties = talloc_zero(mem_ctx, struct SPropTagArray);
		properties->aulPropTag = talloc_zero(properties, enum MAPITAGS);
		/* TODO: this list might not be complete */
		SPropTagArray_add(properties, properties, PR_FID);
		SPropTagArray_add(properties, properties, PR_PARENT_FID);
		SPropTagArray_add(properties, properties, PR_DISPLAY_NAME_UNICODE);
		SPropTagArray_add(properties, properties, PR_COMMENT_UNICODE);
		SPropTagArray_add(properties, properties, PR_ACCESS);
		SPropTagArray_add(properties, properties, PR_ACCESS_LEVEL);
		SPropTagArray_add(properties, properties, PidTagRights);
		SPropTagArray_add(properties, properties, PidTagExtendedFolderFlags);
		SPropTagArray_add(properties, properties, PidTagDesignInProgress);
		SPropTagArray_add(properties, properties, PidTagSecureOrigination);
		SPropTagArray_add(properties, properties, PR_NTSD_MODIFICATION_TIME);
		SPropTagArray_add(properties, properties, PR_ADDITIONAL_REN_ENTRYIDS);
		SPropTagArray_add(properties, properties, PR_ADDITIONAL_REN_ENTRYIDS_EX);
		SPropTagArray_add(properties, properties, PR_CREATION_TIME);
		SPropTagArray_add(properties, properties, PR_CREATOR_SID);
		SPropTagArray_add(properties, properties, PR_CREATOR_ENTRYID);
		SPropTagArray_add(properties, properties, PR_LAST_MODIFICATION_TIME);
		SPropTagArray_add(properties, properties, PR_LAST_MODIFIER_SID);
		SPropTagArray_add(properties, properties, PR_LAST_MODIFIER_ENTRYID);
		SPropTagArray_add(properties, properties, PR_ATTR_HIDDEN);
		SPropTagArray_add(properties, properties, PidTagAttributeSystem);
		SPropTagArray_add(properties, properties, PR_ATTR_READONLY);
		SPropTagArray_add(properties, properties, PR_EXTENDED_ACL_DATA);
		SPropTagArray_add(properties, properties, PR_CONTAINER_CLASS_UNICODE);
		SPropTagArray_add(properties, properties, PR_CONTENT_COUNT);
		SPropTagArray_add(properties, properties, PidTagAssociatedContentCount);
		SPropTagArray_add(properties, properties, PR_SUBFOLDERS);
		SPropTagArray_add(properties, properties, PR_MAPPING_SIGNATURE);
		SPropTagArray_add(properties, properties, PR_USER_ENTRYID);
		SPropTagArray_add(properties, properties, PR_MAILBOX_OWNER_ENTRYID);
		SPropTagArray_add(properties, properties, PR_MAILBOX_OWNER_NAME_UNICODE);
		SPropTagArray_add(properties, properties, PR_IPM_APPOINTMENT_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_CONTACT_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_JOURNAL_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_NOTE_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_TASK_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_DRAFTS_ENTRYID);
		SPropTagArray_add(properties, properties, PR_REMINDERS_ONLINE_ENTRYID);
		SPropTagArray_add(properties, properties, PR_IPM_PUBLIC_FOLDERS_ENTRYID);
		SPropTagArray_add(properties, properties, PR_FOLDER_XVIEWINFO_E);
		SPropTagArray_add(properties, properties, PR_FOLDER_VIEWLIST);
		SPropTagArray_add(properties, properties, PR_FREEBUSY_ENTRYIDS);
		SPropTagArray_add(properties, properties, 0x36de0003); /* some unknown prop that outlook sets */
		*propertiesp = properties;

		retval = MAPISTORE_SUCCESS;
	}

	return retval;
}

_PUBLIC_ void **emsmdbp_object_table_get_row_props(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *table_object, uint32_t row_id, enum mapistore_query_type query_type, enum MAPISTATUS **retvalsp)
{
        void				**data_pointers;
        enum MAPISTATUS			retval;
	enum mapistore_error		ret;
        enum MAPISTATUS			*retvals;
        struct emsmdbp_object_table	*table;
        struct mapistore_property_data	*properties;
        uint32_t			contextID, i, num_props;
	struct emsmdbp_object		*rowobject;
	uint64_t			*rowFMId;
	uint64_t			parentFolderId;
	bool				mapistore_folder;
	void				*odb_ctx;
	char				*owner;
	struct Binary_r			*binr;

        table = table_object->object.table;
        num_props = table_object->object.table->prop_count;

	data_pointers = talloc_zero_array(mem_ctx, void *, num_props);
	if (!data_pointers) {
		OC_DEBUG(0, "No more memory");
		return NULL;
	}
	retvals = talloc_zero_array(mem_ctx, enum MAPISTATUS, num_props);
	if (!retvals) {
		OC_DEBUG(0, "No more memory");
		talloc_free(data_pointers);
		return NULL;
	}

	if (emsmdbp_is_mapistore(table_object)) {
		contextID = emsmdbp_get_contextID(table_object);
		ret = mapistore_table_get_row(emsmdbp_ctx->mstore_ctx, contextID,
					      table_object->backend_object, data_pointers,
					      query_type, row_id, &properties);
		if (ret == MAPISTORE_SUCCESS) {
			for (i = 0; i < num_props; i++) {
				data_pointers[i] = properties[i].data;
                                        
				if (properties[i].error != MAPISTORE_SUCCESS) {
					retvals[i] = mapistore_error_to_mapi(properties[i].error);
				}
				else {
					if (properties[i].data == NULL) {
						retvals[i] = MAPI_E_NOT_FOUND;
					}
				}
			}
		}
		else {
			OC_DEBUG(5, "invalid object (likely due to a restriction)\n");
			talloc_free(retvals);
			talloc_free(data_pointers);
			return NULL;
		}
	} else {
		if (table_object->parent_object->type == EMSMDBP_OBJECT_FOLDER) {
			parentFolderId = table_object->parent_object->object.folder->folderID;
		}
		else if (table_object->parent_object->type == EMSMDBP_OBJECT_MAILBOX) {
			parentFolderId = table_object->parent_object->object.mailbox->folderID;
		}
		else {
			OC_DEBUG(5, "non-mapistore tables can only be client of folder objects\n");
			talloc_free(retvals);
			talloc_free(data_pointers);
			return NULL;
		}

		odb_ctx = talloc_zero(NULL, void);
		if (!odb_ctx) {
			OC_DEBUG(0, "No more memory");
			talloc_free(retvals);
			talloc_free(data_pointers);
			return NULL;
		}

		/* 1. retrieve the object id from odb */
		switch (table_object->object.table->ulType) {
		case MAPISTORE_FOLDER_TABLE:
			retval = openchangedb_table_get_property(odb_ctx, emsmdbp_ctx->oc_ctx, table_object->backend_object, PR_FID, row_id, (query_type == MAPISTORE_LIVEFILTERED_QUERY), (void **) &rowFMId);
			break;
		case MAPISTORE_MESSAGE_TABLE:
			retval = openchangedb_table_get_property(odb_ctx, emsmdbp_ctx->oc_ctx, table_object->backend_object, PR_MID, row_id, (query_type == MAPISTORE_LIVEFILTERED_QUERY), (void **) &rowFMId);
			break;
			/* case MAPISTORE_FAI_TABLE: 
			   retval = openchangedb_table_get_property(odb_ctx, table_object->backend_object, emsmdbp_ctx->oc_ctx,
			   PR_MID, row_id, (query_type == MAPISTORE_LIVEFILTERED_QUERY), (void **) &rowFMId);
			   break; */
		default:
			OC_DEBUG(5, "table type %d not supported for non-mapistore table\n", table_object->object.table->ulType);
			retval = MAPI_E_INVALID_OBJECT;
		}
		/* printf("openchangedb_table_get_property retval = 0x%.8x\n", retval); */
		if (retval != MAPI_E_SUCCESS) {
			talloc_free(retvals);
			talloc_free(data_pointers);
			talloc_free(odb_ctx);
			return NULL;
		}

		/* 2. open the corresponding object */
		switch (table_object->object.table->ulType) {
		case MAPISTORE_FOLDER_TABLE:
			ret = emsmdbp_object_open_folder(odb_ctx, table_object->parent_object->emsmdbp_ctx, table_object->parent_object, *(uint64_t *)rowFMId, &rowobject);
			if (ret == MAPISTORE_SUCCESS) {
				mapistore_folder = emsmdbp_is_mapistore(rowobject);
				if (mapistore_folder) {
					contextID = emsmdbp_get_contextID(rowobject);
				}
			}
			break;
		case MAPISTORE_MESSAGE_TABLE:
			ret = emsmdbp_object_message_open(odb_ctx, table_object->parent_object->emsmdbp_ctx, table_object->parent_object, parentFolderId, *(uint64_t *)rowFMId, false, &rowobject, NULL);
			mapistore_folder = false;
			break;
		default:
			OC_PANIC(true, ("Trying open row in unsupported type of table. Table type: %d\n", table_object->object.table->ulType));
			return NULL;
		}
		if (ret != MAPISTORE_SUCCESS) {
			talloc_free(retvals);
			talloc_free(data_pointers);
			talloc_free(odb_ctx);
			return NULL;
		}

		/* read the row properties */
                retval = MAPI_E_SUCCESS;
		for (i = 0; retval != MAPI_E_INVALID_OBJECT && i < num_props; i++) {
			if (mapistore_folder) {
				/* a hack to avoid fetching dynamic fields from openchange.ldb */
				switch (table->properties[i]) {
				case PidTagParentFolderId:
				case PR_CONTENT_COUNT:
				case PidTagAssociatedContentCount:
				case PR_CONTENT_UNREAD:
				case PidTagFolderChildCount:
				case PR_SUBFOLDERS:
				case PidTagDeletedCountTotal:
				case PidTagAccess:
				case PidTagAccessLevel:
				case PidTagFolderFlags:
				case PidTagHierRev:
				case PidTagLocalCommitTimeMax:
				case PidTagRights: {
					struct SPropTagArray props;
					void **local_data_pointers;
					enum MAPISTATUS *local_retvals;

					props.cValues = 1;
					props.aulPropTag = table->properties + i;

					local_data_pointers = emsmdbp_object_get_properties(data_pointers, emsmdbp_ctx, rowobject, &props, &local_retvals);
					data_pointers[i] = local_data_pointers[0];
					retval = local_retvals[0];
				}
					break;
				case PidTagSourceKey:
					owner = emsmdbp_get_owner(table_object);
					emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, owner, rowobject->object.folder->folderID, &binr);
					data_pointers[i] = binr;
					retval = MAPI_E_SUCCESS;
					break;
				default:
					retval = openchangedb_table_get_property(data_pointers, emsmdbp_ctx->oc_ctx,
										 table_object->backend_object,
										 table->properties[i], 
										 row_id,
										 (query_type == MAPISTORE_LIVEFILTERED_QUERY),
										 data_pointers + i);
				}
			}
			else {
				retval = openchangedb_table_get_property(data_pointers, emsmdbp_ctx->oc_ctx,
									 table_object->backend_object,
									 table->properties[i], 
									 row_id,
									 (query_type == MAPISTORE_LIVEFILTERED_QUERY),
									 data_pointers + i);
			}

			if (retval == MAPI_E_INVALID_OBJECT) {
				OC_DEBUG(5, "invalid object in non-mapistore folder, count set to 0\n");
				talloc_free(retvals);
				talloc_free(data_pointers);
				talloc_free(odb_ctx);
				return NULL;
			}
			else {
				retvals[i] = retval;
			}
		}

		talloc_free(odb_ctx);
	}

        if (retvalsp) {
                *retvalsp = retvals;
	}

        return data_pointers;
}



/**
   \details This function process the hierarchy of folders recursively
   and fill requested rows.

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb context
   \param context_object pointer to the context object
   \param datablob pointer to the DATA blob to fill
   \param properties pointer to the array of properties used to fill a
   row of the table
   \param current_fid the FolderID of the folder to open and process
   \param remaining pointer on the remaining rows to process
   \param count pointer on the number of rows processed to return

   \note This implementation is very preliminary and lacks several use cases:
   1. It does not handle backward read
   2. It does not handle positioning of the folder object and rewind
   of the hierarchy when position != 0

   We currently rely on Outlook asking for 4096 rows to fill this
   buffer in one operation.

   \return MAPI_E_SUCCESS on success, otherwise MAPI error.
*/
_PUBLIC_ enum MAPISTATUS emsmdbp_object_table_get_recursive_row_props(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
								      struct emsmdbp_object *context_object,
								      DATA_BLOB *datablob, struct SPropTagArray *properties,
								      uint64_t current_fid, int64_t *remaining, uint32_t *count)
{
	enum MAPISTATUS		retval;
	struct emsmdbp_object	*folder = NULL;
	struct emsmdbp_object	*table = NULL;
	struct mapi_handles	*rec = NULL;
	enum MAPISTATUS		*retvals;
	void			**data_pointers = NULL;
	uint32_t		handle = 0;
	uint32_t		i = 0;
	uint32_t		j;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!context_object, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!remaining, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!*remaining, MAPI_E_SUCCESS, NULL);
	OPENCHANGE_RETVAL_IF(!count, MAPI_E_INVALID_PARAMETER, NULL);

	/* open current folder */
	if (current_fid) {
		retval = emsmdbp_object_open_folder_by_fid(mem_ctx, emsmdbp_ctx, context_object, current_fid, &folder);
		OPENCHANGE_RETVAL_IF(retval, MAPI_E_INVALID_OBJECT, NULL);

		retval = mapi_handles_add(emsmdbp_ctx->handles_ctx, handle, &rec);
		OPENCHANGE_RETVAL_IF(retval, MAPI_E_INVALID_OBJECT, folder);

		table = emsmdbp_folder_open_table(rec, folder, MAPISTORE_FOLDER_TABLE, rec->handle);
		table->object.table->prop_count = properties->cValues;
		table->object.table->properties = properties->aulPropTag;
		mapistore_table_set_columns(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(table),
					    table->backend_object, properties->cValues,
					    properties->aulPropTag);
		if (!table) {
			retval = MAPI_E_INVALID_OBJECT;
			goto end;
		}
	} else {
		/* otherwise reuse existing context object */
		table = context_object;
		folder = table->parent_object;
	}

	if (!table->object.table->denominator) {
		retval = MAPI_E_SUCCESS;
		goto end;
	}

	mapidump_SPropTagArray(properties);
	for (i = 0; i < table->object.table->denominator && *remaining > 0; i++) {
		data_pointers = emsmdbp_object_table_get_row_props(mem_ctx, emsmdbp_ctx, table, i, MAPISTORE_PREFILTERED_QUERY, &retvals);
		if (data_pointers) {
			uint64_t	fid = 0;

			emsmdbp_fill_table_row_blob(mem_ctx, emsmdbp_ctx, datablob, properties->cValues,
						    properties->aulPropTag, data_pointers, retvals);
			*remaining = *remaining - 1;
			*count = *count + 1;

			for (j = 0; j < properties->cValues; j++) {
				if ((properties->aulPropTag[j] == PidTagFolderId) && (retvals[j] == MAPI_E_SUCCESS)) {
					fid = *(uint64_t *)data_pointers[j];
					retval = emsmdbp_object_table_get_recursive_row_props(mem_ctx, emsmdbp_ctx,
											      folder, datablob, properties,
											      fid, remaining, count);
					break;
				}
			}

			talloc_free(data_pointers);
			talloc_free(retvals);
		}
	}
end:
	if (current_fid) {
		talloc_free(table);
		mapi_handles_delete(emsmdbp_ctx->handles_ctx, rec->handle);
		talloc_free(folder);
	}
	return retval;
}



_PUBLIC_ void emsmdbp_fill_table_row_blob(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
					  DATA_BLOB *table_row, uint16_t num_props,
					  enum MAPITAGS *properties,
					  void **data_pointers, enum MAPISTATUS *retvals)
{
        uint16_t i;
        uint8_t flagged;
        enum MAPITAGS property;
        void *data;
        uint32_t retval;

        flagged = 0;

        for (i = 0; !flagged && i < num_props; i++) {
                if (retvals[i] != MAPI_E_SUCCESS) {
                        flagged = 1;
                }
        }

        if (flagged) {
                libmapiserver_push_property(mem_ctx,
                                            0x0000000b, (const void *)&flagged,
                                            table_row, 0, 0, 0);
        }
        else {
                libmapiserver_push_property(mem_ctx,
                                            0x00000000, (const void *)&flagged,
                                            table_row, 0, 1, 0);
        }

        for (i = 0; i < num_props; i++) {
                property = properties[i];
                retval = retvals[i];
                if (retval != MAPI_E_SUCCESS) {
                        property = (property & 0xFFFF0000) + PT_ERROR;
                        data = &retval;
                }
                else {
                        data = data_pointers[i];
                }

                libmapiserver_push_property(mem_ctx,
                                            property, data, table_row,
                                            flagged?PT_ERROR:0, flagged, 0);
        }
}

/**
   \details Initialize a message object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param messageID the message identifier
   \param parent emsmdbp object of the parent

   \return Allocated emsmdbp object on success, otherwise NULL
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_init(TALLOC_CTX *mem_ctx,
							    struct emsmdbp_context *emsmdbp_ctx,
							    uint64_t messageID,
							    struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;
        if (parent->type != EMSMDBP_OBJECT_FOLDER && parent->type != EMSMDBP_OBJECT_MAILBOX && parent->type != EMSMDBP_OBJECT_ATTACHMENT) {
		OC_DEBUG(5, "expecting EMSMDBP_OBJECT_FOLDER/_MAILBOX/_ATTACHMENT as type of parent object\n");
		return NULL;
	}

	/* Initialize message object */
	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.message = talloc_zero(object, struct emsmdbp_object_message);
	if (!object->object.message) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_MESSAGE;
	object->object.message->messageID = messageID;
	object->object.message->read_write = false;

	return object;
}

/* Get the legacyExchangeDN for the administrative group . */
static enum MAPISTATUS mapiserver_get_administrative_group_legacyexchangedn(TALLOC_CTX *mem_ctx,
									    struct emsmdbp_context *emsmdbp_ctx,
									    char **legacyexchangedn)
{
	int			ret;
	enum MAPISTATUS		retval;
	struct  ldb_result	*res = NULL;
	const char * const	attrs[] = { "legacyExchangeDN", NULL };
	struct ldb_dn		*basedn = 0;
	char			*group_name = NULL;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!legacyexchangedn, MAPI_E_INVALID_PARAMETER, NULL);

	retval = emsmdbp_get_org_dn(emsmdbp_ctx, &basedn);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);
	retval = emsmdbp_fetch_organizational_units(mem_ctx, emsmdbp_ctx, NULL, &group_name);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);

	ret = ldb_search(emsmdbp_ctx->samdb_ctx, emsmdbp_ctx, &res,
			 basedn, LDB_SCOPE_SUBTREE, attrs,
			 "(&(objectClass=msExchAdminGroup)(msExchDefaultAdminGroup=TRUE)(cn=%s))",
			 ldb_binary_encode_string(mem_ctx, group_name));

	/* If the search failed */
	if (ret != LDB_SUCCESS) {
		OC_DEBUG(1, "[emsmdbp_object]: ldb_search failure.\n");
		return MAPI_E_NOT_FOUND;
	}

	*legacyexchangedn = talloc_strdup(mem_ctx, ldb_msg_find_attr_as_string(res->msgs[0], "legacyExchangeDN", NULL));
	OPENCHANGE_RETVAL_IF(!*legacyexchangedn, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	return MAPI_E_SUCCESS;
}


static enum MAPISTATUS emsmdbp_fetch_freebusy(TALLOC_CTX *mem_ctx,
					      struct emsmdbp_context *emsmdbp_ctx,
					      const char *username,
					      struct tm *start_tm,
					      struct tm *end_tm,
					      struct mapistore_freebusy_properties **fb_props_p)
{
	TALLOC_CTX		*local_mem_ctx;
	char			*email, *tmp, *administrativegroup;
	struct SPropTagArray	*props;
	void			**data_pointers;
	enum MAPISTATUS		*retvals = NULL;
	enum MAPISTATUS		retval;
	enum mapistore_error	retval_mapistore;
	struct emsmdbp_object	*mailbox, *inbox, *calendar;
	uint64_t		inboxFID, calendarFID;
	uint32_t		contextID;
	int			i;

	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!fb_props_p, MAPI_E_INVALID_PARAMETER, NULL);

	local_mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!local_mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	retval = mapiserver_get_administrative_group_legacyexchangedn(local_mem_ctx, emsmdbp_ctx, &administrativegroup);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

	// WARNING: the mechanism here will fail if username is not all lower-case, as LDB does not support case-insensitive queries
	tmp = talloc_strdup(local_mem_ctx, username);
	OPENCHANGE_RETVAL_IF(!tmp, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	while (*tmp) {
		*tmp = tolower(*tmp);
		tmp++;
	}
	email = talloc_asprintf(local_mem_ctx, "%s/cn=Recipients/cn=%s", administrativegroup, username);
	OPENCHANGE_RETVAL_IF(!email, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	/* open user mailbox */
	mailbox = emsmdbp_object_mailbox_init(local_mem_ctx, emsmdbp_ctx, email, true);
	OPENCHANGE_RETVAL_IF(!mailbox, MAPI_E_NOT_FOUND, local_mem_ctx);

	/* open Inbox */
	retval = openchangedb_get_SystemFolderID(emsmdbp_ctx->oc_ctx, username, EMSMDBP_INBOX, &inboxFID);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);
	retval = emsmdbp_object_open_folder_by_fid(local_mem_ctx, emsmdbp_ctx, mailbox, inboxFID, &inbox);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

	/* retrieve Calendar entry id */
	props = talloc_zero(mem_ctx, struct SPropTagArray);
	OPENCHANGE_RETVAL_IF(!props, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	props->cValues = 1;
	props->aulPropTag = talloc_zero(props, enum MAPITAGS);
	props->aulPropTag[0] = PR_IPM_APPOINTMENT_ENTRYID;

	data_pointers = emsmdbp_object_get_properties(local_mem_ctx, emsmdbp_ctx, inbox, props, &retvals);
	OPENCHANGE_RETVAL_IF(!data_pointers || retvals[0] != MAPI_E_SUCCESS, MAPI_E_NOT_FOUND, local_mem_ctx);

	calendarFID = 0;
	for (i = 0; i < 6; i++) {
		calendarFID <<= 8;
		calendarFID |= *(((struct Binary_r *) data_pointers[0])->lpb + (43 - i));
	}
	calendarFID <<= 16;
	calendarFID |= 1;

	/* open user calendar */
	retval = emsmdbp_object_open_folder_by_fid(local_mem_ctx, emsmdbp_ctx, mailbox, calendarFID, &calendar);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

	if (!emsmdbp_is_mapistore(calendar)) {
		OC_DEBUG(5, "[emsmdbp_object]: non-mapistore calendars are not supported for freebusy\n");
		OPENCHANGE_RETVAL_ERR(MAPI_E_NOT_IMPLEMENTED, local_mem_ctx);
	}

	contextID = emsmdbp_get_contextID(calendar);
	retval_mapistore = mapistore_folder_fetch_freebusy_properties(emsmdbp_ctx->mstore_ctx, contextID, calendar->backend_object, start_tm, end_tm, mem_ctx, fb_props_p);
	OPENCHANGE_RETVAL_IF(retval_mapistore != MAPISTORE_SUCCESS, MAPI_E_NOT_FOUND, local_mem_ctx);

	talloc_free(local_mem_ctx);

	return MAPI_E_SUCCESS;
}

static enum MAPISTATUS emsmdbp_object_message_fill_freebusy_properties(struct emsmdbp_object *message_object)
{
	/* freebusy mechanism:
	   - lookup events in range now + 3 months, requesting end date, start date and PidLidBusyStatus
	   - fill (olTentative) PidTagScheduleInfoMonthsTentative,
	   PidTagScheduleInfoFreeBusyTentative
	   - fill (olBusy) PidTagScheduleInfoMonthsBusy,
	   PidTagScheduleInfoFreeBusyBusy
	   - fill (olOutOfOffice) PidTagScheduleInfoMonthsAway,
	   PidTagScheduleInfoFreeBusyAway
	   - fill (olBusy + olOutOfOffice) PidTagScheduleInfoMonthsMerged,
	   PidTagScheduleInfoFreeBusyMerged
	   - fill PidTagFreeBusyPublishStart, PidTagFreeBusyPublishEnd and
	   PidTagFreeBusyRangeTimestamp.
	   - fill PidTagFreeBusyMessageEmailAddress */

	TALLOC_CTX				*mem_ctx;
	struct mapistore_freebusy_properties	*fb_props;
	char					*subject, *username;
	struct SPropTagArray			*props;
	void					**data_pointers;
	enum MAPISTATUS				*retvals = NULL;
	enum MAPISTATUS				retval;

	mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* 1. retrieve subject and deduce username */
	props = talloc_zero(mem_ctx, struct SPropTagArray);
	OPENCHANGE_RETVAL_IF(!props, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	props->cValues = 1;
	props->aulPropTag = talloc_zero(props, enum MAPITAGS);
	OPENCHANGE_RETVAL_IF(!props->aulPropTag, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	props->aulPropTag[0] = PR_NORMALIZED_SUBJECT_UNICODE;
	data_pointers = emsmdbp_object_get_properties(mem_ctx, message_object->emsmdbp_ctx, message_object, props, &retvals);
	OPENCHANGE_RETVAL_IF(!data_pointers || retvals[0] != MAPI_E_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	subject = data_pointers[0];
	/* FIXME: this is wrong, as the CN attribute may differ from the user's username (sAMAccountName) */
	// format is "..../CN="
	username = strrchr(subject, '/');
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_BAD_VALUE, mem_ctx);
	username += 4;  /* strlen("/CN=") == 4 */
	username = talloc_strdup(mem_ctx, username);
	OPENCHANGE_RETVAL_IF(!username, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	retval = emsmdbp_fetch_freebusy(mem_ctx, message_object->emsmdbp_ctx, username, NULL, NULL, &fb_props);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, mem_ctx);
	message_object->object.message->fb_properties = fb_props;

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}

_PUBLIC_ enum mapistore_error emsmdbp_object_message_open(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *parent_object, uint64_t folderID, uint64_t messageID, bool read_write, struct emsmdbp_object **messageP, struct mapistore_message **msgp)
{
	struct emsmdbp_object *folder_object, *message_object = NULL;
	uint32_t contextID;
	bool mapistore;
	TALLOC_CTX *local_mem_ctx;
	enum mapistore_error ret = MAPISTORE_SUCCESS;
	enum MAPISTATUS retval;

	if (!messageP) return MAPISTORE_ERROR;
	if (!parent_object) return MAPISTORE_ERROR;

	local_mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!local_mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	retval = emsmdbp_object_open_folder_by_fid(local_mem_ctx, emsmdbp_ctx, parent_object, folderID, &folder_object);
	if (retval != MAPI_E_SUCCESS)  {
		ret = mapi_error_to_mapistore(retval);
		goto end;
	}

	mapistore = emsmdbp_is_mapistore(folder_object);
	switch ((int)mapistore) {
	case false:
		/* system/special folder */
		message_object = emsmdbp_object_message_init(mem_ctx, emsmdbp_ctx, messageID, folder_object);
		ret = openchangedb_message_open(mem_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, messageID, folderID, &message_object->backend_object, (void **)msgp);
		if (ret != MAPISTORE_SUCCESS) {
			printf("Invalid openchangedb message\n");
			talloc_free(message_object);
			goto end;
		}

		retval = emsmdbp_object_message_fill_freebusy_properties(message_object);
		if (retval != MAPI_E_SUCCESS) {
			OC_DEBUG(5, "[emsmdbp_object]: Error filling freebusy properties on %"PRIu64"\n", messageID);
			ret = MAPISTORE_ERROR;
		}
		break;
	case true:
		/* mapistore implementation goes here */
		message_object = emsmdbp_object_message_init(mem_ctx, emsmdbp_ctx, messageID, folder_object);
		contextID = emsmdbp_get_contextID(folder_object);
		ret = mapistore_folder_open_message(emsmdbp_ctx->mstore_ctx, contextID, folder_object->backend_object, message_object, messageID, read_write, &message_object->backend_object);
		if (ret == MAPISTORE_SUCCESS && msgp) {
			if (mapistore_message_get_message_data(emsmdbp_ctx->mstore_ctx, contextID, message_object->backend_object, mem_ctx, msgp) != MAPISTORE_SUCCESS) {
				ret = MAPISTORE_ERROR;
			}
		}
		if (ret != MAPISTORE_SUCCESS) {
			talloc_free(message_object);
		}
	}

end:
	talloc_free(local_mem_ctx);

	if (ret == MAPISTORE_SUCCESS) {
		message_object->object.message->read_write = read_write;
		*messageP = message_object;
	}

	return ret;
}

_PUBLIC_ struct emsmdbp_object *emsmdbp_object_message_open_attachment_table(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *message_object)
{
	struct emsmdbp_object	*table_object = NULL;
	uint32_t		contextID;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!message_object || message_object->type != EMSMDBP_OBJECT_MESSAGE) return NULL;

	switch ((int)emsmdbp_is_mapistore(message_object)) {
	case false:
		/* system/special folder */
		OC_DEBUG(0, "not implemented yet - shouldn't occur\n");
		table_object = NULL;
		break;
	case true:
                contextID = emsmdbp_get_contextID(message_object);

		table_object = emsmdbp_object_table_init(mem_ctx, emsmdbp_ctx, message_object);
		if (table_object) {
			table_object->object.table->ulType = MAPISTORE_ATTACHMENT_TABLE;
			mapistore_message_get_attachment_table(emsmdbp_ctx->mstore_ctx, contextID,
							       message_object->backend_object,
							       table_object, &table_object->backend_object,
							       &table_object->object.table->denominator);
		}
        }

	return table_object;
}

/**
   \details Initialize a stream object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param property the stream property identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_stream_init(TALLOC_CTX *mem_ctx,
							   struct emsmdbp_context *emsmdbp_ctx,
							   struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.stream = talloc_zero(object, struct emsmdbp_object_stream);
	if (!object->object.stream) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_STREAM;
	object->object.stream->property = 0;
	object->object.stream->needs_commit = false;
	object->object.stream->stream.buffer.data = NULL;
	object->object.stream->stream.buffer.length = 0;
	object->object.stream->stream.position = 0;

	return object;
}


/**
   \details Initialize a attachment object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent 
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_attachment_init(TALLOC_CTX *mem_ctx,
                                                               struct emsmdbp_context *emsmdbp_ctx,
                                                               uint64_t messageID,
                                                               struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
        if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.attachment = talloc_zero(object, struct emsmdbp_object_attachment);
	if (!object->object.attachment) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_ATTACHMENT;
	object->object.attachment->attachmentID = -1;

	return object;
}


_PUBLIC_ struct emsmdbp_object *emsmdbp_object_subscription_init(TALLOC_CTX *mem_ctx,
								 struct emsmdbp_context *emsmdbp_ctx,
								 struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.subscription = talloc_zero(object, struct emsmdbp_object_subscription);
	if (!object->object.subscription) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_SUBSCRIPTION;
	object->object.subscription->handle = 0;

	return object;
}

_PUBLIC_ int emsmdbp_object_get_available_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray **propertiesp)
{
	uint32_t contextID;

	if (!(object->type == EMSMDBP_OBJECT_FOLDER
	      || object->type == EMSMDBP_OBJECT_MAILBOX
	      || object->type == EMSMDBP_OBJECT_MESSAGE
	      || object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		OC_DEBUG(0, "object must be EMSMDBP_OBJECT_FOLDER, EMSMDBP_OBJECT_MAILBOX, EMSMDBP_OBJECT_MESSAGE or EMSMDBP_OBJECT_ATTACHMENT (type =  %d)\n", object->type);
		return MAPISTORE_ERROR;
	}
	
	if (!emsmdbp_is_mapistore(object)) {
		OC_DEBUG(5, "only mapistore is supported at this time\n");
		return MAPISTORE_ERROR;
	}

	contextID = emsmdbp_get_contextID(object);

	return mapistore_properties_get_available_properties(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, mem_ctx, propertiesp);
}

static int emsmdbp_object_get_properties_systemspecialfolder(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	struct emsmdbp_object_folder	*folder;
	char				*owner;
	int				i;
        uint32_t                        *obj_count;
	uint8_t				*has_subobj;
	struct Binary_r			*binr;
	time_t				unix_time;
	NTTIME				nt_time;
	struct FILETIME			*ft;

	folder = (struct emsmdbp_object_folder *) object->object.folder;
        for (i = 0; i < properties->cValues; i++) {
                if (properties->aulPropTag[i] == PR_FOLDER_CHILD_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
			retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.folder->folderID, obj_count);
			data_pointers[i] = obj_count;
                }
		else if (properties->aulPropTag[i] == PR_SUBFOLDERS) {
			obj_count = talloc_zero(NULL, uint32_t);
			retval = openchangedb_get_folder_count(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.folder->folderID, obj_count);
			has_subobj = talloc_zero(data_pointers, uint8_t);
			*has_subobj = (*obj_count > 0) ? 1 : 0;
			data_pointers[i] = has_subobj;
			talloc_free(obj_count);
		}
		else if (properties->aulPropTag[i] == PR_SOURCE_KEY) {
			owner = emsmdbp_get_owner(object);
			emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, owner, object->object.folder->folderID, &binr);
			data_pointers[i] = binr;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PR_CONTENT_COUNT
			 || properties->aulPropTag[i] == PidTagAssociatedContentCount
			 || properties->aulPropTag[i] == PR_CONTENT_UNREAD
			 || properties->aulPropTag[i] == PR_DELETED_COUNT_TOTAL) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
			*obj_count = 0;
			data_pointers[i] = obj_count;
			retval = MAPI_E_SUCCESS;
                }
		else if (properties->aulPropTag[i] == PidTagLocalCommitTimeMax) {
			/* TODO: temporary hack */
			unix_time = time(NULL);
			unix_to_nt_time(&nt_time, unix_time);
			ft = talloc_zero(data_pointers, struct FILETIME);
			ft->dwLowDateTime = (nt_time & 0xffffffff);
			ft->dwHighDateTime = nt_time >> 32;
			data_pointers[i] = ft;
			retval = MAPI_E_SUCCESS;
		}
                else {
			retval = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, properties->aulPropTag[i], folder->folderID, data_pointers + i);
                }
		retvals[i] = retval;
        }

	return MAPISTORE_SUCCESS;
}

static int emsmdbp_object_get_properties_message(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx,
						 struct emsmdbp_object *object, struct SPropTagArray *properties,
						 void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS				retval;
	int					i;
	char					*owner, *email_address, *administrativegroup;
	struct Binary_r				*binr;
	struct mapistore_freebusy_properties	*fb_props;
	struct LongArray_r			*long_array;
	struct BinaryArray_r			*bin_array;

	fb_props = object->object.message->fb_properties;

	owner = emsmdbp_get_owner(object);
	retval = mapiserver_get_administrative_group_legacyexchangedn(mem_ctx, emsmdbp_ctx, &administrativegroup);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, NULL);

	/* FIXME: this is wrong, as the CN attribute may differ from the user's username (sAMAccountName) */
	email_address = talloc_asprintf(data_pointers, "%s/cn=Recipients/cn=%s",
                                    administrativegroup, owner);

	/* Look over properties */
	for (i = 0; i < properties->cValues; i++) {
		if (properties->aulPropTag[i] == PR_SOURCE_KEY) {
			emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, owner, object->object.message->messageID,
						     &binr);
			data_pointers[i] = binr;
			retval = MAPI_E_SUCCESS;
		} else {
			if (fb_props) {
				switch (properties->aulPropTag[i]) {
				case PR_SCHDINFO_MONTHS_TENTATIVE:
				case PR_SCHDINFO_MONTHS_BUSY:
				case PR_SCHDINFO_MONTHS_OOF:
				case PR_SCHDINFO_MONTHS_MERGED:
					long_array = talloc_zero(data_pointers, struct LongArray_r);
					long_array->cValues = fb_props->nbr_months;
					long_array->lpl = fb_props->months_ranges;
					data_pointers[i] = long_array;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_SCHDINFO_FREEBUSY_TENTATIVE:
					bin_array = talloc_zero(data_pointers, struct BinaryArray_r);
					bin_array->cValues = fb_props->nbr_months;
					bin_array->lpbin = fb_props->freebusy_tentative;
					data_pointers[i] = bin_array;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_SCHDINFO_FREEBUSY_BUSY:
					bin_array = talloc_zero(data_pointers, struct BinaryArray_r);
					bin_array->cValues = fb_props->nbr_months;
					bin_array->lpbin = fb_props->freebusy_busy;
					data_pointers[i] = bin_array;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_SCHDINFO_FREEBUSY_OOF:
					bin_array = talloc_zero(data_pointers, struct BinaryArray_r);
					bin_array->cValues = fb_props->nbr_months;
					bin_array->lpbin = fb_props->freebusy_away;
					data_pointers[i] = bin_array;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_SCHDINFO_FREEBUSY_MERGED:
					bin_array = talloc_zero(data_pointers, struct BinaryArray_r);
					bin_array->cValues = fb_props->nbr_months;
					bin_array->lpbin = fb_props->freebusy_merged;
					data_pointers[i] = bin_array;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_FREEBUSY_PUBLISH_START:
					data_pointers[i] = &fb_props->publish_start;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_FREEBUSY_PUBLISH_END:
					data_pointers[i] = &fb_props->publish_end;
					retval = MAPI_E_SUCCESS;
					break;
				case PidTagFreeBusyMessageEmailAddress:
					data_pointers[i] = email_address;
					retval = MAPI_E_SUCCESS;
					break;
				case PR_FREEBUSY_RANGE_TIMESTAMP:
					data_pointers[i] = &fb_props->timestamp;
					retval = MAPI_E_SUCCESS;
					break;
				default:
					retval = openchangedb_message_get_property(data_pointers, emsmdbp_ctx->oc_ctx, object->backend_object, properties->aulPropTag[i], data_pointers + i);
				}
			}
			else {
				retval = openchangedb_message_get_property(data_pointers, emsmdbp_ctx->oc_ctx, object->backend_object, properties->aulPropTag[i], data_pointers + i);
			}
		}
		retvals[i] = retval;
	}

	return MAPI_E_SUCCESS;
}

static int emsmdbp_object_get_properties_mapistore_root(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	enum mapistore_error		mretval;
	struct emsmdbp_object_folder	*folder;
	char				*owner;
	struct Binary_r			*binr;
	int				i;
	uint32_t			contextID;
        uint32_t                        *obj_count;
	uint8_t				*has_subobj;
	uint32_t			*folder_flags;
	uint64_t			*fid;

	contextID = emsmdbp_get_contextID(object);

	folder = (struct emsmdbp_object_folder *) object->object.folder;
        for (i = 0; i < properties->cValues; i++) {
		if (properties->aulPropTag[i] == PidTagFolderFlags) {
			folder_flags = talloc_zero(data_pointers, uint32_t);
			*folder_flags = FolderFlags_IPM|FolderFlags_Normal;
			data_pointers[i] = folder_flags;
		} else if (properties->aulPropTag[i] == PidTagParentFolderId) {
			switch (object->parent_object->type) {
			case EMSMDBP_OBJECT_MAILBOX:
				fid = talloc_zero(data_pointers, uint64_t);
				OPENCHANGE_RETVAL_IF(!fid, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
				*fid = object->parent_object->object.mailbox->folderID;
				retval = MAPI_E_SUCCESS;
				data_pointers[i] = fid;
				break;
			case EMSMDBP_OBJECT_FOLDER:
				fid = talloc_zero(data_pointers, uint64_t);
				OPENCHANGE_RETVAL_IF(!fid, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
				*fid = object->parent_object->object.folder->folderID;
				data_pointers[i] = fid;
				retval = MAPI_E_SUCCESS;
				break;
			default:
				retval = MAPI_E_NOT_FOUND;
				break;
			}
		}
		else if (properties->aulPropTag[i] == PR_CONTENT_COUNT) {
                        /* a hack to avoid fetching dynamic fields from openchange.ldb */
                        obj_count = talloc_zero(data_pointers, uint32_t);
			OPENCHANGE_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
                        mretval = mapistore_folder_get_child_count(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, MAPISTORE_MESSAGE_TABLE, obj_count);
			if (mretval == MAPISTORE_SUCCESS) {
				data_pointers[i] = obj_count;
			}
                }
                else if (properties->aulPropTag[i] == PidTagAssociatedContentCount) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
			OPENCHANGE_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
                        mretval = mapistore_folder_get_child_count(emsmdbp_ctx->mstore_ctx, emsmdbp_get_contextID(object), object->backend_object, MAPISTORE_FAI_TABLE, obj_count);
			if (mretval == MAPISTORE_SUCCESS) {
				data_pointers[i] = obj_count;
			}
                }
                else if (properties->aulPropTag[i] == PR_FOLDER_CHILD_COUNT) {
                        obj_count = talloc_zero(data_pointers, uint32_t);
			OPENCHANGE_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
                        retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, object, obj_count);
			if (retval == MAPI_E_SUCCESS) {
				data_pointers[i] = obj_count;
			}
                }
		else if (properties->aulPropTag[i] == PR_SUBFOLDERS) {
			obj_count = talloc_zero(data_pointers, uint32_t);
			OPENCHANGE_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			retval = emsmdbp_folder_get_folder_count(emsmdbp_ctx, object, obj_count);
			if (retval == MAPI_E_SUCCESS) {
				has_subobj = talloc_zero(data_pointers, uint8_t);
				OPENCHANGE_RETVAL_IF(!has_subobj, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
				*has_subobj = (*obj_count > 0) ? 1 : 0;
				data_pointers[i] = has_subobj;
			}
			talloc_free(obj_count);
		}
		else if (properties->aulPropTag[i] == PR_SOURCE_KEY) {
			owner = emsmdbp_get_owner(object);
			emsmdbp_source_key_from_fmid(data_pointers, emsmdbp_ctx, owner, object->object.folder->folderID, &binr);
			data_pointers[i] = binr;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PR_FOLDER_TYPE) {
			obj_count = talloc_zero(data_pointers, uint32_t);
			OPENCHANGE_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			*obj_count = FOLDER_GENERIC;
			data_pointers[i] = obj_count;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PR_CONTENT_UNREAD || properties->aulPropTag[i] == PR_DELETED_COUNT_TOTAL) {
			/* TODO: temporary hack */
			obj_count = talloc_zero(data_pointers, uint32_t);
			OPENCHANGE_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			*obj_count = 0;
			data_pointers[i] = obj_count;
			retval = MAPI_E_SUCCESS;
		}
		else if (properties->aulPropTag[i] == PidTagLocalCommitTimeMax || properties->aulPropTag[i] == PR_RIGHTS || properties->aulPropTag[i] == PR_ACCESS || properties->aulPropTag[i] == PR_ACCESS_LEVEL || properties->aulPropTag[i] == PidTagRights || properties->aulPropTag[i] == PidTagAccessControlListData || properties->aulPropTag[i] == PidTagExtendedACLData) {
			struct mapistore_property_data prop_data;

			mapistore_properties_get_properties(emsmdbp_ctx->mstore_ctx, contextID,
							    object->backend_object,
							    data_pointers,
							    1,
							    properties->aulPropTag + i,
							    &prop_data);
			data_pointers[i] = prop_data.data;
			if (prop_data.error) {
				retval = mapistore_error_to_mapi(prop_data.error);
			}
			else {
				if (prop_data.data == NULL) {
					retval = MAPI_E_NOT_FOUND;
				}
				else {
					retval = MAPI_E_SUCCESS;
				}
			}
		}
                else {
                        /* We are not using emsmdbp_ctx->username because we want to impersonate to get the properties
                           on shared folders */
			owner = emsmdbp_get_owner(object);
			if (!owner) {
				/* Public folder? Then, use logged user */
				owner = emsmdbp_ctx->username;
			}
			if (properties->aulPropTag[i] == PidTagHierRev) {
				properties->aulPropTag[i] = PidTagCreationTime;
			}
			retval = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx, owner, properties->aulPropTag[i], folder->folderID, data_pointers + i);
                }
		retvals[i] = retval;
        }

	return MAPISTORE_SUCCESS;
}

static int emsmdbp_object_get_properties_mailbox(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t			i;
	struct SBinary_short		*bin;

	for (i = 0; i < properties->cValues; i++) {
		switch (properties->aulPropTag[i]) {
		case PR_MAPPING_SIGNATURE:
		case PidTagIpmPublicFoldersEntryId:
			retvals[i] = MAPI_E_NO_ACCESS;
			break;
		case PR_USER_ENTRYID:
			bin = talloc_zero(data_pointers, struct SBinary_short);
			retvals[i] = entryid_set_AB_EntryID(data_pointers, object->object.mailbox->szUserDN, bin);
			data_pointers[i] = bin;
			break;
		case PR_MAILBOX_OWNER_ENTRYID:
			if (object->object.mailbox->mailboxstore == false) {
				retvals[i] = MAPI_E_NO_ACCESS;
			} else {
				bin = talloc_zero(data_pointers, struct SBinary_short);
				retvals[i] = entryid_set_AB_EntryID(data_pointers, object->object.mailbox->owner_EssDN,
								    bin);
				data_pointers[i] = bin;
			}
			break;
		case PidTagMailboxOwnerName:
			if (object->object.mailbox->mailboxstore == false) {
				retvals[i] = MAPI_E_NO_ACCESS;
			} else {
				retvals[i] = MAPI_E_SUCCESS;
				data_pointers[i] = talloc_strdup(data_pointers, object->object.mailbox->owner_Name);
			}
			break;
		default:
			retvals[i] = openchangedb_get_folder_property(data_pointers, emsmdbp_ctx->oc_ctx, object->object.mailbox->owner_username, properties->aulPropTag[i], object->object.mailbox->folderID, data_pointers + i);
		}
	}

	return MAPISTORE_SUCCESS;
}

static int emsmdbp_object_get_properties_mapistore(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t		contextID = -1;
	struct mapistore_property_data  *prop_data;
	int			i, ret;

	contextID = emsmdbp_get_contextID(object);
	prop_data = talloc_array(NULL, struct mapistore_property_data, properties->cValues);
	memset(prop_data, 0, sizeof(struct mapistore_property_data) * properties->cValues);

	ret = mapistore_properties_get_properties(emsmdbp_ctx->mstore_ctx, contextID,
						  object->backend_object,
						  prop_data,
						  properties->cValues,
						  properties->aulPropTag,
						  prop_data);
	if (ret == MAPISTORE_SUCCESS) {
		for (i = 0; i < properties->cValues; i++) {
			if (prop_data[i].error) {
				retvals[i] = mapistore_error_to_mapi(prop_data[i].error);
			}
			else {
				if (prop_data[i].data == NULL) {
					retvals[i] = MAPI_E_NOT_FOUND;
				}
				else {
					data_pointers[i] = prop_data[i].data;
					(void) talloc_reference(data_pointers, prop_data[i].data);
				}
			}
		}
	}
	talloc_free(prop_data);

	return ret;
}

_PUBLIC_ void **emsmdbp_object_get_properties(TALLOC_CTX *mem_ctx, struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SPropTagArray *properties, enum MAPISTATUS **retvalsp)
{
        void		**data_pointers;
        enum MAPISTATUS	*retvals;
	bool		mapistore;
	int		retval = MAPISTORE_SUCCESS;

        data_pointers = talloc_array(mem_ctx, void *, properties->cValues);
        memset(data_pointers, 0, sizeof(void *) * properties->cValues);

        retvals = talloc_array(mem_ctx, enum MAPISTATUS, properties->cValues);
        memset(retvals, 0, sizeof(enum MAPISTATUS) * properties->cValues);

	/* Temporary hack: If this is a mapistore root container
	 * (e.g. Inbox, Calendar etc.), directly stored under
	 * IPM.Subtree, then fetch properties from openchange
	 * dispatcher db, not mapistore */
	if (object && object->type == EMSMDBP_OBJECT_FOLDER &&
	    object->object.folder->mapistore_root == true) {
		if (object->object.folder->postponed_props) {
			retval = emsmdbp_object_folder_commit_creation(emsmdbp_ctx, object, true);
			if (retval != MAPISTORE_SUCCESS) {
				OC_DEBUG(0, "folder_commit_creation() failed with 0x%.8X", retval);
				goto end;
			}
		}

		retval = emsmdbp_object_get_properties_mapistore_root(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
	} else {
		mapistore = emsmdbp_is_mapistore(object);
		/* Nasty hack */
		if (!object) {
			OC_DEBUG(5, "what's that hack!??\n");
			mapistore = true;
		}

		switch ((int)mapistore) {
		case false:
			switch (object->type) {
			case EMSMDBP_OBJECT_MAILBOX:
				retval = emsmdbp_object_get_properties_mailbox(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			case EMSMDBP_OBJECT_FOLDER:
				retval = emsmdbp_object_get_properties_systemspecialfolder(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			case EMSMDBP_OBJECT_MESSAGE:
				retval = emsmdbp_object_get_properties_message(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
				break;
			default:
				retval = MAPISTORE_ERROR;
				break;
			}
			break;
		case true:
			/* folder or messages handled by mapistore */
			retval = emsmdbp_object_get_properties_mapistore(mem_ctx, emsmdbp_ctx, object, properties, data_pointers, retvals);
			break;
		}
	}

end:
	if (retval != MAPISTORE_SUCCESS) {
		talloc_free(data_pointers);
		data_pointers = NULL;
		talloc_free(retvals);
		retvals = NULL;
	}

	if (retvalsp) {
		*retvalsp = retvals;
	}

        return data_pointers;
}

/* TODO: handling of "property problems" */
_PUBLIC_ int emsmdbp_object_set_properties(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *object, struct SRow *rowp)
{
	TALLOC_CTX		*mem_ctx;
	uint32_t		contextID, new_cvalues;
	char			*mapistore_uri, *new_uri;
	size_t			mapistore_uri_len, new_uri_len;
	bool			mapistore;
	enum mapistore_error	ret;
	struct SRow		*postponed_props;
	bool			soft_deleted;

	/* Sanity checks */
	if (!emsmdbp_ctx) return MAPI_E_CALL_FAILED;
	if (!object) return MAPI_E_CALL_FAILED;
	if (!rowp) return MAPI_E_CALL_FAILED;
	if (!(object->type == EMSMDBP_OBJECT_FOLDER
	      || object->type == EMSMDBP_OBJECT_MAILBOX
	      || object->type == EMSMDBP_OBJECT_MESSAGE
	      || object->type == EMSMDBP_OBJECT_ATTACHMENT)) {
		OC_DEBUG(0, "object must be EMSMDBP_OBJECT_FOLDER, EMSMDBP_OBJECT_MAILBOX, EMSMDBP_OBJECT_MESSAGE or EMSMDBP_OBJECT_ATTACHMENT (type = %d)\n", object->type);
		return MAPI_E_NO_SUPPORT;
	}

	if (object->type == EMSMDBP_OBJECT_FOLDER) {
		postponed_props = object->object.folder->postponed_props;
		if (postponed_props) {
			new_cvalues = postponed_props->cValues + rowp->cValues;
			postponed_props->lpProps = talloc_realloc(postponed_props, postponed_props->lpProps, struct SPropValue, new_cvalues);
			mapi_copy_spropvalues(postponed_props, rowp->lpProps, postponed_props->lpProps + postponed_props->cValues, rowp->cValues);
			postponed_props->cValues = new_cvalues;

			ret = emsmdbp_object_folder_commit_creation(emsmdbp_ctx, object, false);
			if (ret == MAPISTORE_SUCCESS) {
				return MAPI_E_SUCCESS;
			}
			else {
				return MAPI_E_NOT_FOUND;
			}
		}
	}

	/* Temporary hack: If this is a mapistore root container
	 * (e.g. Inbox, Calendar etc.), directly stored under
	 * IPM.Subtree, then set properties from openchange
	 * dispatcher db, not mapistore */
	if (object->type == EMSMDBP_OBJECT_FOLDER
	    && object->object.folder->mapistore_root == true) {
		mem_ctx = talloc_new(NULL);
		OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

		mapistore_uri = NULL;
		openchangedb_get_mapistoreURI(mem_ctx, emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.folder->folderID, &mapistore_uri, true);
		openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.folder->folderID, rowp);
		contextID = emsmdbp_get_contextID(object);
		mapistore_properties_set_properties(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, rowp);
		/* if the display name of a resource has changed, some backends may have modified the folder uri and we need to update the openchangedb record accordingly */
		if (mapistore_uri) {
			new_uri = NULL;
			mapistore_indexing_record_get_uri(emsmdbp_ctx->mstore_ctx, emsmdbp_ctx->username, mem_ctx, object->object.folder->folderID, &new_uri, &soft_deleted);
			if (new_uri) {
				mapistore_uri_len = strlen(mapistore_uri);
				new_uri_len = strlen(new_uri);
				/* handling of the final '/' */
				if (mapistore_uri[mapistore_uri_len-1] == '/') {
					if (new_uri[new_uri_len-1] != '/') {
						new_uri = talloc_asprintf(mem_ctx, "%s/", new_uri);
						new_uri_len++;
					}
				}
				else {
					if (new_uri[new_uri_len-1] == '/') {
						new_uri[new_uri_len-1] = 0;
						new_uri_len--;
					}
				}
				if (strcmp(mapistore_uri, new_uri) != 0) {
					openchangedb_set_mapistoreURI(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.folder->folderID, new_uri);
				}
			}
		}
		talloc_free(mem_ctx);
	}
	else {
		contextID = emsmdbp_get_contextID(object);
		mapistore = emsmdbp_is_mapistore(object);
		switch ((int)mapistore) {
		case false:
			if (object->type == EMSMDBP_OBJECT_FOLDER) {
				openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.folder->folderID, rowp);
			}
			else if (object->type == EMSMDBP_OBJECT_MAILBOX) {
				openchangedb_set_folder_properties(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, object->object.mailbox->folderID, rowp);
			}
			else if (object->type == EMSMDBP_OBJECT_MESSAGE) {
				openchangedb_message_set_properties((TALLOC_CTX *)object->object.message,
								    emsmdbp_ctx->oc_ctx,
								    object->backend_object, rowp);
			}
			else {
				OC_DEBUG(0, "Setting properties on openchangedb not implemented yet for non-folder object type\n");
				return MAPI_E_NO_SUPPORT;
			}
			break;
		case true:
			mapistore_properties_set_properties(emsmdbp_ctx->mstore_ctx, contextID, object->backend_object, rowp);
			break;
		}
	}

	return MAPI_E_SUCCESS;
}

_PUBLIC_ void emsmdbp_fill_row_blob(TALLOC_CTX *mem_ctx,
				    struct emsmdbp_context *emsmdbp_ctx,
				    uint8_t *layout,
				    DATA_BLOB *property_row,
				    struct SPropTagArray *properties,
				    void **data_pointers,
				    enum MAPISTATUS *retvals,
				    bool *untyped_status)
{
        uint16_t i;
        uint8_t flagged;
        enum MAPITAGS property;
        void *data;
        uint32_t retval;

        flagged = 0;
        for (i = 0; !flagged && i < properties->cValues; i++) {
                if (retvals[i] != MAPI_E_SUCCESS || untyped_status[i] || !data_pointers[i]) {
                        flagged = 1;
                }
        }
	*layout = flagged;

        for (i = 0; i < properties->cValues; i++) {
                retval = retvals[i];
                if (retval != MAPI_E_SUCCESS) {
                        property = (properties->aulPropTag[i] & 0xFFFF0000) + PT_ERROR;
                        data = &retval;
                }
                else {
                        property = properties->aulPropTag[i];
                        data = data_pointers[i];
                }
                libmapiserver_push_property(mem_ctx,
                                            property, data, property_row,
                                            flagged ? PT_ERROR : 0, flagged, untyped_status[i]);
        }
}

_PUBLIC_ struct emsmdbp_stream_data *emsmdbp_stream_data_from_value(TALLOC_CTX *mem_ctx, enum MAPITAGS prop_tag, void *value, bool read_write)
{
	uint16_t			prop_type;
	struct emsmdbp_stream_data	*stream_data;
	size_t				converted_size;

	stream_data = talloc_zero(mem_ctx, struct emsmdbp_stream_data);
        stream_data->prop_tag = prop_tag;
	prop_type = prop_tag & 0xffff;
	if (prop_type == PT_STRING8) {
		stream_data->data.length = strlen(value) + 1;
		stream_data->data.data = value;
                (void) talloc_reference(stream_data, stream_data->data.data);
	}
	else if (prop_type == PT_UNICODE) {
		stream_data->data.length = strlen_m_ext((char *) value, CH_UTF8, CH_UTF16LE) * 2;
		stream_data->data.data = talloc_array(stream_data, uint8_t, stream_data->data.length + 2);
		convert_string(CH_UTF8, CH_UTF16LE,
			       value, strlen(value),
			       stream_data->data.data, stream_data->data.length,
			       &converted_size);
		memset(stream_data->data.data + stream_data->data.length, 0, 2 * sizeof(uint8_t));
	}
	else if (prop_type == PT_BINARY) {
		stream_data->data.length = ((struct Binary_r *) value)->cb;
		stream_data->data.data = talloc_memdup(stream_data, ((struct Binary_r *) value)->lpb, stream_data->data.length);
                (void) talloc_reference(stream_data, value);
	}
	else {
		talloc_free(stream_data);
		return NULL;
	}

	return stream_data;
}

_PUBLIC_ DATA_BLOB emsmdbp_stream_read_buffer(struct emsmdbp_stream *stream, uint32_t length)
{
	DATA_BLOB buffer;
	uint32_t real_length;

	real_length = length;
	if (real_length + stream->position > stream->buffer.length) {
		real_length = stream->buffer.length - stream->position;
	}
	buffer.length = real_length;
	buffer.data = stream->buffer.data + stream->position;
	stream->position += real_length;

	return buffer;
}

_PUBLIC_ void emsmdbp_stream_write_buffer(TALLOC_CTX *mem_ctx, struct emsmdbp_stream *stream, DATA_BLOB new_buffer)
{
	size_t new_position;
	uint32_t old_length;
	uint8_t *old_data;

	new_position = stream->position + new_buffer.length;
	if (new_position >= stream->buffer.length) {
		old_length = stream->buffer.length;
		stream->buffer.length = new_position;
		if (stream->buffer.data) {
			old_data = stream->buffer.data;
			stream->buffer.data = talloc_realloc(mem_ctx, stream->buffer.data, uint8_t, stream->buffer.length);
			if (!stream->buffer.data) {
				OC_DEBUG(5, "WARNING: [bug] lost buffer pointer (data = NULL)\n");
				stream->buffer.data = talloc_array(mem_ctx, uint8_t, stream->buffer.length);
				memcpy(stream->buffer.data, old_data, old_length);
			}
		}
		else {
			stream->buffer.data = talloc_array(mem_ctx, uint8_t, stream->buffer.length);
		}
	}

	memcpy(stream->buffer.data + stream->position, new_buffer.data, new_buffer.length);
	stream->position = new_position;
}

_PUBLIC_ struct emsmdbp_stream_data *emsmdbp_object_get_stream_data(struct emsmdbp_object *object, enum MAPITAGS prop_tag)
{
        struct emsmdbp_stream_data *current_data;

	for (current_data = object->stream_data; current_data; current_data = current_data->next) {
		if (current_data->prop_tag == prop_tag) {
			OC_DEBUG(5, "found data for tag %.8x\n", prop_tag);
			return current_data;
		}
	}

	return NULL;
}

/**
   \details Initialize a synccontext object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param whole_store whether the subscription applies to the specified change on the entire store or stricly on the specified folder/message
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_synccontext_init(TALLOC_CTX *mem_ctx,
								struct emsmdbp_context *emsmdbp_ctx,
								struct emsmdbp_object *parent_object)
{
	struct emsmdbp_object	*synccontext_object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent_object) return NULL;
	if (!(parent_object->type == EMSMDBP_OBJECT_FOLDER || parent_object->type == EMSMDBP_OBJECT_MAILBOX)) {
		OC_DEBUG(0, "parent_object must be EMSMDBP_OBJECT_FOLDER or EMSMDBP_OBJECT_MAILBOX (type = %d)\n", parent_object->type);
		return NULL;
	}

	synccontext_object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent_object);
	if (!synccontext_object) return NULL;

	synccontext_object->object.synccontext = talloc_zero(synccontext_object, struct emsmdbp_object_synccontext);
	if (!synccontext_object->object.synccontext) {
		talloc_free(synccontext_object);
		return NULL;
	}

	synccontext_object->type = EMSMDBP_OBJECT_SYNCCONTEXT;

	(void) talloc_reference(synccontext_object->object.synccontext, parent_object);
        synccontext_object->object.synccontext->state_property = 0;
        synccontext_object->object.synccontext->state_stream.buffer.length = 0;
        synccontext_object->object.synccontext->state_stream.buffer.data = talloc_zero(synccontext_object->object.synccontext, uint8_t);
        synccontext_object->object.synccontext->stream.buffer.length = 0;
        synccontext_object->object.synccontext->stream.buffer.data = NULL;

	synccontext_object->object.synccontext->cnset_seen = talloc_zero(emsmdbp_ctx, struct idset);
	openchangedb_get_MailboxReplica(emsmdbp_ctx->oc_ctx, emsmdbp_ctx->username, NULL, &synccontext_object->object.synccontext->cnset_seen->repl.guid);
	synccontext_object->object.synccontext->cnset_seen->ranges = talloc_zero(synccontext_object->object.synccontext->cnset_seen, struct globset_range);
	synccontext_object->object.synccontext->cnset_seen->range_count = 1;
	synccontext_object->object.synccontext->cnset_seen->ranges->next = NULL;
	synccontext_object->object.synccontext->cnset_seen->ranges->prev = synccontext_object->object.synccontext->cnset_seen->ranges;
	synccontext_object->object.synccontext->cnset_seen->ranges->low = 0xffffffffffffffffLL;
	synccontext_object->object.synccontext->cnset_seen->ranges->high = 0x0;

        /* synccontext_object->object.synccontext->property_tags.cValues = 0; */
        /* synccontext_object->object.synccontext->property_tags.aulPropTag = NULL; */

	return synccontext_object;
}

/**
   \details Initialize a ftcontext object

   \param mem_ctx pointer to the memory context
   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param whole_store whether the subscription applies to the specified change on the entire store or stricly on the specified folder/message
   \param folderID the folder identifier
   \param messageID the message identifier
   \param parent emsmdbp object of the parent
 */
_PUBLIC_ struct emsmdbp_object *emsmdbp_object_ftcontext_init(TALLOC_CTX *mem_ctx,
							      struct emsmdbp_context *emsmdbp_ctx,
							      struct emsmdbp_object *parent)
{
	struct emsmdbp_object	*object;

	/* Sanity checks */
	if (!emsmdbp_ctx) return NULL;
	if (!parent) return NULL;

	object = emsmdbp_object_init(mem_ctx, emsmdbp_ctx, parent);
	if (!object) return NULL;

	object->object.ftcontext = talloc_zero(object, struct emsmdbp_object_ftcontext);
	if (!object->object.ftcontext) {
		talloc_free(object);
		return NULL;
	}

	object->type = EMSMDBP_OBJECT_FTCONTEXT;

	return object;
}

/**
   \details Convenience function to extract values from data_pointers safely.

   \param ulPropTag MAPI property tag
   \param SPropTagArray array of property tags
   \param data_pointers array with the property values that matches the previous SPropTagArray
   \param retvals array of returned value result of getting the property

   \return pointer to the property value, NULL otherwise
*/
static void *get_emsmdbp_data(enum MAPITAGS ul_prop_tag, struct SPropTagArray *s_prop_tag_array, void **data_pointers, enum MAPISTATUS *retvals)
{
        uint32_t        i;

        for (i = 0; i < s_prop_tag_array->cValues; i++) {
                if (ul_prop_tag == s_prop_tag_array->aulPropTag[i]) {
                        return (retvals[i] == MAPI_E_SUCCESS) ? data_pointers[i] : NULL;
                }
        }
        return NULL;
}

/**
   \details Store the target recipients attribute in out parameter.

   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param sharing_object the sharing message object to get the XML data from
   \param [out] pointer to string where the TargetRecipients XML attribute is stored
   \param mem_ctx pointer to memory context where the target_recipients_attr is created

   \note On success target_recipients_attr string is allocated in
   mem_ctx. It is up to the developer to free that memory context when
   it is not needed anymore.

   \return MAPI_E_SUCCESS on success, a mapi error code otherwise
 */
static enum MAPISTATUS emsmdbp_object_sharing_metadata_recipients_attr(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *sharing_object, char **target_recipients_attr, TALLOC_CTX *mem_ctx)
{
	char			       *xml = NULL;
	int			       i;
	enum mapistore_error	       ret;
	struct mapistore_message       *msg_data = NULL;
	TALLOC_CTX		       *local_mem_ctx;
	uint32_t		       contextID;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!sharing_object, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	local_mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!local_mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	contextID = emsmdbp_get_contextID(sharing_object);
	ret = mapistore_message_get_message_data(emsmdbp_ctx->mstore_ctx, contextID, sharing_object->backend_object, local_mem_ctx, &msg_data);
	OPENCHANGE_RETVAL_IF(ret != MAPISTORE_SUCCESS, mapistore_error_to_mapi(ret), local_mem_ctx);

	if (msg_data && msg_data->recipients_count > 0) {
		int	 email_address_idx;

		xml = talloc_asprintf(local_mem_ctx, "TargetRecipients=\"");
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
		email_address_idx = get_email_address_index_SPropTagArray(msg_data->columns);
		for (i = 0; i < msg_data->recipients_count; i++) {
			if (email_address_idx != -1) {
				xml = talloc_asprintf_append(xml, "%s", (char *)msg_data->recipients[i].data[email_address_idx]);
				OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
				if (i + 1 < msg_data->recipients_count) {
					/* ; separated */
					xml = talloc_asprintf_append(xml, ";");
					OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
				}
			}
		}
		xml = talloc_asprintf_append(xml, "\"");
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	} else {
		/* This shouldn't happen, but just in case */
		xml = "";
	}

	*target_recipients_attr = talloc_strdup(mem_ctx, xml);
	OPENCHANGE_RETVAL_IF(!*target_recipients_attr, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	talloc_free(local_mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Store the Providers node into providers_node out parameter.

   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param sharing_object the sharing message object to get the XML data from
   \param remote_uid string with the folder id to share, NULL when no required
   \param remote_store_uid string with the mailbox id where the folder belongs to, NULL when non-required
   \param target_recipients the string where the recipients' email addresses separated by semi colons are stored
   \param [out] providers_node pointer to string where the Providers XML node is stored
   \param mem_ctx pointer to memory context where the providers_node is created

   \note On success providers_node string is allocated in mem_ctx. It is up to the developer
   to free that memory context when it is not needed anymore.

   \return MAPI_E_SUCCESS on success, a mapi error code otherwise
 */
static enum MAPISTATUS emsmdbp_object_sharing_metadata_providers_node(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *sharing_object, char *remote_uid, char *remote_store_uid, char *target_recipients, char **providers_node, TALLOC_CTX *mem_ctx)
{
	char	       *xml;
	TALLOC_CTX     *local_mem_ctx;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!sharing_object, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!providers_node, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	local_mem_ctx = talloc_new(NULL);
	OPENCHANGE_RETVAL_IF(!local_mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	xml = talloc_asprintf(local_mem_ctx, "<Providers><Provider Type=\"ms-exchange-internal\" %s >", target_recipients);
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	if (remote_uid && remote_store_uid) {
		xml = talloc_asprintf_append(xml,
					     "<FolderId xmlns=\"http://schemas.microsoft.com/exchange/sharing/2008\">%s</FolderId>",
					     remote_uid);  /* PidLidSharingRemoteUid */
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

		xml = talloc_asprintf_append(xml,
					     "<MailboxId xmlns=\"http://schemas.microsoft.com/exchange/sharing/2008\">%s</MailboxId>",
					     remote_store_uid);	 /* PidLidSharingRemoteStoreUid */
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	}

	xml = talloc_asprintf_append(xml, "</Provider></Providers>");
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	*providers_node = talloc_strdup(mem_ctx, xml);
	OPENCHANGE_RETVAL_IF(!*providers_node, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	talloc_free(local_mem_ctx);
	return MAPI_E_SUCCESS;
}

/**
   \details Create the XML file from the sharing message object and store it as
            binary property in attach_bin out parameter.

   \param emsmdbp_ctx pointer to the emsmdb provider cotnext
   \param sharing_object the sharing message object to get the XML data from
   \param [out] attach_bin pointer to Binary_r property value pointer where the XML data is stored
   \param mem_ctx pointer to memory context where the attach_bin is created

   \note On success attach_bin is allocated in mem_ctx. It is up to the developer
   to free that memory context when it is not needed anymore.

   \return MAPI_E_SUCCESS on success, a mapi error code otherwise
 */
static enum MAPISTATUS emsmdbp_object_sharing_metadata_property(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *sharing_object, struct Binary_r **attach_bin, TALLOC_CTX *mem_ctx)
{
	struct Binary_r		       *bin;
	const char		       *data_type;
	char			       *providers_node;
	char			       *prop_value;
	char			       *recipients_attr;
	char			       *remote_uid;
	char			       *remote_store_uid;
	const char                     *sharing_type;
	char			       *xml;
	int			       i;
	enum MAPISTATUS		       *retvals = NULL, retval;
	const enum MAPITAGS	       sharing_prop_tags[] = {PidLidSharingLocalType, PidLidSharingInitiatorName, PidLidSharingInitiatorSmtp, PidLidSharingInitiatorEntryId, PidLidSharingFlavor, PidNameXSharingFlavor, PidLidSharingRemoteName, PidLidSharingRemoteUid, PidLidSharingRemoteStoreUid};
	struct SBinary_short	       *entryId;
	struct SPropTagArray	       *sharing_properties;
	struct SPropValue	       *attach_bin_property;
	TALLOC_CTX		       *local_mem_ctx;
	const uint32_t		       SHARING_PROPS_COUNT = sizeof(sharing_prop_tags) / sizeof(enum MAPITAGS);
	const uint32_t		       COMMON_SHARING_PROPS_COUNT = 3;
	uint32_t		       sharing_flavour, *sharing_flavour_ptr;
	void			       **data_pointers;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!sharing_object, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_INVALID_PARAMETER, NULL);

	attach_bin_property = talloc_zero(mem_ctx, struct SPropValue);
	OPENCHANGE_RETVAL_IF(!attach_bin_property, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	local_mem_ctx = talloc_named(NULL, 0, "sharing_metadata_property");
	OPENCHANGE_RETVAL_IF(!local_mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* We get the required properties from the mapistore */
	sharing_properties = talloc_zero(mem_ctx, struct SPropTagArray);
	OPENCHANGE_RETVAL_IF(!sharing_properties, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	sharing_properties->cValues = SHARING_PROPS_COUNT;
	sharing_properties->aulPropTag = talloc_array(local_mem_ctx, enum MAPITAGS, SHARING_PROPS_COUNT);
	memcpy(sharing_properties->aulPropTag, sharing_prop_tags, sizeof(sharing_prop_tags));

	data_pointers = emsmdbp_object_get_properties(local_mem_ctx, emsmdbp_ctx, sharing_object,
						      sharing_properties, &retvals);
	OPENCHANGE_RETVAL_IF(!data_pointers, MAPI_E_NOT_FOUND, local_mem_ctx);

	/* Check every retval from shared properties is SUCCESS to build up the XML */
	for (i = 0; i < COMMON_SHARING_PROPS_COUNT; i++) {
		OPENCHANGE_RETVAL_IF(retvals[i] != MAPI_E_SUCCESS, MAPI_E_NOT_FOUND, local_mem_ctx);
	}

	/* Check data_type on PidLidSharingLocalType */
	sharing_type = (const char *)get_emsmdbp_data(PidLidSharingLocalType, sharing_properties, data_pointers, retvals);
	if (strncmp(sharing_type, "IPF.Appointment", strlen("IPF.Appointment")) == 0) {
		data_type = "calendar";
	} else if (strncmp(sharing_type, "IPF.Contact", strlen("IPF.Contact")) == 0) {
		data_type = "contacts";
	} else if (strncmp(sharing_type, "IPF.Task", strlen("IPF.Task")) == 0) {
		data_type = "tasks";
	} else if (strncmp(sharing_type, "IPF.StickyNote", strlen("IPF.StickyNote")) == 0) {
		data_type = "notes";
	} else if (strncmp(sharing_type, "IPF.Journal", strlen("IPF.Journal")) == 0) {
		data_type = "journals";
	} else {
		OC_DEBUG(1, "Sharing type %s not supported", sharing_type);
		talloc_free(local_mem_ctx);
		return MAPI_E_NO_SUPPORT;
	}

	/* Check we have flavour to know the kind of sharing message */
	sharing_flavour_ptr = (uint32_t *)get_emsmdbp_data(PidLidSharingFlavor, sharing_properties, data_pointers, retvals);
	if (sharing_flavour_ptr) {
		sharing_flavour = *sharing_flavour_ptr;
	} else {
		prop_value = (char *)get_emsmdbp_data(PidNameXSharingFlavor, sharing_properties, data_pointers, retvals);
		if (prop_value) {
			char	  *error = NULL;

			sharing_flavour = strtoul(prop_value, &error, 16);
			if (error && *error != '\0') {
				OC_DEBUG(1, "Impossible to convert to integer: %s", prop_value);
				talloc_free(local_mem_ctx);
				return MAPI_E_INVALID_PARAMETER;
			}
		} else {
			OC_DEBUG(1, "Sharing flavour not available. Impossible to create XML metadata");
			talloc_free(local_mem_ctx);
			return MAPI_E_NO_SUPPORT;
		}
	}

	xml = talloc_asprintf(local_mem_ctx,
			      "<?xml version=\"1.0\"?> "
			      "<SharingMessage xmlns=\"http://schemas.microsoft.com/sharing/2008\">"
			      "<DataType>%s</DataType>"
			      "<Initiator>", data_type);
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	/*** Initiator ***/
	xml = talloc_asprintf_append(xml, "<Name>%s</Name>",
				     (char *)get_emsmdbp_data(PidLidSharingInitiatorName, sharing_properties, data_pointers, retvals));
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	xml = talloc_asprintf_append(xml, "<SmtpAddress>%s</SmtpAddress>",
				     (char *)get_emsmdbp_data(PidLidSharingInitiatorSmtp, sharing_properties, data_pointers, retvals));
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	/* Take the binary EntryId and transform to Hex encoded field */
	xml = talloc_asprintf_append(xml, "<EntryId>");
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	entryId = get_emsmdbp_data(PidLidSharingInitiatorEntryId, sharing_properties, data_pointers, retvals);
	for (i = 0; i < entryId->cb; i++) {
		xml = talloc_asprintf_append(xml, "%.2X", *(entryId->lpb + i));
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	}
	xml = talloc_asprintf_append(xml, "</EntryId>");
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	xml = talloc_asprintf_append(xml, "</Initiator>");
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	/* Get recipients */
	retval = emsmdbp_object_sharing_metadata_recipients_attr(emsmdbp_ctx, sharing_object, &recipients_attr, local_mem_ctx);
	OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

	remote_uid = (char *)get_emsmdbp_data(PidLidSharingRemoteUid, sharing_properties, data_pointers, retvals);
	remote_store_uid = (char *)get_emsmdbp_data(PidLidSharingRemoteStoreUid, sharing_properties, data_pointers, retvals);

	/* See [MS-OXSHARE] Section 2.2.2.5 */
	if (sharing_flavour == SHARING_ACCEPT_REQUEST) {
		/*** Accept Of Request */
		retval = emsmdbp_object_sharing_metadata_providers_node(emsmdbp_ctx, sharing_object,
									remote_uid, remote_store_uid,
									recipients_attr,
									&providers_node, local_mem_ctx);
		OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

		xml = talloc_asprintf_append(xml, "<AcceptOfRequest>%s</AcceptOfRequest>", providers_node);
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	} else if (sharing_flavour == SHARING_DENY_REQUEST) {
		/*** Deny of Request ***/
		retval = emsmdbp_object_sharing_metadata_providers_node(emsmdbp_ctx, sharing_object,
									NULL, NULL,
									recipients_attr,
									&providers_node, local_mem_ctx);
		OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

		xml = talloc_asprintf_append(xml, "<DenyOfRequest>%s</DenyOfRequest>", providers_node);
		OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	} else {
		if ((sharing_flavour & SHARING_REQUEST) == SHARING_REQUEST) {
			/*** Request ***/
			retval = emsmdbp_object_sharing_metadata_providers_node(emsmdbp_ctx, sharing_object,
										NULL, NULL,
										recipients_attr,
										&providers_node, local_mem_ctx);
			OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

			xml = talloc_asprintf_append(xml, "<Request>%s</Request>", providers_node);
			OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
		}
		if ((sharing_flavour & SHARING_INVITATION_FOLDER) == SHARING_INVITATION_FOLDER) {
			/*** Invitation ***/
			xml = talloc_asprintf_append(xml, "<Invitation>");
			OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

			/* PidLidSharingRemoteName */
			prop_value = (char *)get_emsmdbp_data(PidLidSharingRemoteName, sharing_properties, data_pointers, retvals);
			if (prop_value) {
				/* This seems to be included when the calendar is not a default one */
				xml = talloc_asprintf_append(xml, "<Title>%s</Title>", prop_value);
				OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
			}

			retval = emsmdbp_object_sharing_metadata_providers_node(emsmdbp_ctx, sharing_object,
										remote_uid, remote_store_uid,
										recipients_attr,
										&providers_node, local_mem_ctx);
			OPENCHANGE_RETVAL_IF(retval != MAPI_E_SUCCESS, retval, local_mem_ctx);

			xml = talloc_asprintf_append(xml, "%s</Invitation>", providers_node);
			OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
		}
		if ((sharing_flavour & SHARING_REQUEST) != SHARING_REQUEST
		    && (sharing_flavour & SHARING_INVITATION_FOLDER) != SHARING_INVITATION_FOLDER) {
			OC_DEBUG(1, "Unknown sharing flavour 0x%X\n", sharing_flavour);
			talloc_free(local_mem_ctx);
			return MAPI_E_NO_SUPPORT;
		}
	}

	xml = talloc_asprintf_append(xml, "</SharingMessage>");
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);

	bin = talloc_zero(mem_ctx, struct Binary_r);
	OPENCHANGE_RETVAL_IF(!xml, MAPI_E_NOT_ENOUGH_MEMORY, local_mem_ctx);
	bin->lpb = (uint8_t *) talloc_strdup(mem_ctx, xml);
	bin->cb = strlen(xml);

	*attach_bin = bin;

	talloc_free(local_mem_ctx);

	return MAPI_E_SUCCESS;
}

/**
   \details Create a sharing metadata XML file for folder sharing invitation, request and response
            and store it as attachment in the sharing object.
            See [MS-OXSHARE] for this kind of messages and [MS-OXSHRMSG] for the XML format.

   \param emsmdbp_ctx pointer to the emsmdb provider context
   \param sharing_object the original sharing message object to dump the XML data from

   \return MAPI_E_SUCCESS on success, a MAPI error code otherwise
 */
_PUBLIC_ enum MAPISTATUS emsmdbp_object_attach_sharing_metadata_XML_file(struct emsmdbp_context *emsmdbp_ctx, struct emsmdbp_object *sharing_object)
{
	struct Binary_r		*attach_bin;
	struct emsmdbp_object	*attach;
	enum MAPISTATUS		*retvals = NULL;
	enum MAPISTATUS		retval;
	enum mapistore_error	ret;
	struct SPropTagArray	*props;
	struct SRow		aRow;
	TALLOC_CTX		*mem_ctx;
	uint32_t		contextID, aid;
	void			**data_pointers;

	/* Sanity checks */
	OPENCHANGE_RETVAL_IF(!emsmdbp_ctx, MAPI_E_INVALID_PARAMETER, NULL);
	OPENCHANGE_RETVAL_IF(!sharing_object, MAPI_E_INVALID_PARAMETER, NULL);

	OPENCHANGE_RETVAL_IF(!emsmdbp_is_mapistore(sharing_object), MAPI_E_NO_SUPPORT, NULL);

	mem_ctx = talloc_named(NULL, 0, "attach_sharing_metadata");
	OPENCHANGE_RETVAL_IF(!mem_ctx, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

	/* 1. Check if the message is a IPM.Sharing message */
	props = talloc_zero(mem_ctx, struct SPropTagArray);
	OPENCHANGE_RETVAL_IF(!props, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	props->cValues = 1;
	props->aulPropTag = talloc_zero(props, enum MAPITAGS);
	OPENCHANGE_RETVAL_IF(!props->aulPropTag, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);
	props->aulPropTag[0] = PidTagMessageClass;

	data_pointers = emsmdbp_object_get_properties(mem_ctx, emsmdbp_ctx, sharing_object, props, &retvals);
	OPENCHANGE_RETVAL_IF(!data_pointers || retvals[0] != MAPI_E_SUCCESS, MAPI_E_NOT_FOUND, mem_ctx);

	if (strncmp((char *)data_pointers[0], "IPM.Sharing", strlen("IPM.Sharing")) != 0) {
		talloc_free(mem_ctx);
		/* No sharing object, then no problems */
		return MAPI_E_SUCCESS;
	}

	/* 2. Create the attachment */
	attach = emsmdbp_object_attachment_init(mem_ctx, emsmdbp_ctx, sharing_object->object.message->messageID, sharing_object);
	OPENCHANGE_RETVAL_IF(!attach, MAPI_E_NOT_ENOUGH_MEMORY, mem_ctx);

	contextID = emsmdbp_get_contextID(sharing_object);

	ret = mapistore_message_create_attachment(emsmdbp_ctx->mstore_ctx, contextID, sharing_object->backend_object, attach, &attach->backend_object, &aid);
	OPENCHANGE_RETVAL_IF((ret != MAPISTORE_SUCCESS), mapistore_error_to_mapi(ret), mem_ctx);

	/* 3. Set properties to the attachment */
	retval = emsmdbp_object_sharing_metadata_property(emsmdbp_ctx, sharing_object, &attach_bin, mem_ctx);
	OPENCHANGE_RETVAL_IF((retval != MAPI_E_SUCCESS), retval, mem_ctx);

	aRow.cValues = 3;
	aRow.lpProps = talloc_array(mem_ctx, struct SPropValue, 3);
	set_SPropValue_proptag(aRow.lpProps, PidTagAttachLongFilename, "sharing_metadata.xml");
	set_SPropValue_proptag(aRow.lpProps + 1, PidTagAttachDataBinary, attach_bin);
	set_SPropValue_proptag(aRow.lpProps + 2, PidTagAttachMimeTag, "application/x-sharing-metadata-xml");

	ret = emsmdbp_object_set_properties(emsmdbp_ctx, attach, &aRow);
	OPENCHANGE_RETVAL_IF(ret != MAPISTORE_SUCCESS, mapistore_error_to_mapi(ret), mem_ctx);

	/* 4. Save the attachment */
	/* TODO: Include this code once all available backends implement for attachment save */
	/* ret = mapistore_attachment_save(emsmdbp_ctx->mstore_ctx, contextID, attach->backend_object, mem_ctx); */

	talloc_free(mem_ctx);

	return MAPI_E_SUCCESS;
}
