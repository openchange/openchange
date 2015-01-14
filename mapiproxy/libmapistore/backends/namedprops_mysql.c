/*
   MAPI Proxy - Named properties backend MySQL implementation

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

#include "namedprops_mysql.h"
#include "../mapistore.h"
#include "../mapistore_private.h"
#include "mapiproxy/util/mysql.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>
#include <ldb.h>
#include <samba_util.h>


static enum mapistore_error get_mapped_id(struct namedprops_context *self,
					  struct MAPINAMEID nameid,
					  uint16_t *mapped_id)
{
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	int type = nameid.ulKind;
	char *guid = GUID_string(mem_ctx, &nameid.lpguid);
	MYSQL *conn = self->data;

	char *sql = NULL;
	if (type == MNID_ID) {
		uint32_t prop_id = nameid.kind.lid;
		sql = talloc_asprintf(mem_ctx,
			"SELECT mappedId FROM "NAMEDPROPS_MYSQL_TABLE" "
			"WHERE `type`=%d AND `oleguid`='%s' AND `propId`=%d",
			type, guid, prop_id);
	} else if (type == MNID_STRING) {
		const char *prop_name = nameid.kind.lpwstr.Name;
		sql = talloc_asprintf(mem_ctx,
			"SELECT mappedId FROM "NAMEDPROPS_MYSQL_TABLE" "
			"WHERE `type`=%d AND `oleguid`='%s' AND `propName`='%s'",
			type, guid, prop_name);
	} else {
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERROR, mem_ctx);
	}

	if (mysql_query(conn, sql) != 0) {
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);
	}

	MYSQL_RES *res = mysql_store_result(conn);
	if (mysql_num_rows(res) == 0) {
		// Not found
		mysql_free_result(res);
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_NOT_FOUND, mem_ctx);
	}
	MYSQL_ROW row = mysql_fetch_row(res);
	*mapped_id = strtol(row[0], NULL, 10);
	mysql_free_result(res);

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}


/**
   \details Return the next unused namedprops ID

   \param nprops pointer to the namedprops creontext
   \param highest_id pointer to the next ID to return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error next_unused_id(struct namedprops_context *nprops,
					   uint16_t *highest_id)
{
	TALLOC_CTX	*mem_ctx;
	MYSQL		*conn;
	MYSQL_RES	*res;
	MYSQL_ROW	row;
	char		*sql_query;
	int		ret;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!highest_id, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	conn = (MYSQL *) nprops->data;
	MAPISTORE_RETVAL_IF(!conn, MAPISTORE_ERR_DATABASE_OPS, NULL);

	mem_ctx = talloc_named(NULL, 0, "next_unused_id");
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	sql_query = talloc_asprintf(mem_ctx, "SELECT max(mappedId) FROM %s", NAMEDPROPS_MYSQL_TABLE);
	MAPISTORE_RETVAL_IF(!sql_query, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	ret = mysql_query(conn, sql_query);
	talloc_free(sql_query);
	MAPISTORE_RETVAL_IF(ret, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	res = mysql_store_result(conn);
	MAPISTORE_RETVAL_IF(!res, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);

	row = mysql_fetch_row(res);
	if (!row) {
		mysql_free_result(res);
		mapistore_set_errno(MAPISTORE_ERR_DATABASE_OPS);
		talloc_free(mem_ctx);
		return MAPISTORE_ERR_DATABASE_OPS;
	}

	*highest_id = strtol(row[0], NULL, 10);

	mysql_free_result(res);
	talloc_free(mem_ctx);

	*highest_id = *highest_id + 1;
	return MAPISTORE_SUCCESS;
}


static enum mapistore_error create_id(struct namedprops_context *self,
				      struct MAPINAMEID nameid,
				      uint16_t mapped_id)
{
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	const char **fields = (const char **) str_list_make_empty(mem_ctx);

	fields = str_list_add(fields, talloc_asprintf(mem_ctx, "type=%d",
						      nameid.ulKind));
	fields = str_list_add(fields, talloc_asprintf(mem_ctx, "propType=%d",
						      PT_NULL));
	char *guid = GUID_string(mem_ctx, &nameid.lpguid);
	fields = str_list_add(fields, talloc_asprintf(mem_ctx, "oleguid='%s'",
						      guid));
	fields = str_list_add(fields, talloc_asprintf(mem_ctx, "mappedId=%u",
						      mapped_id));
	if (nameid.ulKind == MNID_ID) {
		fields = str_list_add(fields,
				      talloc_asprintf(mem_ctx, "propId=%u",
						      nameid.kind.lid));
	} else if (nameid.ulKind == MNID_STRING) {
		fields = str_list_add(fields,
				      talloc_asprintf(mem_ctx, "propName='%s'",
						      nameid.kind.lpwstr.Name));
	} else {
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERROR, mem_ctx);
	}

	char *fields_sql = str_list_join(mem_ctx, fields, ',');
	char *sql = talloc_asprintf(mem_ctx,
		"INSERT INTO " NAMEDPROPS_MYSQL_TABLE " SET %s", fields_sql);
	DEBUG(5, ("Inserting record:\n%s\n", sql));
	MYSQL *conn = self->data;
	if (mysql_query(conn, sql) != 0) {
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);
	}

	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error get_nameid(struct namedprops_context *self,
				       uint16_t mapped_id,
				       TALLOC_CTX *mem_ctx,
				       struct MAPINAMEID **nameidp)
{
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	MYSQL *conn = self->data;
	const char *sql = talloc_asprintf(local_mem_ctx,
		"SELECT type, oleguid, propName, propId FROM "NAMEDPROPS_MYSQL_TABLE" "
		"WHERE mappedId=%d", mapped_id);
	if (mysql_query(conn, sql) != 0) {
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_DATABASE_OPS,
				    local_mem_ctx);
	}
	MYSQL_RES *res = mysql_store_result(conn);
	if (mysql_num_rows(res) == 0) {
		// Not found
		mysql_free_result(res);
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_NOT_FOUND,
				    local_mem_ctx);
	}
	MYSQL_ROW row = mysql_fetch_row(res);

	enum mapistore_error ret = MAPISTORE_SUCCESS;
	struct MAPINAMEID *nameid = talloc_zero(mem_ctx, struct MAPINAMEID);
	const char *guid = row[1];
	GUID_from_string(guid, &nameid->lpguid);
	int type = strtol(row[0], NULL, 10);
	nameid->ulKind = type;
	if (type == MNID_ID) {
		nameid->kind.lid = strtol(row[3], NULL, 10);
	} else if (type == MNID_STRING) {
		const char *propName = row[2];
		nameid->kind.lpwstr.NameSize = strlen(propName) * 2 + 2;//FIXME WHY *2+2 and not just +1?
		nameid->kind.lpwstr.Name = talloc_strdup(nameid, propName);
	} else {
		nameid = NULL;
		ret = MAPISTORE_ERROR;
	}

	*nameidp = nameid;

	mysql_free_result(res);
	talloc_free(local_mem_ctx);

	return ret;
}

static enum mapistore_error get_nameid_type(struct namedprops_context *self,
					    uint16_t mapped_id,
					    uint16_t *prop_type)
{
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	MYSQL *conn = self->data;
	const char *sql = talloc_asprintf(mem_ctx,
		//FIXME mappedId or propId? mappedId is not unique
		"SELECT propType FROM "NAMEDPROPS_MYSQL_TABLE" WHERE mappedId=%d",
		mapped_id);
	if (mysql_query(conn, sql) != 0) {
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_DATABASE_OPS, mem_ctx);
	}
	MYSQL_RES *res = mysql_store_result(conn);
	if (mysql_num_rows(res) == 0) {
		// Not found
		mysql_free_result(res);
		MAPISTORE_RETVAL_IF(true, MAPISTORE_ERR_NOT_FOUND, mem_ctx);
	}
	MYSQL_ROW row = mysql_fetch_row(res);
	*prop_type = strtol(row[0], NULL, 10);
	mysql_free_result(res);
	talloc_free(mem_ctx);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error transaction_start(struct namedprops_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "START TRANSACTION");
	MAPISTORE_RETVAL_IF(res, MAPISTORE_ERR_DATABASE_OPS, NULL);
	return MAPISTORE_SUCCESS;
}

static enum mapistore_error transaction_commit(struct namedprops_context *self)
{
	MYSQL *conn = self->data;
	int res = mysql_query(conn, "COMMIT");
	MAPISTORE_RETVAL_IF(res, MAPISTORE_ERR_DATABASE_OPS, NULL);
	return MAPISTORE_SUCCESS;
}

static int mapistore_namedprops_mysql_destructor(struct namedprops_context *self)
{
	DEBUG(5, ("[%s:%d] Destroying namedprops mysql context\n", __FUNCTION__, __LINE__));
	if (self && self->data) {
		MYSQL *conn = self->data;
		release_connection(conn);
	} else {
		DEBUG(0, ("[%s:%d] Error: tried to destroy corrupted namedprops mysql context\n",
			  __FUNCTION__, __LINE__));
	}
	return 0;
}


/**
   \details Retrieve MySQL backend parametric options from
   configuration file and store them into a data structure.

   \param lp_ctx Pointer to the loadparm context
   \param p pointer to the structure with individual
   parameters to return

   \return MAPISTORE_SUCCES on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_namedprops_mysql_parameters(struct loadparm_context *lp_ctx,
							   struct namedprops_mysql_params *p)
{
	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!lp_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!p, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve parametric options */
	p->data = lpcfg_parm_string(lp_ctx, NULL, "namedproperties", "mysql_data");
	p->sock = lpcfg_parm_string(lp_ctx, NULL, "namedproperties", "mysql_sock");
	p->user = lpcfg_parm_string(lp_ctx, NULL, "namedproperties", "mysql_user");
	p->pass = lpcfg_parm_string(lp_ctx, NULL, "namedproperties", "mysql_pass");
	p->host = lpcfg_parm_string(lp_ctx, NULL, "namedproperties", "mysql_host");
	p->port = lpcfg_parm_int(lp_ctx, NULL, "namedproperties", "mysql_port", 3306);
	p->db = lpcfg_parm_string(lp_ctx, NULL, "namedproperties", "mysql_db");

	/* Enforce the logic */
	MAPISTORE_RETVAL_IF(!p->user, MAPISTORE_ERR_BACKEND_INIT, NULL);
	MAPISTORE_RETVAL_IF(!p->db, MAPISTORE_ERR_BACKEND_INIT, NULL);
	MAPISTORE_RETVAL_IF(!p->host && !p->sock, MAPISTORE_ERR_BACKEND_INIT, NULL);

	return MAPISTORE_SUCCESS;
}


/**
   \details Check if the named properties schema is created

   \param conn pointer to the MySQL connection

   \return true if the schema is created, otherwise false;
 */
static bool is_schema_created(MYSQL *conn)
{
	return table_exists(conn, NAMEDPROPS_MYSQL_TABLE);
}


/**
   \details Check if the database is empty

   \param conn pointer to the MySQL connection

   \return true if the database is empty, otherwise false
 */
static bool is_database_empty(MYSQL *conn)
{
	enum MYSQLRESULT	ret;
	uint64_t		n = 0;

	ret = select_first_uint(conn, "SELECT count(*) FROM "NAMEDPROPS_MYSQL_TABLE, &n);
	if (ret != MYSQL_SUCCESS) {
		/* query failed, table does not exist? */
		return true;
	}

	return n == 0;
}

static bool add_field_from_ldif(TALLOC_CTX *mem_ctx, struct ldb_message *ldif,
				const char ***fields, const char *field,
				bool mandatory)
{
	const char *val = ldb_msg_find_attr_as_string(ldif, field, "");
	if (strlen(val) == 0) {
		if (mandatory) {
			DEBUG(0, ("%s value hasn't been found! malformed ldif?",
				  field));
		}
		return false;
	}
	char *end;
	int intval = strtol(val, &end, 10);
	if (end && strlen(val) == (end - val)) {
		*fields = str_list_add(*fields,
				       talloc_asprintf(mem_ctx, "%s=%d", field,
						       intval));
	} else {
		*fields = str_list_add(*fields,
				       talloc_asprintf(mem_ctx, "%s='%s'",
						       field, val));
	}
	return true;
}


/**
  Table fields:
    * Mandatory fields:
        * `type` TINYINT(1)
        * `propType` INT(10) unsigned
        * `oleguid` VARCHAR(255)
        * `mappedId` INT(10) unsigned
    * Optional fields:
        * `propId` INT(10) unsigned
        * `propName` VARCHAR(255)
        * `oom` VARCHAR(255)
        * `canonical` VARCHAR(255)
 */
static bool insert_ldif_msg(MYSQL *conn, struct ldb_message *ldif)
{
	const char *val = ldb_msg_find_attr_as_string(ldif, "objectClass", "");
	if (strlen(val) < strlen("MNID_ID")) {
		// It's not a valid entry, ignore it
		return true;
	}
	TALLOC_CTX *mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	const char **fields = (const char **) str_list_make_empty(mem_ctx);

	// Optional fields
	add_field_from_ldif(mem_ctx, ldif, &fields, "propId", false);
	add_field_from_ldif(mem_ctx, ldif, &fields, "propName", false);
	add_field_from_ldif(mem_ctx, ldif, &fields, "oom", false);
	add_field_from_ldif(mem_ctx, ldif, &fields, "canonical", false);
	// Mandatory fields
	// oleguid and mappedId
	if (!add_field_from_ldif(mem_ctx, ldif, &fields, "oleguid", true) ||
	    !add_field_from_ldif(mem_ctx, ldif, &fields, "mappedId", true)) {
		return false;
	}
	// type
	int type;
	if (strcmp(val, "MNID_STRING") == 0) {
		type = MNID_STRING;
	} else if (strcmp(val, "MNID_ID") == 0) {
		type = MNID_ID;
	} else {
		DEBUG(0, ("Invalid objectClass %s", val));
		return false;
	}
	fields = str_list_add(fields, talloc_asprintf(mem_ctx, "type=%d", type));
	// propType: it could be either an integer or a constant PT_*, we have
	//           to store it as an integer
	val = ldb_msg_find_attr_as_string(ldif, "propType", "");
	if (strlen(val) == 0) {
		DEBUG(0, ("propType value hasn't been found! malformed ldif?"));
		return false;
	}
	int propType;
	if (isalpha(val[0])) {
		propType = mapistore_namedprops_prop_type_from_string(val);
		if (propType == -1) {
			DEBUG(0, ("Invalid propType %s", val));
			return false;
		}
	} else {
		propType = strtol(val, NULL, 10);
	}
	fields = str_list_add(fields,
			      talloc_asprintf(mem_ctx, "propType=%d", propType));
	// Done, we have all fields on fields array
	char *fields_sql = str_list_join(mem_ctx, fields, ',');
	char *sql = talloc_asprintf(mem_ctx,
			"INSERT INTO " NAMEDPROPS_MYSQL_TABLE " SET %s", fields_sql);
	mysql_query(conn, sql);
	talloc_free(mem_ctx);
	return true;
}


/**
  \details Initialize the database and provision it

  \param conn pointer to the MySQL context
  \param schema_path pointer to the path holding schema files

  \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
static enum mapistore_error initialize_database(MYSQL *conn, const char *schema_path)
{
	TALLOC_CTX		*mem_ctx;
	enum mapistore_error	retval = MAPISTORE_SUCCESS;
	struct ldb_context	*ldb_ctx;
	struct ldb_ldif		*ldif;
	struct ldb_message	*msg;
	int			ret;
	char			*filename;
	FILE			*f;
	bool			inserted;
	bool			schema_created;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!conn, MAPISTORE_ERR_DATABASE_INIT, NULL);

	mem_ctx = talloc_named(NULL, 0, "initialize_database");
	MAPISTORE_RETVAL_IF(!mem_ctx, MAPISTORE_ERR_NO_MEMORY, NULL);

	filename = talloc_asprintf(mem_ctx, "%s/" NAMEDPROPS_MYSQL_SCHEMA,
				   schema_path ? schema_path : mapistore_namedprops_get_ldif_path());
	MAPISTORE_RETVAL_IF(!filename, MAPISTORE_ERR_NO_MEMORY, mem_ctx);
	schema_created = create_schema(conn, filename);
	if (!schema_created) {
		DEBUG(1, ("Failed named properties schema creation, "
			  "last mysql error was: `%s`\n", mysql_error(conn)));
		MAPISTORE_RETVAL_ERR(MAPISTORE_ERR_DATABASE_INIT, mem_ctx);
	}

	ldb_ctx = ldb_init(mem_ctx, NULL);
	MAPISTORE_RETVAL_IF(!ldb_ctx, MAPISTORE_ERR_BACKEND_INIT, mem_ctx);

	filename = talloc_asprintf(mem_ctx, "%s/mapistore_namedprops.ldif",
				   schema_path ? schema_path : mapistore_namedprops_get_ldif_path());
	MAPISTORE_RETVAL_IF(!filename, MAPISTORE_ERR_NO_MEMORY, mem_ctx);

	f = fopen(filename, "r");
	talloc_free(filename);
	MAPISTORE_RETVAL_IF(!f, MAPISTORE_ERR_BACKEND_INIT, mem_ctx);
	
	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		ret = ldb_msg_normalize(ldb_ctx, mem_ctx, ldif->msg, &msg);
		if (ret) {
			retval = MAPISTORE_ERR_DATABASE_INIT;
			mapistore_set_errno(MAPISTORE_ERR_DATABASE_INIT);
			goto end;
		}

		inserted = insert_ldif_msg(conn, msg);
		ldb_ldif_read_free(ldb_ctx, ldif);
		if (!inserted) {
			retval = MAPISTORE_ERR_DATABASE_OPS;
			mapistore_set_errno(MAPISTORE_ERR_DATABASE_OPS);
			goto end;
		}
	}

end:
	talloc_free(mem_ctx);
	fclose(f);

	return retval;
}

static char *connection_string_from_parameters(TALLOC_CTX *mem_ctx, struct namedprops_mysql_params *parms)
{
	char *connection_string;

	connection_string = talloc_asprintf(mem_ctx, "mysql://%s", parms->user);
	if (!connection_string) return NULL;
	if (parms->pass && parms->pass[0]) {
		connection_string = talloc_asprintf_append(connection_string, ":%s", parms->pass);
		if (!connection_string) return NULL;
	}
	connection_string = talloc_asprintf_append(connection_string, "@%s", parms->host);
	if (!connection_string) return NULL;
	if (parms->port) {
		connection_string = talloc_asprintf_append(connection_string, ":%d", parms->port);
		if (!connection_string) return NULL;
	}
	return talloc_asprintf_append(connection_string, "/%s", parms->db);
}

/**
   \details Initialize mapistore named properties MySQL backend

   \param mem_ctx pointer to the memory context
   \param lp_ctx pointer to the loadparm context
   \param nprops_ctx pointer on pointer to the namedprops context to
   return

   \return MAPISTORE_SUCCESS on success, otherwise MAPISTORE error
 */
enum mapistore_error mapistore_namedprops_mysql_init(TALLOC_CTX *mem_ctx,
						     struct loadparm_context *lp_ctx,
						     struct namedprops_context **nprops_ctx)
{
	enum mapistore_error		retval;
	struct namedprops_context	*nprops = NULL;
	struct namedprops_mysql_params	parms;
	MYSQL				*conn = NULL;

	/* Sanity checks */
	MAPISTORE_RETVAL_IF(!lp_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);
	MAPISTORE_RETVAL_IF(!nprops_ctx, MAPISTORE_ERR_INVALID_PARAMETER, NULL);

	/* Retrieve smb.conf arguments */
	retval = mapistore_namedprops_mysql_parameters(lp_ctx, &parms);
	if (retval != MAPISTORE_SUCCESS) {
		DEBUG(0, ("[%s:%d] ERROR: parsing MySQL named properties "
			  "parametric option failed with %s\n",
			  __FUNCTION__, __LINE__, mapistore_errstr(retval)));
		MAPISTORE_RETVAL_ERR(retval, NULL);
	}

	/* Establish MySQL connection */
	if (parms.sock) {
		// FIXME
		DEBUG(0, ("Not implemented connect through unix socket to mysql"));
	} else {
		char *connection_string = connection_string_from_parameters(mem_ctx, &parms);
		MAPISTORE_RETVAL_IF(!connection_string, MAPISTORE_ERR_NOT_INITIALIZED, NULL);
		create_connection(connection_string, &conn);
	}
	MAPISTORE_RETVAL_IF(!conn, MAPISTORE_ERR_NOT_INITIALIZED, NULL);

	/* Initialize the database */
	if (!is_schema_created(conn) || is_database_empty(conn)) {
		retval = initialize_database(conn, parms.data);
		MAPISTORE_RETVAL_IF(retval != MAPISTORE_SUCCESS, retval, NULL);
	}

	/* Create context */
	nprops = talloc_zero(mem_ctx, struct namedprops_context);
	MAPISTORE_RETVAL_IF(!nprops, MAPISTORE_ERR_NO_MEMORY, NULL);

	nprops->backend_type = NAMEDPROPS_BACKEND_MYSQL;

	nprops->create_id = create_id;
	nprops->get_mapped_id = get_mapped_id;
	nprops->get_nameid = get_nameid;
	nprops->get_nameid_type = get_nameid_type;
	nprops->next_unused_id = next_unused_id;
	nprops->transaction_commit = transaction_commit;
	nprops->transaction_start = transaction_start;

	nprops->data = conn;
	talloc_set_destructor(nprops, mapistore_namedprops_mysql_destructor);

	*nprops_ctx = nprops;
	return MAPISTORE_SUCCESS;
}
