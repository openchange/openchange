#!/usr/bin/python
# OpenChange python sample backend
#
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2014
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
#

import os,sys

root = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.sep.join((root, '..', '..')))

import json
import uuid
from datetime import datetime, timedelta
from pytz import timezone
from openchange import mapistore

import MySQLdb
from samba import Ldb
import optparse
import samba.getopt as options

class BackendObject(object):

    name = "sample"
    description = "Sample backend"
    namespace = "sample://"

    def __init__(self):
        print '[PYTHON]: %s backend class __init__' % self.name
        return

    def init(self):
        """ Initialize sample backend
        """
        print '[PYTHON]: %s backend.init: init()' % self.name
        return 0

    def list_contexts(self, username):
        """ List context capabilities of this backend.
        """
        print '[PYTHON]: %s backend.list_contexts(): username = %s' % (self.name, username)
        deadbeef = {}
        deadbeef["url"] = "deadbeef0000001/"
        deadbeef["name"] = "deadbeef"
        deadbeef["main_folder"] = True
        deadbeef["role"] = mapistore.ROLE_MAIL
        deadbeef["tag"] = "tag"

        cacabeef = {}
        cacabeef["url"] = "cacabeef0000001/"
        cacabeef["name"] = "cacabeef"
        cacabeef["main_folder"] = True
        cacabeef["role"] = mapistore.ROLE_CALENDAR
        cacabeef["tag"] = "tag"

        cadabeef = {}
        cadabeef["url"] = "cadabeef0000001/"
        cadabeef["name"] = "cadabeef"
        cadabeef["main_folder"] = True
        cadabeef["role"] = mapistore.ROLE_CONTACTS
        cadabeef["tag"] = "tag"

        cafebeef = {}
        cafebeef["url"] = "cafebeef0000001/"
        cafebeef["name"] = "cafebeef"
        cafebeef["main_folder"] = True
        cafebeef["role"] = mapistore.ROLE_TASKS
        cafebeef["tag"] = "tag"

        contexts = [deadbeef, cacabeef, cadabeef, cafebeef]
        return contexts

    def create_context(self, uri):
        """ Create a context.
        """

        print '[PYTHON]: %s backend.create_context: uri = %s' % (self.name, uri)
        context = ContextObject()

        return (0, context)


class ContextObject(BackendObject):

    mapping = {}

    def __init__(self):

        #
        # Mail data
        #

        # Attachments
        attachment1 = {}
        attachment1["properties"] = {}
        attachment1["properties"]["PidTagMid"] = 0xdead00010000001
        attachment1["properties"]["PidTagAttachNumber"] = 0
        attachment1["properties"]["PidTagAttachMethod"] = 1
        attachment1["properties"]["PidTagAttachmentLinkId"] = 0x0
        attachment1["properties"]["PidTagAttachmentHidden"] = False
        attachment1["properties"]["PidTagAttachFilename"] = "Dummyfile.txt"
        attachment1["properties"]["PidTagAttachLongFilename"] = "Long Dummyfile.txt"
        attachment1["properties"]["PidTagAttachLongPathname"] = "Long Dummyfile.txt"
        attachment1["properties"]["PidTagAttachPathname"] = "Dummyfile.txt"
        attachment1["properties"]["PidTagAttachExtension"] = "txt"
        attachment1["properties"]["PidTagAttachContentId"] = "Content Id"
        attachment1["properties"]["PidTagAttachRendering"] = 1
        attachment1["properties"]["PidTagAttachDataBinary"] = bytearray("<html><head></head><h1>Attach me</h1></body></html>")
        attachment1["properties"]["PidTagAttachSize"] = len(attachment1["properties"]["PidTagAttachDataBinary"])

        # Recipients
        DummyTo = {}
        DummyTo["PidTagRecipientType"] = 0x1
        DummyTo["PidTagSmtpAddress"] = "dummy@openchange.org"
        DummyTo["PidTagRecipientDisplayName"] = "Dummy User"
        DummyTo["PidTagSentRepresentingName"] = "Dummy User"
        DummyTo["PidTagDisplayTo"] = "dummy@openchange.org"

        message1 = {}
        message1["recipients"] = [DummyTo, ]
        message1["attachments"] = [attachment1, ]
        message1["mid"] = 0xdead00010000001
        message1["fai"] = False
        message1["cache"] = {}
        message1["properties"] = {}
        message1["properties"]["PidTagFolderId"] = 0xdeadbeef0000001
        message1["properties"]["PidTagMid"] = message1["mid"]
        message1["properties"]["PidTagInstID"] =  message1["mid"]
        message1["properties"]["PidTagSubjectPrefix"] = "Re:"
        message1["properties"]["PidTagInstanceNum"] = 0
        message1["properties"]["PidTagRowType"] = 1
        message1["properties"]["PidTagDepth"] = 0
        message1["properties"]["PidTagMessageClass"] = "IPM.Note"
        message1["properties"]["PidTagSubject"] = "Dummy Sample Email"
        message1["properties"]["PidTagNormalizedSubject"] = message1["properties"]["PidTagSubject"]
        message1["properties"]["PidTagConversationTopic"] = message1["properties"]["PidTagSubject"]
        message1["properties"]["PidTagBody"] = "This is the content of this sample email"
        message1["properties"]["PidTagHtml"] = bytearray("<html><head></head><h1>"+ message1["properties"]["PidTagBody"] + "</h1></body></html>")
        message1["properties"]["PidTagImportance"] = 2
        message1["properties"]["PidTagSensitivity"] = 1
        message1["properties"]["PidTagHasAttachments"] = True
        message1["properties"]["PidTagContentCount"] = len(message1["attachments"])
        message1["properties"]["PidTagInternetMessageId"] = "internet-message-id@openchange.org"
        message1["properties"]["PidTagChangeKey"] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        message1["properties"]["PidTagMessageStatus"] = 0
        message1["properties"]["PidTagMessageFlags"] = 0
        message1["properties"]["PidTagLastModificationTime"] = float(datetime.now().strftime("%s.%f"))
        message1["properties"]["PidTagMessageDeliveryTime"] = float(datetime.now().strftime("%s.%f"))

        subfolder = {}
        subfolder["uri"] = "deadbeef0000001/dead0010000001/"
        subfolder["fid"] = 0xdead10010000001
        subfolder["subfolders"] = []
        subfolder["messages"] = []
        subfolder["properties"] = {}
        subfolder["properties"]["PidTagParentFolderId"] = 0xdeadbeef0000001
        subfolder["properties"]["PidTagFolderId"] = subfolder["fid"]
        subfolder["properties"]["PidTagFolderType"] = 1
        subfolder["properties"]["PidTagDisplayName"] = "DEAD-1001"
        subfolder["properties"]["PidTagContainerClass"] = "IPF.Note"
        subfolder["properties"]["PidTagDefaultPostMessageClass"] = "IPM.Note"
        subfolder["properties"]["PidTagSubfolders"] = False
        subfolder["properties"]["PidTagAttributeHidden"] = False
        subfolder["properties"]["PidTagComment"] = "WALKING COMMENT"
        subfolder["properties"]["PidTagContentCount"] = len(subfolder["messages"])
        subfolder["properties"]["PidTagContentUnreadCount"] = len(subfolder["messages"])
        subfolder["properties"]["PidTagChildFolderCount"] = len(subfolder["subfolders"])
        subfolder["cache"] = {}
        subfolder["cache"]["properties"] = {}
        subfolder["cache"]["messages"] = []

        self.mapping[0xdeadbeef0000001] = {}
        self.mapping[0xdeadbeef0000001]["uri"] = "deadbeef0000001/"
        self.mapping[0xdeadbeef0000001]["properties"] = {}
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagFolderId"] = 0xdeadbeef0000001
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagDisplayName"] = "SampleMail"
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagComment"] = "Sample Mail Folder"
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagContainerClass"] = "IPF.Note"
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagDefaultPostMessageClass"] = "IPM.Note"
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagAccess"] = 63
        self.mapping[0xdeadbeef0000001]["properties"]["PidTagRights"] = 2043
        self.mapping[0xdeadbeef0000001]["cache"] = {}
        self.mapping[0xdeadbeef0000001]["subfolders"] = [subfolder, ]
        self.mapping[0xdeadbeef0000001]["messages"] = [message1, ]

        #
        # Calendar data
        #
        appt1 = {}
        appt1["recipients"] = []
        appt1["attachments"] = []
        appt1["mid"] = 0xcaca00010000001
        appt1["fai"] = False
        appt1["cache"] = {}
        appt1["properties"] = {}

        # General properties (read-only)
        appt1["properties"]["PidTagAccess"] = 63
        appt1["properties"]["PidTagAccessLevel"] = 1
        appt1["properties"]["PidTagChangeKey"] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        appt1["properties"]["PidTagCreationTime"] = float((datetime.now(tz=timezone('Europe/Madrid')) - timedelta(hours=1)).strftime("%s.%f"))
        appt1["properties"]["PidTagLastModificationTime"] = appt1["properties"]["PidTagCreationTime"]
        appt1["properties"]["PidTagLastModifierName"] = "julien"
        appt1["properties"]["PidTagObjectType"] = 5
        appt1["properties"]["PidTagRecordKey"] = bytearray(uuid.uuid1().bytes)
        appt1["properties"]["PidTagSearchKey"] = bytearray(uuid.uuid1().bytes)

        appt1["properties"]["PidTagFolderId"] = 0xcacabeef0000001
        appt1["properties"]["PidTagMid"] = appt1["mid"]
        appt1["properties"]["PidTagInstID"] = appt1["mid"]
        appt1["properties"]["PidTagInstanceNum"] = 0
        appt1["properties"]["PidTagRowType"] = 1
        appt1["properties"]["PidTagDepth"] = 0
        appt1["properties"]["PidTagMessageClass"] = "IPM.Appointment"
        appt1["properties"]["PidTagIconIndex"] = 0x400 # single-instance appointment 2.2.1.49 [MS-OXOCAL]
        appt1["properties"]["0x91f80003"] = 0 # PidLidSideEffects 2.2.2.1.16 [MS-OXCMSG]
        # appt1["properties"]["0x94e60003"] = # PidLidHeaderItem
        appt1["properties"]["PidTagMessageStatus"] = 0
        appt1["properties"]["PidTagMessageFlags"] = 0
        # appt1["properties"]["0x92020102" = # PidLidTimeZoneStruct
        appt1["properties"]["0x910c001f"] = "Location of the event" # PidLidLocation
        appt1["properties"]["0x91900003"] = 0 # PidLidAppointmentStateFlags 2.2.1.10 [MS-OXOCAL]
        appt1["properties"]["0x91ee000b"] = True # PidLidReminderSet
        #appt1["properties"]["0x918b0102"] = # PidLidAppointmentRecur
        appt1["properties"]["0x90Fa000b"] = False # PidLidAppointmentSubType

        appt1["properties"]["PidTagSubject"] = "Meet Sample backend appointment folder"
        appt1["properties"]["PidTagNormalizedSubject"] = appt1["properties"]["PidTagSubject"]
        appt1["properties"]["PidTagHasAttachments"] = False
        appt1["properties"]["PidTagSensitivity"] = 1
        appt1["properties"]["PidTagInternetMessageId"] = "internet-message-id@openchange.org"
        appt1["properties"]["PidTagBody"] = "Description of the event"
        appt1["properties"]["PidTagHtml"] = bytearray(appt1["properties"]["PidTagBody"])
        appt1["properties"]["0x90db101f"] = ["Blue Category", "OpenChange", ]
        appt1["properties"]["PidTagMessageDeliveryTime"] = appt1["properties"]["PidTagCreationTime"]
        appt1["properties"]["0x918f0040"] = float(datetime.now(tz=timezone('Europe/Madrid')).strftime("%s.%f")) # PidLidAppointmentStartWhole
        appt1["properties"]["0x91980040"] =  appt1["properties"]["0x918f0040"]  # PidLidCommonEnd
        appt1["properties"]["0x918a0040"] = float((datetime.now(tz=timezone('Europe/Madrid')) + timedelta(hours=1)).strftime("%s.%f")) # PidLidAppointmentEndWhole
        appt1["properties"]["0x91990040"] =  appt1["properties"]["0x918a0040"] # PidLidCommonStart

        appt1["properties"]["0x91930003"] = 2 # PidLidBusyStatus
        appt1["properties"]["0x90fc0003"] = 0 # PidLidResponseStatus

        self.mapping[0xcacabeef0000001] = {}
        self.mapping[0xcacabeef0000001]["uri"] = "cacabeef0000001/"
        self.mapping[0xcacabeef0000001]["properties"] = {}
        self.mapping[0xcacabeef0000001]["properties"]["PidTagFolderId"] = 0xcacabeef0000001
        self.mapping[0xcacabeef0000001]["properties"]["PidTagDisplayName"] = "SampleCalendar"
        self.mapping[0xcacabeef0000001]["properties"]["PidTagComment"] = "Sample Calendar Folder"
        self.mapping[0xcacabeef0000001]["properties"]["PidTagContainerClass"] = "IPF.Appointment"
        self.mapping[0xcacabeef0000001]["properties"]["PidTagDefaultPostMessageClass"] = "IPM.Appointment"
        self.mapping[0xcacabeef0000001]["properties"]["PidTagAccess"] = 63
        self.mapping[0xcacabeef0000001]["properties"]["PidTagRights"] = 2043
        self.mapping[0xcacabeef0000001]["subfolders"] = []
        self.mapping[0xcacabeef0000001]["messages"] = [appt1,]
        self.mapping[0xcacabeef0000001]["cache"] = {}
        self.mapping[0xcacabeef0000001]["cache"]["properties"] = {}
        self.mapping[0xcacabeef0000001]["cache"]["messages"] = []


        #
        # Contacts data
        #

        contact1 = {}
        contact1["recipients"] = []
        contact1["mid"] = 0xcada00010000001
        contact1["fai"] = False
        contact1["cache"] = {}
        contact1["properties"] = {}
        contact1["attachments"] = {}

        contact1["properties"]["PidTagAccess"] = 63
        contact1["properties"]["PidTagAccessLevel"] = 1
        contact1["properties"]["PidTagChangeKey"] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        contact1["properties"]["PidTagCreationTime"] = float((datetime.now(tz=timezone('Europe/Madrid')) - timedelta(hours=1)).strftime("%s.%f"))
        contact1["properties"]["PidTagLastModificationTime"] = contact1["properties"]["PidTagCreationTime"]
        contact1["properties"]["PidTagObjectType"] = 5
        contact1["properties"]["PidTagRecordKey"] = bytearray(uuid.uuid1().bytes)
        contact1["properties"]["PidTagSearchKey"] = bytearray(uuid.uuid1().bytes)

        contact1["properties"]["PidTagFolderId"] = 0xcadabeef0000001
        contact1["properties"]["PidTagMid"] = contact1["mid"]
        contact1["properties"]["PidTagInstID"] = contact1["mid"]
        contact1["properties"]["PidTagInstanceNum"] = 0
        contact1["properties"]["PidTagRowType"] = 1
        contact1["properties"]["PidTagDepth"] = 0
        contact1["properties"]["PidTagMessageFlags"] = 1 # msfRead
        contact1["properties"]["PidTagMessageClass"] = "IPM.Contact"

        # Full Name
        contact1["properties"]["PidTagDisplayNamePrefix"] = ''
        contact1["properties"]["PidTagGivenName"] = "Julien"
        contact1["properties"]["PidTagMiddleName"] = ''
        contact1["properties"]["PidTagSurname"] = "Kerihuel"
        contact1["properties"]["PidTagDisplayName"] = "%s%s %s %s" % (contact1["properties"]["PidTagDisplayNamePrefix"],
                                                                      contact1["properties"]["PidTagGivenName"],
                                                                      contact1["properties"]["PidTagMiddleName"],
                                                                      contact1["properties"]["PidTagSurname"])
        contact1["properties"]["PidTagCompanyName"] = "OpenChange Project" # Company
        contact1["properties"]["PidTagTitle"] = "Project Founder" # Job tile
        contact1["properties"]["0x91c2001f"] = "OpenChange Project Founder" # File As
        contact1["properties"]["PidTagBody"] = "OpenChange Project Founder and Lead Developer since December, 2003"

        contact1["properties"]["0x91ad0001"] = "SMTP" # PidLidEmail1AddressType
        contact1["properties"]["0x91ae001f"] = "j.kerihuel@openchange.org" # PidLidEmail1EmailAddress
        contact1["properties"]["0x91b0001f"] = contact1["properties"]["0x91ae001f"] # PidLidEmail1OriginalDisplayName
        # PidLidEmail1DisplayName Display as
        contact1["properties"]["0x91af001f"] = "%s <%s>" % (contact1["properties"]["PidTagDisplayName"],
                                                            contact1["properties"]["0x91ae001f"])
        contact1["properties"]["0x930d001f"] = "http://www.openchange.org" # PidLidHtml
        contact1["properties"]["PidTagBusinessHomePage"] = contact1["properties"]["0x930d001f"]
        contact1["properties"]["0x92a6001f"] = "@jkerihuel" # PidLidInstantMessagingAddress
        contact1["properties"]["PidTagBusinessTelephoneNumber"] = "Phone Office"
        contact1["properties"]["PidTagHomeTelephoneNumber"] = "Phone Home"
        contact1["properties"]["PidTagMobileTelephoneNumber"] = "Phone Mobile"
        contact1["properties"]["PidTagBusinessFaxNumber"] = "Fax Office"

        contact1["properties"]["PidTagStreetAddress"] = "Street"
        contact1["properties"]["PidTagLocality"] = "City"
        contact1["properties"]["PidTagStateOrProvince"] = "State/Province"
        contact1["properties"]["PidTagPostalCode"] = "ZIP/Postal code"
        contact1["properties"]["PidTagCountry"] = "France"
        contact1["properties"]["0x9309001f"] = "FR" # PidLidAddressCountryCode

        # PidTagPostalAddress
        contact1["properties"]["PidTagPostalAddress"] = "%s\n%s %s %s\n%s" % (contact1["properties"]["PidTagStreetAddress"],
                                                                              contact1["properties"]["PidTagPostalCode"],
                                                                              contact1["properties"]["PidTagLocality"],
                                                                              contact1["properties"]["PidTagStateOrProvince"],
                                                                              contact1["properties"]["PidTagCountry"])

        # Business Address
        contact1["properties"]["0x90a6001f"] = contact1["properties"]["PidTagStreetAddress"] # PidLidWorkAddressStreet
        contact1["properties"]["0x9097001f"] = contact1["properties"]["PidTagLocality"] # PidLidWorkAddressCity
        contact1["properties"]["0x90a5001f"] = contact1["properties"]["PidTagStateOrProvince"] # PidLidWorkAddressState
        contact1["properties"]["0x909e001f"] = contact1["properties"]["PidTagPostalCode"] # PidLidWorkAddressPostalCode
        contact1["properties"]["0x908d001f"] = contact1["properties"]["PidTagCountry"] # PidLidWorkAddressCountry
        contact1["properties"]["0x9306001f"] = contact1["properties"]["0x9309001f"] # PidLidWorkAddressCountryCode

        # PidLidWorkAddress
        contact1["properties"]["0x90a7001f"] = contact1["properties"]["PidTagPostalAddress"]


        self.mapping[0xcadabeef0000001] = {}
        self.mapping[0xcadabeef0000001]["uri"] = "cadabeef0000001/"
        self.mapping[0xcadabeef0000001]["properties"] = {}
        self.mapping[0xcadabeef0000001]["properties"]["PidTagFolderId"] = 0xcadabeef0000001
        self.mapping[0xcadabeef0000001]["properties"]["PidTagDisplayName"] = "SampleContact"
        self.mapping[0xcadabeef0000001]["properties"]["PidTagComment"] = "Sample Contact Folder"
        self.mapping[0xcadabeef0000001]["properties"]["PidTagContainerClass"] = "IPF.Contact"
        self.mapping[0xcadabeef0000001]["properties"]["PidTagDefaultPostMessageClass"] = "IPM.Contact"
        self.mapping[0xcadabeef0000001]["properties"]["PidTagAccess"] = 63
        self.mapping[0xcadabeef0000001]["properties"]["PidTagRights"] = 2043
        self.mapping[0xcadabeef0000001]["subfolders"] = []
        self.mapping[0xcadabeef0000001]["messages"] = [contact1]
        self.mapping[0xcadabeef0000001]["cache"] = {}
        self.mapping[0xcadabeef0000001]["cache"]["properties"] = {}
        self.mapping[0xcadabeef0000001]["cache"]["messages"] = []



        #
        # Tasks data
        #
        task1 = {}
        task1["recipients"] = []
        task1["attachments"] = []
        task1["mid"] = 0xcafe00010000001
        task1["fai"] = False
        task1["cache"] = {}
        task1["properties"] = {}

        task1["properties"]["PidTagAccess"] = 63
        task1["properties"]["PidTagAccessLevel"] = 1
        task1["properties"]["PidTagChangeKey"] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        task1["properties"]["PidTagCreationTime"] = float((datetime.now(tz=timezone('Europe/Madrid')) - timedelta(hours=1)).strftime("%s.%f"))
        task1["properties"]["PidTagLastModificationTime"] = task1["properties"]["PidTagCreationTime"]
        task1["properties"]["PidTagLastModifierName"] = "julien"
        task1["properties"]["PidTagObjectType"] = 5
        task1["properties"]["PidTagRecordKey"] = bytearray(uuid.uuid1().bytes)
        task1["properties"]["PidTagSearchKey"] = bytearray(uuid.uuid1().bytes)
        task1["properties"]["PidTagFolderId"] = 0xcafebeef0000001
        task1["properties"]["PidTagMid"] = task1["mid"]
        task1["properties"]["PidTagInstID"] = task1["mid"]
        task1["properties"]["PidTagInstanceNum"] = 0
        task1["properties"]["PidTagRowType"] = 1
        task1["properties"]["PidTagDepth"] = 0
        task1["properties"]["PidTagMessageClass"] = "IPM.Task"
        task1["properties"]["PidTagMessageFlags"] = 1

        task1["properties"]["PidTagSubjectPrefix"] = ''
        task1["properties"]["PidTagSubject"] = "Subject of the task"
        task1["properties"]["PidTagNormalizedSubject"] = task1["properties"]["PidTagSubject"]
        task1["properties"]["PidTagConversationTopic"] = task1["properties"]["PidTagSubject"]
        task1["properties"]["PidTagPriority"] = 1
        task1["properties"]["PidTagImportance"] = 2
        task1["properties"]["PidTagSensitivity"] = 2
        task1["properties"]["PidTagIconIndex"] = 0x500
        task1["properties"]["0x91200003"] = 1 # PidLidTaskStatus
        task1["properties"]["0x91210005"] = float(0.500000) # PidLidPercentComplete
        task1["properties"]["0x9224000b"] = True # PidLidPrivate
        task1["properties"]["0x911e0040"] = float(datetime.now(tz=timezone('Europe/Madrid')).strftime("%s.%f")) # PidLidTaskStartDate
        task1["properties"]["0x911f0040"] = float((datetime.now(tz=timezone('Europe/Madrid')) + timedelta(hours=2)).strftime("%s.%f")) # PidLidTaskEndDate
        task1["properties"]["0x90db101f"] = ["Green Category", "Orange Category"]
        task1["properties"]["PidTagBody"] = "This task is not very complex. It however tries to fill most of the common and available fields."

        # Reminder
        task1["properties"]["0x91ee000b"] = True # PidLidReminderSet
        task1["properties"]["0x91eb0040"] = float((datetime.now(tz=timezone('Europe/Madrid')) + timedelta(minutes=1)).strftime("%s.%f")) # PidLidReminderSignalTime
        task1["properties"]["0x91ef0040"] = float((datetime.now(tz=timezone('Europe/Madrid')) + timedelta(minutes=1)).strftime("%s.%f")) # PidLidReminderTime

        self.mapping[0xcafebeef0000001] = {}
        self.mapping[0xcafebeef0000001]["uri"] = "cafebeef0000001/"
        self.mapping[0xcafebeef0000001]["properties"] = {}
        self.mapping[0xcafebeef0000001]["properties"]["PidTagFolderId"] = 0xcafebeef0000001
        self.mapping[0xcafebeef0000001]["properties"]["PidTagDisplayName"] = "SampleTask"
        self.mapping[0xcafebeef0000001]["properties"]["PidTagComment"] = "Sample Task Folder"
        self.mapping[0xcafebeef0000001]["properties"]["PidTagContainerClass"] = "IPF.Task"
        self.mapping[0xcafebeef0000001]["properties"]["PidTagDefaultPostMessageClass"] = "IPM.Task"
        self.mapping[0xcafebeef0000001]["properties"]["PidTagAccess"] = 63
        self.mapping[0xcafebeef0000001]["properties"]["PidTagRights"] = 2043
        self.mapping[0xcafebeef0000001]["subfolders"] = []
        self.mapping[0xcafebeef0000001]["messages"] = [task1]
        self.mapping[0xcafebeef0000001]["cache"] = {}
        self.mapping[0xcafebeef0000001]["cache"]["properties"] = {}
        self.mapping[0xcafebeef0000001]["cache"]["messages"] = []

        print '[PYTHON]: %s context class __init__' % self.name
        return

    def get_path(self, fmid):
        print '[PYTHON]: %s context.get_path' % self.name
        if fmid in self.mapping:
            print '[PYTHON]: %s get_path URI: %s' % (self.name, self.mapping[fmid]["uri"])
            return self.mapping[fmid]["uri"]
        else:
            print '[Python]: %s get_path URI: None' % (self.name)
            return None

    def get_root_folder(self, folderID):
        print '[PYTHON]: %s context.get_root_folder' % self.name
        folder = FolderObject(self.mapping[folderID], None, folderID, 0x0)
        return (0, folder)


class FolderObject(ContextObject):

    def __init__(self, basedict, parentdict, folderID, parentFID):
        print '[PYTHON]: %s folder.__init__(fid=%s)' % (self.name, folderID)
        self.basedict = basedict
        self.parentdict = parentdict
        self.parentFID = parentFID;
        self.folderID = folderID;
        return

    def open_folder(self, folderID):
        print '[PYTHON]: %s folder.open_folder(0x%x)' % (self.name, folderID)

        for item in self.basedict["subfolders"]:
            if str(item["fid"]) == str(folderID):
                print '[PYTHON]: folderID 0x%x found\n' % (folderID)
                return FolderObject(item, self.basedict, folderID, self.folderID)
        return None

    def create_folder(self, properties, folderID):
        print '[PYTHON]: %s folder.create_folder(%s)' % (self.name, folderID)
        j = json.dumps(properties, indent=4)
        print '[PYTHON]: %s ' % j

        folder = {}
        folder["uri"] = "%s/0x%x" % (self.basedict["uri"], folderID)
        folder["fid"] = folderID
        folder["properties"] = {}
        folder["properties"]["PidTagFolderId"] = folder["fid"]
        if properties.has_key("PidTagDisplayName"):
            folder["properties"]["PidTagDisplayName"] = properties["PidTagDisplayName"]
        if properties.has_key("PidTagComment"):
            folder["properties"]["PidTagComment"] = properties["PidTagComment"]
        folder["subfolders"] = []
        folder["messages"] = []
        folder["cache"] = {}
        folder["cache"]["properties"] = {}
        folder["cache"]["messages"] = []

        self.basedict["subfolders"].append(folder)

        return (0, FolderObject(folder, self.basedict, folderID, self.folderID))

    def delete(self):
        print '[PYTHON]: %s folder.delete(%s)' % (self.name, self.folderID)
        for item in self.parentdict["subfolders"]:
            if str(item["fid"]) == str(self.folderID):
                self.parentdict["subfolders"].remove(item)
                return 0
        return 17

    def open_table(self, table_type):
        print '[PYTHON]: %s folder.open_table' % (self.name)
        table = TableObject(self, table_type)
        return (table, self.get_child_count(table_type))

    def get_child_count(self, table_type):
        print '[PYTHON]: %s folder.get_child_count' % (self.name)
        counter = { 1: self._count_folders,
                    2: self._count_messages,
                    3: self._count_zero,
                    4: self._count_zero,
                    5: self._count_zero,
                    6: self._count_zero
                }
        return counter[table_type]()

    def _count_folders(self):
        print '[PYTHON][INTERNAL]: %s folder._count_folders(0x%x): %d' % (self.name, self.folderID, len(self.basedict["subfolders"]))
        return len(self.basedict["subfolders"])

    def _count_messages(self):
        print '[PYTHON][INTERNAL]: %s folder._count_messages(0x%x): %d' % (self.name, self.folderID, len(self.basedict["messages"]))
        return len(self.basedict["messages"])

    def _count_zero(self):
        return 0

    def open_message(self, mid, rw):
        print '[PYTHON]: %s folder.open_message()' % self.name

        for item in self.basedict["messages"]:
            if str(item["mid"]) == str(mid):
                print '[PYTHON]: messageID 0x%x found\n' % (mid)
                return MessageObject(item, self, mid, rw)
        return None

    def create_message(self, mid, associated):
        print '[PYTHON]: %s folder.create_message()' % self.name
        newmsg = {}
        newmsg["recipients"] = []
        newmsg["attachments"] = []
        newmsg["mid"] = mid
        newmsg["fai"] = associated
        newmsg["cache"] = {}
        newmsg["properties"] = {}
        newmsg["properties"]["PidTagMessageId"] = newmsg["mid"]
        self.basedict["cache"]["messages"].append(newmsg)
        return MessageObject(newmsg, self, mid, 1)

    def get_properties(self, properties):
        print '[PYTHON]: %s folder.get_properties()' % (self.name)
        print properties
        return self.basedict["properties"]

    def set_properties(self, properties):
        print '[PYTHON]: %s folder.set_properties()' % (self.name)

        tmpdict = self.basedict["cache"].copy()
        tmpdict.update(properties)
        self.basedict["cache"] = tmpdict

        print self.basedict["cache"]
        return 0

class TableObject(BackendObject):

    def __init__(self, folder, tableType):
        print '[PYTHON]: %s table.__init__()' % (self.name)
        self.folder = folder
        self.tableType = tableType
        self.properties = []
        return

    def set_columns(self, properties):
        print '[PYTHON]: %s table.set_columns()' % (self.name)
        self.properties = properties
        print 'properties: [%s]\n' % ', '.join(map(str, self.properties))
        return 0

    def get_row(self, rowId, query_type):
        print '[PYTHON]: %s table.get_row()' % (self.name)

        rowdata = None
        if self.tableType == 1:
            subfolders = self.folder.basedict["subfolders"]
            if (len(subfolders) > rowId and
                subfolders[rowId] and
                subfolders[rowId].has_key("properties")):
                rowdata = subfolders[rowId]["properties"]
        elif self.tableType == 2:
            messages = self.folder.basedict["messages"]
            if (len(messages) > rowId and
                messages[rowId] and
                messages[rowId].has_key("properties")):
                rowdata = messages[rowId]["properties"]
        elif self.tableType == 5:
            attachments = self.folder.message["attachments"]
            if (len(attachments) > rowId and
                attachments[rowId] and
                attachments[rowId].has_key("properties")):
                rowdata = attachments[rowId]["properties"]
        return (self.properties, rowdata)


    def get_row_count(self, query_type):
        print '[PYTHON]: %s table.get_row_count()' % (self.name)
        return self.folder.get_child_count(self.tableType)

class MessageObject(BackendObject):

    def __init__(self, message, folder, mid, rw):
        print '[PYTHON]: %s message.__init__()' % (self.name)
        self.folder = folder
        self.message = message
        self.mid = mid
        self.rw = rw

        return

    def get_message_data(self):
        print '[PYTHON]: %s message.get_message_data()' % (self.name)
        return (self.message["recipients"], self.message["properties"])

    def get_properties(self, properties):
        print '[PYTHON]: %s message.get_properties()' % (self.name)
        return self.message["properties"]

    def set_properties(self, properties):
        print '[PYTHON]: %s message.set_properties()' % (self.name)

        tmpdict = self.message["cache"].copy()
        tmpdict.update(properties)
        self.message["cache"] = tmpdict;

        print self.message["cache"]
        return 0

    def save(self):
        print '[PYTHON]: %s message.save()' % (self.name)

        for item in self.folder.basedict["cache"]["messages"]:
            if str(item["mid"]) == str(self.mid):
                self.folder.basedict["messages"].append(item)
                self.folder.basedict["cache"]["messages"].remove(item)
                return 0
        return 17

    def _count_attachments(self):
        print '[PYTHON][INTERNAL]: %s message._count_folders(0x%x): %d' % (self.name, self.mid, len(self.message["attachments"]))
        return len(self.message["attachments"])

    def get_child_count(self, table_type):
        print '[PYTHON]: %s message.get_child_count' % (self.name)
        counter = { 5: self._count_attachments }
        return counter[table_type]()

    def open_attachment(self, attach_id):
        print '[PYTHON]: %s message.open_attachment(): %d/%d' % (self.name, attach_id,
                                                                 len(self.message["attachments"]))
        if attach_id > len(self.message["attachments"]):
            return None
        return AttachmentObject(self.message["attachments"][attach_id], self, attach_id)


    def get_attachment_table(self):
        print '[PYTHON]: %s message.get_attachment_table()' % (self.name)
        table = TableObject(self, 5)
        return (table, self.get_child_count(5))


class AttachmentObject(BackendObject):

    def __init__(self, attachment, message, attachid):
        print '[PYTHON]: %s attachment.__init__()' % (self.name)
        print attachment
        self.attachment = attachment
        self.message = message
        self.attachid = attachid
        return

    def get_properties(self, properties):
        print '[PYTHON]: %s message.get_properties()' % (self.name)
        return self.attachment["properties"]



###################################################################################################################################
####### Provision Code ############################################################################################################
###################################################################################################################################


class OpenchangeDBWithMySQL():
    def __init__(self, uri):
        self.uri = uri
        self.db = self._connect_to_mysql(uri)

    def _connect_to_mysql(self, uri):
        if not uri.startswith('mysql://'):
            raise ValueError("Bad connection string for mysql: invalid schema")

        if '@' not in uri:
            raise ValueError("Bad connection string for mysql: expected format "
                             "mysql://user[:passwd]@host/db_name")
        user_passwd, host_db = uri.split('@')

        user_passwd = user_passwd[len('mysql://'):]
        if ':' in user_passwd:
            user, passwd = user_passwd.split(':')
        else:
            user, passwd = user_passwd, ''

        if '/' not in host_db:
            raise ValueError("Bad connection string for mysql: expected format "
                             "mysql://user[:passwd]@host/db_name")
        host, db = host_db.split('/')

        return MySQLdb.connect(host=host, user=user, passwd=passwd, db=db)

    def select(self, sql, params=()):
        try:
            cur = self.db.cursor()
            cur.execute(sql, params)
            rows = cur.fetchall()
            cur.close()
            self.op_error = False
            return rows
        except MySQLdb.OperationalError as e:
            # FIXME: It may be leaded by another error
            # Reconnect and rerun this
            if self.op_error:
                raise e
            self.db = self._connect_to_mysql(self.uri)
            self.op_error = True
            return self.select(sql, params)
        except MySQLdb.ProgrammingError as e:
            print "Error executing %s with %r: %r" % (sql, params, e)
            raise e

    def insert(self, sql, params=()):
        self.delete(sql, params)

    def delete(self, sql, params=()):
        cur = self.db.cursor()
        cur.execute(sql, params)
        self.db.commit()
        cur.close()
        return

if __name__ == '__main__':
    parser = optparse.OptionParser("sample.py [options]")

    sambaopts = options.SambaOptions(parser)
    parser.add_option_group(sambaopts)

    parser.add_option("--status", action="store_true",
                      help="Status of sample backend for specified user")
    parser.add_option("--provision", action="store_true",
                      help="Provision sample backend for specified user")
    parser.add_option("--deprovision", action="store_true",
                      help="Deprovision sample backend for specified user")
    parser.add_option("--username", type="string", help="User mailbox to update")

    opts,args = parser.parse_args()
    if len(args) != 0:
        parser.print_help()
        sys.exit(1)

    lp = sambaopts.get_loadparm()

    username = opts.username
    if username is None:
        print "[ERROR] No username specified"
        sys.exit(1)
    if username.isalnum() is False:
        print "[ERROR] Username must be alpha numeric"
        sys.exit(1)

    openchangedb_uri = lp.get("mapiproxy:openchangedb")
    if openchangedb_uri is None:
        print "This script only supports MySQL backend for openchangedb"

    if opts.provision and opts.deprovision:
        print "[ERROR] Incompatible options: provision and deprovision"
        sys.exit(1)


    c = OpenchangeDBWithMySQL(openchangedb_uri)
    backend = BackendObject()

    # namespace like
    nslike = backend.namespace + '%'

    # Retrieve mailbox_id, ou_id
    rows = c.select("SELECT id,ou_id FROM mailboxes WHERE name=%s", username)
    if len(rows) == 0:
        print "[ERROR] No such user %s" % username
        sys.exit(1)
    mailbox_id = rows[0][0]
    ou_id = rows[0][1]

    rows = c.select("SELECT * FROM folders WHERE MAPIStoreURI LIKE %s AND mailbox_id=%s AND ou_id=%s",
                    (nslike, mailbox_id, ou_id))

    if opts.status:
        if (rows):
            print "[INFO] Sample backend is provisioned for user %s the following folders:" % username
            for row in rows:
                name = c.select("SELECT value FROM folders_properties WHERE name=\"PidTagDisplayName\" AND folder_id=%s", row[0])
                print "\t* %-40s (%s)" % (name[0][0], row[len(row) - 1])
        else:
            print "[INFO] Sample backend is not provisioned for user %s" % username
        sys.exit(0)

    if opts.provision:
        if (rows):
            print "[WARN] Sample backend is already provisioned for user %s" % username
            sys.exit(0)

        rows = c.select("SELECT id,folder_id FROM folders WHERE SystemIdx=12 AND mailbox_id=%s AND ou_id=%s", (mailbox_id, ou_id))
        if len(rows) == 0:
            print "[ERROR] No such SystemIdx for mailbox_id=%s" % mailbox_id
        system_id = rows[0][0]
        parent_id = rows[0][1]

        contexts = backend.list_contexts(username)
        for context in contexts:
            url = "%s%s" % (backend.namespace, context["url"])
            folder_id = int(context["url"].replace('/', ''), 16)
            c.insert("INSERT INTO folders (ou_id,folder_id,folder_class,mailbox_id,"
                     "parent_folder_id,FolderType,SystemIdx,MAPIStoreURI) VALUES "
                     "(%s, %s, \"system\", %s, %s, 1, -1, %s)",
                     (ou_id, folder_id, mailbox_id, system_id, url))

            rows = c.select("SELECT MAX(id) FROM folders")
            fid = rows[0][0]
            (ret,ctx) = backend.create_context(url)
            (ret,fld) = ctx.get_root_folder(folder_id)

            props = fld.get_properties(None)

            for prop in props:
                c.insert("INSERT INTO folders_properties (folder_id, name, value) VALUES (%s, %s, %s)", (fid, prop, props[prop]))
            c.insert("INSERT INTO folders_properties (folder_id, name, value) VALUES (%s, \"PidTagParentFolderId\", %s)", (fid, parent_id))
        print "[DONE]"

    if opts.deprovision:
        if not rows:
            print "[WARN] Sample backend is not provisioned for user %s" % username
            sys.exit(0)

        for row in rows:
            c.delete("DELETE FROM folders WHERE id=%s", row[0])
            c.delete("DELETE FROM folders_properties WHERE folder_id=%s", row[0])
        c.delete("DELETE FROM mapistore_indexing WHERE url LIKE %s", nslike)
        print "[DONE]"
