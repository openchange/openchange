/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008-2011.

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

#include "libocpf/ocpf.h"
#include "libocpf/ocpf_api.h"
#include "libocpf/ocpf_dump.h"

/**
   \file ocpf_dump.c

   \brief ocpf Dump API
 */

static void ocpf_do_dump(const char *format, ...)
{
	va_list	ap;
	char	*s = NULL;
	int	ret;

	va_start(ap, format);
	ret = vasprintf(&s, format, ap);
	va_end(ap);

	if (ret == -1) {
		printf("[Dump failure]\n");
	} else {
		printf("%s\n", s);
	}
	free(s);
}


/**
   \details Dump OCPF Type

   Dump OCPF Registered Type
*/
_PUBLIC_ void ocpf_dump_type(uint32_t context_id)
{
	struct ocpf_context	*ctx;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "TYPE", OCPF_DUMP_TOPLEVEL);
	indent++;
	
	INDENT();
	OCPF_DUMP(("* %s", ctx->type ? ctx->type : "Undefined"));
	indent--;
}


/**
   \details Dump OCPF Destination Folder

   Dump OCPF Registered Destination Folder
 */
_PUBLIC_ void ocpf_dump_folder(uint32_t context_id)
{
	struct ocpf_context	*ctx;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "FOLDER", OCPF_DUMP_TOPLEVEL);
	indent++;

	INDENT();
	OCPF_DUMP(("* 0x%llx", ctx->folder ? ctx->folder : 0xFFFFFFFF));
	indent--;
}


/**
   \details Dump OCPF Recipients

   Dump OCPF Recipients
 */
_PUBLIC_ void ocpf_dump_recipients(uint32_t context_id)
{
	struct ocpf_context	*ctx;
	uint32_t		i;
	struct SPropValue	*lpProps;
	uint32_t		*RecipClass;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "RECIPIENTS", OCPF_DUMP_TOPLEVEL);
	indent++;

	for (i = 0; i < ctx->recipients->cRows; i++) {
		lpProps = get_SPropValue_SRow(&(ctx->recipients->aRow[i]), PidTagRecipientType);
		if (lpProps) {
			RecipClass = (uint32_t *)get_SPropValue_data(lpProps);
			if (RecipClass) {
				switch (*RecipClass) {
				case MAPI_TO:
					OCPF_DUMP_TITLE(indent, "TO", OCPF_DUMP_SUBLEVEL);
					break;
				case MAPI_CC:
					OCPF_DUMP_TITLE(indent, "CC", OCPF_DUMP_SUBLEVEL);
					break;
				case MAPI_BCC:
					OCPF_DUMP_TITLE(indent, "BCC", OCPF_DUMP_SUBLEVEL);
					break;
				}
				mapidump_SRow(&ctx->recipients->aRow[i], "\t * ");
			}
		}
	}

	indent--;
	printf("\n");
}


/**
   \details Dump OCPF OLEGUID

   Dump OCPF Registered OLEGUID
*/
_PUBLIC_ void ocpf_dump_oleguid(uint32_t context_id)
{
	struct ocpf_context	*ctx;
	struct ocpf_oleguid	*element;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "OLEGUID", OCPF_DUMP_TOPLEVEL);
	indent++;
	for (element = ctx->oleguid; element->next; element = element->next) {
		INDENT();
		printf("%-25s: %s\n", element->name, element->guid);
	}
	indent--;
}


_PUBLIC_ void ocpf_dump_variable(uint32_t context_id)
{
	struct ocpf_context	*ctx;
	struct ocpf_var		*element;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "VARIABLE", OCPF_DUMP_TOPLEVEL);
	indent++;
	for (element = ctx->vars; element->next; element = element->next) {
		INDENT();
		printf("%s\n", element->name);
	}
	indent--;
}

_PUBLIC_ void ocpf_dump_property(uint32_t context_id)
{
	struct ocpf_context	*ctx;
	struct ocpf_property	*element;
	const char		*proptag;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "PROPERTIES", OCPF_DUMP_TOPLEVEL);
	indent++;
	for (element = ctx->props; element->next; element = element->next) {
		INDENT();
		proptag = (const char *)get_proptag_name(element->aulPropTag);
		printf("0x%.8x = %s\n", element->aulPropTag,
		       (char *)(proptag ? proptag : "UNKNOWN"));
	
	}
	indent--;
}


_PUBLIC_ void ocpf_dump_named_property(uint32_t context_id)
{
	struct ocpf_context	*ctx;
	struct ocpf_nproperty	*element;

	ctx = ocpf_context_search_by_context_id(ocpf->context, context_id);
	if (!ctx) return;

	OCPF_DUMP_TITLE(indent, "NAMED PROPERTIES", OCPF_DUMP_TOPLEVEL);
	indent++;

	OCPF_DUMP_TITLE(indent, "OOM", OCPF_DUMP_SUBLEVEL);
	indent++;
	for (element = ctx->nprops; element->next; element = element->next) {
		if (element->kind == OCPF_OOM) {
			INDENT();
			printf("* %s\n", element->OOM);
		}
	}
	indent--;

	OCPF_DUMP_TITLE(indent, "MNID_ID", OCPF_DUMP_SUBLEVEL);
	indent++;
	for (element = ctx->nprops; element->next; element = element->next) {
		if (element->kind == OCPF_MNID_ID) {
			INDENT();
			printf("* 0x%.4x\n", element->mnid_id);
		}
	}
	indent--;

	OCPF_DUMP_TITLE(indent, "MNID_STRING", OCPF_DUMP_SUBLEVEL);
	indent++;
	for (element = ctx->nprops; element->next; element = element->next) {
		if (element->kind == OCPF_MNID_STRING) {
			INDENT();
			printf("* %s\n", element->mnid_string);
		}
	}
	indent--;

	indent--;
}


_PUBLIC_ void ocpf_dump(uint32_t context_id)
{
	indent = 0;
	ocpf_dump_type(context_id);
	ocpf_dump_folder(context_id);
	ocpf_dump_oleguid(context_id);
	ocpf_dump_recipients(context_id);
	ocpf_dump_variable(context_id);
	ocpf_dump_property(context_id);
	ocpf_dump_named_property(context_id);
}
