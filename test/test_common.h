#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MYSQL_HOST "localhost"
#define MYSQL_USER "root"
#define MYSQL_PASS ""
#define MYSQL_DB   "openchange_test"

#define NAMEDPROPS_LDB_PATH "/tmp/nprops.ldb"

#define CHECK_MYSQL_ERROR \
	if (mysql_errno(conn)) \
		fprintf(stderr, "Error on MySQL: %s", mysql_error(conn))

#endif /* TEST_COMMON_H_ */
