#!/usr/bin/env python2.7
###################################################
#
# script that produces stub entries for missing properties passed as parameter in the form oldguid:olid
#   example:
#       ./append-prop.py 00062008-0000-0000-c000-000000000046:858d outputs this:
#       PidLidUnknownProperty858d	LID_UNKNOWN_PROPERTY_858d	0x858d	NULL	PT_NULL	MNID_ID	PSETID_Common	0x84f5
#
# As the MAPI data type is unknown, it needs to be filled in as requests are performed by outlook on the mapped id.
# Also, note that the output is performed on stdout.
#
# Copyright (C) Wolfgang Sourdeau 2010
# released under the GNU GPL v2
#

import sys

filename = "mapi-named-properties"

def getLineID(line):
    idPos = line.rfind("0x")
    if idPos < len(line) - 8:
        lineID = -1
    else:
        lineID = int(line[idPos:], 16)

    return lineID

def getLastID(filename):
    propFile = open(filename)
    lines = propFile.readlines()
    propFile.close()
    lastID = -1
    for line in lines:
        line = line.strip()
        if len(line) > 0 and line[0] != '#' and line[0] != '#' and line[0] != ' ':
            lineID = getLineID(line)
            if lineID > lastID:
                lastID = lineID

    return lastID

guidArray = [("PS_MAPI",                "00020328-0000-0000-c000-000000000046"),
             ("PS_PUBLIC_STRINGS",      "00020329-0000-0000-c000-000000000046"),
             ("PS_INTERNET_HEADERS",    "00020386-0000-0000-c000-000000000046"),
             ("PSETID_Appointment",     "00062002-0000-0000-c000-000000000046"),
             ("PSETID_Task",            "00062003-0000-0000-c000-000000000046"),
             ("PSETID_Address",         "00062004-0000-0000-c000-000000000046"),
             ("PSETID_Common",          "00062008-0000-0000-c000-000000000046"),
             ("PS_UNKNOWN_0006200b_0000_0000_c000_000000000046", "0006200b-0000-0000-c000-000000000046"),
             ("PSETID_Report",          "00062013-0000-0000-c000-000000000046"),
             ("PSETID_Remote",          "00062014-0000-0000-c000-000000000046"),
             ("PSETID_Sharing",         "00062040-0000-0000-c000-000000000046"),
             ("PSETID_PostRss",         "00062041-0000-0000-c000-000000000046"),
             ("PSETID_Log",             "0006200a-0000-0000-c000-000000000046"),
             ("PSETID_Note",            "0006200e-0000-0000-c000-000000000046"),
             ("PSETID_Meeting",         "6ed8da90-450b-101b-98da-00aa003f1305"),
             ("PSETID_Messaging",       "41f28f13-83f4-4114-a584-eedb5a6b0bff"),
             ("PSETID_UnifiedMessaging", "4442858e-a9e3-4e80-b900-317a210cc15b"),
             ("PSETID_AirSync",          "71035549-0739-4dcb-9163-00f0580dbbdf")]

guidMapping = {}
for guidTpl in guidArray:
    guidMapping[guidTpl[1]] = guidTpl[0]

def canonicalizeName(name):
    for sep in [ ":", "/" ]:
        sepIdx = name.rfind(sep)
        if sepIdx > -1:
            name = name[sepIdx+1:]
    nameArray = name.split("-")
    name = "".join([x.capitalize() for x in nameArray])

    return name

def genNewLine(olID, nextID):
    colIdx = olID.find(":")
    if colIdx == -1:
        raise Exception, "Property line must contain a namespace id and a property name/id"

    mappingKey = olID[0:colIdx]
    if guidMapping.has_key(mappingKey):
        domain = guidMapping[mappingKey]
    else:
        domain = "PS_UNKNOWN_%s" % mappingKey.replace("-", "_")

    olPropID = olID[colIdx+1:]
    try:
        int(olPropID, 16)
        isdigit = True
    except:
        isdigit = False

    if len(olPropID) == 4 and isdigit:
        propKind = "MNID_ID"
        propId = "0x%s" % olPropID.lower()
        propName = "NULL"
        OOM = "LID_UNKNOWN_PROPERTY_%s" % olPropID.lower()
        canonicalName = "PidLidUnknownProperty%s" % olPropID.lower()
    else:
        propKind = "MNID_STRING"
        propId = "0x0000"
        propName = olPropID
        OOM = "NULL"
        if olPropID[0:4] == "urn:":
            canonicalName = "NULL"
        else:
            canonicalName = "PidName%s" % canonicalizeName(olPropID)
    propType = "PT_NULL"

    return "%s\t%s\t%s\t%s\t%s\t%s\t%s\t0x%.4x" \
        % (canonicalName, OOM, propId, propName, propType, propKind, domain, nextID)

    # PidLidServerProcessed				       ExchangeProcessed		    0x85CC NULL			  PT_BOOLEAN	MNID_ID	    PSETID_Common
    # PidNameCalendarAttendeeRole			       NULL			            0x0000 CalendarAttendeeRole	  PT_LONG	MNID_STRING PS_PUBLIC_STRINGS

def main():
    lastID = getLastID(filename)
    # print "lastID: %.4x" % lastID
    if lastID > -1:
        nextID = lastID + 1
        # newLines = []
        if len(sys.argv) > 1:
            print "### generated via append-prop.py"
            for i in xrange(1, len(sys.argv)):
                newLine = genNewLine(sys.argv[i], nextID)
                print newLine
                nextID = nextID + 1

if __name__ == "__main__":
    main()
