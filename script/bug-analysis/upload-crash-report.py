#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (C) Enrique J. Hernandez 2014
#
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
Set Package and Dependencies fields to the crash report and optionally upload to a server
and be notified by email.

It only works in python3 and python >= 2.7
"""
import argparse
from apport.report import Report
from apport.packaging_impl import impl as packaging
import gzip
import os
import re
import subprocess
import sys
import tempfile


def parse_options():
    """Parse command line options"""
    parser = argparse.ArgumentParser(description="Set Package and Dependencies field to a crash report and, optionally, upload it")
    parser.add_argument('crash_file', type=str, help='crash_file')
    parser.add_argument('server_url', type=str, help='URL to POST the crash report',
                        nargs='?', default=None)  # Uploading is optional
    parser.add_argument('--email', '-n', help="Be notified about crash report action by email")

    opts = parser.parse_args()

    return opts


def set_package_info(target):
    """Set Package and Dependencies field"""
    if not os.path.isfile(target):
        raise SystemError("%s is not a file" % target)

    report = Report()
    with open(target, 'rb') as f:
        try:
            report.load(f)
        except Exception as exc:
            raise SystemError("Cannot load file %s: %s" % (target, exc))

        additional_deps = ""
        if 'ExecutablePath' in report and re.search('samba', report['ExecutablePath']):
            try:
                for pkg_name in ('openchangeserver', 'openchange-rpcproxy', 'openchange-ocsmanager',
                                 'sogo-openchange'):
                    packaging.get_version(pkg_name)
                    report.add_package_info(pkg_name)
                    if additional_deps:
                        additional_deps += '\n'
                    additional_deps += report['Package'] + "\n" + report['Dependencies']
            except ValueError:
                # Any of previous packages is not installed
                pass

        # Add executable deps
        report.add_package_info()
        if additional_deps:
            report['Dependencies'] += '\n' + additional_deps
            report['Dependencies'] = '\n'.join(sorted(set(report['Dependencies'].split('\n'))))

        with open(target, 'wb') as f:
            report.write(f)


def upload(crash_file, server_url, email=None):
    """Gzip and upload the crash file to a report"""
    gzip_temp_file = os.path.join(tempfile.gettempdir(), os.path.basename(crash_file) + '.gz')
    with open(crash_file, 'rb') as f_in:
        with gzip.open(gzip_temp_file, 'wb') as f_out:
            f_out.writelines(f_in)

    try:
        # Upload using curl
        curl_args = ['curl', '-F', 'file=@{0}'.format(gzip_temp_file)]
        if email:
            curl_args.extend(['-F', 'email={0}'.format(email)])
        curl_args.append(server_url)

        p = subprocess.Popen(curl_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        if stdout.strip() != b'OK':
            sys.stderr.write("Error in {0} call. Stderr: {1}\n".format(' '.join(curl_args), stderr))
    finally:
        os.unlink(gzip_temp_file)


if __name__ == '__main__':
    opts = parse_options()
    set_package_info(opts.crash_file)
    if opts.server_url:
        upload(opts.crash_file, opts.server_url, opts.email)
