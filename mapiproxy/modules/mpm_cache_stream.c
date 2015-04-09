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
   \file mpm_cache_stream.c

   \brief Storage routines for the cache module
 */

#include "mapiproxy/dcesrv_mapiproxy.h"
#include "mapiproxy/libmapiproxy/libmapiproxy.h"
#include "mapiproxy/modules/mpm_cache.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>

/**
   \details Create a file: message or attachment in the cache

   If the stream is attached to an attachment:	FolderID/MessageID/AttachmentID.stream
   If the stream is attached to a message:	FolderID/MessageID.stream

   \param mpm pointer to the cache module general structure
   \param stream pointer to the mpm_stream entry

   \return Return a FILE pointer otherwise NULL
 */
NTSTATUS mpm_cache_stream_open(struct mpm_cache *mpm, struct mpm_stream *stream)
{
	TALLOC_CTX	*mem_ctx;
	char		*file;
	int		ret;

	mem_ctx = (TALLOC_CTX *) mpm;

	if (stream->filename) {
		stream->fp = fopen(stream->filename, "r");
		stream->offset = 0;
		return NT_STATUS_OK;
	}

	if (stream->message) {
		/* Create the folder */
		file = talloc_asprintf(mem_ctx, "%s/0x%"PRIx64, mpm->dbpath, stream->message->FolderId);
		ret = mkdir(file, 0777);
		talloc_free(file);
		if ((ret == -1) && (errno != EEXIST)) return NT_STATUS_UNSUCCESSFUL;

		/* Open the file */
		file = talloc_asprintf(mem_ctx, "%s/0x%"PRIx64"/0x%"PRIx64".stream",
				       mpm->dbpath, stream->message->FolderId,
				       stream->message->MessageId);

		OC_DEBUG(2, "* Opening Message stream %s", file);
		stream->filename = talloc_strdup(mem_ctx, file);
		stream->fp = fopen(file, "w+");
		stream->offset = 0;
		talloc_free(file);
		
		return NT_STATUS_OK;
	}

	if (stream->attachment) {
		/* Create the folders */
		file = talloc_asprintf(mem_ctx, "%s/0x%"PRIx64, mpm->dbpath, 
				       stream->attachment->message->FolderId);
		ret = mkdir(file, 0777);
		talloc_free(file);
		if ((ret == -1) && (errno != EEXIST)) return NT_STATUS_UNSUCCESSFUL;

		file = talloc_asprintf(mem_ctx, "%s/0x%"PRIx64"/0x%"PRIx64, mpm->dbpath,
				       stream->attachment->message->FolderId, 
				       stream->attachment->message->MessageId);
		ret = mkdir(file, 0777);
		talloc_free(file);
		if ((ret == -1) && (errno != EEXIST)) return NT_STATUS_UNSUCCESSFUL;

		file = talloc_asprintf(mem_ctx, "%s/0x%"PRIx64"/0x%"PRIx64"/%d.stream", 
				       mpm->dbpath,
				       stream->attachment->message->FolderId, 
				       stream->attachment->message->MessageId,
				       stream->attachment->AttachmentID);

		OC_DEBUG(2, "* Opening Attachment stream %s", file);
		stream->filename = talloc_strdup(mem_ctx, file);
		stream->fp = fopen(file, "w+");
		stream->offset = 0;
		talloc_free(file);

		return NT_STATUS_OK;
	}

	return NT_STATUS_OK;
}


/**
   \details Close the filesystem stream

   \param stream pointer to the mpm_stream entry

   \return NT_STATUS_OK on success, otherwise NT_STATUS_NOT_FOUND
 */
NTSTATUS mpm_cache_stream_close(struct mpm_stream *stream)
{
	if (stream && stream->fp) {
		fclose(stream->fp);
		stream->fp = NULL;
	} else {
		return NT_STATUS_NOT_FOUND;
	}

	return NT_STATUS_OK;
}


/**
   \details Read input_size bytes from a local binary stream

   \param stream pointer to the mpm_stream entry
   \param input_size the number of bytes to read
   \param length output pointer to the length effectively read from the
   stream
   \param data output pointer to the binary data read from the stream

   \return NT_STATUS_OK
 */
NTSTATUS mpm_cache_stream_read(struct mpm_stream *stream, size_t input_size, size_t *length, uint8_t **data)
{
	fseek(stream->fp, stream->offset, SEEK_SET);
	*length = fread(*data, sizeof (uint8_t), input_size, stream->fp);
	stream->offset += *length;
	OC_DEBUG(5, "* Current offset: 0x%zx", stream->offset);

	return NT_STATUS_OK;
}


/**
   \details Write length bytes to a local stream

   \param stream pointer to the mpm_stream entry
   \param length the data length to write to the stream
   \param data pointer to the data to write to the stream

   \return NT_STATUS_OK on success, otherwise NT_STATUS_UNSUCCESSFUL
 */
NTSTATUS mpm_cache_stream_write(struct mpm_stream *stream, uint16_t length, uint8_t *data)
{
	uint32_t	WrittenSize;

	fseek(stream->fp, stream->offset, SEEK_SET);
	WrittenSize = fwrite(data, sizeof (uint8_t), length, stream->fp);
	if (WrittenSize != length) {
		OC_DEBUG(0, "* WrittenSize != length");
		return NT_STATUS_UNSUCCESSFUL;
	}

	stream->offset += WrittenSize;

	return NT_STATUS_OK;
}


/**
   \details Rewind a stream to the beginning

   \param stream pointer to the mpm_stream entry

   \return NT_STATUS_OK on success
 */
NTSTATUS mpm_cache_stream_reset(struct mpm_stream *stream)
{
	fseek(stream->fp, 0, SEEK_SET);
	stream->offset = 0;

	return NT_STATUS_OK;
}
