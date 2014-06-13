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
 2. Add Package field to the crash at the beginning of the file to
    identify the source package.

    For instance:

        Package: samba 2:4.1.6+dfsg-1~zentyal1~106
  
    If you are retracing a crash from Precise distribution release you
    need to set up the proper repository at `apport-config/Ubuntu
    12.04/sources.list`.

 3. Run the following script
 
        $ bash crash-retrace.sh -v [-g|-s] crash_file
 
    If we set the -g flag, then the GDB application is launched to
    examine the crash. If omitted or -s flag is set, then the full
    backtrace is shown in the standard out. The -v flag indicates to the
    retrace process to be verbose.
