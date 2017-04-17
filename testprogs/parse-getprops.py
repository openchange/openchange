#!/usr/bin/env python2.7
#
# parse-getprops.py
#
# Copyright (C) Wolfgang Sourdeau 2010
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
# RopGetProps in human readable form
# usage: ./parse-getprops.py output.log

import struct
import sys
import time

PARSER_START, PARSER_PREREQUEST, PARSER_REQUEST, PARSER_BETWEEN, PARSER_PRERESPONSE, PARSER_RESPONSE, PARSER_DONE = range(7)

class GetPropsParser:
    def __init__(self, lines, pos = 0):
        self.lines = lines
        self.columns = []
        self.state = PARSER_START
        self.pos = pos
        self.layout = False
        self.response = None
        self.responses = []

    def run(self):
        while self.pos < len(self.lines) and self.state != PARSER_DONE:
            line = self.lines[self.pos]
            if self.state == PARSER_START:
                if line.find("mapi_GetProps: struct GetProps_req") > -1:
                    self.state = PARSER_PREREQUEST
            elif self.state == PARSER_PREREQUEST:
                if line.find("properties: ARRAY(") > -1:
                    self.state = PARSER_REQUEST
            elif self.state == PARSER_REQUEST:
                match = "properties               : ";
                matchIdx = line.find(match)
                if matchIdx > -1:
                    propIdx = matchIdx + len(match)
                    self._registerColumn(line[propIdx:])
                else:
                    self.state = PARSER_BETWEEN
            elif self.state == PARSER_BETWEEN:
                if line.find("mapi_GetProps: struct GetProps_repl") > -1:
                    self.state = PARSER_PRERESPONSE
            elif self.state == PARSER_PRERESPONSE:
                if line.find("prop_data") > -1:
                    self.state = PARSER_RESPONSE
                    self.response = ""
                elif line.find("layout") > -1:
                    self.layout = (line.find(": 0x01") > -1)
            elif self.state == PARSER_RESPONSE:
                if line.find("[") == 0:
                    # print "appending %s" % line[0:-1]
                    self._appendResponse(line)
                else:
                    self._printResponse()
                    self.response = None
                    self.layout = False
                    self.state = PARSER_DONE

            self.pos = self.pos + 1

        if self.state != PARSER_DONE:
            self.pos = -1

        return self.pos

    def _registerColumn(self, remain):
        spaceIdx = remain.find(" ")
        propName = remain[0:spaceIdx]
        parenCloseIdx = remain.find(")")
        propTag = remain[spaceIdx+2:parenCloseIdx]
        propTagInt = int(propTag, 16)
        if propName == "UNKNOWN_ENUM_VALUE":
            propName = propTag
        newColumn = { "name": propName, "tag": propTagInt }
        self.columns.append(newColumn)

    def _appendResponse(self, line):
        pos = 6;
        maxLength = 56
        done = False
        while not done:
            it = 0
            while it < 8 and not done:
                responseByte = line[pos+1:pos+3]
                if pos > maxLength or responseByte.strip() == "":
                    done = True
                else:
                    self._appendResponseByte(responseByte)
                    it = it + 1
                    pos = pos + 3
            pos = pos + 2

    def _appendResponseByte(self, byteData):
        charValue = chr(int(byteData, 16))
        self.response = self.response + charValue

    def _printResponse(self):
        length = len(self.response)
        pos = 0
        for column in self.columns:
            pos = pos + self._printResponseColumn(column, pos)
            if self.layout:
                pos = pos + 1

    def _printResponseColumn(self, column, pos):
        colType = column["tag"] & 0xffff;

        print "pos: %d" % pos
        if self.layout:
            flag = ord(self.response[pos])
            pos = pos + 1
            if flag == 0x0a:
                colType = 0x0a
            elif flag != 0x00:
                raise Exception, "Unhandled flag value: %d" % flag
        else:
            flag = 0

        print "%s:" % column["name"],

        return self._printValue(pos, colType)

    def _printValue(self, pos, colType):
        if colType == 0xb:
            consumed = self._printBool(pos)
        elif colType == 0x2:
            consumed = self._printShort(pos)
        elif colType == 0x3:
            consumed = self._printLong(pos, False)
        elif colType == 0x5:
            consumed = self._printDouble(pos)
        elif colType == 0x0a:
            consumed = self._printLong(pos, True)
        elif colType == 0x14:
            consumed = self._printLong8(pos)
        elif colType == 0x1f:
            consumed = self._printString(pos)
        elif colType == 0x40:
            consumed = self._printSysTime(pos)
        elif (colType == 0x0102 or colType == 0xfb):
            consumed = self._printBinary(pos)
        elif (colType & 0x1000):
            consumed = self._printMultiValue(pos, colType)
        else:
            raise Exception, "Unhandled column type: 0x%.4x" % colType

        return consumed

    def _printBool(self, pos):
        print "(PT_BOOL)",
        if self.response[pos] == "\0":
            print "false"
        else:
            print "true"

        return 1

    def _printShort(self, pos):
        shortValue = struct.unpack_from("<H", self.response, pos)[0]
        print "(PT_SHORT) 0x%.4x; %d" % (shortValue, shortValue)

        return 2

    def _printDouble(self, pos):
        doubleValue = struct.unpack_from("<d", self.response, pos)[0]

        print "(PT_DOUBLE) %df" % doubleValue

        return 8

    def _printLong(self, pos, error = False):
        longValue = struct.unpack_from("<L", self.response, pos)[0]

        if error:
            longType = "PT_ERROR"
        else:
            longType = "PT_LONG"

        print "(%s) 0x%.8x" % (longType, longValue)

        return 4

    def _printLong8(self, pos):
        long8Value = struct.unpack_from("<Q", self.response, pos)[0]

        print "(PT_I8) 0x%.16x" % long8Value

        return 8

    def _printString(self, pos):
        length = 0
        stringValue = ""
        while (ord(self.response[pos + length]) != 0
               or ord(self.response[pos + 1 +length]) != 0):
            stringValue = (stringValue
                           + self.response[pos + length]
                           + self.response[pos + length + 1])
            length = length + 2
        print "(PT_STRING, %d bytes): \"%s\"" % (length, stringValue.decode("utf-16"))

        return 2 + length

    def _printSysTime(self, pos):
        nano100Seconds = struct.unpack_from("<Q", self.response, pos)[0]
        seconds = (nano100Seconds / 10000000) - 11644473600
        try:
            print "(PT_SYSTIME) %s" % time.strftime("%a, %d %b %Y %T %z", time.localtime(seconds))
        except:
            print "(PT_SYSTIME) %d (seconds)" % seconds

        return 8

    def _printBinary(self, pos):
        length = struct.unpack_from("<H", self.response, pos)[0]
        pos = pos + 2
        print "(PT_BINARY, %d bytes)" % length
        limit = pos + length
        while pos < limit:
            delta = limit - pos
            if delta > 8:
                deltaSpace = ""
                delta = 8
            else:
                deltaSpace = "   " * (8 - delta)
            codes = []
            chars = ""
            for x in xrange(delta):
                ordX = ord(self.response[pos + x])
                codes.append("%.2x" % ordX)
                if ordX > 31 and ordX < 128:
                    chars = chars + "%3s  " % self.response[pos + x]
                else:
                    chars = chars + "\\x%.2x " % ordX
            print "%s  %s|  %s" % (" ".join(codes), deltaSpace, chars)
            pos = pos + delta

        return 2 + length

    def _printMultiValue(self, pos, colType):
        length = struct.unpack_from("<L", self.response, pos)[0]
        subtype = colType & 0x0fff
        print "multivalue (%d, 0xX%.3x)" % (length, subtype)
        consumed = 4
        for x in xrange(length):
            consumed = consumed + self._printValue(pos + consumed, subtype)

        return consumed

if __name__ == "__main__":
    if len(sys.argv) > 1:
        logfile = open(sys.argv[1], "r")
        lines = logfile.readlines()
        logfile.close()
        pos = 0
        while pos < len(lines) and pos != -1:
            print "parsing getprops"
            p = GetPropsParser(lines, pos)
            pos = p.run()
            print ""
