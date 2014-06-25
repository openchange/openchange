#include "test_common.h"
#include <ldb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mysql/mysql.h>
#include <check.h>

void copy(char *source, char *dest)
{
	pid_t pid = fork();

	if (!pid) {
		execl("/bin/cp", "/bin/cp", source, dest, NULL);
	} else if (pid > 0) {
		int status;
		pid_t wpid = waitpid(pid, &status, WNOHANG);
		int exited = WIFEXITED(status);
		int exit_status = WEXITSTATUS(status);
		if (wpid != -1 && exited && !exit_status) return; // copied ok
	}

	fprintf(stderr, "Error on copy function %s -> %s\n", source, dest);

}

void create_ldb_from_ldif(const char *ldb_path, const char *ldif_path,
			  const char *default_context, const char *root_context)
{
	FILE *f;
	struct ldb_ldif *ldif;
	struct ldb_context *ldb_ctx = NULL;
	TALLOC_CTX *local_mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	struct ldb_message *msg;

	ldb_ctx = ldb_init(local_mem_ctx, NULL);
	ldb_connect(ldb_ctx, ldb_path, 0, 0);
	f = fopen(ldif_path, "r");

	msg = ldb_msg_new(local_mem_ctx);
	msg->dn = ldb_dn_new(msg, ldb_ctx, "@INDEXLIST");
	ldb_msg_add_string(msg, "@IDXATTR", "cn");
	ldb_msg_add_string(msg, "@IDXATTR", "oleguid");
	ldb_msg_add_string(msg, "@IDXATTR", "mappedId");
	msg->elements[0].flags = LDB_FLAG_MOD_ADD;
	ldb_add(ldb_ctx, msg);

	while ((ldif = ldb_ldif_read_file(ldb_ctx, f))) {
		struct ldb_message *normalized_msg;
		ldb_msg_normalize(ldb_ctx, local_mem_ctx, ldif->msg,
				  &normalized_msg);
		ldb_add(ldb_ctx, normalized_msg);
		talloc_free(normalized_msg);
		ldb_ldif_read_free(ldb_ctx, ldif);
	}

	if (default_context && root_context) {
		msg = ldb_msg_new(local_mem_ctx);
		msg->dn = ldb_dn_new(msg, ldb_ctx, "@ROOTDSE");
		ldb_msg_add_string(msg, "defaultNamingContext", default_context);
		ldb_msg_add_string(msg, "rootDomainNamingContext", root_context);
		msg->elements[0].flags = LDB_FLAG_MOD_REPLACE;
		ldb_add(ldb_ctx, msg);
	}

	fclose(f);
	talloc_free(local_mem_ctx);
}

void initialize_mysql_with_file(TALLOC_CTX *mem_ctx, const char *sql_file_path,
				struct openchangedb_context **oc_ctx)
{
	const char *database;
	FILE *f;
	long int sql_size;
	size_t bytes_read;
	char *sql, *insert;
	bool inserts_to_execute;
	MYSQL *conn;
	enum MAPISTATUS ret;

	mem_ctx = talloc_zero(NULL, TALLOC_CTX);
	if (strlen(MYSQL_PASS) == 0) {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER "@"
					   MYSQL_HOST "/" MYSQL_DB);
	} else {
		database = talloc_asprintf(mem_ctx, "mysql://" MYSQL_USER ":"
					   MYSQL_PASS "@" MYSQL_HOST "/"
					   MYSQL_DB);
	}
	ret = openchangedb_mysql_initialize(mem_ctx, database, oc_ctx);

	if (ret != MAPI_E_SUCCESS) {
		fprintf(stderr, "Error initializing openchangedb %d\n", ret);
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
		ret = mysql_query(conn, insert) ? false : true;
		if (!ret) {
			fprintf(stderr, "ERROR: %s\n\t%s\n", mysql_error(conn),
				insert);
			break;
		}
		insert = strtok(NULL, ";");
		inserts_to_execute = insert && strlen(insert) > 10;
	}
}
