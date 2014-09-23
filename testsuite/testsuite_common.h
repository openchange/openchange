/*
   Shared defines and helper functions for test suites

   Copyright (C) Jesús García Sáez 2014.

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
#ifndef __TESTSUITE_COMMON_H__
#define __TESTSUITE_COMMON_H__

#include "mapiproxy/libmapiproxy/backends/openchangedb_backends.h"

/* MySQL constants to establish a connection */
#define	OC_TESTSUITE_MYSQL_HOST		"127.0.0.1"
#define	OC_TESTSUITE_MYSQL_USER		"root"
#define	OC_TESTSUITE_MYSQL_PASS		""
#define	OC_TESTSUITE_MYSQL_DB		"openchange_test"

#define RESOURCES_DIR 			"testsuite/resources"
/* According to the initial ldif file (resources dir) we insert into database */
#define NEXT_UNUSED_ID			38392
/* According to the initial sample data (resources dir) */
#define NEXT_CHANGE_NUMBER		402


void initialize_mysql_with_file(TALLOC_CTX *, const char *, struct openchangedb_context **);


#endif /* __TESTSUITE_COMMON_H__ */
