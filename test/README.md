Depencencies
============
* cmake (`apt-get install cmake`)
* check (`apt-get install check`)
* openchange (`cd .. && make install`)

How to execute the test
=======================

    $ cmake .
    $ make

How to add more test
====================
* Add a new test_suite inside test_suites directory
* Include and add your new test_suite in openchange_test_suite.c
* Profit
