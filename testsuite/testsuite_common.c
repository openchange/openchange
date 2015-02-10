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
#include "testsuite_common.h"
#include "mapiproxy/libmapiproxy/backends/openchangedb_mysql.h"
#include <string.h>
#include <check.h>
#include <param.h>

#define OPENCHANGEDB_MYSQL_SCHEMA_PATH "setup/openchangedb"

void initialize_mysql_with_file(TALLOC_CTX *mem_ctx, const char *sql_file_path,
				struct openchangedb_context **oc_ctx)
{
	const char		*database;
	FILE			*f;
	long int		sql_size = 0;
	size_t			bytes_read = 0;
	char			*sql = NULL, *insert = NULL;
	bool			inserts_to_execute;
	MYSQL			*conn;
	struct loadparm_context	*lp_ctx;
	enum MAPISTATUS		retval;
	const char		*mysql_pass = getenv("OC_MYSQL_PASS");

	if (!mysql_pass) {
		mysql_pass = OC_TESTSUITE_MYSQL_PASS;
	}

	if (strlen(mysql_pass) == 0) {
		database = talloc_asprintf(mem_ctx, "mysql://" OC_TESTSUITE_MYSQL_USER "@"
					   OC_TESTSUITE_MYSQL_HOST "/" OC_TESTSUITE_MYSQL_DB);
	} else {
		database = talloc_asprintf(mem_ctx, "mysql://%s:%s@%s/%s", OC_TESTSUITE_MYSQL_USER,
					   mysql_pass, OC_TESTSUITE_MYSQL_HOST, OC_TESTSUITE_MYSQL_DB);
	}

	lp_ctx = loadparm_init(mem_ctx);
	ck_assert(lp_ctx != NULL);

	ck_assert((lpcfg_set_cmdline(lp_ctx, "mapiproxy:openchangedb", database) == true));
	retval = openchangedb_mysql_initialize(mem_ctx, lp_ctx, oc_ctx);

	if (retval != MAPI_E_SUCCESS) {
		fprintf(stderr, "Error initializing openchangedb %d\n", retval);
		ck_abort();
	}

	// Populate database with sample data
	conn = (*oc_ctx)->data;
	f = fopen(sql_file_path, "r");
	if (!f) {
		fprintf(stderr, "file %s not found", sql_file_path);
		ck_abort();
	}
	fseek(f, 0, SEEK_END);
	sql_size = ftell(f);
	rewind(f);
	sql = talloc_zero_array(mem_ctx, char, sql_size + 1);
	bytes_read = fread(sql, sizeof(char), sql_size, f);
	if (bytes_read != sql_size) {
		fprintf(stderr, "error reading file %s", sql_file_path);
		ck_abort();
	}
	insert = strtok(sql, ";");
	inserts_to_execute = insert != NULL;
	while (inserts_to_execute) {
		retval = mysql_query(conn, insert) ? false : true;
		if (!retval) {
			fprintf(stderr, "ERROR: %s\n\t%s\n", mysql_error(conn), insert);
			break;
		}
		insert = strtok(NULL, ";");
		inserts_to_execute = insert && strlen(insert) > 10;
	}
}

void drop_mysql_database(MYSQL *conn, const char *database)
{
	char *sql = talloc_asprintf(NULL, "DROP DATABASE %s", database);
	if (!sql) ck_abort();
	mysql_query(conn, sql);
	talloc_free(sql);
	close_all_connections();
}
