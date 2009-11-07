/*
   OpenChange NSPI implementation.

   Copyright (C) Julien Kerihuel 2005 - 2006.

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

#include <libmapi/libmapi.h>

#include "config.h"

#ifndef HAVE_STRCASESTR
static char *strcasestr(const char *haystack, const char *needle)
{
       const char *s;
       size_t nlen = strlen(needle);

       for (s=haystack;*s;s++) {
               if (toupper(*needle) == toupper(*s) &&
                               strncasecmp(s, needle, nlen) == 0) {
                       return (char *)((uintptr_t)s);
               }
       }
       return NULL;
}
#endif


/**
   \details Extract a DN element from a given DN

   \param mem_ctx pointer to the memory context
   \param dn pointer to a valid DN
   \param element pointer to the substring where extraction should start

   \return pointer to an allocated substring on success, otherwise NULL
 */
_PUBLIC_ char *x500_get_dn_element(TALLOC_CTX *mem_ctx, const char *dn, const char *element)
{
	char	*pdn, *p, *str;
	char	*tmp_dn;

	if ((dn == NULL) || (dn[0] == '\0') || !element) return NULL;

	tmp_dn = talloc_strdup(mem_ctx, dn);
	pdn = strcasestr((const char *)tmp_dn, element);
	if (pdn == NULL) {
		talloc_free(tmp_dn);
		return NULL;
	}

	pdn += strlen(element);
	p = pdn;

	if ((p = strchr(pdn, '/')) != NULL) {
		p[0] = '\0';
	}

	str = talloc_strdup(mem_ctx, pdn);

	talloc_free(tmp_dn);
	return str;
}


/**
   \details Truncate a DN element

   \param mem_ctx pointer to the memory context
   \param dn pointer to a valid DN
   \param elcount the number of elements to remove from the end of the
   DN

   \return pointer to an allocated substring on success, otherwise
   NULL
 */
_PUBLIC_ char *x500_truncate_dn_last_elements(TALLOC_CTX *mem_ctx, const char *dn, uint32_t elcount)
{
	char *tmp_dn;
	int	i;

	if ((dn == NULL) || (dn[0] == '\0') || !elcount) return NULL;

	tmp_dn = talloc_strdup(mem_ctx, dn);
	for (i = strlen(tmp_dn); i > 0; i--) {
		if (tmp_dn[i] == '/') {
			elcount -= 1;
			if (elcount == 0) {
				tmp_dn[i] = '\0';
				return tmp_dn;
			}
		}
	}

	return NULL;
}


/**
 * Retrieve the servername from a string
 * We should definitively find a better way to handle this
 */

_PUBLIC_ char *x500_get_servername(const char *dn)
{
	char *pdn;
	char *servername;

	if (!dn) {
		return NULL;
	}

	pdn = strcasestr(dn, SERVERNAME);
	if (pdn == NULL) return NULL;

	pdn += strlen(SERVERNAME);
	servername = strsep(&pdn, "/");

	return (servername);
}
