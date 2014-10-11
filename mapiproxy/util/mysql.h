/*
   MySQL util functions

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

#ifndef __MYSQL_H__
#define __MYSQL_H__

#include <mysql/mysql.h>
#include <talloc.h>
#include <stdbool.h>
#include <gen_ndr/exchange.h>

#define THRESHOLD_SLOW_QUERIES 0.25
#define _sql(A, B) _sql_escape(A, B, '\'')

const char* _sql_escape(TALLOC_CTX *mem_ctx, const char *s, char c);

enum MYSQLRESULT execute_query(MYSQL *, const char *);
enum MYSQLRESULT select_without_fetch(MYSQL *, const char *, MYSQL_RES **);
enum MYSQLRESULT select_all_strings(TALLOC_CTX *, MYSQL *, const char *, struct StringArrayW_r **);
enum MYSQLRESULT select_first_string(TALLOC_CTX *, MYSQL *, const char *, const char **);
enum MYSQLRESULT select_first_uint32(MYSQL *, const char *, uint32_t *);
enum MYSQLRESULT select_first_uint64(MYSQL *, const char *, uint64_t *);
enum MYSQLRESULT select_first_int(MYSQL *, const char *, int *);
enum MYSQLRESULT select_first_int64(MYSQL *, const char *, int64_t *);

bool table_exists(MYSQL *, char *);
bool create_schema(MYSQL *, const char *);
bool convert_string_to_ul(const char *, uint32_t *);
bool convert_string_to_ull(const char *, uint64_t *);
bool convert_string_to_l(const char *, int *);
bool convert_string_to_ll(const char *, int64_t *);

MYSQL *create_connection(const char *, MYSQL **);
void release_connection(MYSQL *);
void close_all_connections(void);

enum MYSQLRESULT { MYSQL_SUCCESS, MYSQL_NOT_FOUND, MYSQL_ERROR };

#endif /* __MYSQL_H__ */
