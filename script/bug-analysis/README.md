Retrace an OpenChange Crash
===========================

Retrace is meant to have a full stacktrace with symbols from a crash
report.

The managed crash report format is specified by apport tool from
Ubuntu.

https://wiki.ubuntu.com/Apport

Dependencies
------------

It is required to get the following package and its dependencies:

* `apport-retrace`

Retrace
-------

Here you are the steps to retrace a crash report:

 1. Get the crash
 2. If **Package** field is not set in the crash report, then it is
    guessed as samba and its dependencies along with the openchange
    ones are set to retrace the crash.

 3. Run the following script
 
        $ bash crash-retrace.sh -v [-g|-s] crash_file
 
    If we set the -g flag, then the GDB application is launched to
    examine the crash. If omitted or -s flag is set, then the full
    backtrace is shown in the standard out. The -v flag indicates to the
    retrace process to be verbose.
