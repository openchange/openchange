#include <stdlib.h>
#include <check.h>
#include "test_suites/namedprops_mysql.h"


int main(void)
{
	int number_failed;
	Suite *s = namedprops_mysql_suite();
	SRunner *sr = srunner_create(s);

	srunner_set_xml(sr, "test_results.xml");
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
