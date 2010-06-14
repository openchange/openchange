/* 
   Unix SMB/CIFS implementation.

   LDB wrap functions

   Copyright (C) Andrew Tridgell 2004-2009
   
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

#include "config.h"
#include <stdio.h>
#include <string.h>

#include "mapistore.h"
#include "mapistore_private.h"
#include <dlinklist.h>
#include <ldb.h>

static struct ldb_wrap *ldb_wrap_list;

/*
  see if two database opens are equivalent
 */
static bool mapistore_ldb_wrap_same_context(const struct ldb_wrap_context *c1,
					    const struct ldb_wrap_context *c2)
{
	return (c1->ev == c2->ev &&
		c1->flags == c2->flags &&
		(c1->url == c2->url || strcmp(c1->url, c2->url) == 0));
}

/* 
   free a ldb_wrap structure
 */
static int mapistore_ldb_wrap_destructor(struct ldb_wrap *w)
{
	DLIST_REMOVE(ldb_wrap_list, w);
	return 0;
}

/*
  wrapped connection to a ldb database
  to close just talloc_free() the returned ldb_context

  TODO:  We need an error_string parameter
 */
struct ldb_context *mapistore_ldb_wrap_connect(TALLOC_CTX *mem_ctx,
					       struct tevent_context *ev,
					       const char *url,
					       unsigned int flags)
{
	struct ldb_context	*ldb;
	int			ret;
	struct ldb_wrap		*w;
	struct ldb_wrap_context c;

	c.url          = url;
	c.ev           = ev;
	c.flags        = flags;

	/* see if we can re-use an existing ldb */
	for (w = ldb_wrap_list; w; w = w->next) {
		if (mapistore_ldb_wrap_same_context(&c, &w->context)) {
			return talloc_reference(mem_ctx, w->ldb);
		}
	}

	/* we want to use the existing event context if possible. This
	   relies on the fact that in smbd, everything is a child of
	   the main event_context */
	if (ev == NULL) {
		return NULL;
	}

	ldb = ldb_init(mem_ctx, ev);
	if (ldb == NULL) {
		return NULL;
	}

	ldb_set_create_perms(ldb, 0600);
	
	ret = ldb_connect(ldb, url, flags, NULL);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	/* add to the list of open ldb contexts */
	w = talloc(ldb, struct ldb_wrap);
	if (w == NULL) {
		talloc_free(ldb);
		return NULL;
	}

	w->context = c;
	w->context.url = talloc_strdup(w, url);
	if (w->context.url == NULL) {
		talloc_free(ldb);
		return NULL;
	}

	w->ldb = ldb;

	DLIST_ADD(ldb_wrap_list, w);

	DEBUG(3,("ldb_wrap open of %s\n", url));

	talloc_set_destructor(w, mapistore_ldb_wrap_destructor);

	return ldb;
}
