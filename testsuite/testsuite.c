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

#include "testsuite.h"

int main(int ac, const char *av[])
{
	SRunner	*sr;
	int	nf;

	sr = srunner_create(suite_create("OpenChange unit testing"));

	/* libmapi */
	srunner_add_suite(sr, libmapi_property_suite());
	/* libmapiproxy */
	srunner_add_suite(sr, mapiproxy_openchangedb_mysql_suite());
	srunner_add_suite(sr, mapiproxy_openchangedb_ldb_suite());
	srunner_add_suite(sr, mapiproxy_openchangedb_multitenancy_mysql_suite());
	/* libmapistore */
	srunner_add_suite(sr, mapistore_namedprops_suite());
	srunner_add_suite(sr, mapistore_namedprops_mysql_suite());
	srunner_add_suite(sr, mapistore_namedprops_tdb_suite());
	srunner_add_suite(sr, mapistore_indexing_mysql_suite());
	srunner_add_suite(sr, mapistore_indexing_tdb_suite());
	/* mapiproxy */
	srunner_add_suite(sr, mapiproxy_util_mysql_suite());

	srunner_run_all(sr, CK_NORMAL);
	nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
