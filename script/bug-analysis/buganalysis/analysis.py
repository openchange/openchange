#!/usr/bin/python
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
Methods related to the analysis of crash reports based on Apport

This is specific to OpenChange
"""
import os.path
import re


def guess_components(report):
    """
    Guess components from a given report.

    It is usually done using the stacktrace.

    For now, it only inspects StacktraceTop.

    :returns: a list of components related to this crash report
    :rtype: set
    """
    comps = set()
    # We can mimetise Parse::StackTrace from Perl CPAN
    if 'Stacktrace' not in report:
        # Impossible to guess without a symbolic stacktrace
        return comps

    if 'StacktraceTop' not in report:
        report._gen_stacktrace_top()  # Generate the stacktrace top to start debugging

    if re.search(r'waitpid', report.stacktrace_top_function()) and report['Signal'] == '6':
        # Manually killed, no component
        return comps

    lines = report['StacktraceTop'].splitlines()

    bt_fn_re = re.compile('^(\w+)\s(\(.*\))\sat\s(.*):(\d+)$')
    parsed_stacktrace_top = []
    for line in lines:
        m = bt_fn_re.match(line)
        if m:
            parsed_stacktrace_top.append({'fname': m.group(1),
                                          'params': m.group(2),
                                          'file_name': m.group(3),
                                          'line_number': m.group(4)})

    # High-level OpenChange components
    oc_comps = set(('mapiproxy', 'libmapi', 'libmapiadmin', 'libocpf'))
    # RegExp
    micro_comps = re.compile(r'ox')

    # Check based on the filename
    for frame in parsed_stacktrace_top:
        file_path_parts = frame['file_name'].split(os.sep)
        # High-level OC comps
        if file_path_parts[0] == "":
            first_dir = file_path_parts[1]
        else:
            first_dir = file_path_parts[0]

        (basename, ext) = os.path.splitext(file_path_parts[-1])

        if ext == '.m':
            comps.add('SOGo')
        elif first_dir in oc_comps:
            comps.add(first_dir)
            if micro_comps.match(basename):
                comps.add(basename)

    return comps
