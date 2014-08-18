Retrace an OpenChange Crash
===========================

Retrace is meant to have a full stacktrace with symbols from a crash
report.

The crash report format this tool manages is specified by apport tool from
Ubuntu.

https://wiki.ubuntu.com/Apport

Dependencies
------------

It is required to have the following package and its dependencies:

* `apport-retrace`

Retrace
-------

Here you are the steps to retrace a crash report:

 1. Get the crash
 2. If **Package** field is not set in the crash report, then it is
    guessed as samba and its dependencies along with the openchange
    ones are set to retrace the crash.

 3. Run the following script
 
        $ bash crash-retrace.sh -v [-g|-s|-f] crash_file
 
    If we set the *-g* flag, then the GDB application is launched to
    examine the crash. If omitted or *-s* flag is set, then the full
    backtrace is shown in the standard output. The *-v* flag indicates the
    retrace process to be verbose. If you use *-f* flag, then crash
    full backtrace with symbols is stored in `crash_file.stacktrace`
    in the same directory the crash file is generated.

Crash mining tool
=================

**oc-crash-digger** is a mining tool to retrace and analyse crash
reports created by apport tool. This tool is able to manage
duplicates from an already created crash database before uploading it.

Current implemented `crashdb` backends are client-server based but
here we have a local implementation using **sqlite** which stores the
crashes metadata and the pointers to the crash files.

The **sqlite** backend requires the following configuration options:

  * dbfile: the file to store the database. Defaults to:
    ~/crashdb.sqlite

  * crashes_base_url: the URL where the crashes will be
    stored. Current supported schemes are: http and file.

You have an example on `crashdb.conf` file.

In order to install this backend, run the following command (based on
Ubuntu Trusty 14.04):

    $ ln -s $(pwd)/sqlite.py /usr/lib/python2.7/dist-packages/apport/crashdb_impl/

Given this configuration file:

    # Custom crashdb.conf configuration file to do the mining
    import os
    # Use this in a crashdb implementation
    default = 'db'

    databases = {
          'debug': {
          # For debugging and development
          'impl': 'memory',
          },
	      'db': {
	          # For local management
	          'impl': 'sqlite',
              'dbfile': 'crash.sqlite',
              'dupdb_url': 'file://' + os.path.join(os.getcwd(), 'published.dupdb'),
              'crashes_base_url': 'file://' + os.path.join(os.getcwd(), 'crashes-base'),
          } 
    }

An example to upload the crashes from `crashes` directory can be the
following:

    $ APPORT_CRASHDB_CONF=crashdb.conf ./oc-crash-digger -c \
    apport-config -C apport-sandbox-cache -S crashes \
    --oc-cd-conf=crash-digger.conf -v --upload \
    --duplicate-db=crash-dup.sqlite --publish-db=published.dupdb

The *crashdb* will be stored at `crash.sqlite` file and the processed
crashes will be stored at `crashes-base` directory.
