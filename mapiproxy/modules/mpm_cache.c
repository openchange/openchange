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
   \file mpm_cache.c
   
   \brief Cache messages and attachments so we can reduce WAN traffic
 */

#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/modules/mpm_cache.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

struct mpm_cache *mpm = NULL;

/**
   \details Find the position of the given MAPI call in a serialized
   MAPI request.

   If the request includes a Release call, then request and replies
   indexes for other calls will mismatch.

   \param opnum The MAPI opnum to seek
   \param mapi_req Pointer to the MAPI request calls array

   \return On success, returns the call position, otherwise -1.
 */
static uint32_t cache_find_call_request_index(uint8_t opnum, struct EcDoRpc_MAPI_REQ *mapi_req)
{
	uint32_t	i;

	for (i = 0; mapi_req[i].opnum; i++) {
		if (mapi_req[i].opnum == opnum) {
			return i;
		}
	}

	return -1;
}

/**
   \details Dump time statistic between OpenStream and Release

   This function monitors the effective time required to open, read
   and close a stream.

   \param stream the mpm_stream entry
 */
static void cache_dump_stream_stat(struct mpm_stream *stream)
{
	TALLOC_CTX	*mem_ctx;
	struct timeval	tv_end;
	uint64_t       	sec;
	uint64_t       	usec;
	char		*name;
	const char	*stage;

	mem_ctx = (TALLOC_CTX *)mpm;

	if (stream->attachment) {
		name = talloc_asprintf(mem_ctx, "0x%"PRIx64"/0x%"PRIx64"/%d",
				       stream->attachment->message->FolderId,
				       stream->attachment->message->MessageId,
				       stream->attachment->AttachmentID);
	} else if (stream->message) {
		name = talloc_asprintf(mem_ctx, "0x%"PRIx64"/0x%"PRIx64,
				       stream->message->FolderId,
				       stream->message->MessageId);
	} else {
		return;
	}

	gettimeofday(&tv_end, NULL);
	sec = tv_end.tv_sec - stream->tv_start.tv_sec;
	if ((tv_end.tv_usec - stream->tv_start.tv_usec) < 0) {
		sec -= 1;
		usec = tv_end.tv_usec + stream->tv_start.tv_usec;
		while (usec > 1000000) {
			usec -= 1000000;
			sec += 1;
		}
	} else {
		usec = tv_end.tv_usec - stream->tv_start.tv_usec;
	}

	if (stream->ahead == true) {
		stage = "[read ahead]";
	} else if ((stream->ahead == false) && (stream->cached == true)) {
		stage = "[cached mode]";
	} else {
		stage = "[non cached]";
	}

	OC_DEBUG(1, "STATISTIC: %-20s %s The difference is %ld seconds %ld microseconds",
		  stage, name, (long int)sec, (long int)usec);
	talloc_free(name);
}


/**
   \details

   1. close the existing FILE *
   2. build complete file path
   3. replace __FILE__ arguments with complete file path
   4. call execve
   5. stat the sync'd file
   6. open the stream again
   7. mark the file as cached

   \param stream pointer on the mpm_stream entry
 */
static NTSTATUS cache_exec_sync_cmd(struct mpm_stream *stream)
{
	uint32_t	i;
	int		ret = 0;
	char		**args;
	struct stat	sb;
	pid_t		pid;
	int		status;

	mpm_cache_stream_close(stream);

	for (i = 0; mpm->sync_cmd[i]; i++);

	args = talloc_array((TALLOC_CTX *)mpm, char *, i + 1);

	for (i = 0; mpm->sync_cmd[i]; i++){
		if (strstr(mpm->sync_cmd[i], "__FILE__")) {
			args[i] = string_sub_talloc((TALLOC_CTX *)args, mpm->sync_cmd[i], "__FILE__", stream->filename);
		} else {
			args[i] = talloc_strdup((TALLOC_CTX *)args, mpm->sync_cmd[i]);
		}
	}
	args[i] = NULL;

	for (i = 0; args[i]; i++){
		OC_DEBUG(0, "'%s' ", args[i]);
	}
	OC_DEBUG(0, "\n");

	switch(pid = fork()) {
	case -1:
		OC_DEBUG(0, "Failed to fork\n");
		break;
	case 0:
		ret = execve(args[0], args, NULL);
		break;
	default:
		wait(&status);
		break;
	}
	talloc_free(args);
	if (ret == -1) {
		perror("execve: ");
		return NT_STATUS_INVALID_PARAMETER;
	}

	ret = stat(stream->filename, &sb);
	if (ret == -1) {
		perror("stat: ");
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (sb.st_size != stream->StreamSize) {
		OC_DEBUG(0, "Sync'd file size is 0x%x and 0x%x was expected\n",
			  (uint32_t)sb.st_size, stream->StreamSize);
		return NT_STATUS_INVALID_PARAMETER;
	}

	mpm_cache_stream_open(mpm, stream);
	stream->cached = true;

	return NT_STATUS_OK;
}


/**
   \details Track down Release calls and update the mpm_cache global
   list - removing associated entries.

   This function recursively remove child entries whenever necessary.

   \param dce_call pointer to the session context
   \param EcDoRpc pointer to the EcDoRpc operation
   \param handle_idx the handle to track down

   \return NT_STATUS_OK
 */
static NTSTATUS cache_pull_Release(struct dcesrv_call_state *dce_call,
				   struct EcDoRpc *EcDoRpc, 
				   uint32_t handle_idx)
{
	struct mpm_message		*message;
	struct mpm_attachment		*attach;
	struct mpm_stream		*stream;
	char				*server_id_printable = NULL;

	/* Look over messages */
	for (message = mpm->messages; message; message = message->next) {
		if ((mpm_session_cmp(message->session, dce_call) == true) &&
		    (EcDoRpc->in.mapi_request->handles[handle_idx] == message->handle)) {
		        server_id_printable = server_id_str(NULL, &(message->session->server_id));
			OC_DEBUG(2, "* [s(%s),c(0x%x)] Del: Message 0x%"PRIx64" 0x%"PRIx64": 0x%x",
				  server_id_printable, message->session->context_id,
				  message->FolderId, message->MessageId, message->handle);
			talloc_free(server_id_printable);

			/* Loop over children attachments */
			attach = mpm->attachments;
			while (attach) {
				if ((mpm_session_cmp(attach->session, dce_call) == true) &&
				    (message->handle == attach->parent_handle)) {
				        server_id_printable = server_id_str(NULL, &(attach->session->server_id));
					OC_DEBUG(2, "* [s(%s),c(0x%x)] Del recursive 1: Attachment %d: 0x%x",
						  server_id_printable, attach->session->context_id, attach->AttachmentID, attach->handle);
					talloc_free(server_id_printable);

					/* Loop over children streams */
					stream = mpm->streams;
					while (stream) {
						if ((mpm_session_cmp(stream->session, dce_call) == true) &&
						    (attach->handle == stream->parent_handle)) {
						        server_id_printable = server_id_str(NULL, &(stream->session->server_id));
							OC_DEBUG(2, "* [s(%s),c(0x%x)] Del recursive 1-2: Stream 0x%x",
								  server_id_printable, stream->session->context_id, stream->handle);
							talloc_free(server_id_printable);
							mpm_session_release(stream->session);
							mpm_cache_stream_close(stream);
							talloc_free(stream->filename);
							DLIST_REMOVE(mpm->streams, stream);
							talloc_free(stream);
							stream = mpm->streams;
						} else {
							stream = stream->next;
						}
					}

					mpm_session_release(attach->session);
					DLIST_REMOVE(mpm->attachments, attach);
					talloc_free(attach);
					attach = mpm->attachments;
				} else {
					attach = attach->next;
				}
			}

			/* Look over children streams */
			stream = mpm->streams;
			while (stream) {
				if ((mpm_session_cmp(stream->session, dce_call) == true) &&
				    (message->handle == stream->parent_handle)) {
					server_id_printable = server_id_str(NULL, &(stream->session->server_id));
					OC_DEBUG(2, "* [s(%s),c(0x%x)] Del recursive 1: Stream 0x%x",
						  server_id_printable, stream->session->context_id, stream->handle);
					talloc_free(server_id_printable);
					mpm_session_release(stream->session);
					mpm_cache_stream_close(stream);
					DLIST_REMOVE(mpm->streams, stream);
					talloc_free(stream->filename);
					talloc_free(stream);
					stream = mpm->streams;
				} else {
					stream = stream->next;
				}
			}

			mpm_session_release(message->session);
			DLIST_REMOVE(mpm->messages, message);
			talloc_free(message);
			return NT_STATUS_OK;
		}
	}

 	/* Look over attachments */
	for (attach = mpm->attachments; attach; attach = attach->next) {
		if ((mpm_session_cmp(attach->session, dce_call) == true) &&
		    (EcDoRpc->in.mapi_request->handles[handle_idx] == attach->handle)) {
			server_id_printable = server_id_str(NULL, &(attach->session->server_id));
			OC_DEBUG(2, "* [s(%s),c(0x%x)] Del: Attachment %d: 0x%x",
				  server_id_printable, attach->session->context_id, attach->AttachmentID, attach->handle);
			talloc_free(server_id_printable);

			/* Loop over children streams */
			stream = mpm->streams;
			while (stream) {
				if ((mpm_session_cmp(stream->session, dce_call) == true) &&
				    (attach->handle == stream->parent_handle)) {
					server_id_printable = server_id_str(NULL, &(stream->session->server_id));
					OC_DEBUG(2, "* [s(%s),c(0x%x)] Del recursive 2: Stream 0x%x",
						  server_id_printable, stream->session->context_id, stream->handle);
					talloc_free(server_id_printable);
					mpm_session_release(stream->session);
					mpm_cache_stream_close(stream);
					DLIST_REMOVE(mpm->streams, stream);
					talloc_free(stream->filename);
					talloc_free(stream);
					stream = mpm->streams;
				} else {
					stream = stream->next;
				}
			}

			mpm_session_release(attach->session);
			DLIST_REMOVE(mpm->attachments, attach);
			talloc_free(attach);
			return NT_STATUS_OK;
		}
	}

	/* Look over streams */
	for (stream = mpm->streams; stream; stream = stream->next) {
		if ((mpm_session_cmp(stream->session, dce_call) == true) &&
		    (EcDoRpc->in.mapi_request->handles[handle_idx] == stream->handle)) {
			server_id_printable = server_id_str(NULL, &(stream->session->server_id));
			OC_DEBUG(2, "* [s(%s),c(0x%x)] Del: Stream 0x%x\n",
				  server_id_printable, stream->session->context_id, stream->handle);
			talloc_free(server_id_printable);
			mpm_session_release(stream->session);
			mpm_cache_stream_close(stream);
			DLIST_REMOVE(mpm->streams, stream);
			talloc_free(stream->filename);
			talloc_free(stream);
			return NT_STATUS_OK;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Monitor OpenMessage requests and register a message in the
   mpm_messages list.

   This is the first step for message registration: 
   * set Folder ID and Message ID
   * set the handle to 0xFFFFFFFF
   * Insert the message to the list

   \param dce_call pointer to the session context
   \param mem_ctx the memory context
   \param request reference to the OpenMessage request

   \return NT_STATUS_OK on success
 */
static NTSTATUS cache_pull_OpenMessage(struct dcesrv_call_state *dce_call,
				       TALLOC_CTX *mem_ctx, 
				       struct OpenMessage_req request)
{
	struct mpm_message		*message;

	/* Check if the message has already been registered */
	for (message = mpm->messages; message; message = message->next) {
		if ((mpm_session_cmp(message->session, dce_call) == true) &&
		    (request.FolderId == message->FolderId) &&
		    (request.MessageId == message->MessageId)) {
			DLIST_REMOVE(mpm->messages, message);
		}
	}

	message = talloc((TALLOC_CTX *)mpm, struct mpm_message);
	NT_STATUS_HAVE_NO_MEMORY(message);
	
	message->session = mpm_session_init((TALLOC_CTX *)mpm, dce_call);
	NT_STATUS_HAVE_NO_MEMORY(message->session);

	message->FolderId = request.FolderId;
	message->MessageId = request.MessageId;
	message->handle = 0xFFFFFFFF;
	
	DLIST_ADD_END(mpm->messages, message, struct mpm_message *);

	return NT_STATUS_OK;
}


/**
   \details Monitor OpenMessage replies and store OpenMessage MAPI
   handle.

   This is the second step for message registration:

   * Seek for a given FolderId/MessageId in the mpm_message list

   * If a match is found (expected) and MAPI retval is set to
     MAPI_E_SUCCESS, update the handle field for the element and
     commit this message in the tdb store.
     
   * If retval is different from MAPI_E_SUCCESS, then delete the
     record


   \param dce_call pointer to the session context
   \param mapi_req reference to the OpenMessage MAPI request entry
   \param mapi_repl reference to the OpenMessage MAPI response entry
   \param EcDoRpc pointer to the current EcDoRpc operation

   \return NT_STATUS_OK
 */
static NTSTATUS cache_push_OpenMessage(struct dcesrv_call_state *dce_call,
				       struct EcDoRpc_MAPI_REQ mapi_req, 
				       struct EcDoRpc_MAPI_REPL mapi_repl, 
				       struct EcDoRpc *EcDoRpc)
{
	struct mpm_message	*el;
	struct mapi_response	*mapi_response;
	struct OpenMessage_req	request;
	char *server_id_printable = NULL;

	request = mapi_req.u.mapi_OpenMessage;

	mapi_response = EcDoRpc->out.mapi_response;

	for (el = mpm->messages; el; el = el->next) {
		if ((el->FolderId == request.FolderId) && (el->MessageId == request.MessageId) &&
		    (mpm_session_cmp(el->session, dce_call) == true)) {
			if (mapi_repl.error_code == MAPI_E_SUCCESS) {
				mpm_cache_ldb_add_message((TALLOC_CTX *)mpm, mpm->ldb_ctx, el);
				el->handle = mapi_response->handles[request.handle_idx];
				server_id_printable = server_id_str(NULL, &(el->session->server_id));
				OC_DEBUG(2, "* [s(%s),c(0x%x)] Add: Message 0x%"PRIx64" 0x%"PRIx64" 0x%x",
					  server_id_printable, el->session->context_id, el->FolderId,
					  el->MessageId, el->handle);
				talloc_free(server_id_printable);
			} else {
				server_id_printable = server_id_str(NULL, &(el->session->server_id));
				OC_DEBUG(0, "* [s(%s),c(0x%x)] Del: Message OpenMessage returned %s",
					  server_id_printable, el->session->context_id,
				          mapi_get_errstr(mapi_repl.error_code));
				talloc_free(server_id_printable);
				DLIST_REMOVE(mpm->messages, el);
			}
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_OK;
}


/**
   \details Monitor OpenAttach requests and register an attachment in
   the mpm_messages list.

   This is the first step for attachment registration.  This
   function first ensures the attachment is not already registered,
   otherwise delete it. It next creates the attachment entry in the
   global mpm_message structure.

   \param dce_call pointer to the session context
   \param mem_ctx the memory context
   \param mapi_req reference to the OpenAttach EcDoRpc_MAPI_REQ entry
   \param EcDoRpc pointer to the EcDoRpc operation

   \return NT_STATUS_OK on success, otherwise NT_STATUS_NO_MEMORY
 */
static NTSTATUS cache_pull_OpenAttach(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx, 
				      struct EcDoRpc_MAPI_REQ mapi_req, 
				      struct EcDoRpc *EcDoRpc)
{
	struct mpm_message	*el;
	struct mpm_attachment	*attach;
	struct mapi_request	*mapi_request;
	struct OpenAttach_req	request;
	char 			*server_id_printable = NULL;

	mapi_request = EcDoRpc->in.mapi_request;
	request = mapi_req.u.mapi_OpenAttach;

	for (attach = mpm->attachments; attach; attach = attach->next) {
		/* Check if the attachment has already been registered */
		if ((mpm_session_cmp(attach->session, dce_call) == true) &&
		    (mapi_request->handles[mapi_req.handle_idx] == attach->parent_handle) && (request.AttachmentID == attach->AttachmentID)) {
			DLIST_REMOVE(mpm->attachments, attach);
		}
	}

	attach = talloc((TALLOC_CTX *)mpm, struct mpm_attachment);
	NT_STATUS_HAVE_NO_MEMORY(attach);

	attach->session = mpm_session_init((TALLOC_CTX *)mpm, dce_call);
	NT_STATUS_HAVE_NO_MEMORY(attach->session);

	attach->AttachmentID = request.AttachmentID;
	attach->parent_handle = mapi_request->handles[mapi_req.handle_idx];
	attach->handle = 0xFFFFFFFF;

	for (el = mpm->messages; el; el = el->next) {
		if ((mpm_session_cmp(el->session, dce_call) == true) && attach->parent_handle == el->handle) {
			attach->message = el;
			break;
		}
	}

	server_id_printable = server_id_str(NULL, &(attach->session->server_id));
	OC_DEBUG(2, "* [s(%s),c(0x%x)] Add [1]: Attachment %d  parent handle (0x%x) 0x%"PRIx64", 0x%"PRIx64" added to the list",
		  server_id_printable, attach->session->context_id, request.AttachmentID, attach->parent_handle, 
		  attach->message->FolderId, attach->message->MessageId);
	talloc_free(server_id_printable);
	DLIST_ADD_END(mpm->attachments, attach, struct mpm_attachment *);
	
	return NT_STATUS_OK;
}


/**
   \details Monitor OpenAttach replies and store OpenAttach MAPI
   handle.

   This is the second step for attachment registration:

   * Seek for a given parent_handle/attachmentID in the
     mpm_attachments list.

   * if a match is found (expected) and MAPI retval is set to
     MAPI_E_SUCCESS, update the handle parameter for the element and
     commit this attachment in the tdb store.

   * If retval is different from MAPI_E_SUCCESS, then delete the
     element.
     
   \param dce_call pointer to the session context
   \param mapi_req reference to the OpenAttach request entry
   \param mapi_repl reference to the OpenAttach MAPI response entry
   \param EcDoRpc pointer to the current EcDoRpc operation

   \return NT_STATUS_OK

 */
static NTSTATUS cache_push_OpenAttach(struct dcesrv_call_state *dce_call,
				      struct EcDoRpc_MAPI_REQ mapi_req, 
				      struct EcDoRpc_MAPI_REPL mapi_repl, 
				      struct EcDoRpc *EcDoRpc)
{
	struct mpm_attachment		*el;
	struct mapi_request		*mapi_request;
	struct mapi_response		*mapi_response;
	struct OpenAttach_req		request;
	char				*server_id_printable = NULL;

	mapi_request = EcDoRpc->in.mapi_request;
	mapi_response = EcDoRpc->out.mapi_response;
	request = mapi_req.u.mapi_OpenAttach;

	for (el = mpm->attachments; el; el = el->next) {
		if ((mpm_session_cmp(el->session, dce_call) == true) &&
		    (mapi_request->handles[mapi_req.handle_idx] == el->parent_handle) &&
		    (request.AttachmentID == el->AttachmentID)) {
			if (mapi_repl.error_code == MAPI_E_SUCCESS) {
				el->handle = mapi_response->handles[request.handle_idx];
				server_id_printable = server_id_str(NULL, &(el->session->server_id));
				OC_DEBUG(2, "* [s(%s),c(0x%x)] Add [2]: Attachment %d with handle 0x%x and parent handle 0x%x",
					  server_id_printable, el->session->context_id, el->AttachmentID, el->handle,
					  el->parent_handle);
				talloc_free(server_id_printable);
				mpm_cache_ldb_add_attachment((TALLOC_CTX *)mpm, mpm->ldb_ctx, el);
			} else {
			        server_id_printable = server_id_str(NULL, &(el->session->server_id));
				OC_DEBUG(0, "* [s(%s),c(0x%x)] Del: Attachment OpenAttach returned %s",
					  server_id_printable, el->session->context_id,
					  mapi_get_errstr(mapi_repl.error_code));
				talloc_free(server_id_printable);
				DLIST_REMOVE(mpm->attachments, el);
			}
			return NT_STATUS_OK;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Monitor OpenStream requests and register a stream in the
   mpm_streams list.

   We are only interested in monitoring streams related to attachments
   or messages. This is the first step for stream registration:

   * Look whether this stream inherits from a message or attachment
   * Fill the stream element according to previous statement
   * Add it to the mpm_stream list

   \param dce_call pointer to the session context
   \param mem_ctx the memory context
   \param mapi_req reference to the OpenStream MAPI request
   \param EcDoRpc pointer to the current EcDoRpc operation
   
   \return NT_STATUS_OK on success, otherwise NT_STATUS_NO_MEMORY
 */
static NTSTATUS cache_pull_OpenStream(struct dcesrv_call_state *dce_call,
				      TALLOC_CTX *mem_ctx, 
				      struct EcDoRpc_MAPI_REQ mapi_req, 
				      struct EcDoRpc *EcDoRpc)
{
	struct mpm_stream	*stream;
	struct mpm_attachment	*attach;
	struct mpm_message	*message;
	struct mapi_request	*mapi_request;
	struct OpenStream_req	request;
	char			*server_id_printable = NULL;

	mapi_request = EcDoRpc->in.mapi_request;
	request = mapi_req.u.mapi_OpenStream;

	for (attach = mpm->attachments; attach; attach = attach->next) {
		if ((mpm_session_cmp(attach->session, dce_call) == true) &&
		    mapi_request->handles[mapi_req.handle_idx] == attach->handle) {
			stream = talloc((TALLOC_CTX *)mpm, struct mpm_stream);
			NT_STATUS_HAVE_NO_MEMORY(stream);

			stream->session = mpm_session_init((TALLOC_CTX *)mpm, dce_call);
			NT_STATUS_HAVE_NO_MEMORY(stream->session);

			stream->handle = 0xFFFFFFFF;
			stream->parent_handle = attach->handle;
			stream->PropertyTag = request.PropertyTag;
			stream->StreamSize = 0;
			stream->filename = NULL;
			stream->attachment = attach;
			stream->cached = false;
			stream->message = NULL;
			stream->ahead = (mpm->ahead == true) ? true : false;
			gettimeofday(&stream->tv_start, NULL);
			server_id_printable = server_id_str(NULL, &(stream->session->server_id));
			OC_DEBUG(2, "* [s(%s),c(0x%x)] Stream::attachment added 0x%x 0x%"PRIx64" 0x%"PRIx64,
				  server_id_printable, stream->session->context_id, stream->parent_handle, 
				  stream->attachment->message->FolderId, stream->attachment->message->MessageId);
			talloc_free(server_id_printable);
			DLIST_ADD_END(mpm->streams, stream, struct mpm_stream *);
			return NT_STATUS_OK;
		}
	}

	for (message = mpm->messages; message; message = message->next) {
		if ((mpm_session_cmp(message->session, dce_call) == true) &&
		    mapi_request->handles[mapi_req.handle_idx] == message->handle) {
			stream = talloc((TALLOC_CTX *)mpm, struct mpm_stream);
			NT_STATUS_HAVE_NO_MEMORY(stream);

			stream->session = mpm_session_init((TALLOC_CTX *)mpm, dce_call);
			NT_STATUS_HAVE_NO_MEMORY(stream->session);

			stream->handle = 0xFFFFFFFF;
			stream->parent_handle = message->handle;
			stream->PropertyTag = request.PropertyTag;
			stream->StreamSize = 0;
			stream->filename = NULL;
			stream->attachment = NULL;
			stream->cached = false;
			stream->ahead = (mpm->ahead == true) ? true : false;
			gettimeofday(&stream->tv_start, NULL);
			server_id_printable = server_id_str(NULL, &(stream->session->server_id));
			OC_DEBUG(2, "* [s(%s),c(0x%x)] Stream::message added 0x%x",
				  server_id_printable, stream->session->context_id, stream->parent_handle);
			talloc_free(server_id_printable);
			stream->message = message;
			DLIST_ADD_END(mpm->streams, stream, struct mpm_stream *);
			return NT_STATUS_OK;
		}
	}

	OC_DEBUG(1, "* Stream: Not related to any attachment or message ?!?");
	return NT_STATUS_OK;
}


/**
   \details Monitor OpenStream replies and store the OpenStream MAPI
   handle.

   This is the second step for stream registration:

   * Seek the parent_handle/PropertyTag couple in the mpm_streams
     list.

   * If a match is found (expected) and MAPI retval is set to
     MAPI_E_SUCCESS, update the handle field and StreamSize parameters
     for the element and commit this stream in the tdb store.

   * If retval is different from MAPI_E_SUCCESS, then delete the
   * element.

   \param dce_call pointer to the session context
   \param mapi_req reference to the OpenStream MAPI request entry
   \param mapi_repl reference to the OpenStream MAPI response entry
   \param EcDoRpc pointer to the current EcDoRpc operation

   \return NT_STATUS_OK
 */
static NTSTATUS cache_push_OpenStream(struct dcesrv_call_state *dce_call,
				      struct EcDoRpc_MAPI_REQ mapi_req, 
				      struct EcDoRpc_MAPI_REPL mapi_repl, 
				      struct EcDoRpc *EcDoRpc)
{
	struct mpm_stream	*el;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct OpenStream_req	request;
	struct OpenStream_repl	response;
	char			*server_id_printable = NULL;

	mapi_request = EcDoRpc->in.mapi_request;
	mapi_response = EcDoRpc->out.mapi_response;
	request = mapi_req.u.mapi_OpenStream;
	response = mapi_repl.u.mapi_OpenStream;

	for (el = mpm->streams; el; el = el->next) {
		if ((mpm_session_cmp(el->session, dce_call) == true) &&
		    (mapi_request->handles[mapi_req.handle_idx] == el->parent_handle)) {
			if (request.PropertyTag == el->PropertyTag) {
				if (mapi_repl.error_code == MAPI_E_SUCCESS) {
					el->handle = mapi_response->handles[request.handle_idx];
					el->StreamSize = response.StreamSize;
					server_id_printable = server_id_str(NULL, &(el->session->server_id));
					OC_DEBUG(2, "* [s(%s),c(0x%x)] Add [2]: Stream for Property Tag 0x%x, handle 0x%x and size = %d",
						  server_id_printable, el->session->context_id, el->PropertyTag, el->handle,
						  el->StreamSize);
					talloc_free(server_id_printable);
					mpm_cache_ldb_add_stream(mpm, mpm->ldb_ctx, el);
				} else {
					server_id_printable = server_id_str(NULL, &(el->session->server_id));
					OC_DEBUG(0, "* [s(%s),c(0x%x)] Del: Stream OpenStream returned %s",
						  server_id_printable, el->session->context_id, mapi_get_errstr(mapi_repl.error_code));
					talloc_free(server_id_printable);
					DLIST_REMOVE(mpm->streams, el);
				}
				return NT_STATUS_OK;
			}
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Monitor ReadStream replies.

   This function writes ReadStream data received from remote server
   and associated to messages or attachments to the opened associated
   file. This function only writes data if the file is not already
   cached, otherwise it just returns.

   \param dce_call pointer to the session context
   \param mapi_req reference to the ReadStream MAPI request
   \param mapi_repl reference to the ReadStream MAPI reply
   \param EcDoRpc pointer to the current EcDoRpc operation

   \return NT_STATUS_OK

   \sa cache_dispatch
 */
static NTSTATUS cache_push_ReadStream(struct dcesrv_call_state *dce_call,
				      struct EcDoRpc_MAPI_REQ mapi_req,
				      struct EcDoRpc_MAPI_REPL mapi_repl, 
				      struct EcDoRpc *EcDoRpc)
{
	struct mpm_stream	*stream;
	struct mapi_response	*mapi_response;
	struct ReadStream_repl	response;
	/* struct ReadStream_req	request; */
	char			*server_id_printable = NULL;

	mapi_response = EcDoRpc->out.mapi_response;
	response = mapi_repl.u.mapi_ReadStream;
	/* request = mapi_req.u.mapi_ReadStream; */

	/* Check if the handle is registered */
	for (stream = mpm->streams; stream; stream = stream->next) {
		if ((mpm_session_cmp(stream->session, dce_call) == true) &&
		    mapi_response->handles[mapi_repl.handle_idx] == stream->handle) {
			if (stream->fp && stream->cached == false) {
				if (mpm->sync == true && stream->StreamSize > mpm->sync_min) {
					cache_exec_sync_cmd(stream);
				} else {
					server_id_printable = server_id_str(NULL, &(stream->session->server_id));
					OC_DEBUG(5, "* [s(%s),c(0x%x)] %zd bytes from remove server",
						  server_id_printable, stream->session->context_id, response.data.length);
					talloc_free(server_id_printable);
					mpm_cache_stream_write(stream, response.data.length, response.data.data);
					if (stream->offset == stream->StreamSize) {
						if (response.data.length) {
							cache_dump_stream_stat(stream);
						}
					}
				}
			} else if (stream->cached == true) {
				/* This is managed by the dispatch routine */
			}
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_OK;
}


/**
   \details Analyze EcDoRpc MAPI requests

   This function loops over EcDoRpc MAPI calls and search for the
   opnums required by the cache module to monitor Stream traffic
   properly.

   \param dce_call the session context
   \param mem_ctx the memory context
   \param r generic pointer on EcDoRpc operation

   \return NT_STATUS_OK
 */
static NTSTATUS cache_pull(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	struct EcDoRpc		*EcDoRpc;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	uint32_t		i;

	if (dce_call->pkt.u.request.opnum != 0x2) {
		return NT_STATUS_OK;
	}

	EcDoRpc = (struct EcDoRpc *) r;
	if (!EcDoRpc) return NT_STATUS_OK;
	if (!&(EcDoRpc->in)) return NT_STATUS_OK;
	if (!EcDoRpc->in.mapi_request) return NT_STATUS_OK;
	if (!EcDoRpc->in.mapi_request->mapi_req) return NT_STATUS_OK;

	/* If this is an idle request, do not go further */
	if (EcDoRpc->in.mapi_request->length == 2) {
		return NT_STATUS_OK;
	}

	mapi_req = EcDoRpc->in.mapi_request->mapi_req;
	for (i = 0; mapi_req[i].opnum; i++) {
		switch (mapi_req[i].opnum) {
		case op_MAPI_OpenMessage:
			cache_pull_OpenMessage(dce_call, (TALLOC_CTX *)mpm, mapi_req[i].u.mapi_OpenMessage);
			break;
		case op_MAPI_OpenAttach:
			cache_pull_OpenAttach(dce_call, (TALLOC_CTX *)mpm, mapi_req[i], EcDoRpc);
			break;
		case op_MAPI_OpenStream:
			cache_pull_OpenStream(dce_call, (TALLOC_CTX *)mpm, mapi_req[i], EcDoRpc);
			break;
		case op_MAPI_Release:
			cache_pull_Release(dce_call, EcDoRpc, mapi_req[i].handle_idx);
			break;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Analyze EcDoRpc MAPI responses

   This function loops over EcDoRpc MAPI calls and search for the
   opnums required by the cache module to monitor Stream traffic
   properly.

   \param dce_call pointer to the session context
   \param mem_ctx the memory context
   \param r generic pointer on EcDoRpc operation

   \return NT_STATUS_OK
 */
static NTSTATUS cache_push(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx, void *r)
{
	struct EcDoRpc			*EcDoRpc;
	struct EcDoRpc_MAPI_REPL	*mapi_repl;
	struct EcDoRpc_MAPI_REQ		*mapi_req;
	uint32_t			i;
	uint32_t			index;

	if (dce_call->pkt.u.request.opnum != 0x2) {
		return NT_STATUS_OK;
	}

	EcDoRpc = (struct EcDoRpc *) r;
	if (!EcDoRpc) return NT_STATUS_OK;
	if (!&(EcDoRpc->out)) return NT_STATUS_OK;
	if (!EcDoRpc->out.mapi_response) return NT_STATUS_OK;
	if (!EcDoRpc->out.mapi_response->mapi_repl) return NT_STATUS_OK;

	/* If this is an idle request, do not got further */
	if (EcDoRpc->out.mapi_response->length == 2) {
		return NT_STATUS_OK;
	}

	mapi_repl = EcDoRpc->out.mapi_response->mapi_repl;
	mapi_req = EcDoRpc->in.mapi_request->mapi_req;

	for (i = 0; mapi_repl[i].opnum; i++) {
		switch (mapi_repl[i].opnum) {
		case op_MAPI_OpenMessage:
			index = cache_find_call_request_index(op_MAPI_OpenMessage, mapi_req);
			if (index == -1) break;
			cache_push_OpenMessage(dce_call, mapi_req[index], mapi_repl[i], EcDoRpc);
			break;
		case op_MAPI_OpenAttach:
			index = cache_find_call_request_index(op_MAPI_OpenAttach, mapi_req);
			if (index == -1) break;
			cache_push_OpenAttach(dce_call, mapi_req[index], mapi_repl[i], EcDoRpc);
			break;
		case op_MAPI_OpenStream:
			index = cache_find_call_request_index(op_MAPI_OpenStream, mapi_req);
			if (index == -1) break;
			cache_push_OpenStream(dce_call, mapi_req[index], mapi_repl[i], EcDoRpc);
			break;
		case op_MAPI_ReadStream:
			index = cache_find_call_request_index(op_MAPI_ReadStream, mapi_req);
			if (index == -1) break;
			cache_push_ReadStream(dce_call, mapi_req[index], mapi_repl[i], EcDoRpc);
			break;
		default:
			break;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Dispatch function. 

   This function avoids calling dcerpc_ndr_request - understand
   forwarding client request to remove server - when the client is
   reading a message/attachment stream available in the cache.

   This function can also be used to loop over dcerpc_ndr_request and
   perform a read-ahead operation.

   \param dce_call the session context
   \param mem_ctx the memory context
   \param r pointer on EcDoRpc operation
   \param mapiproxy pointer to a mapiproxy structure controlling
   mapiproxy behavior.

   \return NT_STATUS_OK
 */
static NTSTATUS cache_dispatch(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
			       void *r, struct mapiproxy *mapiproxy)
{
	struct EcDoRpc		*EcDoRpc;
	struct mapi_request	*mapi_request;
	struct mapi_response	*mapi_response;
	struct EcDoRpc_MAPI_REQ	*mapi_req;
	struct mpm_stream	*stream;
	uint32_t		i;
	uint32_t		count;

	if (dce_call->pkt.u.request.opnum != 0x2) {
		return NT_STATUS_OK;
	}

	EcDoRpc = (struct EcDoRpc *) r;
	if (!EcDoRpc->in.mapi_request->mapi_req) return NT_STATUS_OK;

	/* If this is an idle request, do not got further */
	if (EcDoRpc->in.mapi_request->length == 2) {
		return NT_STATUS_OK;
	}

	mapi_request = EcDoRpc->in.mapi_request;
	mapi_response = EcDoRpc->out.mapi_response;
	mapi_req = mapi_request->mapi_req;
	
	for (count = 0, i = 0; mapi_req[i].opnum; i++) {
		switch (mapi_req[i].opnum) {
		case op_MAPI_ReadStream:
			count++;
			break;
		}
	}

	/* If we have more than count cached calls, forward to Exchange */
	if (i > count) return NT_STATUS_OK;

	for (i = 0; mapi_req[i].opnum; i++) {
		switch (mapi_req[i].opnum) {
		case op_MAPI_ReadStream:
		{
			struct ReadStream_req	request;

			request = mapi_req[i].u.mapi_ReadStream;
			for (stream = mpm->streams; stream; stream = stream->next) {
				if ((mpm_session_cmp(stream->session, dce_call) == true) &&
				    (mapi_request->handles[mapi_req[i].handle_idx] == stream->handle)) {
					if (stream->cached == true) {
					cached:
						mapiproxy->norelay = true;
						mapiproxy->ahead = false;
						/* Create a fake ReadStream reply */
						mapi_response->mapi_repl = talloc_array(mem_ctx, struct EcDoRpc_MAPI_REPL, i + 2);
						mapi_response->mapi_repl[i].opnum = op_MAPI_ReadStream;
						mapi_response->mapi_repl[i].handle_idx = mapi_req[i].handle_idx;
						mapi_response->mapi_repl[i].error_code = MAPI_E_SUCCESS;
						mapi_response->mapi_repl[i].u.mapi_ReadStream.data.length = 0;
						mapi_response->mapi_repl[i].u.mapi_ReadStream.data.data = talloc_size(mem_ctx, request.ByteCount);
						mpm_cache_stream_read(stream, (size_t) request.ByteCount, 
								      &mapi_response->mapi_repl[i].u.mapi_ReadStream.data.length,
								      &mapi_response->mapi_repl[i].u.mapi_ReadStream.data.data);
						if (stream->offset == stream->StreamSize) {
							if (mapi_response->mapi_repl[i].u.mapi_ReadStream.data.length) {
								cache_dump_stream_stat(stream);
							}
						}
						OC_DEBUG(5, "* %zd bytes read from cache",
							  mapi_response->mapi_repl[i].u.mapi_ReadStream.data.length);
						mapi_response->handles = talloc_array(mem_ctx, uint32_t, 1);
						mapi_response->handles[0] = stream->handle;
						mapi_response->mapi_len = 0xE + mapi_response->mapi_repl[i].u.mapi_ReadStream.data.length;
						mapi_response->length = mapi_response->mapi_len - 4;
						*EcDoRpc->out.length = mapi_response->mapi_len;
						EcDoRpc->out.size = EcDoRpc->in.size;
						break;
					} else if ((stream->cached == false) && (stream->ahead == true)) {
						if (mapiproxy->ahead == true)  {
							mpm_cache_stream_write(stream,
									       mapi_response->mapi_repl[i].u.mapi_ReadStream.data.length,
									       mapi_response->mapi_repl[i].u.mapi_ReadStream.data.data);
							/* When read ahead is over */
							if (stream->offset == stream->StreamSize) {
								cache_dump_stream_stat(stream);
								mpm_cache_stream_reset(stream);
								stream->cached = true;
								stream->ahead = false;
								goto cached;
							}
						} else {
							mapiproxy->ahead = true;
						}
					}
				}
			}
		}
		break;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details 
 */
static NTSTATUS cache_unbind(struct server_id server_id, uint32_t context_id)
{
	struct mpm_message	*message;
	struct mpm_attachment	*attach;
	struct mpm_stream	*stream;
	char			*server_id_printable = NULL;

	/* Look over messages still attached to the session */
	message = mpm->messages;
	while (message) {
		if ((mpm_session_cmp_sub(message->session, server_id, context_id) == true)) {
			server_id_printable = server_id_str(NULL, &(message->session->server_id));
			OC_DEBUG(2, "[s(%s),c(0x%x)] Message - 0x%"PRIx64"/0x%"PRIx64" handle(0x%x)",
				  server_id_printable, message->session->context_id,
				  message->FolderId, message->MessageId, message->handle);
			talloc_free(server_id_printable);
			mpm_session_release(message->session);
			DLIST_REMOVE(mpm->messages, message);
			talloc_free(message);
			message = mpm->messages;
		} else {
			message = message->next;
		}
	}

	/* Look over attachments still attached to the session */
	attach = mpm->attachments;
	while (attach) {
		if ((mpm_session_cmp_sub(attach->session, server_id, context_id) == true)) {
			server_id_printable = server_id_str(NULL, &(attach->session->server_id));
			OC_DEBUG(2, "[s(%s),c(0x%x)] Attachment - AttachmentID(0x%x) handle(0x%x)",
				  server_id_printable, attach->session->context_id,
				  attach->AttachmentID, attach->handle);
			talloc_free(server_id_printable);
			mpm_session_release(attach->session);
			DLIST_REMOVE(mpm->attachments, attach);
			talloc_free(attach);
			attach = mpm->attachments;
		} else {
			attach = attach->next;
		}
	}

	stream = mpm->streams;
	while (stream) {
		if ((mpm_session_cmp_sub(stream->session, server_id, context_id) == true)) {
			server_id_printable = server_id_str(NULL, &(stream->session->server_id));
			OC_DEBUG(2, "[s(%s),c(0x%x)] Stream - handle(0x%x)",
				  server_id_printable, stream->session->context_id, stream->handle);
			talloc_free(server_id_printable);
			mpm_session_release(stream->session);
			mpm_cache_stream_close(stream);
			talloc_free(stream->filename);
			DLIST_REMOVE(mpm->streams, stream);
			talloc_free(stream);
			stream = mpm->streams;
		} else {
			stream = stream->next;
		}
	}

	return NT_STATUS_OK;
}


/**
   \details Initialize the cache module and retrieve configuration from
   smb.conf

   Possible smb.conf parameters:
	* mpm_cache:database

   \param dce_ctx the session context

   \return NT_STATUS_OK on success otherwise
   NT_STATUS_INVALID_PARAMETER, NT_STATUS_NO_MEMORY
 */
static NTSTATUS cache_init(struct dcesrv_context *dce_ctx)
{
	char			*database;
	NTSTATUS		status;
	struct loadparm_context	*lp_ctx;

	mpm = talloc_zero(dce_ctx, struct mpm_cache);
	if (!mpm) return NT_STATUS_NO_MEMORY;
	mpm->messages = NULL;
	mpm->attachments = NULL;
	mpm->streams = NULL;

	mpm->ahead = lpcfg_parm_bool(dce_ctx->lp_ctx, NULL, MPM_NAME, "ahead", false);
	mpm->sync = lpcfg_parm_bool(dce_ctx->lp_ctx, NULL, MPM_NAME, "sync", false);
	mpm->sync_min = lpcfg_parm_int(dce_ctx->lp_ctx, NULL, MPM_NAME, "sync_min", 500000);
	mpm->sync_cmd = str_list_make(dce_ctx, lpcfg_parm_string(dce_ctx->lp_ctx, NULL, MPM_NAME, "sync_cmd"), " ");
	mpm->dbpath = lpcfg_parm_string(dce_ctx->lp_ctx, NULL, MPM_NAME, "path");

	if ((mpm->ahead == true) && mpm->sync) {
		OC_DEBUG(0, "%s: cache:ahead and cache:sync are exclusive!", MPM_ERROR);
		talloc_free(mpm);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!mpm->dbpath) {
		OC_DEBUG(0, "%s: Missing mpm_cache:path parameter", MPM_ERROR);
		talloc_free(mpm);
		return NT_STATUS_INVALID_PARAMETER;
	}

	database = talloc_asprintf(dce_ctx->lp_ctx, "tdb://%s/%s", mpm->dbpath, MPM_DB);
	status = mpm_cache_ldb_createdb(dce_ctx, database, &mpm->ldb_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(database);
		talloc_free(mpm);
		return NT_STATUS_NO_MEMORY;
	}

	lp_ctx = loadparm_init(dce_ctx);
	lpcfg_load_default(lp_ctx);
	dcerpc_init();

	talloc_free(database);

	return NT_STATUS_OK;
}


/**
   \details Entry point for the cache mapiproxy module

   \return NT_STATUS_OK on success, otherwise NTSTATUS error
 */
NTSTATUS samba_init_module(void)
{
	struct mapiproxy_module	module;
	NTSTATUS		ret;

	/* Fill in our name */
	module.name = "cache";
	module.description = "Cache MAPI messages and attachments";
	module.endpoint = "exchange_emsmdb";

	/* Fill in all the operations */
	module.init = cache_init;
	module.unbind = cache_unbind;
	module.push = cache_push;
	module.ndr_pull = NULL;
	module.pull = cache_pull;
	module.dispatch = cache_dispatch;

	/* Register ourselves with the MAPIPROXY subsystem */
	ret = mapiproxy_module_register(&module);
	if (!NT_STATUS_IS_OK(ret)) {
		OC_DEBUG(0, "Failed to register the 'cache' mapiproxy module!");
		return ret;
	}

	return ret;
}
