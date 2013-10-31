#include <stdlib.h>
#include <check.h>

#include "test_suites/namedprops_backends.h"


int main(void)
{
	SRunner *sr = srunner_create(namedprops_ldb_suite());
	srunner_add_suite(sr, namedprops_mysql_suite());

	srunner_set_xml(sr, "test_results.xml");
	srunner_run_all(sr, CK_NORMAL);
	int number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
