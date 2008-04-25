/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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

#include <libmapi/libmapi.h>
#include "utils/mapitest/mapitest.h"


/**
   \details Initialize a mapitest suite
   
   \param mt the top-level mapitest structure
   \param name the suite name
   \param description the suite description

   \return An allocated mapitest_suite pointer, otherwise NULL.
 */
_PUBLIC_ struct mapitest_suite *mapitest_suite_init(struct mapitest *mt, 
						    const char *name,
						    const char *description)
{
	struct mapitest_suite	*suite = NULL;

	/* Sanity check */
	if (!mt || !mt->mem_ctx) return NULL;
	if (!name) return NULL;

	suite = talloc_zero(mt->mem_ctx, struct mapitest_suite);
	suite->tests = NULL;

	suite->name = talloc_strdup((TALLOC_CTX *) suite, name);
	if (!description) {
		suite->description = NULL;
	} else {
		suite->description = talloc_strdup((TALLOC_CTX *)suite, description);
	}

	return suite;
}


/**
   \details Register a mapitest suite

   \param mt the top-level mapitest structure
   \param suite the mapitest suite we want to add

   \return MAPITEST_SUCCESS on success, otherwise MAPITETS_ERROR

   \sa mapitest_suite_init
 */
_PUBLIC_ uint32_t mapitest_suite_register(struct mapitest *mt, 
					  struct mapitest_suite *suite)
{
	struct mapitest_suite *el = NULL;

	/* Sanity check */
	if (!mt || !mt->mem_ctx) return MAPITEST_ERROR;
	if (!suite) return MAPITEST_ERROR;

	/* Ensure the name is not yet registered */
	for (el = mt->mapi_suite; el; el = el->next) {
		if (el->name && !strcmp(el->name, suite->name)) {
			fprintf(stderr, "Suite already registered\n");
			return MAPITEST_ERROR;
		}
	}

	DLIST_ADD_END(mt->mapi_suite, suite, struct mapitest_suite *);

	return MAPITEST_SUCCESS;
}


/**
   \details add a simple test to the mapitest suite

   \param suite pointer on the parent suite
   \param name the test name
   \param run the test function

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR

   \sa mapitest_suite_init, mapitest_suite_register
 */
_PUBLIC_ uint32_t mapitest_suite_add_simple_test(struct mapitest_suite *suite,
						 const char *name,
						 bool (*run) (struct mapitest *test))
{
	struct mapitest_test	*el = NULL;

	/* Sanity check */
	if (!suite || !name || !run) return MAPITEST_ERROR;

	/* Ensure the test is not yet registered */
	for (el = suite->tests; el; el = el->next) {
		if (el->name && !strcmp(el->name, name)) {
			return MAPITEST_ERROR;
		}
	}

	el = talloc_zero((TALLOC_CTX *) suite, struct mapitest_test);
	el->name = talloc_asprintf((TALLOC_CTX *)suite, "%s-%s", suite->name, name);
	el->description = NULL;
	el->fn = run;

	DLIST_ADD_END(suite->tests, el, struct mapitest_test *);

	return MAPITEST_SUCCESS;
}


/**
   \details add a test to the mapitest suite with description

   \param suite pointer on the parent suite
   \param name the test name
   \param description the test description
   \param run the test function

   \return MAPITEST_SUCCESS on success, otherwise MAPITEST_ERROR
   
   \sa mapitest_suite_init, mapitest_suite_register
*/
_PUBLIC_ uint32_t mapitest_suite_add_test(struct mapitest_suite *suite,
					  const char *name, const char *description,
					  bool (*run) (struct mapitest *test))
{
	struct mapitest_test	*el = NULL;

	/* Sanity check */
	if (!suite || !name || !run || !description) return MAPITEST_ERROR;

	/* Ensure the test is not yet registered */
	for (el = suite->tests; el; el = el->next) {
		if (el->name && !strcmp(el->name, name)) {
			return MAPITEST_ERROR;
		}
	}

	el = talloc_zero((TALLOC_CTX *) suite, struct mapitest_test);
	el->name = talloc_asprintf((TALLOC_CTX *)suite, "%s-%s", suite->name, name);
	el->description = talloc_strdup((TALLOC_CTX *)suite, description);
	el->fn = run;

	DLIST_ADD_END(suite->tests, el, struct mapitest_test *);

	return MAPITEST_SUCCESS;
}


/**
   \details Find a suite given its name

   \param mt top-level mapitest structure
   \param name the suite name to be searched

   \return Pointer on a suite on success, otherwise NULL
 */
_PUBLIC_ struct mapitest_suite *mapitest_suite_find(struct mapitest *mt, 
						    const char *name)
{
	struct mapitest_suite	*suite = NULL;
	
	if (!name) return NULL;

	for (suite = mt->mapi_suite; suite; suite = suite->next) {
		if (!strcmp(name, suite->name)) {
			return suite;
		}
	}

	return NULL;
}


/**
   \details run a test from a suite given its name
   
   \param mt pointer on the top-level mapitest structure
   \param suite pointer on the mapitest suite
   \param name the name of the test to be run

   \return true on success, otherwise false
 */
_PUBLIC_ bool mapitest_suite_run_test(struct mapitest *mt,
				      struct mapitest_suite *suite, 
				      const char *name)
{
	struct mapitest_test	*el;
	bool			ret;
	bool			(*fn)(struct mapitest *);

	if (!suite || !name) return false;

	for (el = suite->tests; el; el = el->next) {
		if (!strcmp(el->name, name)) {
			mapitest_print_test_title_start(mt, el->name);
			fn = el->fn;
			ret = fn(mt);

			mapitest_print_test_title_end(mt);
			mapitest_print_test_result(mt, el->name, ret);
			return ret;
		}
	}

	fprintf(stderr, "[ERROR] %s test doesn't exist\n", name);
	return false;
}


/**
   \details run the special SUITE-ALL test

   \param mt the top-level mapitest structure
   \param name the mapitest test name

   \return true on success, otherwise -1
 */
static bool mapitest_run_test_all(struct mapitest *mt, const char *name)
{
	char			*test_name;
	char			*sname;
	char			*tmp;
	struct mapitest_test	*el;
	struct mapitest_suite	*suite;
	bool			ret = false;

	test_name = talloc_strdup(mt->mem_ctx, name);
	if ((tmp = strtok(test_name, "-")) == NULL) {
		talloc_free(test_name);
	}

	sname = talloc_strdup(mt->mem_ctx, tmp);
	if ((tmp = strtok(NULL, "-")) == NULL) {
		talloc_free(test_name);
		talloc_free(sname);
		return false;
	}

	if (!strcmp(tmp,"ALL")) {
		suite = mapitest_suite_find(mt, sname);

		if (suite) {
			for (el = suite->tests; el; el = el->next) {
				mapitest_suite_run_test(mt, suite, el->name);
				ret = true;
			}
		}
	}
	talloc_free(sname);
	talloc_free(test_name);
	return ret;
}


/**
   \details run a specific test from a particular suite

   \param mt the top-level mapitest structure
   \param name the mapitest test name

   \return true on success, otherwise -1
 */
_PUBLIC_ bool mapitest_run_test(struct mapitest *mt, const char *name)
{
	struct mapitest_suite	*suite;
	struct mapitest_test	*el;
	bool			ret;

	/* sanity check */
	if (!mt || !name) return false;

	/* try to find the test */
	for (suite = mt->mapi_suite; suite; suite = suite->next) {
		for (el = suite->tests; el; el = el->next) {
			if (!strcmp(name, el->name)) {
				ret = mapitest_suite_run_test(mt, suite, name);
				return ret;
			}
		}
	}
	
	/* if no name matches, look it it matches ALL */
	ret = mapitest_run_test_all(mt, name);

	if (ret != true) {
		fprintf(stderr, "[ERROR] Unknown test: \"%s\"\n", name);
	}

	return ret;
}


/**
   \details all tests from all suites

   \param mt the top-level mapitest structure

   \return true on success, otherwise -1
 */
_PUBLIC_ bool mapitest_run_all(struct mapitest *mt)
{
	struct mapitest_suite	*suite;
	struct mapitest_test	*el;
	bool			(*fn)(struct mapitest *);
	bool			ret = false;

	for (suite = mt->mapi_suite; suite; suite = suite->next) {
		mapitest_print_module_title_start(mt, suite->name);

		for (el = suite->tests; el; el = el->next) {
			mapitest_print_test_title_start(mt, el->name);

			fn = el->fn;
			ret = fn(mt);

			mapitest_print_test_title_end(mt);
			mapitest_print_test_result(mt, el->name, ret);
		}

		mapitest_print_module_title_end(mt);
	}
	return ret;
}
