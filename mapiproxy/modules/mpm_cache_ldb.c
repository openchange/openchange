/*
   MAPI Proxy - Cache module

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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
   \file mpm_cache_ldb.c

   \brief LDB routines for the cache module
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/modules/mpm_cache.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

/**
   \details Create the cache database

   \param dce_ctx pointer to the session context
   \param database the complete path to the tdb store
   \param ldb_ctx pointer to pointer on the the LDB context

   \return NT_STATUS_OK on success, otherwise NT_ERROR:
   NT_STATUS_NO_MEMORY, NT_STATUS_NOT_FOUND.
 */
NTSTATUS mpm_cache_ldb_createdb(struct dcesrv_context *dce_ctx, 
				const char *database, 
				struct ldb_context **ldb_ctx)
{
	struct ldb_context	*tmp_ctx;
	struct tevent_context	*ev;
	int			ret;

	ev = tevent_context_init(dce_ctx);
	if (!ev) return NT_STATUS_NO_MEMORY;

	tmp_ctx = ldb_init(dce_ctx, ev);
	if (!tmp_ctx) return NT_STATUS_NO_MEMORY;
	
	ret = ldb_connect(tmp_ctx, database, 0, NULL);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_NOT_FOUND;
	}

	*ldb_ctx = tmp_ctx;

	return NT_STATUS_OK;
}


/**
   \details Add a folder record to the TDB store

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer to the LDB context
   \param FolderId the ID we will be using to uniquely create the
   record

   \return NT_STATUS_OK on success, otherwise NT_STATUS_NOT_FOUND
 */
static NTSTATUS  mpm_cache_ldb_add_folder(TALLOC_CTX *mem_ctx, 
					  struct ldb_context *ldb_ctx,
					  uint64_t FolderId)
{
	struct ldb_message	*msg;
	char			*dn;
	int			ret;

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	dn = talloc_asprintf(mem_ctx, "CN=0x%"PRIx64",CN=Cache", FolderId);
	msg->dn = ldb_dn_new(ldb_ctx, ldb_ctx, dn);
	talloc_free(dn);
	if (!msg->dn) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = ldb_add(ldb_ctx, msg);
	if (ret != 0) {
		OC_DEBUG(0, "* Failed to modify record %s: %s",
			  ldb_dn_get_linearized(msg->dn), ldb_errstring(ldb_ctx));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}


/**
   \details Add a message record to the TDB store

   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer to the LDB context
   \param message pointer to the mpm_message entry with the folder and
   message ID

   \return NT_STATUS_OK on success, otherwise a NT error
 */
NTSTATUS mpm_cache_ldb_add_message(TALLOC_CTX *mem_ctx, 
				   struct ldb_context *ldb_ctx, 
				   struct mpm_message *message)
{
	NTSTATUS	       	status;
	struct ldb_message     	*msg;
	struct ldb_dn	       	*dn;
	struct ldb_result      	*res;
	char		       	*basedn;
	int		       	ret;

	/* First check if the CN=Folder,CN=Cache entry exists */
	basedn = talloc_asprintf(mem_ctx, "CN=0x%"PRIx64",CN=Cache", message->FolderId);
	dn = ldb_dn_new(mem_ctx, ldb_ctx, basedn);
	talloc_free(basedn);
	if (!dn) return NT_STATUS_UNSUCCESSFUL;
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret == LDB_SUCCESS && !res->count) {
		OC_DEBUG(5, "* We have to create folder TDB record: CN=0x%"PRIx64",CN=Cache",
			  message->FolderId);
		status = mpm_cache_ldb_add_folder(mem_ctx, ldb_ctx, message->FolderId);
		if (!NT_STATUS_IS_OK(status)) return status;
	}

	/* Search if the message doesn't already exist */
	basedn = talloc_asprintf(mem_ctx, "CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache", 
				 message->MessageId, message->FolderId);
	dn = ldb_dn_new(mem_ctx, ldb_ctx, basedn);
	talloc_free(basedn);
	if (!dn) return NT_STATUS_UNSUCCESSFUL;
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (res->count) return NT_STATUS_OK;

	/* Create the CN=Message,CN=Folder,CN=Cache */
	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) return NT_STATUS_NO_MEMORY;

	basedn = talloc_asprintf(mem_ctx, "CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache", 
				 message->MessageId, message->FolderId);
	msg->dn = ldb_dn_new(ldb_ctx, ldb_ctx, basedn);
	talloc_free(basedn);
	if (!msg->dn) return NT_STATUS_NO_MEMORY;

	ret = ldb_add(ldb_ctx, msg);
	if (ret != 0) {
		OC_DEBUG(0, "* Failed to modify record %s: %s",
			  ldb_dn_get_linearized(msg->dn),
			  ldb_errstring(ldb_ctx));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}


/**
   \details Add an attachment record to the TDB store
 
   \param mem_ctx pointer to the memory context
   \param ldb_ctx pointer to the LDB context
   \param attach pointer to the mpm_attachment entry

   \return NT_STATUS_OK on success, otherwise a NT error
*/
NTSTATUS mpm_cache_ldb_add_attachment(TALLOC_CTX *mem_ctx,
				      struct ldb_context *ldb_ctx,
				      struct mpm_attachment *attach)
{
	struct mpm_message	*message;
	struct ldb_message	*msg;
	struct ldb_dn		*dn;
	struct ldb_result	*res;
	char			*basedn;
	int			ret;

	message = attach->message;

	/* Search if the attachment doesn't already exist */
	basedn = talloc_asprintf(mem_ctx, "CN=%d,CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache",
				 attach->AttachmentID, message->MessageId, 
				 message->FolderId);
	dn = ldb_dn_new(mem_ctx, ldb_ctx, basedn);
	talloc_free(basedn);
	if (!dn) return NT_STATUS_UNSUCCESSFUL;
	ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret == LDB_SUCCESS && res->count) return NT_STATUS_OK;

	OC_DEBUG(2, "* Create the attachment TDB record");

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) return NT_STATUS_NO_MEMORY;
	
	basedn = talloc_asprintf(mem_ctx, "CN=%d,CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache",
				 attach->AttachmentID, message->MessageId, 
				 message->FolderId);
	msg->dn = ldb_dn_new(ldb_ctx, ldb_ctx, basedn);
	talloc_free(basedn);
	if (!msg->dn) return NT_STATUS_NO_MEMORY;

	ret = ldb_add(ldb_ctx, msg);
	if (ret != 0) {
		OC_DEBUG(0, "* Failed to modify record %s: %s",
			  ldb_dn_get_linearized(msg->dn), 
			  ldb_errstring(ldb_ctx));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}


/**
   \details Add stream references to a message or attachment in the
   TDB store

   \param mpm pointer to the cache module general structure
   \param ldb_ctx pointer to the LDB context
   \param stream pointer to the mpm_stream entry

   \return NT_STATUS_OK on success, otherwise NT error
 */
NTSTATUS mpm_cache_ldb_add_stream(struct mpm_cache *mpm, 
				  struct ldb_context *ldb_ctx,
				  struct mpm_stream *stream)
{
	TALLOC_CTX		*mem_ctx;
	struct mpm_message	*message;
	struct mpm_attachment	*attach;
	struct ldb_message	*msg;
	struct ldb_dn		*dn;
	const char * const	attrs[] = { "*", NULL };
	struct ldb_result	*res;
	char			*basedn = NULL;
	char			*attribute;
	int			ret;
	uint32_t		i;

	mem_ctx = (TALLOC_CTX *) mpm;
	
	if (stream->attachment) {
		attach = stream->attachment;
		message = attach->message;
	} else if (stream->message) {
		attach = NULL;
		message = stream->message;
	} else {
		return NT_STATUS_OK;
	}

	/* This is a stream for an attachment */
	if (stream->attachment) {
		basedn = talloc_asprintf(mem_ctx, "CN=%d,CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache",
					 attach->AttachmentID, message->MessageId,
					 message->FolderId);
		dn = ldb_dn_new(mem_ctx, ldb_ctx, basedn);
		talloc_free(basedn);
		if (!dn) return NT_STATUS_UNSUCCESSFUL;
		
		ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_BASE, attrs, 
				 "(0x%x=*)", stream->PropertyTag);

		if (ret == LDB_SUCCESS && res->count == 1) {
			attribute = talloc_asprintf(mem_ctx, "0x%x", stream->PropertyTag);
			basedn = (char *) ldb_msg_find_attr_as_string(res->msgs[0], attribute, NULL);
			talloc_free(attribute);
			OC_DEBUG(2, "* Loading from cache 0x%x = %s", stream->PropertyTag, basedn);
			stream->filename = talloc_strdup(mem_ctx, basedn);
			stream->cached = true;
			stream->ahead = false;
			mpm_cache_stream_open(mpm, stream);

			return NT_STATUS_OK;
		}

		/* Otherwise create the stream with basedn above */
		basedn = talloc_asprintf(mem_ctx, "CN=%d,CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache",
					 attach->AttachmentID, message->MessageId,
					 message->FolderId);

		OC_DEBUG(2, "* Create the stream TDB record for attachment");
	} 

	if (stream->message) {
		basedn = talloc_asprintf(mem_ctx, "CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache",
					 message->MessageId, message->FolderId);
		dn = ldb_dn_new(mem_ctx, ldb_ctx, basedn);
		talloc_free(basedn);
		if (!dn) return NT_STATUS_UNSUCCESSFUL;

		ret = ldb_search(ldb_ctx, mem_ctx, &res, dn, LDB_SCOPE_BASE, attrs, "(0x%x=*)", stream->PropertyTag);

		if (ret == LDB_SUCCESS && res->count == 1) {
			attribute = talloc_asprintf(mem_ctx, "0x%x", stream->PropertyTag);
			basedn = (char *) ldb_msg_find_attr_as_string(res->msgs[0], attribute, NULL);
			talloc_free(attribute);
			OC_DEBUG(2, "* Loading from cache 0x%x = %s", stream->PropertyTag, basedn);
			stream->filename = talloc_strdup(mem_ctx, basedn);
			stream->cached = true;
			stream->ahead = false;
			mpm_cache_stream_open(mpm, stream);

			return NT_STATUS_OK;
		}

		/* Otherwise create the stream with basedn above */
		basedn = talloc_asprintf(mem_ctx, "CN=0x%"PRIx64",CN=0x%"PRIx64",CN=Cache",
					 message->MessageId, message->FolderId);
		
		OC_DEBUG(2, "* Modify the message TDB record and append stream information");
	}

	stream->cached = false;
	mpm_cache_stream_open(mpm, stream);

	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) return NT_STATUS_NO_MEMORY;

	msg->dn = ldb_dn_new(ldb_ctx, ldb_ctx, basedn);
	talloc_free(basedn);
	if (!msg->dn) return NT_STATUS_NO_MEMORY;
	
	attribute = talloc_asprintf(mem_ctx, "0x%x", stream->PropertyTag);
	ldb_msg_add_fmt(msg, attribute, "%s", stream->filename);
	talloc_free(attribute);
	
	attribute = talloc_asprintf(mem_ctx, "0x%x_StreamSize", stream->PropertyTag);
	ldb_msg_add_fmt(msg, attribute, "%d", stream->StreamSize);
	talloc_free(attribute);

	/* mark all the message elements as LDB_FLAG_MOD_REPLACE */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}
	
	ret = ldb_modify(ldb_ctx, msg);
	if (ret != 0) {
		OC_DEBUG(0, "* Failed to modify record %s: %s",
			  ldb_dn_get_linearized(msg->dn), 
			  ldb_errstring(ldb_ctx));
		return NT_STATUS_UNSUCCESSFUL;
	}

	return NT_STATUS_OK;
}
