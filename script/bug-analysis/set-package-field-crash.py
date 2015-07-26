#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-

# Copyright (C) Enrique J. Hernández 2014

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Script to set the Package field, if missing, to Apport crashes.

This is specific to Zentyal.
"""
from apport.report import Report
from datetime import datetime
import gzip
import optparse
import os
from sys import stdout
import time


def parse_options():
    """
    Parse command line options
    """
    optparser = optparse.OptionParser('%prog [-h] DIR_FILE')

    (opts, args) = optparser.parse_args()

    return (opts, args)


def log(string):
    """
    If verbosity is enabled, log the given string to stdout, and prepend
    the current date and time.
    """
    stdout.write('%s: %s\n' % (time.strftime('%x %X'), string))
    stdout.flush()


def map_package(report):
    distro_release = report['DistroRelease']
    crash_date = datetime.fromtimestamp(int(report['ExecutableTimestamp']))
    if distro_release == 'Ubuntu 14.04':
        if crash_date >= datetime(2014, 5, 24, 1, 31):  # Release date
            return ('samba', '3:4.1.7+dfsg-2~zentyal2~64')
        return ('samba', '3:4.1.7+dfsg-2~zentyal1~32')
    elif distro_release == 'Ubuntu 13.10':
        return ('samba', '2:4.1.6+dfsg-1~zentyal1~106')
    elif distro_release == 'Ubuntu 12.04':
        if crash_date < datetime(2013, 10, 2):
            return ('samba4', '4.1.0rc3-zentyal3')
        elif crash_date < datetime(2013, 12, 10, 13, 03):
            return ('samba4', '4.1.0rc4-zentyal1')
        elif crash_date < datetime(2013, 12, 17, 11, 34):
            return ('samba4', '4.1.2-zentyal2')
        elif crash_date < datetime(2014, 3, 5, 20, 16):
            return ('samba4', '4.1.3-zentyal2')
        elif crash_date < datetime(2014, 5, 30, 8, 41):
            return ('samba4', '4.1.5-zentyal1')
        else:
            return ('samba4', '4.1.7-zentyal1')
    else:
        raise SystemError('Invalid Distro Release %s' % distro_release)


def run(target):
    crashes_files = []
    if os.path.isdir(target):
        for fname in os.listdir(target):
            crashes_files.append(os.path.join(target, fname))
    elif os.path.isfile(target):
        crashes_files = [target]
    else:
        raise SystemError("%s is not a directory neither a file" % target)

    for fpath in crashes_files:
        report = Report()

        # This may lead to bad guesses...
        if fpath.endswith('.gz'):
            open_f = gzip.open
        else:
            open_f = open

        with open_f(fpath, 'rb') as f:
            try:
                report.load(f)
                log('%s loaded.' % fpath)
            except Exception as exc:
                log("Cannot load %s: %s" % (fpath, exc))
                continue

        if 'Package' in report:
            log("%s has already the Package field: %s" % (fpath, report['Package']))
        else:
            try:
                mapped_package = map_package(report)
            except KeyError as exc:
                log("Cannot map the package field %s: %s" % (fpath, exc))
                continue
            log("%s package is set to %s" % (fpath, mapped_package))
            report['Package'] = "%s %s" % (mapped_package[0], mapped_package[1])
            with open_f(fpath, 'wb') as f:
                report.write(f)


if __name__ == '__main__':
    (opts, args) = parse_options()
    run(args[0])
