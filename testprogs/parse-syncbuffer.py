#!/usr/bin/env python2.7
#
# parse-syncbuffer.py
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
# RopFastTransferSourceGetBuffer in human readable form
# usage: ./parse-syncbuffer.py output.log
# warning: 'PropFile' should be adjusted below

import codecs
import struct
import sys
import time

from globset import GLOBSetRunner

PARSER_START, PARSER_PRERESPONSE, PARSER_RESPONSE, PARSER_DONE = range(4)

MarkerTags = { 0x40090003: "PidTagStartTopFld",
               0x400B0003: "PidTagEndFolder",
               0x400A0003: "PidTagStartSubFld",
               0x400C0003: "PidTagStartMessage",
               0x400D0003: "PidTagEndMessage",
               0x40100003: "PidTagStartFAIMsg",
               0x40010003: "PidTagStartEmbed",
               0x40020003: "PidTagEndEmbed",
               0x40030003: "PidTagStartRecip",
               0x40040003: "PidTagEndToRecip",
               0x40000003: "PidTagNewAttach",
               0x400E0003: "PidTagEndAttach",
               0x40120003: "PidTagIncrSyncChg",
               0x407D0003: "PidTagIncrSyncChgPartial",
               0x40130003: "PidTagIncrSyncDel",
               0x40140003: "PidTagIncrSyncEnd",
               0x402F0003: "PidTagIncrSyncRead",
               0x403A0003: "PidTagIncrSyncStateBegin",
               0x403B0003: "PidTagIncrSyncStateEnd",
               0x4074000B: "PidTagIncrSyncProgressMode",
               0x4075000B: "PidTagIncrSyncProgressPerMsg",
               0x40150003: "PidTagIncrSyncMessage",
               0x407B0102: "PidTagIncrSyncGroupInfo",
               0x40180003: "PidTagFXErrorInfo" }
ICSStateProperties = { 0x40170003: "PidTagIdsetGiven",
                       0x67960102: "PidTagCnsetSeen",
                       0x67da0102: "PidTagCnsetSeenFAI",
                       0x67d20102: "PidTagCnsetRead" }
DeltaStateProperties = { 0x67e50102: "PidTagIdsetDeleted",
                         0x40210102: "PidTagIdsetNoLongerInScope",
                         0x67930102: "PidTagIdsetExpired",
                         0x402d0102: "PidTagIdsetRead",
                         0x402e0102: "PidTagIdsetUnread" }
PropFile = "/home/wsourdeau/src/openchange/libmapi/conf/mapi-properties"
Properties = None
PropertyTypes = { 0x0000: "PT_UNSPECIFIED",
                  0x0001: "PT_NULL",
                  0x0002: "PT_SHORT",
                  0x0003: "PT_LONG",
                  0x0005: "PT_DOUBLE",
                  0x000a: "PT_ERROR",
                  0x000b: "PT_BOOL",
                  0x0014: "PT_LONG8",
                  0x001e: "PT_STRING",
                  0x001f: "PT_UNICODE",
                  0x0040: "PT_SYSTIME",
                  0x0048: "PT_CLSID",
                  0x00fb: "PT_SVREID",
                  0x0102: "PT_BINARY" }

def ReadProperties(filename):
    props = {}
    pFile = open(PropFile, "r")
    pLines = pFile.readlines()
    for line in pLines:
        if line.startswith("0x"):
            idx = line.find(" ")
            propTag = int(line[0:idx], 16)
            while line[idx] == " ":
                idx = idx + 1
            nameEndIdx = line.find(" ", idx)
            if nameEndIdx < 0:
                nameEndIdx = line.find("\t", idx)
                if nameEndIdx < 0:
                    nameEndIdx = len(line) - 1
            propName = line[idx:nameEndIdx]
            props[propTag] = propName
            propType = propTag & 0xffff
            if propType == 0x1e:
                propTag = (propTag & 0xffff0000 | 0x1f)
                propName = propName + "_UNICODE"
                props[propTag] = propName

    return props

class SyncBufferParser:
    def __init__(self, lines, pos = 0):
        self.lines = lines
        self.state = PARSER_START
        self.pos = pos
        self.lastBlock = False
        self.buffer = None
        self.bufferLines = None

    def run(self):
        self.bufferLines = []
        state_methods = { PARSER_START: self._doParserStart,
                          PARSER_PRERESPONSE: self._doParserPreResponse,
                          PARSER_RESPONSE: self._doParserResponse }
                          
        while self.pos < len(self.lines) and self.state != PARSER_DONE:
            state_methods[self.state](self.lines[self.pos])
            self.pos = self.pos + 1

        if self.state == PARSER_DONE:
            self.buffer = ''.join(self.bufferLines)
        else:
            self.pos = -1

        return self.pos

    def force(self):
        self.buffer = ''.join(self.bufferLines)

    def _doParserStart(self, line):
        if line.find("mapi_FastTransferSourceGetBuffer: struct FastTransferSourceGetBuffer_repl") > -1:
            self.state = PARSER_PRERESPONSE

    def _doParserPreResponse(self, line):
        if line.find("TransferStatus") > -1:
            if line.find("TransferStatus_Done") > -1:
                self.lastBlock = True
        elif line.find("DATA_BLOB") > -1:
            self.state = PARSER_RESPONSE

    def _doParserResponse(self, line):
        if line.find("[") == 0:
            # print "appending %s" % line[0:-1]
            self._appendResponse(line)
        else:
            if self.lastBlock:
                self.state = PARSER_DONE
            else:
                self.state = PARSER_START

    def _appendResponse(self, line):
        pos = 6;
        maxLength = 56
        done = False
        parsed = []
        while not done:
            it = 0
            while it < 8 and not done:
                responseByte = line[pos+1:pos+3]
                if pos > maxLength or responseByte.strip() == "":
                    done = True
                else:
                    charValue = chr(int(responseByte, 16))
                    parsed.append(charValue)
                    it = it + 1
                    pos = pos + 3
            pos = pos + 2
        self.bufferLines.append(''.join(parsed))

class SyncBufferPrinter:
    def __init__(self, data):
        self.data = data

    def run(self):
        length = len(self.data)
        pos = 0
        while pos < length:
            tagstr = self.data[pos:pos+4]
            tag = struct.unpack("<L", tagstr)[0]
            pos = pos + 4
            pos = pos + self._printResponseColumn(tag, pos)

    def _printResponseColumn(self, tag, pos):
        if MarkerTags.has_key(tag):
            marker_name = MarkerTags[tag]
            print "* %.8x (%s)" % (tag, marker_name)
            consumed = 0
        elif ICSStateProperties.has_key(tag):
            property_name = ICSStateProperties[tag]
            print "ICS %.8x (%s):" % (tag, property_name),
            consumed = self._printIDSet(pos)
        elif DeltaStateProperties.has_key(tag):
            property_name = DeltaStateProperties[tag]
            print "Delta %.8x (%s):" % (tag, property_name),
            consumed = self._printIDSet(pos, True)
        else:
            colType = tag & 0x0fff
            # print "  pos: %d, length: %d, TAG: %.8x, " % (pos, len(self.data), tag)
            prefix = "  %.8x (" % tag
            if Properties.has_key(tag):
                prefix = prefix + "%s, " % Properties[tag]
            if tag & 0x1000:
                multiState = 0x1000
                prefix = prefix + "MULTI-"
            else:
                multiState = 0x0
            prefix = prefix + "%s):" % PropertyTypes[colType]
            print prefix,
            # PidTagSourceKey, PidTagChangeKey or PidTagParentSourceKey
            if tag == 0x65e00102 or tag == 0x65e20102 or tag == 0x65e10102:
                consumed = self._printBinXID(pos)
            elif tag == 0x65e30102: # PidTagPredecessorChangeList
                consumed = self._printPredecessorChangeList(pos)
            else:
                if tag > 0x80000000:
                    consumed = self._printNamedPropValue(pos, multiState | colType)
                else:
                    consumed = self._printValue(pos, multiState | colType)

        return consumed

    def _printPredecessorChangeList(self, pos):
        length = struct.unpack_from("<L", self.data, pos)[0]
        print "(%d bytes)" % length
        pos = pos + 4
        maxPos = pos + length
        count = 1
        while pos < maxPos:
            print "    XID %d:" % count
            pos = pos + self._printSizedXID(pos)
            count = count + 1

        return 4 + length

    def _printSizedXID(self, pos):
        length = ord(self.data[pos])
        self._printXID(pos + 1, length)

        return length + 1

    def _printBinXID(self, pos):
        length = struct.unpack_from("<L", self.data, pos)[0]
        if length > 0:
            print "XID (%d):" % length
            self._printXID(pos + 4, length)
        else:
            print "(length: %d, skipped)" % length

        return 4 + length

    def _printXID(self, pos, size):
        print "      namespace GUID:",
        pos = pos + self._printGUID(pos)

        if size > 16:
            localid = ""
            remain = size - 16
            while remain > 0:
                remain = remain - 1
                localid = localid + "%.2x" % ord(self.data[pos+remain])
            print "             localid: 0x%s" % localid
        else:
            print "(size: %d, skipped)" % size

    def _printNamedPropValue(self, pos, colType):
        print "\n    named prop\n    guid:",
        consumed = self._printGUID(pos)
        namedPropType = ord(self.data[pos + consumed])
        consumed = consumed + 1
        if namedPropType == 0x00:
            print "    dispid:",
            consumed = consumed + self._printValue(pos + consumed, 0x0003)
        elif namedPropType == 0x01:
            print "    name:",
            consumed = consumed + self._printNamedPropName(pos + consumed)
        else:
            raise Exception, "Invalid named prop type: %d" % namedPropType

        print "        ",
        consumed = consumed + self._printValue(pos + consumed, colType)

        return consumed

    def _printNamedPropName(self, pos):
        length = 0
        byteValue = ""
        while (ord(self.data[pos + length]) != 0
               or ord(self.data[pos + 1 + length]) != 0):
            byteValue = (byteValue
                         + self.data[pos + length]
                         + self.data[pos + length + 1])
            length = length + 2
        stringValue = byteValue.decode("utf-16")
        stringLength = len(stringValue)
        print "(%d chars): \"%s\"" % (stringLength, stringValue)

        return length + 2

    def _printValue(self, pos, colType):
        base_methods = { 0x0002: self._printShort,
                         0x0003: self._printLong,
                         0x0005: self._printDouble,
                         0x000a: self._printLong,
                         0x000b: self._printBool,
                         0x0014: self._printLong8,
                         0x001e: self._printString,
                         0x001f: self._printUnicode,
                         0x0040: self._printSysTime,
                         0x0048: self._printGUID,
                         0x00fb: self._printBinary,
                         0x0102: self._printBinary }
        if base_methods.has_key(colType):
            method = base_methods[colType]
            consumed = method(pos)
        elif (colType & 0x1000):
            consumed = self._printMultiValue(pos, colType)
        else:
            raise Exception, "Unhandled column type: 0x%.4x" % colType

        return consumed

    def _printShort(self, pos):
        shortValue = struct.unpack_from("<H", self.data, pos)[0]
        print "0x%.4x; %d" % (shortValue, shortValue)

        return 2

    def _printDouble(self, pos):
        doubleValue = struct.unpack_from("<d", self.data, pos)[0]
        print "%df" % doubleValue

        return 8

    def _printLong(self, pos):
        longValue = struct.unpack_from("<L", self.data, pos)[0]
        print "0x%.8x" % longValue

        return 4

    def _printBool(self, pos):
        if self.data[pos] == "\0":
            print "false"
        else:
            print "true"

        return 2

    def _printLong8(self, pos):
        long8Value = struct.unpack_from("<Q", self.data, pos)[0]
        print "0x%.16x" % long8Value

        return 8

    def _printString(self, pos):
        declared = struct.unpack_from("<L", self.data, pos)[0]
        length = 0
        byteValue = ""
        pos = pos + 4
        while (ord(self.data[pos + length]) != 0):
            byteValue = byteValue + self.data[pos + length]
            length = length + 1
        stringValue = byteValue.decode("iso-8859-1")
        stringLength = len(stringValue)
        print "(%d chars, %d declared): \"%s\"" % (stringLength, declared, stringValue)

        return 5 + length

    def _printUnicode(self, pos):
        declared = struct.unpack_from("<L", self.data, pos)[0]
        length = 0
        byteValue = ""
        pos = pos + 4
        while (ord(self.data[pos + length]) != 0
               or ord(self.data[pos + 1 + length]) != 0):
            byteValue = (byteValue
                         + self.data[pos + length]
                         + self.data[pos + length + 1])
            length = length + 2
        stringValue = byteValue.decode("utf-16")
        stringLength = len(stringValue)
        print "(%d chars, %d declared): \"%s\"" % (stringLength, declared, stringValue)

        return 6 + length

    def _printSysTime(self, pos):
        nano100Seconds = struct.unpack_from("<Q", self.data, pos)[0]
        if nano100Seconds > 0:
            seconds = (nano100Seconds / 10000000)
            if (seconds > 11644473600):
                print time.strftime("%a, %d %b %Y %T %z", time.localtime(seconds - 11644473600))
            else:
                print "%d seconds" % seconds
        else:
            print "empty time"

        return 8

    def _printGUID(self, pos):
        start = struct.unpack_from("<LHH", self.data, pos)
        final = struct.unpack_from(">HHL", self.data, pos + 8)
        print "{%.8x-%.4x-%.4x-%.4x-%.4x%.8x}" % (start + final)

        return 16

    def _printBinary(self, pos):
        length = struct.unpack_from("<L", self.data, pos)[0]
        pos = pos + 4
        print "(%d bytes)" % length
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
                value = self.data[pos + x]
                ordValue = ord(value)
                codes.append("%.2x" % ordValue)
                if ordValue > 31 and ordValue < 128:
                    chars = chars + "%3s  " % value
                else:
                    chars = chars + "\\x%.2x " % ordValue
            print "            %s  %s|  %s" % (" ".join(codes), deltaSpace, chars)
            pos = pos + delta

        return 4 + length

    def _printMultiValue(self, pos, colType):
        length = struct.unpack_from("<L", self.data, pos)[0]
        print "(%d entries)" % length
        subType = colType & 0x0fff
        consumed = 4
        for x in xrange(length):
            print "        %d>" % x,
            consumed = consumed + self._printValue(pos + consumed, subType)

        return consumed

    def _printIDSet(self, pos, short = False):
        length = struct.unpack_from("<L", self.data, pos)[0]
        print "(%d bytes)" % length
        pos = pos + 4
        limit = pos + length
        count = 0
        while pos < limit:
            print "  IDSET %s:" % count
            if short:
                print "    ReplID:",
                pos = pos + self._printShort(pos)
            else:
                print "    ReplGUID:",
                pos = pos + self._printGUID(pos)
            print "     GLOBSET:"
            pos = pos + self._printGLOBSET(pos)
            count = count + 1

        return 4 + length

    def _printGLOBSET(self, pos):
        runner = GLOBSetRunner(self.data, pos)
        consumed = runner.run()
        count = 0
        for r in runner.ranges:
            print "        %d: [0x%.12x:0x%.12x]" % ((count,) + r)
            count = count + 1

        return consumed

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if not sys.stdout.isatty():
            streamWriter = codecs.getwriter('utf8')
            sys.stdout = streamWriter(sys.stdout)
        if not sys.stderr.isatty():
            streamWriter = codecs.getwriter('utf8')
            sys.stderr = streamWriter(sys.stderr)

        Properties = ReadProperties(PropFile)

        logfile = open(sys.argv[1], "r")
        lines = logfile.readlines()
        logfile.close()
        pos = 0
        while pos < len(lines) and pos != -1:
            parser = SyncBufferParser(lines, pos)
            pos = parser.run()
            if pos == -1 and len(parser.bufferLines) > 0:
                parser.force()
                pos = 0xffffffff
                print "(forced parser)"

            if pos != -1:
                p = SyncBufferPrinter(parser.buffer)
                p.run()
                print ""
