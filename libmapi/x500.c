/*
 *  OpenChange NSPI implementation.
 *
 *  Copyright (C) Julien Kerihuel 2005-2006
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "openchange.h"

/*
 * x500_get_element
 * Fetch an element from a DN
 *
 */

char *x500_get_dn_element(TALLOC_CTX *mem_ctx, const char *dn, const char *element)
{
	char	*pdn, *p, *str;
	char	*tmp_dn;

	if ((dn == NULL) || (dn[0] == '\0') || !element) return NULL;

	tmp_dn = talloc_strdup(mem_ctx, dn);
	pdn = strstr((const char *)tmp_dn, element);
	if (pdn != NULL) {
		pdn += strlen(element);
		p = pdn;
	}
	if ((p = strchr(pdn, '/')) != NULL) {
		p[0] = '\0';
	}
	DEBUG(3, ("x500 %s %s\n", element, pdn));

	str = talloc_strdup(mem_ctx, pdn);
	
	talloc_free(tmp_dn);
	return str;
}
