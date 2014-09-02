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

#ifndef	__MPM_CACHE_H
#define	__MPM_CACHE_H

#include <stdio.h>

#include <dlinklist.h>
#include <ldb_errors.h>
#include <ldb.h>

#ifndef	__BEGIN_DECLS
#ifdef	__cplusplus
#define	__BEGIN_DECLS		extern "C" {
#define	__END_DECLS		}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

struct mpm_message {
	struct mpm_session	*session;
	uint32_t		handle;
	int64_t       		FolderId;
	int64_t       		MessageId;
	struct mpm_message	*prev;
	struct mpm_message	*next;
};

struct mpm_attachment {
	struct mpm_session	*session;
	uint32_t		parent_handle;
	uint32_t		handle;
	uint32_t		AttachmentID;
	struct mpm_message	*message;
	struct mpm_attachment	*prev;
	struct mpm_attachment	*next;
};

/**
   A stream can either be for a message or attachment
 */
struct mpm_stream {
	struct mpm_session	*session;
	uint32_t		parent_handle;
	uint32_t		handle;
	enum MAPITAGS		PropertyTag;
	uint32_t		StreamSize;
	size_t			offset;
	FILE			*fp;
	char			*filename;
	bool			cached;
	bool			ahead;
	struct timeval		tv_start;
	struct mpm_attachment	*attachment;
	struct mpm_message	*message;
	struct mpm_stream	*prev;
	struct mpm_stream	*next;
};

/* TODO: Make use of dce_ctx->context->context_id to differentiate sessions ? */

struct mpm_cache {
	struct ldb_context	*ldb_ctx;
	struct mpm_message	*messages;
	struct mpm_attachment	*attachments;
	struct mpm_stream	*streams;
	const char		*dbpath;
	bool			ahead;
	bool			sync;
	int			sync_min;
	char     		**sync_cmd;
};

__BEGIN_DECLS

NTSTATUS       	samba_init_module(void);

NTSTATUS	mpm_cache_ldb_createdb(struct dcesrv_context *, const char *, struct ldb_context **);
NTSTATUS	mpm_cache_ldb_add_message(TALLOC_CTX *, struct ldb_context *, struct mpm_message *);
NTSTATUS	mpm_cache_ldb_add_attachment(TALLOC_CTX *, struct ldb_context *, struct mpm_attachment *);
NTSTATUS	mpm_cache_ldb_add_stream(struct mpm_cache *, struct ldb_context *, struct mpm_stream *);

NTSTATUS	mpm_cache_stream_open(struct mpm_cache *, struct mpm_stream *);
NTSTATUS	mpm_cache_stream_close(struct mpm_stream *);
NTSTATUS	mpm_cache_stream_write(struct mpm_stream *, uint16_t, uint8_t *);
NTSTATUS	mpm_cache_stream_read(struct mpm_stream *, size_t, size_t *, uint8_t **);
NTSTATUS	mpm_cache_stream_reset(struct mpm_stream *);

__END_DECLS

/*
 * Defines
 */

#define	MPM_NAME	"mpm_cache"
#define	MPM_ERROR	"[ERROR] mpm_cache:"
#define	MPM_DB		"mpm_cache.ldb"
#define	MPM_DB_STORAGE	"data"

#define	MPM_LOCATION	__FUNCTION__, __LINE__
#define	MPM_SESSION(x)	x->session->server_id.pid, x->session->server_id.task_id, x->session->server_id.vnn, x->session->context_id

#endif /* __MPM_CACHE_H */
