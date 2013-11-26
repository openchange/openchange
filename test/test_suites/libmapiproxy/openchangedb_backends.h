#include <check.h>

// According to the initial openchange.ldb file
#define NEXT_CHANGE_NUMBER 402

#define CHECK_SUCCESS ck_assert_int_eq(ret, MAPI_E_SUCCESS)
#define CHECK_FAILURE ck_assert_int_ne(ret, MAPI_E_SUCCESS)

Suite *openchangedb_ldb_suite(void);
Suite *openchangedb_mysql_suite(void);
