/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2010.

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
   \file ocpf_context.c

   \brief OCPF context API
 */

#include <sys/stat.h>

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"
#include "libocpf/ocpf_private.h"

#include <sys/stat.h>

/**
   \details Initialize a new OCPF context

   \param mem_ctx pointer to the memory context
   \param filename the OCPF filename used for this context
   \param flags Flags controlling how the OCPF should be opened
   \param context_id the identifier representing the context
   \param context_id the context identifier to use for this context

   \return new allocated OCPF context on success, otherwise NULL
 */
struct ocpf_context *ocpf_context_init(TALLOC_CTX *mem_ctx, 
				       const char *filename,
				       uint8_t flags,
				       uint32_t context_id)
{
	struct ocpf_context	*ctx;
	struct stat		sb;

	OCPF_RETVAL_TYPE(!mem_ctx, NULL, OCPF_NOT_INITIALIZED, NULL, NULL);
	OCPF_RETVAL_TYPE(!context_id, NULL, OCPF_INVALID_CONTEXT, NULL, NULL);
	OCPF_RETVAL_TYPE(!filename, NULL, OCPF_WARN_FILENAME_INVALID, NULL, NULL);

	switch (flags) {
	case OCPF_FLAGS_RDWR:
	case OCPF_FLAGS_READ:
	case OCPF_FLAGS_WRITE:
		OCPF_RETVAL_TYPE((stat(filename, &sb) == -1), NULL, OCPF_WARN_FILENAME_INVALID, NULL, NULL)
		break;
	case OCPF_FLAGS_CREATE:
		OCPF_RETVAL_TYPE(!(stat(filename, &sb)), NULL, OCPF_WARN_FILENAME_EXIST, NULL, NULL);
		break;
		
	}

	/* Initialize the context */
	ctx = talloc_zero(mem_ctx, struct ocpf_context);

	/* Initialize ocpf context parameters */
	ctx->vars = talloc_zero(ctx, struct ocpf_var);
	ctx->oleguid = talloc_zero(ctx, struct ocpf_oleguid);
	ctx->props = talloc_zero(ctx, struct ocpf_property);
	ctx->nprops = talloc_zero(ctx, struct ocpf_nproperty);

	ctx->recipients = talloc_zero(ctx, struct SRowSet);
	ctx->recipients->aRow = talloc_array(ctx->recipients, struct SRow, 2);
	ctx->recipients->aRow[0].lpProps = talloc_array(ctx->recipients->aRow, struct SPropValue, 2);
	ctx->recipients->cRows = 0;

	ctx->lpProps = NULL;
	ctx->cValues = 0;
	ctx->folder = 0;

	/* Initialize ocpf internal context parameters */
	ctx->flags = flags;
	ctx->filename = talloc_strdup(ctx, filename);
	ctx->ref_count = 0;
	ctx->context_id = context_id;

	/* Initialize lexer parameters */
	ctx->lineno = 1;
	ctx->typeset = 0;
	ctx->folderset = false;
	ctx->recip_type = 0;
	ctx->type = NULL;

	switch (flags) {
	case OCPF_FLAGS_RDWR:
		ctx->fp = fopen(filename, "r+");
		break;
	case OCPF_FLAGS_READ:
		ctx->fp = fopen(filename, "r");
		break;
	case OCPF_FLAGS_WRITE:
		ctx->fp = fopen(filename, "w");
		break;
	case OCPF_FLAGS_CREATE:
	  /* defer fopen to ocpf_write_commit */
	  ctx->fp = NULL;
		break;
	}

	OCPF_RETVAL_TYPE(!ctx->fp && flags != OCPF_FLAGS_CREATE, NULL, OCPF_WARN_FILENAME_INVALID, NULL, ctx);

	return ctx;
}

/**
   \details Add an OCPF context to the list

   \param ocpf_ctx pointer to the global ocpf context
   \param filename pointer to the 
   \param context_id pointer to the context_id the function returns
   \param flags Flags controlling how the OCPF should be opened
   \param existing boolean returned by the function to specify if the
   context was already existing or not

   \return valid ocpf context pointer on success, otherwise NULL
 */
struct ocpf_context *ocpf_context_add(struct ocpf *ocpf_ctx, 
				      const char *filename,
				      uint32_t *context_id,
				      uint8_t flags,
				      bool *existing)
{
	struct ocpf_context	*el;
	struct ocpf_freeid	*elf;
	bool			found = false;

	/* Sanity checks */
	if (!ocpf_ctx) return NULL;
	if (!filename) return NULL;
	if (!context_id) return NULL;

	/* Search for an existing context */
	for (el = ocpf_ctx->context; el; el = el->next)  {
		if (el->filename && !strcmp(el->filename, filename)) {
			*context_id = el->context_id;
			el->ref_count += 1;
			*existing = true;
			return el;
		}
	}

	/* Search for an available free context_id, otherwise generate one */
	for (elf = ocpf_ctx->free_id; elf; elf = elf->next) {
		if (elf && elf->context_id) {
			found = true;
			break;
		}
	}

	if (found == true) {
		*context_id = elf->context_id;
		DLIST_REMOVE(ocpf_ctx->free_id, elf);
		talloc_free(elf);
	} else {
		*context_id = ocpf_ctx->last_id;
		ocpf_ctx->last_id += 1;
	}

	/* Initialize the new context */
	*existing = false;
	el = ocpf_context_init(ocpf_ctx->mem_ctx, filename, flags, *context_id);
	/* handle the case where file couldn't be opened */

	return el;
}


/**
   \details Delete an OCPF context

   \param ocpf_ctx pointer to the global ocpf context
   \param ctx pointer to the OCPF context to delete

   \return 0 on success, 1 if there were still a ref_count, otherwise
   -1 on errors.
 */
int ocpf_context_delete(struct ocpf *ocpf_ctx, 
			struct ocpf_context *ctx)
{
	struct ocpf_freeid	*el;
	uint32_t		context_id;

	/* Sanity checks */
	if (!ocpf_ctx) return -1;
	if (!ctx) return -1;

	/* If we still have a reference counter, do nothing */
	if (ctx->ref_count) {
		ctx->ref_count -= 1;
		return 1;
	}

	/* Close the file */
	if (ctx->fp) {
		fclose(ctx->fp);
	}

	/* Remove the context from the list and free it */
	context_id = ctx->context_id;
	DLIST_REMOVE(ocpf_ctx->context, ctx);
	talloc_free(ctx);

	/* Add the context identifier to the free list */
	el = talloc_zero(ocpf_ctx->mem_ctx, struct ocpf_freeid);
	el->context_id = context_id;
	DLIST_ADD_END(ocpf_ctx->free_id, el, struct ocpf_freeid *);

	return 0;
}


/**
   \details Search a context given its filename

   \param ctx pointer to the ocpf context list
   \param filename the filename to use for search

   \return pointer to valid ocpf context on success, otherwise NULL
 */
struct ocpf_context *ocpf_context_search_by_filename(struct ocpf_context *ctx,
						     const char *filename)
{
	struct ocpf_context	*el;

	/* Sanity checks */
	if (!ctx) return NULL;
	if (!filename) return NULL;

	for (el = ctx; el; el = el->next) {
		if (el->filename && !strcmp(el->filename, filename)) {
			return el;
		}
	}

	return NULL;
}


/**
   \details Search a context given its context identifier

   \param ctx pointer to the ocpf context list
   \param context_id the context identifier to use for search

   \return pointer to valid ocpf context on success, otherwise NULL
 */
struct ocpf_context *ocpf_context_search_by_context_id(struct ocpf_context *ctx,
						       uint32_t context_id)
{
	struct ocpf_context	*el;

	/* Sanity checks */
	if (!ctx) return NULL;
	if (!context_id) return NULL;

	for (el = ctx; el; el = el->next) {
		if (el->context_id == context_id) {
			return el;
		}
	}

	return NULL;
}
