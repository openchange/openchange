/*
   OpenChange Server implementation   

   Copyright (C) Jesús García Sáez 2014

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

#include "dcesrv_exchange_emsmdb.h"
#include <libmapi/libmapi.h>
#include <string.h>


struct locale_names {
	const char *locale;
	const char *names[PROVISIONING_FOLDERS_SIZE];
};

static const struct locale_names folders_names[] =
{
	{"en", {"OpenChange Mailbox: %s", "Deferred Action", "Spooler Queue", "Common Views", "Schedule", "Finder", "Views", "Shortcuts", "Reminders", "To-Do", "Tracked Mail Processing", "Top of Information Store", "Inbox", "Outbox", "Sent Items", "Deleted Items"}},
	{"es", {"Bandeja de entrada OpenChange: %s", "Deferred Action", "Spooler Queue", "Common Views", "Programación", "Buscador", "Vistas", "Accesos directos", "Recordatorios", "To-Do", "Tracked Mail Processing", "Top of Information Store", "Carpeta de entrada", "Carpeta de salida", "Elementos enviados", "Elementos Borrados"}},
	{NULL}
};

struct locale_special_names {
	const char *locale;
	const char *names[PROVISIONING_SPECIAL_FOLDERS_SIZE];
};

static const struct locale_special_names special_folders[] =
{
	{"en", {"Drafts", "Calendar", "Contacts", "Tasks", "Notes", "Journal"}},
	{"es", {"Borradores", "Calendario", "Contactos", "Tareas", "Notas", "Diario"}},
	{NULL}
};

const char **emsmdbp_get_folders_names(TALLOC_CTX *mem_ctx,
				       struct emsmdbp_context *emsmdbp_ctx)
{
	uint32_t i, locale_len;
	const char *locale = mapi_get_locale_from_lcid(emsmdbp_ctx->userLanguage);
	const char **default_ret = (const char **)folders_names[0].names;
	const char **ret;

	if (locale == NULL) return default_ret;

	ret = openchangedb_get_folders_names(mem_ctx, emsmdbp_ctx->oc_ctx,
					     locale, "folders");
	if (ret) return ret;

	locale_len = strlen(locale);
	for (i = 0; folders_names[i].locale; i++) {
		if (strlen(folders_names[i].locale) == locale_len &&
		    strncmp(locale, folders_names[i].locale, locale_len) == 0) {
			return (const char **)folders_names[i].names;
		}
	}

	if (locale_len > 2 && locale[2] == '_') {
		// Search only for the first chars of the locale.
		// e.g. en_UK will match against en_US
		for (i = 0; folders_names[i].locale; i++) {
			if (strncmp(locale, folders_names[i].locale, 2) == 0) {
				return (const char **)folders_names[i].names;
			}
		}
	}

	return default_ret;
}

const char **emsmdbp_get_special_folders(TALLOC_CTX *mem_ctx,
					 struct emsmdbp_context *emsmdbp_ctx)
{
	uint32_t i, locale_len;
	const char *locale = mapi_get_locale_from_lcid(emsmdbp_ctx->userLanguage);
	const char **default_ret = (const char **)special_folders[0].names;
	const char **ret;

	if (locale == NULL) return default_ret;

	ret = openchangedb_get_folders_names(mem_ctx, emsmdbp_ctx->oc_ctx,
					     locale, "special_folders");
	if (ret) return ret;

	locale_len = strlen(locale);
	for (i = 0; special_folders[i].locale; i++) {
		if (strlen(special_folders[i].locale) == locale_len &&
		    strncmp(locale, special_folders[i].locale, locale_len) == 0) {
			return (const char **)special_folders[i].names;
		}
	}

	if (locale_len > 2 && locale[2] == '_') {
		// Search only for the first chars of the locale.
		// e.g. en_UK will match against en_US
		for (i = 0; special_folders[i].locale; i++) {
			if (strncmp(locale, special_folders[i].locale, 2) == 0) {
				return (const char **)special_folders[i].names;
			}
		}
	}

	return default_ret;
}
