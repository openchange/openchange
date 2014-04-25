/*
   MAPI Proxy - Named properties backend interface

   OpenChange Project

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

#ifndef __NAMEDPROPS_BACKEND_H__
#define __NAMEDPROPS_BACKEND_H__

#include <talloc.h>
#include <stdint.h>
#include "../mapistore_errors.h"

struct MAPINAMEID;

struct namedprops_context {
	enum mapistore_error (*get_mapped_id)(struct namedprops_context *, struct MAPINAMEID, uint16_t *);
	enum mapistore_error (*next_unused_id)(struct namedprops_context *, uint16_t *);
	enum mapistore_error (*create_id)(struct namedprops_context *, struct MAPINAMEID, uint16_t);
	enum mapistore_error (*get_nameid)(struct namedprops_context *, uint16_t, TALLOC_CTX *, struct MAPINAMEID **);
	enum mapistore_error (*get_nameid_type)(struct namedprops_context *, uint16_t, uint16_t *);
	enum mapistore_error (*transaction_start)(struct namedprops_context *);
	enum mapistore_error (*transaction_commit)(struct namedprops_context *);

	const char *backend_type;
	void *data;
};


// Implemented on mapistore_namedprops
enum mapistore_error mapistore_namedprops_init(TALLOC_CTX *, const char *, struct namedprops_context **);
const char *mapistore_namedprops_get_ldif_path(void);
int mapistore_namedprops_prop_type_from_string(const char *);


#endif /* __NAMEDPROPS_BACKEND_H__ */
