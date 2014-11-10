/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2014.

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

#ifndef	__TESTSUITE_H__
#define	__TESTSUITE_H__

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <check.h>
#ifdef HAVE_SUBUNIT
#include <subunit/child.h>
#endif

#ifndef	__BEGIN_DECLS
#ifdef	__cplusplus
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif
#endif

__BEGIN_DECLS

/* libmapi */
Suite *libmapi_property_suite(void);
/* libmapiproxy */
Suite *mapiproxy_openchangedb_mysql_suite(void);
Suite *mapiproxy_openchangedb_ldb_suite(void);
Suite *mapiproxy_openchangedb_multitenancy_mysql_suite(void);
Suite *mapiproxy_openchangedb_logger_suite(void);
/* libmapistore */
Suite *mapistore_namedprops_suite(void);
Suite *mapistore_namedprops_mysql_suite(void);
Suite *mapistore_namedprops_tdb_suite(void);
Suite *mapistore_indexing_mysql_suite(void);
Suite *mapistore_indexing_tdb_suite(void);
/* mapiproxy */
Suite *mapiproxy_util_mysql_suite(void);

__END_DECLS

#endif /*! __TESTSUITE_H__ */
