/*
   Asynchronous EMSMDB server

   OpenChange Project

   Copyright (C) Julien Kerihuel 2015

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

#include "dcesrv_asyncemsmdb.h"
#include "utils/dlinklist.h"
#include "mapiproxy/libmapistore/mapistore_private.h"
#include "mapiproxy/libmapistore/gen_ndr/ndr_mapistore_notification.h"

/**
   \file dcesrv_asyncemsmdb.c

   \brief Asynchronous EMSMDB server implementation

 */

struct exchange_asyncemsmdb_session	*asyncemsmdb_session = NULL;
void					*openchangedb_ctx = NULL;
static struct ldb_context		*samdb_ctx = NULL;

static struct exchange_asyncemsmdb_session *dcesrv_find_asyncemsmdb_session(struct GUID *uuid)
{
	struct exchange_asyncemsmdb_session	*session, *found_session = NULL;

	for (session = asyncemsmdb_session; !found_session && session; session = session->next) {
		if (GUID_equal(uuid, &session->uuid)) {
			found_session = session;
		}
	}

	return found_session;
}

/**
   \details Return an available random port

   \return port > 0 on success, otherwise -1
 */
static int _get_random_port(void)
{
	int			sockfd;
	struct sockaddr_in	addr;
	socklen_t		addr_len;
	uint16_t		port = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) return -1;

	bzero((char *) &addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0);
	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(sockfd);
		return -1;
	}

	bzero((char *) &addr, sizeof(addr));
	addr_len = sizeof(addr);
	if (getsockname(sockfd, &addr, &addr_len) == -1) {
		close(sockfd);
		return -1;
	}

	port = addr.sin_port;
	close(sockfd);

	return port;
}


/**
   \details Release the mapistore context used by the asyncemsmdb context

   \param data pointer on data to destroy

   \return 0 on success, otherwise -1
 */
static int asyncemsmdb_mapistore_destructor(void *data)
{
	struct mapistore_context	*mstore_ctx = (struct mapistore_context *) data;
	enum mapistore_error		retval;

	retval = mapistore_release(mstore_ctx);
	if (retval != MAPISTORE_SUCCESS) return false;

	OC_DEBUG(0, "MAPIStore context released");
	return true;
}


/**
   \details The SOGo backend does not hold the FolderID of the parent
   folder object within its property list and relies on the parent
   sogo container to fetch this information. The problem is that
   dcesrv_asyncemsmdb creates a mapistore context directly over the
   destination folder and therefore has no parent. Trying to fetch
   this information return 0x0 as opening a mailbox would in SOGo.

   This function works around this problem by guessing parent folder
   container from mapistoreURI and retrieving its folder id from
   mapistore indexing database.

   SOGo URI for folders entries typically end with a trailing '/'
   which explains why we seek 2 slash (count == 2) to proceed. e.g.:
   sogo://msft:msft@mail/folderINBOX/folderone/foldertwo/

   \param mem_ctx pointer to the memory context
   \param p pointer to the private asyncemsmdb data
   \param uri the sogo string to parse
   \param fid pointer to the fid to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error get_mapistore_sogo_PidTagParentFolderId(TALLOC_CTX *mem_ctx, struct asyncemsmdb_private_data *p,
								    char *uri, uint64_t *fid)
{
	enum mapistore_error	retval;
	int			i;
	uint32_t		count = 0;
	uint64_t		parent_fid;
	char			*parent_uri;
	bool			soft_deleted;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!parent_fid, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	for (i = strlen(uri); i; i--) {
		if (uri[i] == '/') {
			count++;
		}
		if (count == 2) {
			parent_uri = talloc_strndup(mem_ctx, uri, i + 1);
			MAPISTORE_RETVAL_IF(!parent_uri, MAPI_E_NOT_ENOUGH_MEMORY, NULL);

			retval = mapistore_indexing_record_get_fmid(p->mstore_ctx, p->username, parent_uri, false, &parent_fid, &soft_deleted);
			talloc_free(parent_uri);
			MAPISTORE_RETVAL_IF(retval, retval, NULL);

			*fid = parent_fid;
			return MAPISTORE_SUCCESS;
		}
	}

	return MAPISTORE_ERR_NOT_FOUND;
}

/**
   \details build a SOGo URL

   \param _mem_ctx pointer to the memory context to use data to return
   \param type the sogo element type (mail, calendar, task etc.)
   \param username the name of the user
   \param folder the IMAP folder
   \param separator the IMAP separator
   \param uri pointer on pointer to the string to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */

static enum mapistore_error build_mapistore_sogo_url(TALLOC_CTX *_mem_ctx, char *type, char *username,
						     const char *folder, char separator, char **uri)
{
	TALLOC_CTX	*mem_ctx;
	char		*url_tmp = NULL;
	char		*_uri = NULL;
	size_t		baselen = 0;
	size_t		len = 0;
	char		*folder_tmp = NULL;
	char		*sep = NULL;
	char		*token = NULL;
	int		i, j;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!type, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!username, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!folder, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!uri, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	mem_ctx = talloc_new(NULL);
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	url_tmp = talloc_asprintf(mem_ctx, "sogo://%s:%s@%s/", username, username, type);
	MAPISTORE_RETVAL_IF(!url_tmp, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	baselen = strlen(url_tmp);

	if ((folder_tmp = strchr(folder, separator))) {
		sep = talloc_asprintf(mem_ctx, "%c", separator);
		MAPISTORE_RETVAL_IF(!sep, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

		folder_tmp = (char *)folder;
		while ((token = strtok_r(folder_tmp, sep, &folder_tmp))) {
			url_tmp = talloc_asprintf_append(url_tmp, "folder%s/", token);
			MAPISTORE_RETVAL_IF(!url_tmp, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
		}
		talloc_free(sep);
	} else {
		url_tmp = talloc_asprintf_append(url_tmp, "folder%s/", folder);
		MAPISTORE_RETVAL_IF(!url_tmp, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	}

	/* Replace following characters:
	* ' ' with '_SP_',
	* '_' with '_U_',
	* '@' with '_A_'
	*/
	len = strlen(url_tmp);
	for (i = baselen; i < strlen(url_tmp); i++) {
		switch (url_tmp[i]) {
		case ASYNCEMSMDB_SPACE:
			len += ASYNCEMSMDB_SOGO_SPACE_LEN;
			break;
		case ASYNCEMSMDB_UNDERSCORE:
			len += ASYNCEMSMDB_SOGO_UNDERSCORE_LEN;
			break;
		case ASYNCEMSMDB_AT:
			len += ASYNCEMSMDB_SOGO_AT_LEN;
			break;
		default:
			len += 1;
		}
	}

	_uri = talloc_array(mem_ctx, char, len + 1);
	MAPISTORE_RETVAL_IF(!_uri, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	memset(_uri, 0, len + 1);

	/* copy baselen */
	memcpy(_uri, url_tmp, baselen);

	/* build remaining part */
	for (i = baselen, j = baselen; i < strlen(url_tmp); i++) {
		switch (url_tmp[i]) {
		case ASYNCEMSMDB_SPACE:
			memcpy(&_uri[j], ASYNCEMSMDB_SOGO_SPACE, ASYNCEMSMDB_SOGO_SPACE_LEN);
			j += ASYNCEMSMDB_SOGO_SPACE_LEN;
			break;
		case ASYNCEMSMDB_UNDERSCORE:
			memcpy(&_uri[j], ASYNCEMSMDB_SOGO_UNDERSCORE, ASYNCEMSMDB_SOGO_UNDERSCORE_LEN);
			j += ASYNCEMSMDB_SOGO_UNDERSCORE_LEN;
			break;
		case ASYNCEMSMDB_AT:
			memcpy(&_uri[j], ASYNCEMSMDB_SOGO_AT, ASYNCEMSMDB_SOGO_AT_LEN);
			j += ASYNCEMSMDB_SOGO_AT_LEN;
			break;
		default:
			_uri[j] = url_tmp[i];
			j += 1;
			break;
		}
	}
	talloc_free(url_tmp);
	*uri = talloc_strdup(_mem_ctx, _uri);
	MAPISTORE_RETVAL_IF(!*uri, MAPISTORE_ERR_NO_MEMORY, NULL);
	talloc_free(_uri);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

/**
   \details Retrieve properties from mapisore object

   \param mem_ctx pointer to the memory context to use for returned data memory allocation
   \param p pointer to the private asyncemsmdb data
   \param folderId the ID of the folder from which data are to be retrieved
   \param properties pointer to the array of MAPI properties to retrieve
   \param data_pointers pointer on pointer to the data to return
   \param retvals pointers to the return valud of each data_pointers entry to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error get_properties_mapistore(TALLOC_CTX *mem_ctx, struct asyncemsmdb_private_data *p,
						     uint64_t folderId, struct SPropTagArray *properties,
						     void **data_pointers, enum MAPISTATUS *retvals)
{
	uint32_t			contextID;
	enum mapistore_error		retval;
	void				*context_object;
	char				*uri = NULL;
	bool				soft_deleted;
	struct mapistore_property_data	*prop_data;
	int				i;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!p, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
	MAPISTORE_RETVAL_IF(!properties, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!data_pointers, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!retvals, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve the mapistore folder URI */
	retval = mapistore_indexing_record_get_uri(p->mstore_ctx, p->username, mem_ctx, folderId, &uri, &soft_deleted);
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	/* Set or add context */
	retval = mapistore_search_context_by_uri(p->mstore_ctx, uri, &contextID, &context_object);
	if (retval == MAPISTORE_SUCCESS) {
		retval = mapistore_add_context_ref_count(p->mstore_ctx, contextID);
	} else {
		retval = mapistore_add_context(p->mstore_ctx, p->username, uri, folderId, &contextID, &context_object);
	}
	MAPISTORE_RETVAL_IF(retval, retval, NULL);

	prop_data = talloc_array(NULL, struct mapistore_property_data, properties->cValues);
	MAPISTORE_RETVAL_IF(!prop_data, MAPISTORE_ERR_NO_MEMORY, NULL);
	memset(prop_data, 0, sizeof(struct mapistore_property_data) * properties->cValues);

	retval = mapistore_properties_get_properties(p->mstore_ctx, contextID, context_object, mem_ctx, properties->cValues,
						     properties->aulPropTag, prop_data);
	MAPISTORE_RETVAL_IF(retval, retval, prop_data);

	for (i = 0; i < properties->cValues; i++) {
		if (prop_data[i].error) {
				if (properties->aulPropTag[i] == PidTagFolderFlags) {
					prop_data[i].data = talloc_zero(prop_data, uint32_t);
					*((uint32_t *)prop_data[i].data) = FolderFlags_IPM|FolderFlags_Normal;
					data_pointers[i] = prop_data[i].data;
					(void) talloc_reference(data_pointers, prop_data[i].data);
					retvals[i] = MAPI_E_SUCCESS;
				} else {
					retvals[i] = mapistore_error_to_mapi(prop_data[i].error);
				}
		} else {
			if (prop_data[i].data == NULL) {
				if (properties->aulPropTag[i] == PidTagFolderFlags) {
					prop_data[i].data = talloc_zero(prop_data, uint32_t);
					*(uint32_t *)prop_data[i].data = FolderFlags_IPM|FolderFlags_Normal;
					data_pointers[i] = prop_data[i].data;
					(void) talloc_reference(data_pointers, prop_data[i].data);
					retvals[i] = MAPI_E_SUCCESS;
				} else {
					retvals[i] = MAPI_E_NOT_FOUND;
				}
			} else {
				if (properties->aulPropTag[i] == PidTagParentFolderId) {
					if (*(uint64_t *)prop_data[i].data == 0x0) {
						uint64_t	parent_fid;

						retval = get_mapistore_sogo_PidTagParentFolderId(mem_ctx, p, uri, &parent_fid);
						if (retval == MAPISTORE_SUCCESS) {
							*(uint64_t *)(prop_data[i].data) = parent_fid;
							retvals[i] = MAPI_E_SUCCESS;
						}
					}
				} else if (properties->aulPropTag[i] == PidTagFolderFlags) {
					prop_data[i].data = talloc_zero(prop_data, uint32_t);
					*(uint32_t *)prop_data[i].data = FolderFlags_IPM|FolderFlags_Normal;
					data_pointers[i] = prop_data[i].data;
					retvals[i] = MAPI_E_SUCCESS;
				}
				data_pointers[i] = prop_data[i].data;
				(void) talloc_reference(data_pointers, prop_data[i].data);
			}
		}
	}

	/* Delete mapistore context */
	retval = mapistore_del_context(p->mstore_ctx, contextID);
	MAPISTORE_RETVAL_IF(retval, retval, prop_data);

	talloc_free(prop_data);
	return MAPISTORE_SUCCESS;
}


/**
   \details Retrieve properties from openchangedb system folder

   \param mem_ctx pointer to the memory context to use to allocate returned data
   \param p pointer to the private asyncemsmdb data
   \param folderId the ID of the folder from which data are to be retrieved
   \param properties pointer to the array of MAPI properties to retrieve
   \param data_pointers pointer on pointer to the data to return
   \param retvals pointers to the return value of each data_pointers entry to return

   \return MAPI_E_SUCCESS on success, otherwise MAPI error
 */
static enum MAPISTATUS get_properties_systemspecialfolder(TALLOC_CTX *mem_ctx, struct asyncemsmdb_private_data *p,
							  uint64_t folderId, struct SPropTagArray *properties,
							  void **data_pointers, enum MAPISTATUS *retvals)
{
	enum MAPISTATUS			retval = MAPI_E_SUCCESS;
	int				i;
	uint32_t			*obj_count;
	uint8_t				*has_subobj;
	time_t				unix_time;
	NTTIME				nt_time;
	struct FILETIME			*ft;
	uint32_t			*folder_flags;

	/* Sanity checks */
	MAPI_RETVAL_IF(!p, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!properties, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!data_pointers, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!retvals, MAPI_E_INVALID_PARAMETER, NULL);

	for (i = 0; i < properties->cValues; i++) {
		if (properties->aulPropTag[i] == PidTagFolderFlags) {
			folder_flags = talloc_zero(data_pointers, uint32_t);
			MAPI_RETVAL_IF(!folder_flags, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			*folder_flags = FolderFlags_IPM|FolderFlags_Normal;
			data_pointers[i] = folder_flags;
		} else if (properties->aulPropTag[i] == PR_FOLDER_CHILD_COUNT) {
			obj_count = talloc_zero(data_pointers, uint32_t);
			MAPI_RETVAL_IF(!obj_count, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			retval = openchangedb_get_folder_count(openchangedb_ctx, p->username, folderId, obj_count);
			data_pointers[i] = obj_count;
		} else if (properties->aulPropTag[i] == PR_SUBFOLDERS) {
			obj_count = talloc_zero(NULL, uint32_t);
			retval = openchangedb_get_folder_count(openchangedb_ctx, p->username, folderId, obj_count);
			has_subobj = talloc_zero(data_pointers, uint8_t);
			MAPI_RETVAL_IF(!has_subobj, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			*has_subobj = (*obj_count > 0) ? 1 : 0;
			data_pointers[i] = has_subobj;
			talloc_free(obj_count);
		} else if (properties->aulPropTag[i] == PR_CONTENT_COUNT
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
			MAPI_RETVAL_IF(!ft, MAPI_E_NOT_ENOUGH_MEMORY, NULL);
			ft->dwLowDateTime = (nt_time & 0xffffffff);
			ft->dwHighDateTime = nt_time >> 32;
			data_pointers[i] = ft;
			retval = MAPI_E_SUCCESS;
		}
		else {
			retval = openchangedb_get_folder_property(data_pointers, openchangedb_ctx, p->username,
								  properties->aulPropTag[i], folderId, data_pointers + i);
		}
		retvals[i] = retval;
	}

	return MAPI_E_SUCCESS;
}



/**
   \details Process a TableModified event on a ContentsTable for row modified specific event type

   \param mem_ctx pointer to the memory context
   \param p pointer to the private asyncemsmdb data
   \param s pointer to the array of subscriptions for this session
   \param folderId the folder in which the tablemodified event occurred

   \note This first iteration has the following limitations:
   - Only TABLE_ROW_MODIFIED case are handled
   - Only handle notifications within Inbox folder (openchangedb folders)

   \return 0 on success, otherwise -1
 */
static int process_tablemodified_contentstable_notification(TALLOC_CTX *mem_ctx,
							    struct asyncemsmdb_private_data *p,
							    struct mapistore_notification_subscription *s,
							    uint64_t folderId)
{
	int				i;
	struct EcDoRpc_MAPI_REPL	reply;
	DATA_BLOB			payload;
	struct SPropTagArray		SPropTagArray;
	void				**data_pointers;
	enum MAPISTATUS			*retvals;
	enum MAPISTATUS			retval;
	int				ret;
	uint32_t			prop_idx;
	int				flagged = 0;
	uint32_t			property;
	void				*data = NULL;
	enum ndr_err_code		ndr_err_code;
	struct ndr_push			*ndr;
	char				*mapistoreURI = NULL;

	/* Looking */
	for (i = 0; i < s->v.v1.count; i++) {
		if (s->v.v1.subscription[i].flags & sub_TableModified) {
			if ((s->v.v1.subscription[i].flags & sub_WholeStore) ||
			    (s->v.v1.subscription[i].fid == folderId)) {
				memset(&reply, 0, sizeof(struct EcDoRpc_MAPI_REPL));
				reply.opnum = op_MAPI_Notify;
				reply.error_code = MAPI_E_SUCCESS;
				reply.u.mapi_Notify.NotificationHandle = s->v.v1.subscription[i].handle;
				reply.u.mapi_Notify.LogonId = 0;
				reply.u.mapi_Notify.NotificationType = fnevMbit | fnevSbit | fnevTableModified;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.TableEvent = 0x5; /* TABLE_ROW_MODIFED */
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.FID = folderId;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.MID = 0x0;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.Instance = 0x0;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.InsertAfterFID = 0x0;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.InsertAfterMID = 0x0;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.InsertAfterInstance = 0x0;

				SPropTagArray.cValues = s->v.v1.subscription[i].count;
				SPropTagArray.aulPropTag = (enum MAPITAGS *)s->v.v1.subscription[i].properties;

				data_pointers = talloc_array(mem_ctx, void *, SPropTagArray.cValues);
				if (!data_pointers) {
					OC_DEBUG(0, "No memory available");
					return -1;
				}
				memset(data_pointers, 0, sizeof(void *) * SPropTagArray.cValues);

				retvals = talloc_array(mem_ctx, enum MAPISTATUS, SPropTagArray.cValues);
				if (!retvals) {
					OC_DEBUG(0, "No memory available");
					return -1;
				}
				memset(retvals, 0, sizeof(enum MAPISTATUS) * SPropTagArray.cValues);

				retval = openchangedb_get_mapistoreURI(mem_ctx, openchangedb_ctx,
								       p->username, folderId,
								       &mapistoreURI, true);
				if (retval == MAPI_E_SUCCESS) {
					retval = get_properties_systemspecialfolder(mem_ctx, p, folderId, &SPropTagArray,
										    data_pointers, retvals);
				} else {
					retval = MAPI_E_SUCCESS;
					ret = get_properties_mapistore(mem_ctx, p, folderId, &SPropTagArray,
								       data_pointers, retvals);
					if (ret != MAPISTORE_SUCCESS) {
						OC_DEBUG(0, "Unable to set mapistore properties: %s", mapistore_errstr(ret));
						retval = MAPISTORE_ERROR;
					}
				}

				if (retval != MAPI_E_SUCCESS) {
					talloc_free(data_pointers);
					talloc_free(retvals);
					return -1;
				}

				/* Check if any of the properties are flagged */
				for (prop_idx = 0; prop_idx < SPropTagArray.cValues; prop_idx++) {
					if (retvals[prop_idx] != MAPI_E_SUCCESS) {
						flagged = 1;
						break;
					}
				}

				memset(&payload, 0, sizeof(DATA_BLOB));
				if (flagged) {
					libmapiserver_push_property(mem_ctx, 0xb, (const void *)&flagged, &payload, 0, 0, 0);
				} else {
					libmapiserver_push_property(mem_ctx, 0x0, (const void *)&flagged, &payload, 0, 1, 0);
				}

				/* Push properties */
				for (prop_idx = 0; prop_idx < SPropTagArray.cValues; prop_idx++) {
					property = SPropTagArray.aulPropTag[prop_idx];
					retval = retvals[prop_idx];
					if (retval == MAPI_E_NOT_FOUND) {
						property = (property & 0xFFFF0000) + PT_ERROR;
						data = &retval;
					} else {
						data = data_pointers[prop_idx];
					}
					libmapiserver_push_property(mem_ctx, property, data, &payload, flagged?PT_ERROR:0, flagged, 0);
				}

				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.Columns = payload;
				reply.u.mapi_Notify.NotificationData.ContentsTableChange.ContentsTableChangeUnion.ContentsRowModifiedNotification.ColumnsSize = payload.length;

				/* Pack and push */
				ndr = ndr_push_init_ctx(mem_ctx);
				if (!ndr) {
					OC_DEBUG(0, "Unable to allocate memory");
					talloc_free(data_pointers);
					talloc_free(retvals);
					return -1;
				}
				ndr->offset = 0;

				ndr_err_code = ndr_push_EcDoRpc_MAPI_REPL(ndr, NDR_SCALARS, &reply);
				if (ndr_err_code != NDR_ERR_SUCCESS) {
					talloc_free(data_pointers);
					talloc_free(retvals);
					talloc_free(ndr);
					OC_DEBUG(0, "Unable to push Notify response blob for newmail notification");
					return -1;
				}

				ret = mapistore_notification_deliver_add(p->mstore_ctx, p->emsmdb_uuid, ndr->data, ndr->offset);
				talloc_free(data_pointers);
				talloc_free(retvals);
				talloc_free(ndr);
				if (ret != MAPISTORE_SUCCESS) {
					OC_DEBUG(0, "Unable to deliver notification blob");
					return -1;
				}
			}
		}
	}

	return 0;
}


/**
   \details Process newmail notification

   \param mem_ctx pointer to the memory context
   \param p pointer to the private asyncemsmdb data
   \param notif pointer to the mapistore newmail notification
   \param s pointer to the array of subscriptions for this session

   \note newmail notification (popup) will only be triggered for
   emails delivered to Inbox. However, TableModified notification
   should be triggered in every case if the subscription exists

   \todo handle tablemodified delivered in other folders but Inbox

   \return 0 on success, otherwise -1
 */
static int process_newmail_notification(TALLOC_CTX *mem_ctx,
					struct asyncemsmdb_private_data *p,
					struct mapistore_notification *notif,
					struct mapistore_notification_subscription *s)
{
	int				i;
	int				index = -1;
	enum MAPISTATUS			retval;
	enum mapistore_error		ret;
	int				rval;
	struct indexing_context		*ictx;
	uint64_t			fid;
	uint64_t			mid;
	char				*folder_uri = NULL;
	char				*message_uri = NULL;
	bool				soft_deleted;
	struct EcDoRpc_MAPI_REPL	reply;
	struct ndr_push			*ndr;
	enum ndr_err_code		ndr_err_code;

	/* Loop over subscriptions to find subscription for newmail notification */
	for (i = 0; i < s->v.v1.count; i++) {
		if (s->v.v1.subscription[i].flags & sub_NewMail) {
			index = i;
			break;
		}
	}
	if (index == -1) {
		OC_DEBUG(0, "No subscription found with newmail flag enabled");
		return -1;
	}

	/* Check if we need to register message in a different folder than Inbox */
	if (notif->v.v1.u.newmail.folder && (notif->v.v1.u.newmail.folder[0] == '\0')) {
		/* Retrieve Inbox FID */
		retval = openchangedb_get_SystemFolderID(openchangedb_ctx, p->username, ASYNCEMSMDB_INBOX_SYSTEMIDX, &fid);
		if (retval != MAPI_E_SUCCESS) {
			OC_DEBUG(0, "Failed to retrieve Inbox FolderId for user %s", p->username);
			return -1;
		}

		/* Fetch the Inbox folder mapistore URI */
		retval = openchangedb_get_mapistoreURI(mem_ctx, openchangedb_ctx, p->username, fid, &folder_uri, true);
		if (retval != MAPI_E_SUCCESS) {
			OC_DEBUG(0, "Failed to retrieve Inbox mapistore URI for user %s", p->username);
			return -1;
		}
	} else {
		/* handle sogo url case here */
		if (!strcmp(notif->v.v1.u.newmail.backend, "sogo")) {
			bool	soft_deletep;

			ret = build_mapistore_sogo_url(mem_ctx, "mail", p->username, notif->v.v1.u.newmail.folder,
						       notif->v.v1.u.newmail.separator, &folder_uri);
			if (ret != MAPISTORE_SUCCESS) {
				OC_DEBUG(0, "Unable to generate sogo URL");
				return -1;
			}

			ret = mapistore_indexing_record_get_fmid(p->mstore_ctx, p->username,
								 folder_uri, true, &fid,
								 &soft_deletep);
			if (ret != MAPISTORE_SUCCESS) {
				OC_DEBUG(0, "Unable to find FolderId from uri='%s'", folder_uri);
				talloc_free(folder_uri);
				return -1;
			}
		}
	}

	/* Open connection to the indexing database */
	ret = mapistore_indexing_add(p->mstore_ctx, p->username, &ictx);
	if (ret != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "Failed to open connection to indexing database for user %s", p->username);
		return -1;
	}

	/* Build the message URI: append folderID with message id */
	message_uri = talloc_asprintf(mem_ctx, "%s%s", folder_uri, notif->v.v1.u.newmail.eml);
	talloc_free(folder_uri);
	if (!message_uri) {
		OC_DEBUG(0, "Unable to allocate memory");
		return -1;
	}

	/* Check if the URI already exists with mapistore_indexing_record_get_fmid */
	ret = mapistore_indexing_record_get_fmid(p->mstore_ctx, p->username, message_uri, false, &mid, &soft_deleted);
	if (ret == MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "URL %s already registered for user %s and associated to MID 0x%"PRIx64, message_uri, p->username, mid);
		talloc_free(message_uri);
		goto process;
	}

	/* Allocate new mid */
	ret = mapistore_indexing_get_new_folderID_as_user(p->mstore_ctx, p->username, &mid);
	if (ret != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "Failed to allocate new mid for user %s", p->username);
		talloc_free(message_uri);
		return -1;
	}

	/* Register message */
	ret = ictx->add_fmid(ictx, p->username, mid, message_uri);
	if (ret != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "Unable to register 0x%"PRIx64" with URL=%s", mid, message_uri);
		talloc_free(message_uri);
		return -1;
	}
	talloc_free(message_uri);

	/* Generate Notify_reply blob */
process:
	memset(&reply, 0, sizeof(struct EcDoRpc_MAPI_REPL));
	reply.opnum = op_MAPI_Notify;
	reply.error_code = MAPI_E_SUCCESS;
	reply.u.mapi_Notify.NotificationHandle = s->v.v1.subscription[i].handle;
	reply.u.mapi_Notify.LogonId = 0;
	reply.u.mapi_Notify.NotificationType = fnevMbit | fnevNewMail;
	reply.u.mapi_Notify.NotificationData.NewMessageNotification.FID = fid;
	reply.u.mapi_Notify.NotificationData.NewMessageNotification.MID = mid;
	reply.u.mapi_Notify.NotificationData.NewMessageNotification.MessageFlags = MSGFLAG_UNMODIFIED;
	reply.u.mapi_Notify.NotificationData.NewMessageNotification.UnicodeFlag = false;
	reply.u.mapi_Notify.NotificationData.NewMessageNotification.MessageClass.lpszA = "IPM.Note";

	ndr = ndr_push_init_ctx(mem_ctx);
	if (!ndr) {
		OC_DEBUG(0, "Unable to allocate memory");
		return -1;
	}
	ndr->offset = 0;

	ndr_err_code = ndr_push_EcDoRpc_MAPI_REPL(ndr, NDR_SCALARS, &reply);
	if (ndr_err_code != NDR_ERR_SUCCESS) {
		talloc_free(ndr);
		OC_DEBUG(0, "Unable to push Notify response blob for newmail notification");
		return -1;
	}

	ret = mapistore_notification_deliver_add(p->mstore_ctx, p->emsmdb_uuid, ndr->data, ndr->offset);
	talloc_free(ndr);
	if (ret != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "Unable to deliver notification blob");
		return -1;
	}

	rval = process_tablemodified_contentstable_notification(mem_ctx, p, s, fid);
	if (rval != 0) {
		OC_DEBUG(0, "TableModified notification failed");
	}

	return 0;
}

static void EcDoAsyncWaitEx_handler(struct tevent_context *ev,
				    struct tevent_fd *fde,
				    uint16_t flags,
				    void *private_data)
{
	struct asyncemsmdb_private_data			*p = talloc_get_type(private_data,
									     struct asyncemsmdb_private_data);
	TALLOC_CTX					*mem_ctx;
	enum mapistore_error				retval;
	int						ret;
	DATA_BLOB					blob;
	char						*str = NULL;
	int						bytes = 0;
	NTSTATUS					status;
	struct mapistore_notification_subscription	r;
	struct mapistore_notification			n;
	struct ndr_print				*ndr_print;
	struct ndr_pull					*ndr_pull;
	enum ndr_err_code				ndr_err_code;

	if (!p) {
		OC_DEBUG(0, "[asyncemsmdb]: private_data is NULL");
		return;
	}

	bytes = nn_recv(p->sock, &str, NN_MSG, NN_DONTWAIT);
	if (bytes == 0) {
		OC_DEBUG(0, "[asyncemsmdb]: EcDoAsyncWaitEx_handler: str is NULL!");
		return;
	}

	mem_ctx = talloc_new(NULL);
	if (!mem_ctx) {
		OC_DEBUG(0, "[asyncemsmdb]: No more memory");
		return;
	}

	blob.data = (uint8_t *) str;
	blob.length = bytes;
	ndr_pull = ndr_pull_init_blob(&blob, mem_ctx);
	if (!ndr_pull) {
		nn_freemsg(str);
		OC_DEBUG(0, "[asyncemsmdb]: No more memory");
		return;
	}
	ndr_set_flags(&ndr_pull->flags, LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_REF_ALLOC);

	ndr_err_code = ndr_pull_mapistore_notification(ndr_pull, NDR_SCALARS, &n);
	nn_freemsg(str);
	if (ndr_err_code != NDR_ERR_SUCCESS) {
		OC_DEBUG(0, "[asyncemsmdb]: Invalid mapistore_notification structure");
		talloc_free(mem_ctx);
		return;
	}

	/* Sanity checks on notification payload */
	if (n.vnum >= MAPISTORE_NOTIFICATION_VMAX) {
		OC_DEBUG(0, "[asyncemsmdb]: Invalid version, expected at max %d but got %d",
			 MAPISTORE_NOTIFICATION_VMAX - 1, n.vnum);
		talloc_free(mem_ctx);
		return;
	}

	ndr_print = talloc_zero(mem_ctx, struct ndr_print);
	if (!ndr_print) {
		OC_DEBUG(0, "[asyncemsmdb]: No more memory");
		talloc_free(mem_ctx);
		return;
	}
	ndr_print->depth = 1;
	ndr_print->print = ndr_print_debug_helper;
	ndr_print->no_newline = false;

	ndr_print_mapistore_notification(ndr_print, "notification", &n);

	OC_DEBUG(0, "Notification received for session: %s", p->emsmdb_session_str);

	retval = mapistore_notification_subscription_get(mem_ctx, p->mstore_ctx, p->emsmdb_uuid, &r);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "no subscription to process");
		talloc_free(mem_ctx);
		return;
	}

	OC_DEBUG(0, "%d subscriptions available:", r.v.v1.count);
	ndr_print_mapistore_notification_subscription(ndr_print, "subscriptions", &r);
	talloc_free(ndr_print);

	/* Process notifications */
	switch (n.v.v1.flags) {
	case (sub_NewMail):
		ret = process_newmail_notification(mem_ctx, p, &n, &r);
		if (ret) {
			OC_DEBUG(0, "[asyncemsmdb]: Failed to process newmail notification (error=0x%x)", ret);
			talloc_free(mem_ctx);
			return;
		}
		break;
	default:
		OC_DEBUG(0, "[asyncemsmdb]: Unsupported notification 0x%x", n.v.v1.flags);
		talloc_free(mem_ctx);
		return;
	}
	talloc_free(ndr_pull);
	talloc_free(mem_ctx);

	p->r->out.pulFlagsOut = talloc_zero(p->dce_call, uint32_t);
	*p->r->out.pulFlagsOut = 0x1;

	status = dcesrv_reply(p->dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		OC_DEBUG(0, "EcDoAsyncWaitEx_handler: dcesrv_reply() failed - %s", nt_errstr(status));
	}

	talloc_free(p->fd_event);
	p->fd_event = NULL;
}


 /**
    \details exchange_async_emsmdb EcDoAsyncWaitEx (0x0) function

   \param dce_call pointer to the session context
   \param mem_ctx pointer to the memory context
   \param r pointer to the EcDoAsyncWaitEx request data

   \return NT_STATUS_OK on success
 */
static NTSTATUS dcesrv_EcDoAsyncWaitEx(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx,
				       struct EcDoAsyncWaitEx *r)
{
	enum mapistore_error			retval;
	struct exchange_asyncemsmdb_session	*session = NULL;
	struct asyncemsmdb_private_data		*p = NULL;
	struct mapistore_context		*mstore_ctx = NULL;
	struct GUID				uuid;
	char					*cn = NULL;
	const char				*ip_addr = NULL;
	int					nn_retval;
	size_t					sz = 0;
	char					*bind_addr = NULL;
	int					port = 0;

	OC_DEBUG(0, "exchange_asyncemsmdb: EcDoAsyncWaitEx (0x0)");

	/* Step 0. Ensure incoming user is authenticated */
	if (!dcesrv_call_authenticated(dce_call)) {
		OC_DEBUG(1, "No challenge requested by client, cannot authenticate");
		*r->out.pulFlagsOut = 0x1;
		return NT_STATUS_OK;
	}

	/* Set the asynchronous dcerpc flag on the connection */
	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;

	/* Step 1. Search for an existing session */
	session = dcesrv_find_asyncemsmdb_session(&r->in.async_handle->uuid);
	if (session) {
		p = (struct asyncemsmdb_private_data *) session->data;
		/* Ensure the session is registered */
		retval = mapistore_notification_session_exist(p->mstore_ctx, r->in.async_handle->uuid);
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb]: no matching emsmdb session found");
			*r->out.pulFlagsOut = 0x1;
			return NT_STATUS_OK;
		}

		p->dce_call = dce_call;
		p->r = r;
	} else {
		/* Step 1. Ensure the session is registered */
		mstore_ctx = mapistore_init(mem_ctx, dce_call->conn->dce_ctx->lp_ctx, NULL);
		if (!mstore_ctx) {
			OC_DEBUG(0, "[asyncemsmdb]: MAPIStore initialized failed");
			*r->out.pulFlagsOut = 0x1;
			return NT_STATUS_OK;
		}

		retval = mapistore_set_connection_info(mstore_ctx, samdb_ctx, openchangedb_ctx,
						       dcesrv_call_account_name(dce_call));
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb]: unable to set mapistore connection info");
			*r->out.pulFlagsOut = 0x1;
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}
		talloc_set_destructor((void *)mstore_ctx, (int (*)(void *))asyncemsmdb_mapistore_destructor);

		retval = mapistore_notification_session_get(dce_call, mstore_ctx, r->in.async_handle->uuid, &uuid, &cn);
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb]: unable to fetch emsmdb session data");
			*r->out.pulFlagsOut = 0x1;
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		/* we're allowed to reply async */
		p = talloc(dce_call->event_ctx, struct asyncemsmdb_private_data);
		if (!p) {
			OC_DEBUG(0, "[asyncemsmdb]: no more memory");
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		p->dce_call = dce_call;
		p->mstore_ctx = talloc_steal(p, mstore_ctx);
		p->emsmdb_uuid = uuid;
		p->username = (char *) dcesrv_call_account_name(dce_call);
		p->emsmdb_session_str = GUID_string(p, (const struct GUID *)&uuid);
		if (!p->emsmdb_session_str) {
			OC_DEBUG(0, "[asyncemsmdb]: no more memory");
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}
		p->r = r;
		p->fd_event = NULL;

		p->sock = nn_socket(AF_SP, NN_PULL);
		if (p->sock == -1) {
			OC_DEBUG(0, "[asyncemsmdb]: failed to create socket: %s", nn_strerror(errno));
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		/* Get a random port */
		port = _get_random_port();
		if (port == -1) {
			OC_DEBUG(0, "[asyncemsmdb]: no port available!");
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		/* Get parametric options */
		ip_addr = lpcfg_parm_string(dce_call->conn->dce_ctx->lp_ctx, NULL, "asyncemsmdb", "listen");
		if (ip_addr == NULL) {
			OC_DEBUG(0, "[asyncemsmdb]: no asyncemsmdb:listen option specified, "
				 "using %s as a fallback", ASYNCEMSMDB_FALLBACK_ADDR);
			ip_addr = ASYNCEMSMDB_FALLBACK_ADDR;
		}

		/* TODO: Add transport as a parametric option */
		bind_addr = talloc_asprintf(mem_ctx, "tcp://%s:%d", ip_addr, port);
		if (!bind_addr) {
			OC_DEBUG(0, "[asyncemsmdb][ERR]: no more memory");
			talloc_free(p);
			talloc_free(mstore_ctx);
			return NT_STATUS_OK;
		}

		nn_retval = nn_bind(p->sock, bind_addr);
		if (nn_retval == -1) {
			OC_DEBUG(0, "[asyncemsmdb] nn_bind failed on %s failed: %s", bind_addr, nn_strerror(errno));
			talloc_free(p);
			talloc_free(mstore_ctx);
			talloc_free(bind_addr);
			return NT_STATUS_OK;
		}

		/* Register the address to the resolver */
		retval = mapistore_notification_resolver_add(p->mstore_ctx, cn, bind_addr);
		if (retval != MAPISTORE_SUCCESS) {
			OC_DEBUG(0, "[asyncemsmdb] unable to add record to the resolver: %s",
				 mapistore_errstr(retval));
			talloc_free(p);
			talloc_free(mstore_ctx);
			talloc_free(bind_addr);
			return NT_STATUS_OK;
		}

		sz = sizeof(p->fd);
		nn_retval = nn_getsockopt(p->sock, NN_SOL_SOCKET, NN_RCVFD, &p->fd, &sz);
		if (nn_retval == -1) {
			OC_DEBUG(0, "[asyncemsmdb] nn_getsockopt failed: %s", nn_strerror(errno));
			talloc_free(p);
			talloc_free(mstore_ctx);
			talloc_free(bind_addr);
			return NT_STATUS_OK;
		}
	}

	if (!p->fd_event) {
		p->fd_event = tevent_add_fd(dce_call->event_ctx,
					    dce_call->event_ctx,
					    p->fd,
					    TEVENT_FD_READ,
					    EcDoAsyncWaitEx_handler,
					    p);
		if (p->fd_event == NULL) {
			OC_DEBUG(0, "[asyncemsmdb] unable to subscribe for fd event in event loop");
			return NT_STATUS_OK;
		}
	}

	/* Register session  */
	if (!session) {
		session = talloc(asyncemsmdb_session, struct exchange_asyncemsmdb_session);
		if (!session) {
		failure:
			OC_DEBUG(0, "[asyncemsmdb][ERR]: No more memory");
			*r->out.pulFlagsOut = 0x1;
			return NT_STATUS_OK;
		}
		session->uuid = r->in.async_handle->uuid;
		session->data = talloc_steal(session, p);
		session->cn = talloc_strdup(session, cn);
		talloc_free(cn);
		if (!session->cn) {
			talloc_free(session);
			goto failure;
		}
		session->bind_addr = talloc_strdup(session, bind_addr);
		talloc_free(bind_addr);
		if (!session->bind_addr) {
			talloc_free(session);
			goto failure;
		}
		session->mstore_ctx = mstore_ctx;

		/* Add the session to the dcesrv_connection_context */
		dce_call->context->private_data = session;

		OC_DEBUG(5, "[asyncemsmdb]: New session added: %s", session->data->emsmdb_session_str);
		DLIST_ADD_END(asyncemsmdb_session, session, struct exchange_asyncemsmdb_session *);
	}

	return NT_STATUS_OK;
}

static NTSTATUS dcerpc_server_asyncemsmdb_unbind(struct dcesrv_connection_context *context, const struct dcesrv_interface *iface)
{
	enum mapistore_error			retval;
	struct exchange_asyncemsmdb_session	*session = (struct exchange_asyncemsmdb_session *) context->private_data;

	OC_DEBUG(0, "dcerpc_server_asyncemsmdb_unbind");

	if (!session) {
		return NT_STATUS_OK;
	}

	OC_DEBUG(0, "[asyncemsmdb] unbind %s", session->bind_addr);
	retval = mapistore_notification_resolver_delete(session->mstore_ctx, session->cn, session->bind_addr);
	if (retval != MAPISTORE_SUCCESS) {
		OC_DEBUG(0, "[asyncemsmdb] unable to delete resolver entry %s from record %s", session->bind_addr, session->cn);
	}

	mapistore_release(session->mstore_ctx);
	DLIST_REMOVE(asyncemsmdb_session, session);

	/* flush pending call on connection */
	context->conn->pending_call_list = NULL;

	return NT_STATUS_OK;
}


/**
   \details Initialize ldb_context to samdb, creates one for all emsmdbp
   contexts

   \param lp_ctx pointer to the loadparm context
 */
static struct ldb_context *samdb_init(struct loadparm_context *lp_ctx)
{
	TALLOC_CTX		*mem_ctx;
	struct tevent_context	*ev;
	const char		*samdb_url;
	struct ldb_context	*_samdb_ctx;

	if (samdb_ctx) return samdb_ctx;

	mem_ctx = talloc_autofree_context();
	ev = tevent_context_init(mem_ctx);
	if (!ev) {
		return NULL;
	}
	tevent_loop_allow_nesting(ev);

	/* Retrieve samdb url (local or external) */
	samdb_url = lpcfg_parm_string(lp_ctx, NULL, "dcerpc_mapiproxy", "samdb_url");

	if (!samdb_url) {
		_samdb_ctx = samdb_connect(mem_ctx, ev, lp_ctx, system_session(lp_ctx), 0);
	} else {
		_samdb_ctx = samdb_connect_url(mem_ctx, ev, lp_ctx, system_session(lp_ctx),
					       LDB_FLG_RECONNECT, samdb_url);
	}

	return _samdb_ctx;
}


static NTSTATUS dcerpc_server_asyncemsmdb_bind(struct dcesrv_call_state *dce_call, const struct dcesrv_interface *iface)
{
	if (!openchangedb_ctx) {
		openchangedb_ctx = mapiproxy_server_openchangedb_init(dce_call->conn->dce_ctx->lp_ctx);
		if (!openchangedb_ctx) {
			OC_DEBUG(OC_LOG_FATAL, "Unable to initialize openchangedb");
			abort();
		}
	}

	if (!samdb_ctx) {
		samdb_ctx = samdb_init(dce_call->conn->dce_ctx->lp_ctx);
		if (!samdb_ctx) {
			OC_DEBUG(OC_LOG_FATAL, "Unable to initialize samdb");
			abort();
		}
	}

	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_PROCESS_PENDING_CALL;
	return NT_STATUS_OK;
}


NTSTATUS samba_init_module(void)
{
	NTSTATUS status;

	status = dcerpc_server_asyncemsmdb_init();
	NT_STATUS_NOT_OK_RETURN(status);

	/* Initialize exchange_async_emsmdb session */
	asyncemsmdb_session = talloc_zero(talloc_autofree_context(), struct exchange_asyncemsmdb_session);
	if (!asyncemsmdb_session) return NT_STATUS_NO_MEMORY;
	asyncemsmdb_session->data = NULL;

	status = ndr_table_register(&ndr_table_asyncemsmdb);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

#include "gen_ndr/ndr_asyncemsmdb_s.c"
