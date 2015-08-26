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

Patch
-----

If the crash you retrace comes from a Zentyal package and the
backtrace you analyze looks corrupted - duplicated functions in the
backtrace, incoherent function dependencies or impossible function
calls - it means the symbols apport-retrace is relying on for the
analysis come from the wrong package.

To fix this problem, apport-retrace needs to be able to fetch the
proper symbols from the version specified in Dependencies and Package
fields. This requires to patch package_impl.py from apport-retrace
package with the following command:

        $ sudo cp patches/packaging_impl.py /usr/lib/python2.7/dist-packages/apport/

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

Tracker management
------------------

You can manage the crash report database linked to a tracker platform
such as [Redmine](http://redmine.org). Every time a new crash is
created in the DB, then a new issue is created in the tracker. And
every time a crash is duplicated, then the issue is updated with a new
duplicate to know the availability of a crash report in the
installation base.

In order to configure the tracker authentication, you must edit
`crash-digger.conf` file and remember to pass it as `--oc-cd-conf`
argument with the following configuration:

    # Redmine authentication to tracker integration
    tracker_auth = {
        # type: supported trackers, redmine by now
        'tracker' : 'redmine',
        # url: the url where the tracker is available
        'url': 'http://redmine.org'
        # key: the API key available on the setings
        'key': '34e34234324e2b2321123213213b583839103e5',
        # project_id: the project where to create the issues
        'project_id': 'openchange',
    }

It requires `python-redmine` package to work. In order to install it,
run the following command:

    $ sudo pip install python-redmine
