openchange-testsuite
======================

Openchange unit tests testsuite utility.
It is using check library for unit testing.

Usage
-----

Build Openchange as usual. Test suite goes to:

    bin/openchange-testsuite

To run all tests:

    bin/openchange-testsuite

By default, check runs in 'fork mode'. Which makes it hard to debug. To run in 'no-fork mode':

    CK_FORK=no bin/openchange-testsuite

To run just one testsuite or testcase, pass the name of the suite in CK_RUN_SUITE environment
variable or CK_RUN_CASE for a test case respectively:

    CK_RUN_SUITE="libmapistore named properties: interface" bin/openchange-testsuite

or

    CK_RUN_CASE="Interface" bin/openchange-testsuite


Check for memory leaks
----------------------

To check for memory leaks, run as:

    bin/openchange-testsuite --leak-report

or
    bin/openchange-testsuite --leak-report-full

for more detailed leak report.


----------

**List of "known" memory leaks (comming from Samba)**

 - struct tevent_ops_list
 - struct ldb_hooks
 - struct backends_list_entry
 - struct loaded
 - struct bitmap
 - struct loadparm_context


Enjoy!
