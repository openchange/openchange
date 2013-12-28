#include <stdlib.h>
#include <check.h>

#include "test_suites/libmapistore/namedprops_backends.h"
#include "test_suites/libmapistore/indexing.h"
#include "test_suites/libmapiproxy/openchangedb.h"


int main(void)
{
	SRunner *sr = srunner_create(suite_create("Open Change unit tests"));

	srunner_add_suite(sr, openchangedb_mysql_suite());
	srunner_add_suite(sr, openchangedb_ldb_suite());

	srunner_add_suite(sr, indexing_tdb_suite());
	srunner_add_suite(sr, indexing_mysql_suite());

	srunner_add_suite(sr, namedprops_ldb_suite());
	srunner_add_suite(sr, namedprops_mysql_suite());

	srunner_set_fork_status(sr, CK_FORK);
	srunner_set_xml(sr, "test_results.xml");
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
