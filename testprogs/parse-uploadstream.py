#!/usr/bin/env python2.7
#
# parse-uploadstream.py
#
# Copyright (C) Wolfgang Sourdeau 2011
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

# this script parses a debugging samba log and displays the replies to
# RopSyncUploadStateStream* in human readable form
# usage: ./parse-uploadstream.py output.log

import struct
import sys

from globset import GLOBSetRunner

PARSER_START,\
    PARSER_START_BEGINSTREAM,\
    PARSER_END_BEGINSTREAM,\
    PARSER_START_CONTINUESTREAM,\
    PARSER_CONTINUE_CONTINUESTREAM,\
    PARSER_END_CONTINUESTREAM,\
    PARSER_DONE = range(7)

class UploadStreamBufferParser:
    def __init__(self):
        self.bytes = []
        self.property = None
        self.state = PARSER_START

    def run(self, lines):
        count = 0
        maxlines = len(lines)
        methods = [ self._doStart,
                    self._doStartBeginStream, self._doEndBeginStream,
                    self._doStartContinueStream,
                    self._doContinueContinueStream ]

        while self.state != PARSER_DONE:
            line = lines[count]
            methods[self.state](line)
            count = count + 1

        return "".join(self.bytes)

    def _doStart(self, line):
        if line.find("struct SyncUploadStateStreamBegin_req") > -1:
            self.state = PARSER_START_BEGINSTREAM

    def _doStartBeginStream(self, line):
        if line.find("StateProperty") > -1:
            col_idx = line.find(":")
            property_end_idx = line.find(" ", col_idx + 2)
            self.property = line[col_idx+2:property_end_idx]
            print "property: %s" % self.property
            self.state = PARSER_END_BEGINSTREAM

    def _doEndBeginStream(self, line):
        if line.find("struct SyncUploadStateStreamContinue_req") > -1:
            self.state = PARSER_START_CONTINUESTREAM

    def _doStartContinueStream(self, line):
        if line.find("StreamData: ARRAY") > -1:
            self.state = PARSER_CONTINUE_CONTINUESTREAM

    def _doContinueContinueStream(self, line):
        bIdx = line.find("[")
        if bIdx > -1:
            byteIdx = line.find("0x", bIdx)
            ebIdx = line.find(" ", byteIdx)
            byteValue = int(line[byteIdx+2:ebIdx], 16)
            self.bytes.append(chr(byteValue))
        else:
            self.state = PARSER_DONE

def PrintUploadStream(data):
    dataLen = len(data)
    if dataLen < 16:
        raise Exception, "Data buffer is only %d bytes long" % dataLen

    start = struct.unpack_from("<LHH", data, 0)
    final = struct.unpack_from(">HHL", data, 8)
    print "{%.8x-%.4x-%.4x-%.4x-%.4x%.8x}" % (start + final)

    runner = GLOBSetRunner(data, 16)
    consumed = runner.run()
    count = 0
    for r in runner.ranges:
        print "        %d: [0x%.12x:0x%.12x]" % ((count,) + r)
        count = count + 1

if __name__ == "__main__":
    if len(sys.argv) > 1:
        logfile = open(sys.argv[1], "r")
        lines = logfile.readlines()
        logfile.close()
        parser = UploadStreamBufferParser()
        data = parser.run(lines)
        if data is not None:
            PrintUploadStream(data)
            print ""
