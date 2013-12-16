#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define MYSQL_HOST "localhost"
#define MYSQL_USER "root"
#define MYSQL_PASS ""
#define MYSQL_DB   "openchange_test"

#define CHECK_MYSQL_ERROR \
	if (mysql_errno(conn)) \
		fprintf(stderr, "Error on MySQL: %s", mysql_error(conn))

#ifndef RESOURCES_DIR
#define RESOURCES_DIR "../../resources"
#endif


void copy(char *source, char *dest);
void create_ldb_from_ldif(const char *ldb_path, const char *ldif_path, const char *default_context, const char *root_context);

#endif /* TEST_COMMON_H_ */
