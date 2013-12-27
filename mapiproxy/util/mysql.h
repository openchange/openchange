#ifndef __MYSQL_H__
#define __MYSQL_H__

#include <mysql/mysql.h>
#include <talloc.h>
#include <stdbool.h>
#include <gen_ndr/exchange.h>

#define THRESHOLD_SLOW_QUERIES 0.25

enum MYSQLRESULT execute_query(MYSQL *, const char *);
enum MYSQLRESULT select_without_fetch(MYSQL *, const char *, MYSQL_RES **);
enum MYSQLRESULT select_all_strings(TALLOC_CTX *, MYSQL *, const char *, struct StringArrayW_r **);
enum MYSQLRESULT select_first_string(TALLOC_CTX *, MYSQL *, const char *, const char **);
enum MYSQLRESULT select_first_uint(MYSQL *conn, const char *sql, uint64_t *n);

MYSQL* create_connection(const char *, MYSQL **);

enum MYSQLRESULT { MYSQL_SUCCESS, MYSQL_NOT_FOUND, MYSQL_ERROR };

#endif /* __MYSQL_H__ */
