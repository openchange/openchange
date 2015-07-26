#!/usr/bin/env python2.7
#
# syncstream-compare.py
#
# Copyright (C) Wolfgang Sourdeau 2012
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#   
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#   
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This script compare two chunks of the output from parse-syncbuffer and
# report the differences on stdout, in order to help in property-related
# mistakes.


import string
import sys


def compare_dict_keys(left_dict, right_dict, label):
    common_keys = [x for x in left_dict if x in right_dict]

    missing_keys = [x for x in left_dict if x not in right_dict]
    if len(missing_keys) > 0:
        print("< %s keys found in the left dict but not in the right one: %s"
              % (label, ", ".join(missing_keys)))

    missing_keys = [x for x in right_dict if x not in left_dict]
    if len(missing_keys) > 0:
        print("> %s keys found in the right dict but not in the left one: %s"
              % (label, ", ".join(missing_keys)))

    return common_keys

class StreamSection(object):
    def __init__(self):
        self.properties = {}
        self.named_properties = {}

    def compare(self, other_section):
        self._compare_property_dicts(self.properties,
                                     other_section.properties,
                                     "property")
        self._compare_property_dicts(self.named_properties,
                                     other_section.named_properties,
                                     "named property")

    @staticmethod
    def _compare_property_dicts(left_dict, right_dict, label):
        common_keys = compare_dict_keys(left_dict, right_dict, label)
        for key in common_keys:
            self_value = left_dict[key]
            other_value = right_dict[key]
            if self_value == other_value:
                print "= values for %s '%s' are the same" % (label, key)
            else:
                print "! values for %s '%s' differ:" % (label, key)
                print "   left has: '%s'" % self_value.strip()
                print "  right has: '%s'" % other_value.strip()

class SyncStream(object):
    def __init__(self, filename):
        self.filename = filename
        self.sections = None

    def _parse(self):
        sections = {}
        section_name = None
        current_section = None

        current_prop_data = []

        inf = open(self.filename)
        line = inf.readline()
        if line != "":
            len_line = len(line)
            if len_line > 0 and line[-1] == "\n":
                line = line[:-1]
                len_line = len_line - 1
            if len_line > 0 and line[-1] == "\r":
                line = line[:-1]
                len_line = len_line - 1
            if len_line > 0:
                if line[0] == "*":
                    if current_section is not None:
                        if len(current_prop_data) > 0:
                            self._push_prop_data(current_section,
                                                 current_prop_data)
                            current_prop_data = []
                    section_name = line[11:]
                    current_section = StreamSection()
                    sections[section_name] = current_section
                else:
                    if current_section is not None:
                        if len_line >= 10 and line[0:2] == "  ":
                            # line is the start of a new property?
                            prop_id = line[2:10].translate(None,
                                                           string.hexdigits)
                            if len(prop_id) == 0:
                                if len(current_prop_data) > 0:
                                    self._push_prop_data(current_section,
                                                         current_prop_data)
                                    current_prop_data = []
                        current_prop_data.append(line)
            line = inf.readline()
        if current_section is not None and len(current_prop_data) > 0:
            self._push_prop_data(current_section,
                                 current_prop_data)

        self.sections = sections

    @staticmethod
    def _push_prop_data(section, prop_data):
        line0 = prop_data[0]
        if line0[0:2] != "  ":
            raise ValueError("'prop_data' does not start as a property: %s" % line0)
        prop_id_str = line0[2:10]
        if len(prop_id_str.translate(None, string.hexdigits)) != 0:
            raise ValueError("8 first characters in 'prop_data' are"
                             " not hex digits: '%s'" % prop_id_str)
        prop_id = int(prop_id_str, 16)
        if prop_id > 0x80000000:
            guid = prop_data[2][10:]
            line3 = prop_data[3]
            proptype = line3[4:8]
            if proptype == "name":
                col_idx = line3.find(":", 9)
                dispid = line3[col_idx+2:]
            else:
                dispid = line3[14:]
            value = prop_data[4:]
            section.named_properties[guid+dispid] = "\n".join(value)
        else:
            # non-named properties are identified by their property 
            value = [line0[11:]]
            if len(prop_data) > 1:
                value.extend(prop_data[1:])
            section.properties["%.8x" % prop_id] = "\n".join(value)

    def compare(self, other_stream):
        if not isinstance(other_stream, SyncStream):
            raise TypeError("'other_stream' must be a SyncStream")

        # ensure streams have been parsed
        if self.sections is None:
            self._parse()
        if other_stream.sections is None:
            other_stream._parse()

        # only fetch the sections that are common to both streams
        common_sections = compare_dict_keys(self.sections,
                                            other_stream.sections,
                                            "section")
        for section_name in common_sections:
            print "* comparing sections '%s'" % section_name
            self.sections[section_name].compare(other_stream.sections[section_name])


if __name__ == "__main__":
    SyncStream(sys.argv[1]).compare(SyncStream (sys.argv[2]))
