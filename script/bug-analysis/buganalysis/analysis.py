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

    top_function = report.stacktrace_top_function()
    if top_function and re.search(r'waitpid', top_function) and report['Signal'] == '6':
        # Manually killed, no component
        return comps

    full_stacktrace = parse_stacktrace(report['Stacktrace'])

    # High-level OpenChange components
    oc_comps = set(('mapiproxy', 'libmapi', 'libmapiadmin', 'libocpf'))

    # RegExps
    micro_comps_re = re.compile(r'ox')
    nspi_server_re = re.compile(r'nspi')
    rfr_server_re = re.compile(r'rfr')

    # Check based on the filename
    for frame in full_stacktrace:
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
            if micro_comps_re.match(basename):
                comps.add(basename)
            elif nspi_server_re.search(frame['file_name']):
                comps.add('nspi')
            elif rfr_server_re.search(frame['file_name']):
                comps.add('rfr')
            elif frame['fname'] == 'emsmdbp_mailbox_provision':
                comps.add('emsmdbp_mailbox_provision')

    # TODO: Check only samba4 crashes

    return comps


def parse_stacktrace(stacktrace):
    """
    Parse stacktrace ignoring unwind_functions

    :param str stacktrace: the 'bt full' gdb command output
    :returns: a list with the frames in a dict (fname, params, file_name and line_number as keys)
    :rtype: list
    """
    # Based on report._gen_stacktrace_top
    unwind_functions = set(['g_logv', 'g_log', 'IA__g_log', 'IA__g_logv',
                            'g_assert_warning', 'IA__g_assert_warning',
                            '__GI_abort', '__GI_raise', '_XError'])
    unwound = False
    unwinding = False
    fn_re = re.compile('^(\w+)\s(\(.*\))\sat\s(.*):(\d+)$')
    bt_fn_re = re.compile('^#(\d+)\s+(?:0x(?:\w+)\s+in\s+\*?(.*)|(<signal handler called>)\s*)$')
    bt_fn_noaddr_re = re.compile('^#(\d+)\s+(?:(.*)|(<signal handler called>)\s*)$')
    # some internal functions like the SSE stubs cause unnecessary jitter
    ignore_functions_re = re.compile('^(__.*_s?sse\d+(?:_\w+)?|__kernel_vsyscall)$')

    parsed_stacktrace = []
    for line in stacktrace.splitlines():
        m = bt_fn_re.match(line)
        if not m:
            m = bt_fn_noaddr_re.match(line)
            if not m:
                continue

        if not unwound or unwinding:
            if m.group(2):
                fn = m.group(2).split()[0].split('(')[0]
            else:
                fn = None

            if m.group(3) or fn in unwind_functions:
                unwinding = True
                parsed_stacktrace = []
                if m.group(3):
                    # we stop unwinding when we found a <signal handler>,
                    # but we continue unwinding otherwise, as e. g. a glib
                    # abort is usually sitting on top of an XError
                    unwound = True

                    continue
            else:
                unwinding = False

        frame = m.group(2) or m.group(3)
        function = frame.split()[0]
        if not ignore_functions_re.match(function):
            m = fn_re.match(frame)
            if m:
                parsed_stacktrace.append({'fname': m.group(1),
                                          'params': m.group(2),
                                          'file_name': m.group(3),
                                          'line_number': m.group(4)})

    return parsed_stacktrace
