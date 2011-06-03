#!/usr/bin/python

# OpenChange provisioning
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2009
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2009
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

import os
import samba
from samba import Ldb, unix2nttime
import ldb
import uuid
import time
from openchange.urlutils import *

__docformat__ = 'restructuredText'


class NoSuchServer(Exception):
    """Raised when a server could not be found."""


class OpenChangeDB(object):
    """The OpenChange database.
    """

    def __init__(self, url):
        self.url = url
        self.ldb = Ldb(self.url)
	self.nttime = samba.unix2nttime(int(time.time()))

    def reopen(self):
        self.ldb = Ldb(self.url)

    def setup(self):
        self.ldb.add_ldif("""
dn: @OPTIONS
checkBaseOnSearch: TRUE

dn: @INDEXLIST
@IDXATTR: cn

dn: @ATTRIBUTES
cn: CASE_INSENSITIVE
dn: CASE_INSENSITIVE

""")
        self.reopen()

    def add_rootDSE(self, ocserverdn, firstorg, firstou):
        self.ldb.add({"dn": "@ROOTDSE",
                      "defaultNamingContext": "CN=%s,%s,%s" % (firstou, firstorg, ocserverdn),
                      "rootDomainNamingContext": ocserverdn,
                      "vendorName": "OpenChange Team (http://www.openchange.org)"})

    def add_server(self, ocserverdn, netbiosname, firstorg, firstou):
        self.ldb.add({"dn": ocserverdn,
                  "objectClass": ["top", "server"],
                  "cn": netbiosname,
                  "GlobalCount": "0x1",
                  "ReplicaID": "0x1"})
        self.ldb.add({"dn": "CN=%s,%s" % (firstorg, ocserverdn),
                  "objectClass": ["top", "org"],
                  "cn": firstorg})
        self.ldb.add({"dn": "CN=%s,CN=%s,%s" % (firstou, firstorg, ocserverdn),
                  "objectClass": ["top", "ou"],
                  "cn": firstou})

    def add_root_public_folder(self, pfdn, fid, SystemIdx, childcount, mapistoreURL):
        self.ldb.add({"dn": "CN=%s,%s" % (fid, pfdn),
                      "objectClass": ["publicfolder"],
                      "cn": fid,
                      "PidTagFolderId": fid,
                      "PidTagDisplayName": "Public Folder Root",
                      "PidTagCreationTime": "0x%.16x" % self.nttime,
                      "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                      "PidTagSubFolders": "TRUE" if (childcount != 0) else "FALSE",
                      "PidTagFolderChildCount": str(childcount),
                      "SystemIdx": str(SystemIdx)})
        return mapistoreURL + "/" + fid

    def add_sub_public_folder(self, pfdn, parentfid, fid, name, SystemIndex, childcount, mapistoreURL):
        self.ldb.add({"dn": "CN=%s,%s" % (fid, pfdn),
                      "objectClass": ["publicfolder"],
                      "cn": fid,
                      "PidTagParentFolderId": parentfid,
                      "PidTagFolderId": fid,
                      "PidTagDisplayName": name,
                      "PidTagCreationTime": "0x%.16x" % self.nttime,
                      "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                      "PidTagAttributeHidden": str(0),
                      "PidTagAttributeReadOnly": str(0),
                      "PidTagAttributeSystem": str(0),
                      "PidTagContainerClass": "IPF.Note (check this)", 
                      "PidTagSubFolders": "TRUE" if (childcount != 0) else "FALSE",
                      "PidTagFolderChildCount": str(childcount),
                      "FolderType": str(1),
                      "FolderType": str(1),
                      "SystemIdx": str(SystemIndex)})
        return mapistoreURL + "/" + fid

    def add_one_public_folder(self, parent_fid, path, children, SystemIndex, names, mapistoreURL):
        name = path[-1]
        GlobalCount = self.get_message_GlobalCount(names.netbiosname)
        ReplicaID = self.get_message_ReplicaID(names.netbiosname)
        pfdn = "CN=publicfolders,CN=%s,CN=%s,%s" % (names.firstou, names.firstorg, names.ocserverdn)
        fid = gen_mailbox_folder_fid(GlobalCount, ReplicaID)
        childcount = len(children)
        print "\t* %-40s %s" % (name, fid)
        if parent_fid == 0:
            mapistoreURL = self.add_root_public_folder(pfdn, fid, SystemIndex, childcount, mapistoreURL)
        else:
            mapistoreURL = self.add_sub_public_folder(pfdn, parent_fid, fid, name, SystemIndex, childcount, mapistoreURL);

        GlobalCount += 1
        self.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)

        for name, grandchildren in children.iteritems():
            self.add_one_public_folder(fid, path + (name,), grandchildren[0], grandchildren[1], names, mapistoreURL)
        return mapistoreURL

    def add_mapistore_pf_dir(self, mapistoreURL):
        mapistorepath = openchangedb_mapistore_url_split(mapistoreURL)[1]
        if not os.path.isdir(mapistorepath):
            os.makedirs(mapistorepath, mode=0700)

    def add_public_folders(self, names, mapistoreURL):
        self.add_mapistore_pf_dir(mapistoreURL)
        pfstoreGUID = str(uuid.uuid4())
        self.ldb.add({"dn": "CN=publicfolders,CN=%s,CN=%s,%s" % (names.firstou, names.firstorg, names.ocserverdn),
                "objectClass": ["container"],
                "cn": "publicfolders",
                "StoreGUID": pfstoreGUID,
                "ReplicaID": str(1)})
        public_folders = ({
            "IPM_SUBTREE": ({}, 1),
            "NON_IPM_SUBTREE": ({
                "EFORMS REGISTRY": ({}, 3),
                "Events Root": ({}, -1),
                "OFFLINE ADDRESS BOOK": ({
                        "/o=%s/cn=addrlists/cn=oabs/cn=Default Offline Address Book" % (names.firstorg): ({}, 8),
                }, 5),
                "SCHEDULE+ FREE BUSY": ({
                        "EX:/o=%s/ou=Exchange Administrative Group (%s)" % (names.firstorg, names.netbiosname): ({}, 7),
                }, 4),
            }, 2),
        }, 0)

        self.add_one_public_folder(0, ("Public Folder Root",), public_folders[0], public_folders[1], names, mapistoreURL)
        
    def lookup_server(self, cn, attributes=[]):
        # Step 1. Search Server object
        filter = "(&(objectClass=server)(cn=%s))" % cn
        res = self.ldb.search("", scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=attributes)
        if len(res) != 1:
            raise NoSuchServer(cn)
        return res[0]

    def lookup_mailbox_user(self, server, username, attributes=[]):
        """Check if a user already exists in openchange database.

        :param server: Server object name
        :param username: Username object
        :return: LDB Object of the user
        """
        server_dn = self.lookup_server(server, []).dn

        # Step 2. Search User object
        filter = "(&(objectClass=mailbox)(cn=%s))" % (username)
        return self.ldb.search(server_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=attributes)

    def user_exists(self, server, username):
        """Check whether a user exists.

        :param username: Username of the user
        :param server: Server object name
        """
        return len(self.lookup_mailbox_user(server, username)) == 1

    def get_message_attribute(self, server, attribute):
        """Retrieve attribute value from given message database (server).

        :param server: Server object name
        """
        return int(self.lookup_server(server, [attribute])[attribute][0], 16)

    def get_message_ReplicaID(self, server):
        """Retrieve current mailbox Replica ID for given message database (server).

        :param server: Server object name
        """
        return self.get_message_attribute(server, "ReplicaID")

    def get_message_GlobalCount(self, server):
        """Retrieve current mailbox Global Count for given message database (server).

        :param server: Server object name
        """
        return self.get_message_attribute(server, "GlobalCount")


    def set_message_GlobalCount(self, server, GlobalCount):
        """Update current mailbox GlobalCount for given message database (server).

        :param server: Server object name
        :param index: Mailbox new GlobalCount value
        """
        server_dn = self.lookup_server(server, []).dn

        newGlobalCount = """
dn: %s
changetype: modify
replace: GlobalCount
GlobalCount: 0x%x
""" % (server_dn, GlobalCount)

        self.ldb.transaction_start()
        try:
            self.ldb.modify_ldif(newGlobalCount)
        finally:
            self.ldb.transaction_commit()

    def get_user_attribute(self, server, username, attribute):
        """Retrieve attribute value from given mailbox (server, username).

        :param server: Server object name
        :param username: User name
        """
        return self.lookup_mailbox_user(server, username, [attribute])[0][attribute]

    def get_user_MailboxGUID(self, server, username):
        return self.get_user_attribute(server, username, "MailboxGUID")

    def get_user_ReplicaGUID(self, server, username):
        return self.get_user_attribute(server, username, "ReplicaGUID")

    def add_mailbox_user(self, basedn, username):
        """Add a user record in openchange database.

        :param username: Username
        :return: DN of the created object
        """
        
        mailboxGUID = str(uuid.uuid4())
        replicaID = str(1)
        replicaGUID = str(uuid.uuid4())
        
        retdn = "CN=%s,%s" % (username, basedn)
        self.ldb.add({"dn": retdn,
                  "objectClass": ["mailbox", "container"],
                  "PidTagDisplayName": "OpenChange Mailbox: %s" % (username),
                  "PidTagCreationTime": "0x%.16x" % self.nttime,
                  "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                  "PidTagParentFolderId": "0x0000000000000000",
                  "PidTagSubFolders": "TRUE",
                  "cn": username,
                  "MailboxGUID": mailboxGUID,
                  "ReplicaID": replicaID,
                  "ReplicaGUID": replicaGUID})
        return retdn

    def add_storage_dir(self, mapistoreURL, username):
        """Add mapistore storage space for the user

        :param username: Username object
        :param mapistore: mapistore object
        """

        mapistore_dir = os.path.join(mapistoreURL, username)
        if not os.path.isdir(mapistore_dir):
            os.makedirs(mapistore_dir, mode=0700)
    
    def add_folder_property(self, folderID, attribute, value):
        """Add a attribute/value to the record folderID refers to

        :param folderID: the folder identifier where to add attribute/value
        :param attribute: the ldb attribute
        :param value: the attribute value
        """

        # Step 1. Find the folder record
        res = self.ldb.search("", ldb.SCOPE_SUBTREE, "(PidTagFolderId=%s)" % folderID, ["*"])
        if len(res) != 1:
            raise Exception("Invalid search (PidTagFolderId=%s)" % folderID)

        # Step 2. Add attribute/value pair
        m = ldb.Message()
        m.dn = ldb.Dn(self.ldb, "%s" % res[0].dn)
        m[attribute] = ldb.MessageElement([value], ldb.CHANGETYPE_ADD, attribute);
        self.ldb.modify(m)

    def add_mailbox_root_folder(self, ocfirstorgdn, username, 
                                foldername, parentfolder, 
                                GlobalCount, ReplicaID,
                                SystemIdx, mapistoreURL,
                                mapistoreSuffix):
        """Add a root folder to the user mailbox

        :param username: Username object
        :param foldername: Folder name
        :param GlobalCount: current global counter for message database
        :param ReplicaID: replica identifier for message database
        :param SystemIdx: System Index for root folders
        :param mapistoreURL: storage URL prefix (including type)
        :param mapistoreSuffix: file type suffix to use with mapistore
        """

        ocuserdn = "CN=%s,%s" % (username, ocfirstorgdn)
        FID = gen_mailbox_folder_fid(GlobalCount, ReplicaID)

        # Step 1. If we are handling Mailbox Root
        if parentfolder == 0:
            m = ldb.Message()
            m.dn = ldb.Dn(self.ldb, ocuserdn)
            m["PidTagFolderId"] = ldb.MessageElement([FID], ldb.CHANGETYPE_ADD, "PidTagFolderId")
            m["SystemIdx"] = ldb.MessageElement([str(SystemIdx)], ldb.CHANGETYPE_ADD, "SystemIdx")
            self.ldb.modify(m)
            return FID

        # Step 2. Lookup parent DN
        res = self.ldb.search("", ldb.SCOPE_SUBTREE, "(PidTagFolderId=%s)" % parentfolder, ["*"])
        if len(res) != 1:
            raise Exception("Invalid search (PidTagFolderId=%s)" % parentfolder)

        # Step 3. Add root folder to correct container
        if (foldername == "IPM Subtree"):
             self.ldb.add({"dn": "CN=%s,%s" % (FID, res[0].dn),
                          "objectClass": ["systemfolder", "container"],
                          "cn": FID,
                          "PidTagParentFolderId": parentfolder,
                          "PidTagFolderId": FID,
                          "PidTagDisplayName": foldername,
                          "PidTagCreationTime": "0x%.16x" % self.nttime,
                          "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                          "PidTagAttributeHidden": str(0),
                          "PidTagAttributeReadOnly": str(0),
                          "PidTagAttributeSystem": str(0),
                          "PidTagContainerClass": "IPF.Note",
                          "PidTagSubFolders": "TRUE",
                          "FolderType": str(1),
                          "SystemIdx": str(SystemIdx)})           
        elif (foldername == "To-Do Search"):
            self.ldb.add({"dn": "CN=%s,%s" % (FID, res[0].dn),
                          "objectClass": ["systemfolder", "container"],
                          "cn": FID,
                          "PidTagParentFolderId": parentfolder,
                          "PidTagFolderId": FID,
                          "PidTagDisplayName": foldername,
                          "PidTagCreationTime": "0x%.16x" % self.nttime,
                          "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                          "PidTagAttributeHidden": str(0),
                          "PidTagAttributeReadOnly": str(0),
                          "PidTagAttributeSystem": str(0),
                          "PidTagContainerClass": "IPF.Note",
                          "PidTagSubFolders": "FALSE",
                          "FolderType": str(1),
                          "SystemIdx": str(SystemIdx)})
        else:
            self.ldb.add({"dn": "CN=%s,%s" % (FID, res[0].dn),
                          "objectClass": ["systemfolder"],
                          "cn": FID,
                          "PidTagParentFolderId": parentfolder,
                          "PidTagFolderId": FID,
                          "PidTagDisplayName": foldername,
                          "PidTagCreationTime": "0x%.16x" % self.nttime,
                          "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                          "PidTagAttributeHidden": str(0),
                          "PidTagAttributeReadOnly": str(0),
                          "PidTagAttributeSystem": str(0),
                          "PidTagContainerClass": "IPF.Note",
                          "PidTagSubFolders": "TRUE",
                          "PidTagFolderChildCount": str(0),
                          "FolderType": str(1),
                          "mapistore_uri": "sogo://%s:%s@%s/" % (username, username, foldername.replace(" ", "-").lower()),
                          "FolderType": str(1),
                          "PidTagAccess": str(63),
                          "PidTagRights":str(2043),
                          "SystemIdx": str(SystemIdx)})

        return FID

    def add_mailbox_special_folder(self, username, parentfolder, 
                                   foldername, containerclass, GlobalCount, ReplicaID, 
                                   mapistoreURL, mapistoreSuffix):
        """Add a special folder to the user mailbox

        :param username: Username object
        :param parent_folder: Folder identifier where record should be added
        :param foldername: Folder name
        :param containerclass: Folder container class
        :param GlobalCount: current global counter for message database
        :param ReplicaID: replica identifier for message database
        :param mapistoreURL: mapistore default content repository URI
        :param mapistoreSuffix: file type suffix to use with mapistore
        """

        FID = gen_mailbox_folder_fid(GlobalCount, ReplicaID)

        # Step 1. Lookup parent DN
        res = self.ldb.search("", ldb.SCOPE_SUBTREE, "(PidTagFolderId=%s)" % parentfolder, ["*"])
        if len(res) != 1:
            raise Exception("Invalid search (PidTagFolderId=%s)" % parentfolder)

        # Step 2. Add special folder to user subtree
        self.ldb.add({"dn": "CN=%s,%s" % (FID, res[0].dn),
                      "objectClass": ["specialfolder"],
                      "cn": FID,
                      "PidTagParentFolderId": parentfolder,
                      "PidTagFolderId": FID,
                      "PidTagDisplayName": foldername,
                      "PidTagCreationTime": "0x%.16x" % self.nttime,
                      "PidTagLastModificationTime": "0x%.16x" % self.nttime,
                      "PidTagContainerClass": containerclass,
                      "mapistore_uri": "sogo://%s:%s@%s/" % (username, username, foldername.replace(" ", "-").lower()),
                      "PidTagContentCount": str(0),
                      "PidTagAttributeHidden": str(0),
                      "PidTagAttributeReadOnly": str(0),
                      "PidTagAttributeSystem": str(0),
                      "PidTagAccess": str(63),
                      "PidTagRights":str(2043),
                      "PidTagContentUnreadCount": str(0),
                      "PidTagSubFolders": str(0),
                      "FolderType": str(1)})

        return FID

    def set_receive_folder(self, username, ocfirstorgdn, fid, messageclass):
        """Set MessageClass for given folder

        :param username: Username object
        :param ocfirstorgdn: Base DN
        :param fid: Folder identifier
        :param messageclass: Explicit Message Class
        """
        
        # Step 1. Search fid DN
        res = self.ldb.search("", ldb.SCOPE_SUBTREE, "(PidTagFolderId=%s)" % fid, ["*"])
        if len(res) != 1:
            raise Exception("Invalid search (PidTagFolderId=%s)" % fid)

        m = ldb.Message()
        m.dn = res[0].dn
        m["PidTagMessageClass"] = ldb.MessageElement([messageclass], ldb.CHANGETYPE_ADD, "PidTagMessageClass")
        self.ldb.modify(m)

def gen_mailbox_folder_fid(GlobalCount, ReplicaID):
    """Generates a Folder ID from index.

    :param GlobalCount: Message database global counter
    :param ReplicaID: Message database replica identifier
    """

    reverseId = ReplicaID
    for x in xrange(6):
        sh = x * 8
        unsh = (7-x) * 8
        reverseId = reverseId | (((GlobalCount >> sh) & 0xff) << unsh)

    folder = "0x%.16x" % reverseId

    return folder
