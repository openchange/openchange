/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008.

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

#include "libocpf/ocpf_private.h"
#include <libocpf/ocpf.h>
#include <libocpf/ocpf_api.h>
#include <libocpf/ocpf_dump.h>

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

	printf("%s\n", s);
	free(s);
}


/**
   \details Dump OCPF Type

   Dump OCPF Registered Type
*/
_PUBLIC_ void ocpf_dump_type(void)
{
	OCPF_DUMP_TITLE(indent, "TYPE", OCPF_DUMP_TOPLEVEL);
	indent++;
	
	INDENT();
	OCPF_DUMP(("* %s", ocpf->type ? ocpf->type : "Undefined"));
	indent--;
}


/**
   \details Dump OCPF Destination Folder

   Dump OCPF Registered Destination Folder
 */
_PUBLIC_ void ocpf_dump_folder(void)
{
	OCPF_DUMP_TITLE(indent, "FOLDER", OCPF_DUMP_TOPLEVEL);
	indent++;

	INDENT();
	OCPF_DUMP(("* 0x%llx", ocpf->folder ? ocpf->folder : -1));
	indent--;
}


/**
   \details Dump OCPF Recipients

   Dump OCPF Recipients
 */
_PUBLIC_ void ocpf_dump_recipients(void)
{
	struct ocpf_recipients	*element;

	OCPF_DUMP_TITLE(indent, "RECIPIENTS", OCPF_DUMP_TOPLEVEL);
	indent++;

	INDENT();
	printf("* To: ");
	for (element = ocpf->recipients; element->next; element = element->next) {
		if (element->class == OCPF_MAPI_TO) {
			printf("%s;", element->name);
		}
	}
	printf("\n");

	INDENT();
	printf("* Cc: ");
	for (element = ocpf->recipients; element->next; element = element->next) {
		if (element->class == OCPF_MAPI_CC) {
			printf("%s;", element->name);
		}
	}
	printf("\n");

	INDENT();
	printf("* Bcc: ");
	for (element = ocpf->recipients; element->next; element = element->next) {
		if (element->class == OCPF_MAPI_BCC) {
			printf("%s;", element->name);
		}
	}
	printf("\n");
}


/**
   \details Dump OCPF OLEGUID

   Dump OCPF Registered OLEGUID
*/
_PUBLIC_ void ocpf_dump_oleguid(void)
{
	struct ocpf_oleguid	*element;

	OCPF_DUMP_TITLE(indent, "OLEGUID", OCPF_DUMP_TOPLEVEL);
	indent++;
	for (element = ocpf->oleguid; element->next; element = element->next) {
		INDENT();
		printf("%-25s: %s\n", element->name, element->guid);
	}
	indent--;
}


_PUBLIC_ void ocpf_dump_variable(void)
{
	struct ocpf_var		*element;

	OCPF_DUMP_TITLE(indent, "VARIABLE", OCPF_DUMP_TOPLEVEL);
	indent++;
	for (element = ocpf->vars; element->next; element = element->next) {
		INDENT();
		printf("%s\n", element->name);
	}
	indent--;
}

_PUBLIC_ void ocpf_dump_property(void)
{
	struct ocpf_property	*element;
	const char		*proptag;

	OCPF_DUMP_TITLE(indent, "PROPERTIES", OCPF_DUMP_TOPLEVEL);
	indent++;
	for (element = ocpf->props; element->next; element = element->next) {
		INDENT();
		proptag = (const char *)get_proptag_name(element->aulPropTag);
		printf("0x%.8x = %s\n", element->aulPropTag,
		       (char *)(proptag ? proptag : "UNKNOWN"));
	
	}
	indent--;
}


_PUBLIC_ void ocpf_dump_named_property(void)
{
	struct ocpf_nproperty	*element;

	OCPF_DUMP_TITLE(indent, "NAMED PROPERTIES", OCPF_DUMP_TOPLEVEL);
	indent++;

	OCPF_DUMP_TITLE(indent, "OOM", OCPF_DUMP_SUBLEVEL);
	indent++;
	for (element = ocpf->nprops; element->next; element = element->next) {
		if (element->kind == OCPF_OOM) {
			INDENT();
			printf("* %s\n", element->OOM);
		}
	}
	indent--;

	OCPF_DUMP_TITLE(indent, "MNID_ID", OCPF_DUMP_SUBLEVEL);
	indent++;
	for (element = ocpf->nprops; element->next; element = element->next) {
		if (element->kind == OCPF_MNID_ID) {
			INDENT();
			printf("* 0x%.4x\n", element->mnid_id);
		}
	}
	indent--;

	OCPF_DUMP_TITLE(indent, "MNID_STRING", OCPF_DUMP_SUBLEVEL);
	indent++;
	for (element = ocpf->nprops; element->next; element = element->next) {
		if (element->kind == OCPF_MNID_STRING) {
			INDENT();
			printf("* %s\n", element->mnid_string);
		}
	}
	indent--;

	indent--;
}


_PUBLIC_ void ocpf_dump(void)
{
	indent = 0;
	ocpf_dump_type();
	ocpf_dump_folder();
	ocpf_dump_oleguid();
	ocpf_dump_recipients();
	ocpf_dump_variable();
	ocpf_dump_property();
	ocpf_dump_named_property();
}
