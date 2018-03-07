#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse
import difflib
import re
import string
import subprocess
import sys
import tempfile

knownpropsets = { "PSETID_PostRss" :           "{00062041-0000-0000-C000-000000000046}",
		  "PSETID_Sharing" :           "{00062040-0000-0000-C000-000000000046}",
		  "PS_PUBLIC_STRINGS" :        "{00020329-0000-0000-C000-000000000046}",
		  "PSETID_Common" :            "{00062008-0000-0000-C000-000000000046}",
		  "PSETID_Appointment" :       "{00062002-0000-0000-C000-000000000046}",
		  "PSETID_Address" :           "{00062004-0000-0000-C000-000000000046}",
		  "PSETID_Meeting" :           "{6ED8DA90-450B-101B-98DA-00AA003F1305}",
		  "PSETID_Log" :               "{0006200A-0000-0000-C000-000000000046}",
		  "PSETID_CalendarAssistant" : "{11000E07-B51B-40D6-AF21-CAA85EDAB1D0}",
		  "PSETID_Note" :              "{0006200E-0000-0000-C000-000000000046}",
		  "PSETID_Task":               "{00062003-0000-0000-C000-000000000046}",
		  "PS_INTERNET_HEADERS" :      "{00020386-0000-0000-C000-000000000046}",
		  "PSETID_UnifiedMessaging" :  "{4442858E-A9E3-4E80-B900-317A210CC15B}",
		  "PS_MAPI" :                  "{00020328-0000-0000-C000-000000000046}",
		  "PSETID_Attachment" :        "{96357f7f-59e1-47d0-99a7-46515c183b54}",
		  "PSETID_AirSync" :           "{71035549-0739-4DCB-9163-00F0580DBBDF}",
		  "PSETID_Messaging" :         "{41F28F13-83F4-4114-A584-EEDB5A6B0BFF}",
		  "PSETID_XmlExtractedEntities": "{23239608-685D-4732-9C55-4C95CB4E8E33}"
		  }

knowndatatypes = { "PtypInteger16" : "0x0002",
                   "PtypInteger32" : "0x0003",
                   "PtypFloating64" : "0x0005",
                   "PtypBoolean" : "0x000B",
                   "PtypEmbeddedTable" : "0x000D",
                   "PtypObject" : "0x000D",
                   "PtypString8" : "0x001E",
                   "PtypString" : "0x001F",
                   "PtypInteger64" : "0x0014",
                   "PtypBinary" : "0x0102",
                   "PtypTime" : "0x0040",
                   "PtypGuid" : "0x0048",
                   "PtypServerId" : "0x00FB",
                   "PtypRestriction" : "0x00FD",
                   "PtypRuleAction" : "0x00FE",
                   "PtypMultipleInteger32" : "0x1003",
                   "PtypMultipleString8" : "0x101E",
                   "PtypMultipleString" : "0x101F",
                   "PtypMultipleTime" : "0x1040",
                   "PtypMultipleBinary" : "0x1102",
                   }
datatypemap = { "PtypInteger16" : "PT_SHORT",
                "PtypInteger32" : "PT_LONG",
                "PtypFloating64" : "PT_DOUBLE",
                "PtypBoolean" : "PT_BOOLEAN",
                "PtypEmbeddedTable" : "PT_OBJECT",
                "PtypObject" : "PT_OBJECT",
                "PtypString8" : "PT_STRING8",
                "PtypString" : "PT_UNICODE",
                "PtypInteger64" : "PT_I8",
                "PtypBinary" : "PT_BINARY",
                "PtypTime" : "PT_SYSTIME",
                "PtypGuid" : "PT_CLSID",
                "PtypServerId" : "PT_SVREID",
                "PtypRestriction" : "PT_SRESTRICT",
                "PtypRuleAction" : "PT_ACTIONS",
                "PtypMultipleInteger32" : "PT_MV_LONG",
                "PtypMultipleString8" : "PT_MV_STRING8",
                "PtypMultipleString" : "PT_MV_UNICODE",
                "PtypMultipleTime" : "PT_MV_SYSTIME",
                "PtypMultipleBinary" : "PT_MV_BINARY"
                }

knownrefs = [
	"[MS-ASAIRS]",
	"[MS-ASCAL]",
	"[MS-ASCMD]",
	"[MS-ASCNTC]",
	"[MS-ASCON]",
	"[MS-ASDOC]",
	"[MS-ASDTYPE]",
	"[MS-ASEMAIL]",
	"[MS-ASHTTP]",
	"[MS-ASMS]",
	"[MS-ASNOTE]",
	"[MS-ASPROV]",
	"[MS-ASRM]",
	"[MS-ASTASK]",
	"[MS-ASWBXML]",
	"[MS-CAB]",
	"[MS-MCI]",
	"[MS-OXABREF]",
	"[MS-OXBBODY]",
	"[MS-OXCDATA]",
	"[MS-OXCETF]",
	"[MS-OXCFOLD]",
	"[MS-OXCFXICS]",
	"[MS-OXCHGTR]",
	"[MS-OXCICAL]",
	"[MS-OXCMAIL]",
	"[MS-OXCMSG]",
	"[MS-OXCNOTIF]",
	"[MS-OXCPERM]",
	"[MS-OXCPRPT]",
	"[MS-OXCROPS]",
	"[MS-OXCRPC]",
	"[MS-OXCSPAM]",
	"[MS-OXCSTOR]",
	"[MS-OXCSYNC]",
	"[MS-OXCTABL]",
	"[MS-OXDISCO]",
	"[MS-OXDOCO]",
	"[MS-OXDSCLI]",
	"[MS-OXGLOS]",
	"[MS-OXIMAP4]",
	"[MS-OXLDAP]",
	"[MS-OXMSG]",
	"[MS-OXMVMBX]",
	"[MS-OXOABK]",
	"[MS-OXOABKT]",
	"[MS-OXOAB]",
	"[MS-OXOCAL]",
	"[MS-OXOCFG]",
	"[MS-OXOCNTC]",
	"[MS-OXODLGT]",
	"[MS-OXODOC]",
	"[MS-OXOFLAG]",
	"[MS-OXOJRNL]",
	"[MS-OXOMSG]",
	"[MS-OXONOTE]",
	"[MS-OXOPFFB]",
	"[MS-OXOPOST]",
	"[MS-OXORMDR]",
	"[MS-OXORMMS]",
	"[MS-OXORSS]",
	"[MS-OXORULE]",
	"[MS-OXOSFLD]",
	"[MS-OXOSMIME]",
	"[MS-OXOSMMS]",
	"[MS-OXOSRCH]",
	"[MS-OXOTASK]",
	"[MS-OXOUM]",
	"[MS-OXPFOAB]",
	"[MS-OXPHISH]",
	"[MS-OXPOP3]",
	"[MS-OXPROPS]",
	"[MS-OXPROTO]",
	"[MS-OXPSVAL]",
	"[MS-OXREF]",
	"[MS-OXRTFCP]",
	"[MS-OXRTFEX]",
	"[MS-OXSHARE]",
	"[MS-OXSHRMSG]",
	"[MS-OXSMTP]",
	"[MS-OXTNEF]",
	"[MS-OXVCARD]",
	"[MS-OXWAVLS]",
	"[MS-OXWCONFIG]",
	"[MS-OXWMT]",
	"[MS-OXWOAB]",
	"[MS-OXWOOF]",
	"[MS-OXWSADISC]",
	"[MS-OXWSATT]",
	"[MS-OXWSAUTID]",
	"[MS-OXWSBTRF]",
	"[MS-OXWSCDATA]",
	"[MS-OXWSCONT]",
	"[MS-OXWSCONV]",
	"[MS-OXWSCORE]",
	"[MS-OXWSCVTID]",
	"[MS-OXWSDLGM]",
	"[MS-OXWSDLIST]",
	"[MS-OXWSFOLD]",
	"[MS-OXWSGTRM]",
	"[MS-OXWSGTZ]",
	"[MS-OXWSLVID]",
	"[MS-OXWSMSG]",
	"[MS-OXWSMSHR]",
	"[MS-OXWSMTGS]",
	"[MS-OXWSMTRK]",
	"[MS-OXWSNTIF]",
	"[MS-OXWSPOST]",
	"[MS-OXWSPSNTIF]",
	"[MS-OXWSRSLNM]",
	"[MS-OXWSRULES]",
	"[MS-OXWSSRCH]",
	"[MS-OXWSSYNC]",
	"[MS-OXWSTASK]",
	"[MS-OXWSUSRCFG]",
	"[MS-OXWSXPROP]",
	"[MS-OXWUMS]",
	"[MS-PATCH]",
	"[MS-XJRNL]",
	"[MS-XLOGIN]",
	"[MS-XWDCAL]",
	"[MS-XWDCNTC]",
	"[MS-XWDDOC]",
	"[MS-XWDEXT]",
	"[MS-XWDFOLD]",
	"[MS-XWDMAIL]",
	"[MS-XWDNOTIF]",
	"[MS-XWDREPL]",
	"[MS-XWDSEARCH]",
	"[MS-XWDSTRUCTDOC]",
	"[MS-XWDVSEC]",
	"[MS-NSPI]"
]

knownareas = [
        "AB Container",
        "Access Control Properties",
        "Access Control Properties Property set",
        "Address book",
        "Address Book",
        "Address Properties",
        "Address Properties Property set",
        "Appointment Property set",
        "Appointment",
        "Archive",
        "BestBody",
        "Calendar",
        "Calendar Document",
        "Calendar Document Property set",
        "Calendar Property set",
        "Common",
        "Common Property set",
        "Conferencing",
        "Configuration",
        "Conflict Note",
        "Contact Properties",
        "Container Properties",
        "Container Properties Property set",
        "Conversation Actions",
        "Conversations",
        "Email",
        "E-mail",
        "Email Property set",
        "Exchange",
        "Exchange Administrative",
        "ExchangeAdministrative",
        "ExchangeAdministrative Property set",
        "ExchangeFolder",
        "ExchangeFolder Property set",
        "ExchangeMessageReadOnly",
        "ExchangeMessageStore",
        "ExchangeNonTransmittableReserved",
        "Exchange Profile Configuration",
        "Exchange Property set",
	"Extracted Entities",
        "Flagging",
        "Folder Properties",
        "Free/Busy Properties",
        "General Message Properties",
        "General Message Properties Property set",
        "General Report Properties",
        "History Properties",
        "IC",
        "ICS",
        "ID Properties",
        "ID Properties Property set",
        "Journal",
        "Mail",
        "MapiAddressBook",
        "MapiAttachment",
        "MapiCommon",
        "MapiContainer",
        "MAPI Display Tables",
        "MapiEnvelope",
        "MapiEnvelope Property set",
        "MapiMailUser",
        "MapiMessage",
        "MapiMessageStore",
        "MapiNonTransmittable",
        "MapiNonTransmittable Property set",
        "MapiRecipient",
        "MapiStatus",
        "Meeting Response",
        "Meetings",
        "Message Attachment Properties",
        "Message Attachment Properties Property set",
        "MessageClassDefinedNonTransmittable",
        "Message Class Defined Transmittable",
        "MessageClassDefinedTransmittable",
        "Message Properties",
        "Message Properties Property set",
        "Message Store Properties",
        "Message Time Properties",
        "Message Time Properties Property set",
        "MIME properties",
        "MIME Properties",
        "MIME Properties Property set",
        "Miscellaneous Properties",
        "Miscellaneous Properties Property set",
        "Offline Address Book Properties",
        "Outlook Application",
        "ProviderDefinedNonTransmittable",
        "PST Internal",
        "Reminders",
        "RenMessageFolder",
        "RSS",
        "Rules",
        "Run-time configuration",
        "Search",
        "Secure Messaging",
        "Secure Messaging Properties",
        "Server",
        "Server-side Rules Properties",
        "Server-Side Rules Properties",
        "Sharing",
	"Site Mailbox",
        "SMS",
        "Spam",
        "Sticky Notes",
        "Structured Documents",
        "Structured Documents Property set",
        "Sync",
        "Table Properties",
        "Tasks",
        "Transport Envelope",
        "TransportEnvelope",
        "TransportRecipient",
        "UM",
        "Unified Messaging"
]

properties = []

def make_properties_list(propsfilename):
	next_num = 1
	propname = ""
	propertyinfo = {}
	propsfile = file(propsfilename)

	for line in propsfile:
		if line.startswith("2     Structures"):
			break

	for line in propsfile:
		if line.startswith("2."):
			section_num = line.split()[0]
			sub_section_num = section_num.split(".")[1]
			if int(sub_section_num) != next_num:
				print "expected", next_num, "got", sub_section_num
			next_num += 1
			propname = line.split()[1]
			if propertyinfo.has_key("CanonicalName"):
				properties.append(propertyinfo.copy())
				propertyinfo = {}
			
		if line.strip().startswith("Canonical name:"):
			canonicalname = line.strip().split(":")[1].strip()
			if ((propname != "") and (propname != canonicalname)):
				print "expected", propname, "got", canonicalname
			propertyinfo["CanonicalName"] = canonicalname

		if line.strip().startswith("Property name:"):
			propertyname = line.split(":", 1)
			propertyinfo["PropertyName"] = propertyname[1].strip()

		if line.strip().startswith("Description:"):
			description = line.strip().split(":")[1].strip()
			while True:
				nextline = propsfile.next().strip()
				if (nextline.isspace() or (len(nextline) == 0)):
					break
				description += (" " + nextline)
			propertyinfo["Description"] = description

		if line.strip().startswith("Alternate names:"):
			altname = line.strip().partition(":")[2]
			while True:
				nextline = propsfile.next().strip()
				if (nextline.isspace() or (len(nextline) == 0)):
					break
				altname += (", " + nextline)
			propertyinfo["AlternateNames"] = altname

		if line.strip().startswith("Data type:"):
			datatype = line.strip().split(":")[1].strip()
			datatype_values = datatype.split(",")
			if len(datatype_values) >= 2:
				datatypename, datatypeval = datatype_values[:2]
				propertyinfo["DataTypeName"] = datatypename.strip()
				# There are three props which does not work very well. Examples:
				# PtypString8, 0x001EPtypEmbeddedTable, 0x000D
				# or
				# PtypString8, 0x001E; PtypEmbeddedTable, 0x000D
				propertyinfo["DataTypeValue"] = datatypeval.strip()[:6]
			else:
				sys.stderr.write("Too few types in %s\n" % line)
				continue

		if line.strip().startswith("Property set:"):
			propset = line.strip().split(":")[1].strip()
			if propset.find(" ") != -1:
				propset = propset.replace(" - ", '-')
				propsetname, propsetval = propset.split(" ")
				propertyinfo["PropertySet"] = propsetname.strip()
				propertyinfo["PropertySetValue"] = propsetval.strip()

		if line.strip().startswith("Property ID:"):
			propid = line.strip().split(":")[1].strip()
			if propid.startswith("0x"):
				propertyinfo["PropertyId"] = int(propid, 16)
			else:
				print "In section 2.%(section)i (%(propname)s):" % { 'section': (next_num -1), 'propname': propname }
				print "\t", propid, "doesn't appear to have correct (hex) format"

		if line.strip().startswith("Property long ID (LID):"):
			proplid = line.strip().split(":")[1].strip()
			if proplid.startswith("0x"):
				propertyinfo["PropertyLid"] = int(proplid, 16)
			else:
				print "In section 2.%(section)i (%(propname)s):" % { 'section': (next_num -1), 'propname': propname }
				print "\t", proplid, "doesn't appear to have correct (hex) format"

		if line.strip().startswith("Area:"):
			areaname = line.strip().split(":")[1].strip()
			if (knownareas.count(areaname) == 1):
				propertyinfo["Area"] = areaname
			else:
				print "In section 2.%(section)i (%(propname)s):" % { 'section': (next_num -1), 'propname': propname }
				print "\t", areaname, "isn't an expected area name (typo?)"

		if line.strip().startswith("References:") or line.strip().startswith("Reference:"):
			references = line.strip().split(":")[1].strip()
			while (1):
				nextline = propsfile.next().strip()
				if (nextline.isspace() or (len(nextline) == 0)):
					break
				references += (nextline)
			propertyinfo["References"] = references

		if line.strip().startswith("Defining Reference:") or line.strip().startswith("Defining reference:") or line.strip().startswith("Defining references"):
			reference = line.strip().split(":")[1].strip()
			propertyinfo["DefiningReference"] = reference

		propertyinfo["OXPROPS_Sect"] = "2.%i" % (next_num -1)

	#The whole file should now be parsed
	properties.append(propertyinfo)
	# sanity check
	print "Last section parsed was section 2.%(section)i" % { 'section': (next_num-1) }

# Debugging dump of everything
def debug_dump():
	for entry in properties:
		print entry

extra_private_tags_struct = """\t{ PidTagFolderChildCount,                                             PT_LONG,      \"PidTagFolderChildCount\"                                            },
"""

temporary_private_tags = """
#define openchange_private_ROOT_FOLDER_FID                  PROP_TAG(PT_I8        , 0xd001) /* 0xd0010014 */
#define openchange_private_ROOT_FOLDER_FID_ERROR            PROP_TAG(PT_ERROR     , 0xd001) /* 0xd001000a */
#define openchange_private_DEFERRED_ACTIONS_FID             PROP_TAG(PT_I8        , 0xd002) /* 0xd0020014 */
#define openchange_private_DEFERRED_ACTIONS_FID_ERROR       PROP_TAG(PT_ERROR     , 0xd002) /* 0xd002000a */
#define openchange_private_SPOOLER_QUEUE_FID                PROP_TAG(PT_I8        , 0xd003) /* 0xd0030014 */
#define openchange_private_SPOOLER_QUEUE_FID_ERROR          PROP_TAG(PT_ERROR     , 0xd003) /* 0xd003000a */
#define openchange_private_IPM_SUBTREE_FID                  PROP_TAG(PT_I8        , 0xd004) /* 0xd0040014 */
#define openchange_private_IPM_SUBTREE_FID_ERROR            PROP_TAG(PT_ERROR     , 0xd004) /* 0xd004000a */
#define openchange_private_INBOX_FID                        PROP_TAG(PT_I8        , 0xd005) /* 0xd0050014 */
#define openchange_private_INBOX_FID_ERROR                  PROP_TAG(PT_ERROR     , 0xd005) /* 0xd005000a */
#define openchange_private_OUTBOX_FID                       PROP_TAG(PT_I8        , 0xd006) /* 0xd0060014 */
#define openchange_private_OUTBOX_FID_ERROR                 PROP_TAG(PT_ERROR     , 0xd006) /* 0xd006000a */
#define openchange_private_SENT_ITEMS_FID                   PROP_TAG(PT_I8        , 0xd007) /* 0xd0070014 */
#define openchange_private_SENT_ITEMS_FID_ERROR             PROP_TAG(PT_ERROR     , 0xd007) /* 0xd007000a */
#define openchange_private_DELETED_ITEMS_FID                PROP_TAG(PT_I8        , 0xd008) /* 0xd0080014 */
#define openchange_private_DELETED_ITEMS_FID_ERROR          PROP_TAG(PT_ERROR     , 0xd008) /* 0xd008000a */
#define openchange_private_COMMON_VIEWS_FID                 PROP_TAG(PT_I8        , 0xd009) /* 0xd0090014 */
#define openchange_private_COMMON_VIEWS_FID_ERROR           PROP_TAG(PT_ERROR     , 0xd009) /* 0xd009000a */
#define openchange_private_SCHEDULE_FID                     PROP_TAG(PT_I8        , 0xd00a) /* 0xd00a0014 */
#define openchange_private_SCHEDULE_FID_ERROR               PROP_TAG(PT_ERROR     , 0xd00a) /* 0xd00a000a */
#define openchange_private_SEARCH_FID                       PROP_TAG(PT_I8        , 0xd00b) /* 0xd00b0014 */
#define openchange_private_SEARCH_FID_ERROR                 PROP_TAG(PT_ERROR     , 0xd00b) /* 0xd00b000a */
#define openchange_private_VIEWS_FID                        PROP_TAG(PT_I8        , 0xd00c) /* 0xd00c0014 */
#define openchange_private_VIEWS_FID_ERROR                  PROP_TAG(PT_ERROR     , 0xd00c) /* 0xd00c000a */
#define openchange_private_SHORTCUTS_FID                    PROP_TAG(PT_I8        , 0xd00d) /* 0xd00d0014 */
#define openchange_private_SHORTCUTS_FID_ERROR              PROP_TAG(PT_ERROR     , 0xd00d) /* 0xd00d000a */
#define openchange_private_MailboxGUID                      PROP_TAG(PT_CLSID     , 0xd00e) /* 0xd00e0048 */
#define openchange_private_MailboxGUID_ERROR                PROP_TAG(PT_ERROR     , 0xd00e) /* 0xd00e000a */
#define openchange_private_ReplicaID                        PROP_TAG(PT_SHORT     , 0xd00f) /* 0xd00f0002 */
#define openchange_private_ReplicaID_ERROR                  PROP_TAG(PT_ERROR     , 0xd00f) /* 0xd00f000a */
#define openchange_private_ReplicaGUID                      PROP_TAG(PT_CLSID     , 0xd010) /* 0xd0100048 */
#define openchange_private_ReplicaGUID_ERROR                PROP_TAG(PT_ERROR     , 0xd010) /* 0xd010000a */
#define openchange_private_CONTACT_FID                      PROP_TAG(PT_I8        , 0xd011) /* 0xd0110014 */
#define openchange_private_CONTACT_FID_ERROR                PROP_TAG(PT_ERROR     , 0xd011) /* 0xd011000a */
#define openchange_private_CALENDAR_FID                     PROP_TAG(PT_I8        , 0xd012) /* 0xd0120014 */
#define openchange_private_CALENDAR_FID_ERROR               PROP_TAG(PT_ERROR     , 0xd012) /* 0xd012000a */
#define openchange_private_JOURNAL_FID                      PROP_TAG(PT_I8        , 0xd013) /* 0xd0130014 */
#define openchange_private_JOURNAL_FID_ERROR                PROP_TAG(PT_ERROR     , 0xd013) /* 0xd013000a */
#define openchange_private_NOTE_FID                         PROP_TAG(PT_I8        , 0xd014) /* 0xd0140014 */
#define openchange_private_NOTE_FID_ERROR                   PROP_TAG(PT_ERROR     , 0xd014) /* 0xd014000a */
#define openchange_private_TASK_FID                         PROP_TAG(PT_I8        , 0xd015) /* 0xd0150014 */
#define openchange_private_TASK_FID_ERROR                   PROP_TAG(PT_ERROR     , 0xd015) /* 0xd015000a */
#define openchange_private_DRAFTS_FID                       PROP_TAG(PT_I8        , 0xd016) /* 0xd0160014 */
#define openchange_private_DRAFTS_FID_ERROR                 PROP_TAG(PT_ERROR     , 0xd016) /* 0xd016000a */
#define openchange_private_PF_ROOT                          PROP_TAG(PT_I8        , 0xd017) /* 0xd0170014 */
#define openchange_private_PF_ROOT_ERROR                    PROP_TAG(PT_ERROR     , 0xd017) /* 0xd017000a */
#define openchange_private_PF_IPM_SUBTREE                   PROP_TAG(PT_I8        , 0xd018) /* 0xd0180014 */
#define openchange_private_PF_IPM_SUBTREE_ERROR             PROP_TAG(PT_ERROR     , 0xd018) /* 0xd018000a */
#define openchange_private_PF_NONIPM_SUBTREE                PROP_TAG(PT_I8        , 0xd019) /* 0xd0190014 */
#define openchange_private_PF_NONIPM_SUBTREE_ERROR          PROP_TAG(PT_ERROR     , 0xd019) /* 0xd019000a */
#define openchange_private_PF_EFORMS                        PROP_TAG(PT_I8        , 0xd01a) /* 0xd01a0014 */
#define openchange_private_PF_EFORMS_ERROR                  PROP_TAG(PT_ERROR     , 0xd01a) /* 0xd01a000a */
#define openchange_private_PF_FREEBUSY                      PROP_TAG(PT_I8        , 0xd01b) /* 0xd01b0014 */
#define openchange_private_PF_FREEBUSY_ERROR                PROP_TAG(PT_ERROR     , 0xd01b) /* 0xd01b000a */
#define openchange_private_PF_OAB                           PROP_TAG(PT_I8        , 0xd01c) /* 0xd01c0014 */
#define openchange_private_PF_OAB_ERROR                     PROP_TAG(PT_ERROR     , 0xd01c) /* 0xd01c000a */
#define openchange_private_PF_LOCAL_EFORMS                  PROP_TAG(PT_I8        , 0xd01d) /* 0xd01d0014 */
#define openchange_private_PF_LOCAL_EFORMS_ERROR            PROP_TAG(PT_ERROR     , 0xd01d) /* 0xd01d000a */
#define openchange_private_PF_LOCAL_FREEBUSY                PROP_TAG(PT_I8        , 0xd01e) /* 0xd01e0014 */
#define openchange_private_PF_LOCAL_FREEBUSY_ERROR          PROP_TAG(PT_ERROR     , 0xd01e) /* 0xd01e000a */
#define openchange_private_PF_LOCAL_OAB                     PROP_TAG(PT_I8        , 0xd01f) /* 0xd01f0014 */
#define openchange_private_PF_LOCAL_OAB_ERROR               PROP_TAG(PT_ERROR     , 0xd01f) /* 0xd01f000a */
"""

temporary_private_tags_struct = """\t{ openchange_private_ROOT_FOLDER_FID,		PT_I8, "openchange_private_ROOT_FOLDER_FID" },
	{ openchange_private_DEFERRED_ACTIONS_FID,	PT_I8, "openchange_private_DEFERRED_ACTIONS_FID" },
	{ openchange_private_SPOOLER_QUEUE_FID,		PT_I8, "openchange_private_SPOOLER_QUEUE_FID" },
	{ openchange_private_IPM_SUBTREE_FID,		PT_I8, "openchange_private_IPM_SUBTREE_FID" },
	{ openchange_private_INBOX_FID,			PT_I8, "openchange_private_INBOX_FID" },
	{ openchange_private_OUTBOX_FID,		PT_I8, "openchange_private_OUTBOX_FID" },
	{ openchange_private_SENT_ITEMS_FID,		PT_I8, "openchange_private_SENT_ITEMS_FID" },
	{ openchange_private_DELETED_ITEMS_FID,		PT_I8, "openchange_private_DELETED_ITEMS_FID" },
	{ openchange_private_COMMON_VIEWS_FID,		PT_I8, "openchange_private_COMMON_VIEWS_FID" },
	{ openchange_private_SCHEDULE_FID,		PT_I8, "openchange_private_SCHEDULE_FID" },
	{ openchange_private_SEARCH_FID,		PT_I8, "openchange_private_SEARCH_FID" },
	{ openchange_private_VIEWS_FID,			PT_I8, "openchange_private_VIEWS_FID" },
	{ openchange_private_SHORTCUTS_FID,		PT_I8, "openchange_private_SHORTCUTS_FID" },
	{ openchange_private_MailboxGUID,		PT_CLSID, "openchange_private_MailboxGUID" },
	{ openchange_private_ReplicaID,			PT_SHORT, "openchange_private_ReplicaID" },
	{ openchange_private_ReplicaGUID,		PT_CLSID, "openchange_private_ReplicaGUID" },
	{ openchange_private_CONTACT_FID,		PT_I8, "openchange_private_CONTACT_FID" },
	{ openchange_private_CALENDAR_FID,		PT_I8, "openchange_private_CALENDAR_FID" },
	{ openchange_private_JOURNAL_FID,		PT_I8, "openchange_private_JOURNAL_FID" },
	{ openchange_private_NOTE_FID,			PT_I8, "openchange_private_NOTE_FID" },
	{ openchange_private_TASK_FID,			PT_I8, "openchange_private_TASK_FID" },
	{ openchange_private_DRAFTS_FID,		PT_I8, "openchange_private_DRAFTS_FID" },
	{ openchange_private_PF_ROOT,			PT_I8, "openchange_private_PF_ROOT" },
	{ openchange_private_PF_IPM_SUBTREE,		PT_I8, "openchange_private_PF_IPM_SUBTREE" },
	{ openchange_private_PF_NONIPM_SUBTREE,		PT_I8, "openchange_private_PF_NONIPM_SUBTREE" },
	{ openchange_private_PF_EFORMS,			PT_I8, "openchange_private_PF_EFORMS" },
	{ openchange_private_PF_FREEBUSY,		PT_I8, "openchange_private_PF_FREEBUSY" },
	{ openchange_private_PF_OAB,			PT_I8, "openchange_private_PF_OAB" },
	{ openchange_private_PF_LOCAL_EFORMS,		PT_I8, "openchange_private_PF_LOCAL_EFORMS" },
	{ openchange_private_PF_LOCAL_FREEBUSY,		PT_I8, "openchange_private_PF_LOCAL_FREEBUSY" },
	{ openchange_private_PF_LOCAL_OAB,		PT_I8, "openchange_private_PF_LOCAL_OAB" },
"""

def make_mapi_properties_file():
	proplines = []
	altnamelines = []
	previous_propid_list = []

	# Additional properties referenced on MSDN but not in MS-OXPROPS
	properties.append({'CanonicalName': 'PidTagRoamingBinary',
			   'DataTypeName': 'PtypBinary',
			   'PropertyId': 0x7C09,
			   'AlternateNames': 'PR_ROAMING_BINARYSTREAM',
			   'Area': 'Configuration'})

	for entry in properties:
		if entry.has_key("CanonicalName") == False:
			print "Section", entry["OXPROPS_Sect"], "has no canonical name entry"
			continue
		if entry.has_key("DataTypeName") == False:
			print "Section", entry["OXPROPS_Sect"], "has no data type entry"
			continue
		if entry.has_key("PropertyId"):
			propline = "#define "
			propline += string.ljust(entry["CanonicalName"], 68)
			propline += " PROP_TAG("
			propline += string.ljust(datatypemap[entry["DataTypeName"]], 13) + ", "
			propline += "0x" + format(entry["PropertyId"], "04X")
			propline += ")  "
			propline += "/* 0x" + format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:] + " */"
			propline += "\n"
			proplines.append(propline)
			propline = "#define "
			propline += string.ljust(entry["CanonicalName"] + "_Error", 68)
			propline += " PROP_TAG("
			propline += string.ljust("PT_ERROR", 13) + ", "
			propline += "0x" + format(entry["PropertyId"], "04X")
			propline += ")  "
			propline += "/* 0x" + format(entry["PropertyId"], "04X") + "000A" + " */"
			propline += "\n"
			proplines.append(propline)
			if entry.has_key("AlternateNames"):
				for altname in entry["AlternateNames"].split(","):
					altname = altname.strip()
					if altname.count(" ") > 0:
						print "skipping non-conforming alternative name:", altname
					elif altname.startswith("PR_"):
						if altname.endswith("_A"):
							continue
						if altname.endswith("_W"):
							continue
						if knowndatatypes[entry["DataTypeName"]][2:] == "001F":
							altline = "#define " + string.ljust(altname + "_UNICODE", 68)
							altline += " PROP_TAG(PT_UNICODE   , 0x" + format(entry["PropertyId"], "04X") + ")"
							altline += "  /* 0x" + format(entry["PropertyId"], "04X") + "001F */" + "\n"
							altnamelines.append(altline)
							altline = "#define " + string.ljust(altname, 68)
							altline += " PROP_TAG(PT_STRING8   , 0x" + format(entry["PropertyId"], "04X") + ")"
							altline += "  /* 0x" + format(entry["PropertyId"], "04X") + "001E */" + "\n"
							altnamelines.append(altline)
						elif knowndatatypes[entry["DataTypeName"]][2:] == "101F":
							altline = "#define " + string.ljust(altname + "_UNICODE", 68)
							altline += " PROP_TAG(PT_MV_UNICODE, 0x" + format(entry["PropertyId"], "04X") + ")"
							altline += "  /* 0x" + format(entry["PropertyId"], "04X") + "101F */" + "\n"
							altnamelines.append(altline)
							altline = "#define " + string.ljust(altname, 68)
							altline += " PROP_TAG(PT_MV_STRING8, 0x" + format(entry["PropertyId"], "04X") + ")"
							altline += "  /* 0x" + format(entry["PropertyId"], "04X") + "101E */" + "\n"
							altnamelines.append (altline)
						else:
							altnamelines.append("#define " + string.ljust(altname, 68) + " " + entry["CanonicalName"] + "\n")
						altline = "#define " + string.ljust(altname + "_ERROR", 68)
						altline += " PROP_TAG(PT_ERROR     , 0x" + format(entry["PropertyId"], "04X") + ")"
						altline += "  /* 0x" + format(entry["PropertyId"], "04X") + "000A */" + "\n"
						altnamelines.append(altline)

	# hack until we properly handle named properties
	proplines.append(temporary_private_tags)

	# supplemental properties / alternative names
	altnamelines.append("#define PidTagFolderChildCount                                               PROP_TAG(PT_LONG      , 0x6638) /* 0x66380003 */\n")
	altnamelines.append("#define PidTagOriginalDisplayName                                            PROP_TAG(PT_UNICODE   , 0x3A13) /* 0x3A13001F */\n")
	altnamelines.append("#define PidTagDesignInProgress						  PROP_TAG(PT_BOOLEAN   , 0x3FE4) /* 0x3FE4000B */\n")
	altnamelines.append("#define PidTagSecureOrigination						  PROP_TAG(PT_BOOLEAN   , 0x3FE5) /* 0x3FE5000B */\n")
	altnamelines.append("#define PidTagExtendedACLData						  PROP_TAG(PT_BINARY    , 0x3FFE) /* 0x3FFE0102 */\n")
	altnamelines.append("#define PidTagAttributeSystem						  PROP_TAG(PT_BOOLEAN   , 0x10F5) /* 0x10F5000B */\n")
	altnamelines.append("#define PidTagUrlCompName							  PROP_TAG(PT_UNICODE   , 0x10F3) /* 0x10F3001F */\n")
	altnamelines.append("#define PidTagNormalMessageSize						  PROP_TAG(PT_LONG      , 0x66b3) /* 0x66B30003 */\n")
	altnamelines.append("#define PidTagUrlCompNameSet                                                 PROP_TAG(PT_BOOLEAN   , 0x0E62) /* 0x0E62000B */\n")
	altnamelines.append("#define PR_NORMAL_MESSAGE_SIZE						  PidTagNormalMessageSize\n");
	altnamelines.append("#define PR_DEFAULT_PROFILE                                                   0x00010102\n")
	altnamelines.append("#define PR_PROFILE_HOME_SERVER_ADDRS                                         0x6613101e\n")
	altnamelines.append("#define PR_FID                                                               PidTagFolderId\n")
	altnamelines.append("#define PR_MID                                                               PidTagMid\n")
	altnamelines.append("#define PR_INSTANCE_NUM                                                      PidTagInstanceNum\n")
	altnamelines.append("#define PR_FOLDER_CHILD_COUNT                                                0x66380003\n")
	altnamelines.append("#define PR_INST_ID                                                           0x674d0014\n")
	altnamelines.append("#define PR_RULE_MSG_PROVIDER                                                 0x65eb001e\n")
	altnamelines.append("#define PR_RULE_MSG_NAME                                                     0x65ec001e\n")
	altnamelines.append("#define PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE                                PidTagTransmittableDisplayName\n")
	altnamelines.append("#define PR_TRANSMITTABLE_DISPLAY_NAME                                        0x3a20001e\n")
	altnamelines.append("#define PR_ADDRBOOK_MID                                                      PidTagAddressBookMessageId\n")
	altnamelines.append("#define PR_FREEBUSY_LAST_MODIFIED                                            PidTagFreeBusyRangeTimestamp\n")
	altnamelines.append("#define PR_FREEBUSY_START_RANGE                                              PidTagFreeBusyPublishStart\n")
	altnamelines.append("#define PR_FREEBUSY_END_RANGE                                                PidTagFreeBusyPublishEnd\n")
	altnamelines.append("#define PR_FREEBUSY_ALL_MONTHS                                               PidTagScheduleInfoMonthsMerged\n")
	altnamelines.append("#define PR_FREEBUSY_ALL_EVENTS                                               PidTagScheduleInfoFreeBusyMerged\n")
	altnamelines.append("#define PR_FREEBUSY_TENTATIVE_MONTHS                                         PidTagScheduleInfoMonthsTentative\n")
	altnamelines.append("#define PR_FREEBUSY_TENTATIVE_EVENTS                                         PidTagScheduleInfoFreeBusyTentative\n")
	altnamelines.append("#define PR_FREEBUSY_BUSY_MONTHS                                              PidTagScheduleInfoMonthsBusy\n")
	altnamelines.append("#define PR_FREEBUSY_BUSY_EVENTS                                              PidTagScheduleInfoFreeBusyBusy\n")
	altnamelines.append("#define PR_FREEBUSY_OOF_MONTHS                                               PidTagScheduleInfoMonthsAway\n")
	altnamelines.append("#define PR_FREEBUSY_OOF_EVENTS                                               PidTagScheduleInfoFreeBusyAway\n")
	altnamelines.append("#define PR_REMINDERS_ONLINE_ENTRYID                                          0x36d50102\n")
	altnamelines.append("#define PR_IPM_PUBLIC_FOLDERS_ENTRYID                                        PidTagIpmPublicFoldersEntryId\n")
	altnamelines.append("#define PR_PARENT_FID                                                        PidTagParentFolderId\n")
	altnamelines.append("#define PR_URL_COMP_NAME_SET                                                 PidTagUrlCompNameSet\n")
	altnamelines.append("#define PR_ASSOC_CONTENT_COUNT						  PidTagAssociatedContentCount\n")
	altnamelines.append("#define PR_NTSD_MODIFICATION_TIME						  0x3FD60040\n")
	altnamelines.append("#define PR_CREATOR_SID							  0x0E580102\n")
	altnamelines.append("#define PR_LAST_MODIFIER_SID						  0x0E590102\n")
	altnamelines.append("#define PR_EXTENDED_ACL_DATA						  0x3FFE0102\n")
	altnamelines.append("#define PR_FOLDER_XVIEWINFO_E						  0x36E00102\n")
	altnamelines.append("#define PR_FOLDER_VIEWLIST							  0x36EB0102\n")
	altnamelines.append("#define PR_EMS_AB_HOME_MTA							  0x8007001F\n")
	altnamelines.append("#define PR_EMS_AB_ASSOC_NT_ACCOUNT						  0x80270102\n")
	altnamelines.append("#define PR_DELETED_MSG_COUNT					  	  0x66400003\n")
	altnamelines.append("#define PR_RECIPIENT_ON_NORMAL_MSG_COUNT					  0x66af0003\n")
	altnamelines.append("#define PR_CONVERSATION_KEY						  PidTagConversationKey\n")
	# write properties out to a master header file
	f = open('libmapi/property_tags.h', 'w')
	f.write("/* Automatically generated by script/makepropslist.py. Do not edit */\n")
	sortedproplines = sorted(proplines)
	for propline in sortedproplines:
		f.write(propline)
	f.close()
	f = open('libmapi/property_altnames.h', 'w')
	f.write("/* Automatically generated by script/makepropslist.py. Do not edit */\n")
	sortedaltnamelines = sorted(altnamelines)
	for propline in sortedaltnamelines:
		f.write(propline)
	f.close()

	# write canonical properties out for lookup 
	proplines = []
	f = open('libmapi/property_tags.c', 'w')
	proplines = []
	f.write("/* Automatically generated by script/makepropslist.py. Do not edit */\n")
	f.write("#include \"libmapi/libmapi.h\"\n")
	f.write("#include \"libmapi/libmapi_private.h\"\n")
	f.write("#include \"gen_ndr/ndr_exchange.h\"\n")
	f.write("#include \"libmapi/property_tags.h\"\n\n")
	f.write("struct mapi_proptags\n")
	f.write("{\n")
	f.write("\tuint32_t	proptag;\n")
	f.write("\tuint32_t	proptype;\n")
	f.write("\tconst char	*propname;\n")
	f.write("};\n")
	f.write("\n")
	for entry in properties:
		if (entry.has_key("CanonicalName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no canonical name entry"
			continue
		if (entry.has_key("DataTypeName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no data type entry"
			continue
		if entry.has_key("PropertyId"):
			propline = "\t{ "
			propline += string.ljust(entry["CanonicalName"] + ",", 68)
			propline += string.ljust(datatypemap[entry["DataTypeName"]] + ",", 14)
			propline += string.ljust("\"" + entry["CanonicalName"] + "\"" , 68) + "},\n"
			proplines.append(propline)
			propline = "\t{ "
			propline += string.ljust(entry["CanonicalName"] + "_Error,", 68)
			propline += string.ljust("PT_ERROR,", 14)
			propline += string.ljust("\"" + entry["CanonicalName"] + "_Error" + "\"" , 68) + "},\n"
			proplines.append(propline)
	proplines.append(extra_private_tags_struct)
	# this is just a temporary hack till we properly support named properties
	proplines.append(temporary_private_tags_struct)
	sortedproplines = sorted(proplines)
	f.write("static struct mapi_proptags canonical_property_tags[] = {\n")
	for propline in sortedproplines:
		f.write(propline)
	f.write("\t{ 0,                                                                  0,            \"NULL\"                                                              }\n")
	f.write("};\n")
	f.write("""
_PUBLIC_ const char *get_proptag_name(uint32_t proptag)
{
	uint32_t idx;

	for (idx = 0; canonical_property_tags[idx].proptag; idx++) {
		if (canonical_property_tags[idx].proptag == proptag) { 
			return canonical_property_tags[idx].propname;
		}
	}
	if (((proptag & 0xFFFF) == PT_STRING8) ||
	    ((proptag & 0xFFFF) == PT_MV_STRING8)) {
		proptag += 1; /* try as _UNICODE variant */
	}
	for (idx = 0; canonical_property_tags[idx].proptag; idx++) {
		if (canonical_property_tags[idx].proptag == proptag) { 
			return canonical_property_tags[idx].propname;
		}
	}
	return NULL;
}

_PUBLIC_ uint32_t get_proptag_value(const char *propname)
{
	uint32_t idx;

	for (idx = 0; canonical_property_tags[idx].proptag; idx++) {
		if (!strcmp(canonical_property_tags[idx].propname, propname)) { 
			return canonical_property_tags[idx].proptag;
		}
	}

	return 0;
}

_PUBLIC_ uint16_t get_property_type(uint16_t untypedtag)
{
	uint32_t	idx;
	uint16_t	current_type;

	for (idx = 0; canonical_property_tags[idx].proptag; idx++) {
		if ((canonical_property_tags[idx].proptag >> 16) == untypedtag) {
			current_type = canonical_property_tags[idx].proptype;
			if (current_type != PT_ERROR && current_type != PT_STRING8) {
				return current_type;
			}
		}
	}

	OC_DEBUG(5, "type for property '%x' could not be deduced", untypedtag);
	return 0;
}


""")
	f.close()

	# write canonical properties out for IDL input
	proplines = []
	previous_idl_proptags = []
	previous_idl_pidtags = []
	
	f = open('properties_enum.h', 'w')
	f.write("/* Automatically generated by script/makepropslist.py. Do not edit */\n")
	f.write("typedef [v1_enum, flag(NDR_PAHEX)] enum {\n")
	for entry in properties:
		if (entry.has_key("CanonicalName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no canonical name entry"
			continue
		if (entry.has_key("DataTypeName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no data type entry"
			continue
		if entry.has_key("PropertyId"):
			if entry["PropertyId"] in previous_idl_proptags:
				# Generate property tag
				pidtag = format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:]
				if pidtag in previous_idl_pidtags:
					print "Skipping output of enum entry for", entry["CanonicalName"], "(duplicate)"
					continue
				else:
					propline = "\t" + string.ljust(entry["CanonicalName"], 68)
					propline += " = 0x" + format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:]
					propline += ",\n"
					proplines.append(propline)
					continue
			propline = "\t" + string.ljust(entry["CanonicalName"], 68)
			propline += " = 0x" + format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:]
			propline += ",\n"
			proplines.append(propline)
			if entry["DataTypeName"] == "PtypString":
				propline = "\t" + string.ljust(entry["CanonicalName"] + "_string8", 68)
				propline += " = 0x" + format(entry["PropertyId"], "04X") + "001E"
				propline += ",\n"
				proplines.append(propline)
			propline = "\t" + string.ljust(entry["CanonicalName"] + "_Error", 68)
			propline += " = 0x" + format(entry["PropertyId"], "04X") + "000A"
			propline += ",\n"
			proplines.append(propline)
			previous_idl_proptags.append(entry["PropertyId"])
			previous_idl_pidtags.append(format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:])
	sortedproplines = sorted(proplines)
	for propline in sortedproplines:
		f.write(propline)

	# Add additional properties, referenced on MSDN but not in MS-OXPROPS
	f.write("\t" + string.ljust("PidTagAssociatedContentCount", 68) + " = 0x36170003,\n")
	f.write("\t" + string.ljust("PidTagFolderChildCount", 68) + " = 0x66380003,\n")
	f.write("\t" + string.ljust("PidTagIpmPublicFoldersEntryId", 68) + " = 0x66310102,\n")
	f.write("\t" + string.ljust("PidTagConversationKey", 68) + " = 0x000b0102,\n")
	f.write("\t" + string.ljust("PidTagContactEmailAddresses", 68) + " = 0x3a56101f,\n")
	f.write("\t" + string.ljust("PidTagGenerateExchangeViews", 68) + " = 0x36e9000b,\n")
	f.write("\t" + string.ljust("PidTagLatestDeliveryTime", 68) + " = 0x00190040,\n")
	f.write("\t" + string.ljust("PidTagMailPermission", 68) + " = 0x3a0e000b,\n")

	f.write("\tMAPI_PROP_RESERVED                                                   = 0xFFFFFFFF\n")
	f.write("} MAPITAGS;\n")
	f.close()
	
	# write canonical properties out for pyopenchange mapistore
	proplines = []
	previous_idl_proptags = []
	previous_idl_pidtags = []
	f = open('pyopenchange/pymapi_properties.c', 'w')
	f.write("""
/* Automatically generated by script/makepropslist.py. Do not edit */

#include <Python.h>
#include "pyopenchange/pymapi.h"

int pymapi_add_properties(PyObject *m)
{
""")
	for entry in properties:
		if (entry.has_key("CanonicalName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no canonical name entry"
			continue
		if (entry.has_key("DataTypeName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no data type entry"
			continue
		if entry.has_key("PropertyId"):
			if entry["PropertyId"] in previous_idl_proptags:
				pidtag = format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:]
				if pidtag in previous_idl_pidtags:
					print "Skipping output of Python bindings entry for", entry["CanonicalName"], "(duplicate)"
					continue
				else:
					propline = "\tPyModule_AddObject(m, \"" + entry["CanonicalName"] + "\", "
					propline += "PyInt_FromLong(0x" +  format(entry["PropertyId"], "04X")
					propline += knowndatatypes[entry["DataTypeName"]][2:]
					propline +=  "));\n"
					proplines.append(propline)
					continue
			propline = "\tPyModule_AddObject(m, \"" + entry["CanonicalName"] + "\", "
			propline += "PyInt_FromLong(0x" +  format(entry["PropertyId"], "04X")
			propline += knowndatatypes[entry["DataTypeName"]][2:]
			propline +=  "));\n"
			proplines.append(propline)

			propline = "\tPyModule_AddObject(m, \"" + entry["CanonicalName"] + "_Error\", "
			propline += "PyInt_FromLong(0x" +  format(entry["PropertyId"], "04X") + "000A"
			propline += "));\n"
			proplines.append(propline)
			previous_idl_proptags.append(entry["PropertyId"])
			previous_idl_pidtags.append(format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:])
	sortedproplines = sorted(proplines)
	for propline in sortedproplines:
		f.write(propline)	
	f.write("""
	return 0;
}
""")
	f.close()

	# write canonical properties out for openchangedb - probably remove this later
	proplines = []
	previous_idl_proptags = []
	previous_idl_pidtags = []
	f = open('mapiproxy/libmapiproxy/openchangedb_property.c', 'w')
	f.write("""
/* Automatically generated by script/makepropslist.py. Do not edit */
#include "mapiproxy/dcesrv_mapiproxy.h"
#include "libmapiproxy.h"
#include "libmapi/libmapi.h"
#include "libmapi/libmapi_private.h"

struct pidtags {
	uint32_t	proptag;
	const char	*pidtag;
};

static struct pidtags pidtags[] = {
""")
	for entry in properties:
		if (entry.has_key("CanonicalName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no canonical name entry"
			continue
		if (entry.has_key("DataTypeName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no data type entry"
			continue
		if entry.has_key("PropertyId"):
			if entry["PropertyId"] in previous_idl_proptags:
				pidtag = format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:]
				if pidtag in previous_idl_pidtags:
					print "Skipping output of pidtags entry for", entry["CanonicalName"], "(duplicate)"
					continue
				else:
					propline = "\t{ " + string.ljust(entry["CanonicalName"] + ",", 68)
					propline += "\"" + entry["CanonicalName"] + "\" },\n"
					proplines.append(propline)
					continue
			propline = "\t{ " + string.ljust(entry["CanonicalName"] + ",", 68)
			propline += "\"" + entry["CanonicalName"] + "\" },\n"
			proplines.append(propline)
			previous_idl_proptags.append(entry["PropertyId"])
			previous_idl_pidtags.append(format(entry["PropertyId"], "04X") + knowndatatypes[entry["DataTypeName"]][2:])
	sortedproplines = sorted(proplines)
	for propline in sortedproplines:
		f.write(propline)
	f.write("""\t{ 0,                                                                   NULL         }
};

static const char *_openchangedb_property_get_string_attribute(uint32_t proptag)
{
	uint32_t i;
	uint32_t tag_id = (proptag >> 16);

	for (i = 0; pidtags[i].pidtag; i++) {
		if (tag_id == (pidtags[i].proptag >> 16)) {
			return pidtags[i].pidtag;
		}
	}

	return NULL;
}

_PUBLIC_ const char *openchangedb_property_get_attribute(uint32_t proptag)
{
	uint32_t i;
	uint32_t prop_type = proptag & 0x0FFF;

	if (prop_type == PT_UNICODE || prop_type == PT_STRING8) {
		return _openchangedb_property_get_string_attribute(proptag);
	}

	for (i = 0; pidtags[i].pidtag; i++) {
		if (pidtags[i].proptag == proptag) {
			return pidtags[i].pidtag;
		}
	}
	OC_DEBUG(0, "Unsupported property tag '0x%.8x'", proptag);

	return NULL;
}
""")

previous_canonical_names = {}
def check_duplicate_canonical_names():
	print "Checking canonical names:"
	for entry in properties:
		canonicalname = entry["CanonicalName"]
		if previous_canonical_names.has_key(canonicalname):
			print "\tIn section", entry["OXPROPS_Sect"], ", canonical name:", entry["CanonicalName"], "duplicates name in section", previous_canonical_names[canonicalname]
		previous_canonical_names[canonicalname] = (entry["OXPROPS_Sect"])

def check_duplicate_alternative_names():
	print "Checking alternative names:"
	previous_alternative_names = {}
	for entry in properties:
		if entry.has_key("AlternateNames"):
			for altname in entry["AlternateNames"].split(", "):
				altname = altname.strip()
				if altname.count(" ") > 0:
					print "\tIn section", entry["OXPROPS_Sect"], ", alternative name:", altname, "contains space"
				if previous_alternative_names.has_key(altname):
					print "\tIn section", entry["OXPROPS_Sect"], ", alternative name:", altname, "duplicates name in section", previous_alternative_names[altname]
				if previous_canonical_names.has_key(altname):
					print "\tIn section", entry["OXPROPS_Sect"], ", alternative name:", altname, "duplicates canonical name in section", previous_alternative_names[altname]
				previous_alternative_names[altname] = (entry["OXPROPS_Sect"])

def check_duplicate_propids():
	print "Checking property IDs / LIDs:"
	previous_propids = {}
	previous_proplids = {}
	for entry in properties:
		if entry.has_key("PropertyId"):
			propnum = entry["PropertyId"]
			if previous_propids.has_key(propnum) and propnum < 0x6800:
				print "\tIn section", entry["OXPROPS_Sect"], "(" + entry["CanonicalName"] + ")"
				print "\t\tProperty id 0x" + format(propnum, "04x"), "(" + entry["DataTypeName"] + ") duplicates property id in section", previous_propids[propnum][0], "(" + previous_propids[propnum][1] + ")"
			if (entry.has_key("DataTypeName")):
				previous_propids[propnum] = (entry["OXPROPS_Sect"], entry["DataTypeName"])
			else:
				previous_propids[propnum] = (entry["OXPROPS_Sect"], "[No DataType]")
		elif entry.has_key("PropertyLid"):
			propnum = entry["PropertyLid"]
			if previous_proplids.has_key(propnum):
				print "\tIn section", entry["OXPROPS_Sect"], "(" + entry["CanonicalName"] + ")"
				print "\t\tProperty LID 0x" + format(propnum, "08x"), "(" + entry["DataTypeName"] + ") duplicates property LID in section", previous_proplids[propnum][0], "(" + previous_proplids[propnum][1] + ")"
			if (entry.has_key("DataTypeName")):
				previous_proplids[propnum] = (entry["OXPROPS_Sect"], entry["DataTypeName"])
			else:
				previous_proplids[propnum] = (entry["OXPROPS_Sect"], "[No DataTypeName]")
		elif entry["CanonicalName"].startswith("PidLid"):
			print "Section", entry["OXPROPS_Sect"], "(" + entry["CanonicalName"] + ") has neither LID nor ID"
		elif entry["CanonicalName"].startswith("PidName"):
			pass
		else:
			print "Section", entry["OXPROPS_Sect"], "(" + entry["CanonicalName"] + ") is weird"

		if (entry["CanonicalName"].startswith("PidName") and (entry.has_key("PropertyId") or entry.has_key("PropertyLid"))):
			print "Section", entry["OXPROPS_Sect"], "(" + entry["CanonicalName"], "in", entry["PropertySet"] + ") has neither LID or ID"

def check_proptypes():
	print "Checking that data types match:"
	for entry in properties:
		datatypename = entry["DataTypeName"]
		datatypevalue = entry["DataTypeValue"]
		if (knowndatatypes.has_key(datatypename) == False):
			print "\tIn section %(section)s : unknown data type %(type)s" % { 'section': entry["OXPROPS_Sect"], 'type': datatypename }
		elif (knowndatatypes[datatypename] != datatypevalue):
			print "\tIn section %(section)s : got value %(value)i for type %(type)i (expected %(expected)i)" % { 'section': entry["OXPROPS_Sect"], 'value': datatypeval, 'type': datatypename, 'expected': knowndatatype[datatypename] }

def check_propsets():
	print "Checking that property sets match:"
	for entry in properties:
		if entry.has_key("PropertySet"):
			propsetname = entry["PropertySet"]
			propsetvalue = entry["PropertySetValue"]
			if (knownpropsets.has_key(propsetname) == False):
				print "\tIn section %(section)s : unknown property set %(propset)s" % { 'section': entry["OXPROPS_Sect"], 'propset': propsetname }
			elif (knownpropsets[propsetname] != propsetvalue):
				print "\tIn section %(section)s : got value %(value)s for type %(type)s (expected %(expected)s)" % { 'section': entry["OXPROPS_Sect"], 'value': propsetvalue, 'type': propsetname, 'expected': knownpropsets[propsetname] }

def check_descriptions():
	print "Checking for descriptions:"
	for entry in properties:
		if entry.has_key("Description") == False:
			print "\tIn section %(section)s : there is no description" % { 'section': entry["OXPROPS_Sect"] }

def check_areas():
	print "Checking for areas:"
	for entry in properties:
		if entry.has_key("Area") == False:
			print "\tIn section %(section)s : there is no area" % { 'section': entry["OXPROPS_Sect"] }

def check_reference_line(entry, line, isdefining):
	if line.endswith(","):
		print "\tIn section %(section)s : trailing comma in (defining?) references" % { 'section': entry["OXPROPS_Sect"] }
		line = line.rstrip(",")
	for docentry in line.split(","):
		docentry = docentry.strip()
		if docentry == "":
			print "\tIn section %(section)s : empty (defining) reference section" % { 'section': entry["OXPROPS_Sect"] }
		elif knownrefs.count(docentry) != 1:
			if len(docentry.split(" ")) > 1:
				if docentry.split(" ")[1].strip().startswith("section"):
					# thats ok
					pass
				else:
					print "\tIn section %(section)s : missing comma in (defining?) references: %(docentry)s" % { 'section': entry["OXPROPS_Sect"], 'docentry': docentry }
			else:
				print "\tIn section %(section)s : unknown document: %(docname)s" % { 'section': entry["OXPROPS_Sect"], 'docname': docentry }
		else:
			try:
				reffile = file("docs/" + docentry + ".txt")
				reftext = reffile.read().replace(" ", "")
				if docentry == "[MS-OXCFXICS]":
					if (reftext.count((entry["CanonicalName"][6:])) < 1):
						print "\tIn section %(section)s : (defining) references contains %(docname)s, but %(prop)s wasn't found in that document" % { 'section': entry["OXPROPS_Sect"], 'docname': docentry, 'prop': entry['CanonicalName'] }
				elif reftext.count(entry["CanonicalName"]) < 1:
					print "\tIn section %(section)s : (defining) references contains %(docname)s, but %(prop)s wasn't found in that document" % { 'section': entry["OXPROPS_Sect"], 'docname': docentry, 'prop': entry['CanonicalName'] }
			except IOError:
				pass

def check_references():
	print "Checking for references:"
	for entry in properties:
		if entry.has_key("References"):
			check_reference_line(entry, entry["References"], False)
		elif entry.has_key("DefiningReference"):
			check_reference_line(entry, entry["DefiningReference"], True)
		else:
			print "\tIn section %(section)s : there is no (defining) reference entry" % { 'section': entry["OXPROPS_Sect"] }

def check_properties_list():
	check_proptypes()
	check_propsets()
	check_duplicate_canonical_names()
	check_duplicate_alternative_names()
	check_duplicate_propids()
	# check_descriptions()
	check_areas()
	check_references()

def next_available_id(knownprops, increment):
	try:
	       	knownprops.index(increment)
	       	knownprops.remove(increment)
	       	increment += 1
	       	return next_available_id(knownprops, increment)
       	except ValueError:
		return increment


def find_key(dic, val):
	"""return the key of dictionary dic given the value"""
	try:
		for k,v in dic.iteritems():
			if v == val:
				return k
	except ValueError:
		print "Value %s not found" % val

def make_mapi_named_properties_file():
	content = ""
	attributes = ""
	start_content = ""
	namedprops = []
	knownprops = []
	previous_ldif_lid = []
	previous_ldif_name = []
	for entry in properties:
		if (entry.has_key("CanonicalName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no canonical name entry"
			continue
		if (entry.has_key("DataTypeName") == False):
			print "Section", entry["OXPROPS_Sect"], "has no data type entry"
			continue
		if (entry.has_key("PropertyId") == False):
			if entry.has_key("PropertySet"):
				guid = entry["PropertySet"]
			else:
				guid = "[No PropSet]"
			# Its a named property
			name = entry["CanonicalName"]
			proptype = entry["DataTypeName"]
			if entry.has_key("PropertyLid"):
				proplid = "0x" + format(entry["PropertyLid"], "04x")
				if proplid in previous_ldif_lid:
					print "Skipping output for named properties MNID_ID", name, "(duplicate)"
					continue;
				kind = "MNID_ID"
				OOM = "NULL" # use as default
				propname = "NULL"
				if entry.has_key("PropertyName"):
					OOM = entry["PropertyName"].strip()
				elif entry.has_key("AlternateNames"):
					altname = entry["AlternateNames"].strip()
					if altname.startswith("dispid"):
						OOM = altname[6:]
					else:
						OOM = altname
				else:
					pass
				previous_ldif_lid.append(proplid)

			else:
				proplid = "0x0000"
				kind = "MNID_STRING"
				OOM = "NULL"
				propname = "NULL" # use as default
				if entry.has_key("PropertyName"):
					propname = entry["PropertyName"].strip()
				elif entry.has_key("AlternateNames"):
					for altname in entry["AlternateNames"]:
						altname = altname.strip()
						if altname.startswith("dispid"):
							propname = altname[6:]
				search_dup = "%s/%s" % (propname, guid)
				if search_dup in previous_ldif_name:
					print "Skipping output for named properties MNID_STRING", name, "(duplicate)"
					continue;
				previous_ldif_name.append(search_dup)

			namedprop = (name, OOM, proplid, propname, knowndatatypes[proptype], kind, guid)
			namedprops.append(namedprop)
		else:
			# It's not a named property
			# Store conflicting properties with propid > 0x8000
			propid = entry["PropertyId"]
			if propid >= 0x8000:
				try:
					knownprops.index(propid)
				except ValueError:
					knownprops.append(propid)

	# Create the default GUID containers
	for key in sorted(knownpropsets):
		cn = knownpropsets[key].strip('{}').lower()
		oleguid_ldif = "dn: CN=%s,CN=External,CN=Server\n"	\
			       "objectClass: External\n"		\
			       "cn: %s\n"				\
			       "name: %s\n"				\
			       "oleguid: %s\n\n" % (cn, cn, str(key), cn)
		content += oleguid_ldif

	# Write named properties
	sortednamedprops = sorted(namedprops, key=lambda namedprops: namedprops[6]) # sort by guid
	increment = next_available_id(knownprops, 0x8000)

	for line in sortednamedprops:
		propset = line[6]
		if propset not in knownpropsets:
			# Try to guess from the closest match
			result = difflib.get_close_matches(propset, knownpropsets.keys(),
							   1, 0.9)
			if len(result) > 0:
				propset = result[0]
			else:
				raise KeyError(propset)

		oleguid = knownpropsets[propset].strip('{}').lower()
		if line[5] == "MNID_STRING":
			named_props_ldif = "dn: CN=%s,CN=MNID_STRING,CN=%s,CN=External,CN=Server\n"	\
					   "objectClass: External\n"					\
					   "objectClass: MNID_STRING\n"					\
					   "cn: %s\n"							\
					   "canonical: %s\n"						\
					   "oleguid: %s\n"						\
					   "mapped_id: 0x%.4x\n"					\
					   "prop_id: %s\n"						\
					   "prop_type: %s\n"						\
					   "prop_name: %s\n\n" % (
				line[3], oleguid, line[3], line[0], oleguid, increment,
				line[2], line[4], line[3])
		else:
			named_props_ldif = "dn: CN=%s,CN=MNID_ID,CN=%s,CN=External,CN=Server\n"		\
					   "objectClass: External\n"					\
					   "objectClass: MNID_ID\n"					\
					   "cn: %s\n"							\
					   "canonical: %s\n"						\
					   "oleguid: %s\n"						\
					   "mapped_id: 0x%.4x\n"					\
					   "prop_id: %s\n"						\
					   "prop_type: %s\n"						\
					   "oom: %s\n\n" % (
				line[2], oleguid, line[2], line[0], oleguid, increment,
				line[2], line[4], line[1])
		
		content += named_props_ldif

		increment += 1
		increment = next_available_id(knownprops, increment)

	# Store remaining reserved named properties IDs in attributes
	for ids in sorted(knownprops):
		attributes += "reserved_tags: 0x%.4x\n" % ids

	start_content =  "# LDIF file automatically auto-generated by script/makepropslist.py. Do not edit\n\n"
	start_content += "dn: CN=Server\n"		\
			 "objectClass: top\n"		\
			 "cn: Server\n\n"		\
							\
			 "dn: CN=Internal,CN=Server\n"	\
			 "objectClass: Internal\n"	\
			 "objectClass: container\n"	\
			 "objectClass: top\n"		\
			 "cn: Internal\n"		\
			 "mapping_index: 0x0001\n\n"	\
							\
			 "dn: CN=External,CN=Server\n"	\
			 "objectClass: External\n"	\
			 "objectClass: container\n"	\
			 "objectClass: top\n"		\
			 "cn: External\n"		\
			 "mapping_index: 0x%.4x\n" % increment
	start_content += attributes + "\n"
	start_content += "dn: CN=Users,CN=Server\n"	\
			 "objectClass: container\n"	\
			 "objectClass: top\n"		\
			 "cn: Users\n\n"

	content = start_content + content

	# wite named properties buffered file out to LDIF file
	f = open('setup/mapistore/mapistore_namedprops_v2.ldif', 'w')
	f.write(content)
	f.close()

	# write named properties defines and structure
	f = open('libmapi/mapi_nameid.h', 'w')
	f.write("""
/* Automatically generated by script/makepropslist.py. Do not edit */
#ifndef	__MAPI_NAMEID_H__
#define	__MAPI_NAMEID_H__

/* NOTE TO DEVELOPERS: If this is a MNID_STRING named property, then
 * we use the arbitrary 0xa000-0xafff property ID range for internal
 * mapping purpose only.
 */

struct mapi_nameid_tags {
	uint32_t		proptag;
	const char		*OOM;
	uint16_t		lid;
	const char		*Name;
	uint32_t		propType;
	uint8_t			ulKind;
	const char		*OLEGUID;
	uint32_t		position;
};

struct mapi_nameid_names {
	uint32_t		proptag;
	const char		*propname;
};

struct mapi_nameid {
	struct MAPINAMEID	*nameid;
	uint16_t		count;
	struct mapi_nameid_tags	*entries;
};

/* MNID_ID named properties */
""")

	for line in sortednamedprops:
		if line[5] == "MNID_ID":
			proptag = "0x%.8x" % (int(line[2], 16) << 16 | int(line[4], 16))
			propline = "#define %s %s\n" % (string.ljust(line[0], 60), string.ljust(proptag, 20))
			f.write(propline)

	f.write("\n/* MNID_STRING named properties (internal mapping) */\n")
	mnstring_id = 0xa000
	for line in sortednamedprops:
		if line[5] == "MNID_STRING":
			proptag = "0x%.8x" % ((mnstring_id << 16) | int(line[4], 16))
			propline = "#define %s %s\n" % (string.ljust(line[0], 60), string.ljust(proptag, 20))
			mnstring_id += 1
			f.write(propline)

	# Additional properties
	propline = "#define %s %s\n" % (string.ljust("PidLidRemoteTransferSize", 60), string.ljust("0x8f050003", 20))
	f.write(propline)

	f.write("#endif /* ! MAPI_NAMEID_H__ */")
	f.close()

	# write named properties internal mapping
	f = open('libmapi/mapi_nameid_private.h', 'w')
	f.write("""
/* Automatically generated by script/makepropslist.py. Do not edit */
#ifndef	__MAPI_NAMEID_PRIVATE_H__
#define	__MAPI_NAMEID_PRIVATE_H__

static struct mapi_nameid_tags mapi_nameid_tags[] = {
""")

	for line in sortednamedprops:
		if line[5] == "MNID_ID":
			OOM = "\"%s\"" % line[1]
			key = find_key(knowndatatypes, line[4])
			datatype = datatypemap[key]
			propline = "{ %s, %s, %s, %s, %s, %s, %s, %s },\n" % (
				string.ljust(line[0], 60), string.ljust(OOM, 65), line[2], line[3], 
				string.ljust(datatype, 15), "MNID_ID", line[6], "0x0")
			f.write(propline)

	for line in sortednamedprops:
		if line[5] == "MNID_STRING":
			OOM = "%s" % line[1]
			key = find_key(knowndatatypes, line[4])
			datatype = datatypemap[key]
			propline = "{ %s, %s, %s, \"%s\", %s, %s, %s, %s },\n" % (
				string.ljust(line[0], 60), string.ljust(OOM, 65), line[2], line[3], 
				string.ljust(datatype, 15), "MNID_STRING", line[6], "0x0")
			f.write(propline)

	# Addtional named properties
	propline = "{ %s, %s, %s, %s, %s, %s, %s, %s },\n" % (
		string.ljust("PidLidRemoteTransferSize", 60), string.ljust("\"RemoteTransferSize\"", 65), "0x8f05",
		"NULL", string.ljust("PT_LONG", 15), "MNID_ID", "PSETID_Remote", "0x0")
	f.write(propline)

	propline = "{ %s, %s, %s, %s, %s, %s, %s, %s }\n" % (
		string.ljust("0x00000000", 60), string.ljust("NULL", 65), "0x0000", "NULL",
		string.ljust("PT_UNSPECIFIED", 15), "0x0", "NULL", "0x0")
	f.write(propline)
	f.write("""
};
""")

	f.write("""
static struct mapi_nameid_names mapi_nameid_names[] = {
""")
	for line in sortednamedprops:
		propline = "{ %s, \"%s\" },\n" % (string.ljust(line[0], 60), line[0])
		f.write(propline)

	# Additional named properties
	propline = "{ %s, \"%s\" }\n" % (string.ljust("PidLidRemoteTransferSize", 60), "PidLidRemoteTransferSize")	

	propline = "{ %s, \"%s\" }\n" % (string.ljust("0x00000000", 60), "NULL")
	f.write(propline)
	f.write("""
};

#endif /* !MAPI_NAMEID_PRIVATE_H__ */
""")
	f.close()

def dump_areas_count():
	areas = {}
	for area in knownareas:
		areas[area] = 0

	for entry in properties:
		if (entry.has_key("Area") == False):
		      print "Section", entry["OXPROPS_Sect"], "has no area entry"
		else:
		      areas[entry["Area"]] += 1

	for area in knownareas:
		print area, ":", areas[area]

def fix_problems(propsfilename):
	retcode = subprocess.call(["sed", "-i",
				   "-e", "s/.Dta type: PtypBoolean, 0x000B/Data type: PtypBoolean, 0x000B/",
				   "-e", "s/.Data Type: PtypString, 0x001F/Data type: PtypString, 0x001F/",
				   "-e", "s/.Data type: PtyString, 0x001F/Data type: PtypString, 0x001F/",
				   "-e", "s/.Area: MAPI Display Tables\[MS-OXOABK\] section 2.2.3.33/Area: MAPI Display Tables\\nDefining Reference: \[MS-OXOABK\] section 2.2.3.33/",
				   "-e", "s/.Area: ProviderDefinedNonTransmittable\[MS-OXCTABL\] section 2.2.1.2/Area: ProviderDefinedNonTransmittable\\nDefining Reference: \[MS-OXCTABL\] section 2.2.1.2/",
				   "-e", "s/.Area: Server-Side Rules Properties\[MS-OXORULE\] section 2.2.1.3.2.2/Area: Server-Side Rules Properties\\nDefining Reference: \[MS-OXORULE\] section 2.2.1.3.2.2/",
				   "-e", "s/.Area: MapiMailUser\[MS-OXOABK\] section 2.2.4.66/Area: MapiMailUser\\nDefining Reference: \[MS-OXOABK\] section 2.2.4.66/",
				   "-e", "s/.Description: \[MS-OXORULE\] section 2.2.7.3/Defining Reference: \[MS-OXORULE\] section 2.2.7.3/",
				   "-e", "s/.Property set: PSTID_Sharing {00062040-0000-0000-C000-000000000046}/Property set: PSETID_Sharing {00062040-0000-0000-C000-000000000046}/",
				   "-e", "s/.Property set: PSETID_Address {00062004-0000-0000-C000-00000000046}/Property set: PSETID_Address {00062004-0000-0000-C000-000000000046}/",
				   "-e", "s/.Property set: PSETID_Address{00062004-0000-0000-C000-000000000046}/Property set: PSETID_Address {00062004-0000-0000-C000-000000000046}/",
				   "-e", "s/.Property set: PSETID_Appointment {00062002-0000-0000-C000-0000000000046}/Property set: PSETID_Appointment {00062002-0000-0000-C000-000000000046}/",
				   "-e", "s/.Property set: PSETID_Address {00062004-0000-0000-C00-0000000000046}/Property set: PSETID_Address {00062004-0000-0000-C000-000000000046}/",
				   "-e", "s/.Consuming Reference: \[MS-OXCICAL\] Alternate names: PR_NEXT_SEND_ACCT/Consuming Reference: \[MS-OXCICAL\]\\nAlternate names: PR_NEXT_SEND_ACCT/",
				   "-e", "s/.Alternate names: PR_WB_SF_ID//",
				   "-e", "s/.Alternate names: PR_WB_SF_TAG//",
				   "-e", "s/.Alternate names: PR_EMS_AB_DL_MEM_REJECT_PERMS//",
				   "-e", "s/.Alternate names: PR_EMS_AB_DL_MEM_SUBMIT_PERMS//",
				   "-e", "s/.General Message Properties Defining reference/General Message Properties\\nDefining reference/",
				   "-e", "s/.Data type: PtypString8, 0x001E; PtypEmbeddedTable, 0x000D/Data type: PtypString8, 0x001E/",
				   "-e", "s/.Data type: PtypString, 0x001F; PtypMultipleBinary, 0x1102/Data type: PtypString, 0x001F/",
				   propsfilename])
	if retcode != 0:
		print "Could not fix problem:", retcode
		sys.exit(retcode)

	# Fix data type error for PidTagWlinkGroupHeaderID - PtypGuid instead of PtypBinary
	with open(propsfilename) as f:
		file_str = f.read()

	file_str = file_str.replace("Description: Specifies the ID of the navigation shortcut that groups other navigation shortcuts.\n\nProperty ID: 0x6842\n\nData type: PtypBinary, 0x0102", "Description: Specifies the ID of the navigation shortcut that groups other navigation shortcuts.\n\nProperty ID: 0x6842\n\nData type: PtypGuid, 0x0048")

	with open(propsfilename, "w") as f:
		f.write(file_str)
	f.close()

def main():
	oxpropsparser = argparse.ArgumentParser(description='Convert MS-OXPROPS to other formats')
	oxpropsparser.add_argument('--pdffile', required=True)
	oxpropsparser.add_argument('--sanitycheck', action='store_true')
	oxpropsparser.add_argument('--sanitycheckonly', action='store_true')

	args = oxpropsparser.parse_args()
	propsfile = tempfile.mkstemp(suffix='txt')
	propsfilename = propsfile[1]
	retcode = subprocess.call(["pdftotext", "-nopgbrk", "-layout", args.pdffile, propsfilename])
	if retcode != 0:
		print "Could not convert file to text:", retcode
		sys.exit(retcode)

	fix_problems(propsfilename)

	make_properties_list(propsfilename)
	if args.sanitycheck or args.sanitycheckonly:
		check_properties_list() # uses global variable
		# dump_areas_count()
	if args.sanitycheckonly == False:
		make_mapi_properties_file()
		make_mapi_named_properties_file()
	
if __name__ == "__main__":
    main()
