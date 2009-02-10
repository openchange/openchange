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
from samba import Ldb
from samba.auth import system_session
from samba.provision import setup_add_ldif
import ldb
import uuid

__docformat__ = 'restructuredText'

class OpenChangeDB(Ldb):
    """The OpenChange database.
    """

    def __init__(self):
        Ldb.__init__(self, url="openchange.ldb")

    def lookup_mailbox_user(self, server, username):
        """Check if a user already exists in openchange database.

        :param server: Server object name
        :param username: Username object
        :return: LDB Object of the user
        """

        # Step 1. Search Server object
        filter = "(&(objectClass=server)(cn=%s))" % (server)
        res = self.search("", scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=[])
        assert(len(res) == 1)
        server_dn = res[0].dn

        # Step 2. Search User object
        filter = "(&(objectClass=user)(cn=%s))" % (username)
        return self.search(server_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=[])

    def user_exists(self, username, server):
        """Check whether a user exists.

        :param username: Username of the user
        :param server: Server object name
        """
        return len(self.lookup_mailbox_user(username, server)) == 1

    def get_message_attribute(self, server, attribute):
        """Retrieve attribute value from given message database (server).

        :param server: Server object name
        """
        # Step 1. Search Attribute from 'server' object
        filter = "(&(objectClass=server)(cn=%s))" % (server)
        res = self.search("", scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=[attribute])
        assert(len(res) == 1)

        # Step 2. Convert result to hexadecimal
        attribute = int(res[0][attribute][0], 16)

        return attribute

    def get_message_ReplicaID(self, server):
        """Retrieve current mailbox Replica ID for given message database (server).

        :param server: Server object name
        """

        return self.get_message_attribute(server=server, 
                                          attribute="ReplicaID")

    def get_message_GlobalCount(self, server):
        """Retrieve current mailbox Global Count for given message database (server).

        :param server: Server object name
        """
        return self.get_message_attribute(server=server,
                                            attribute="GlobalCount")


    def set_message_GlobalCount(self, setup_path, server, GlobalCount):
        """Update current mailbox GlobalCount for given message database (server).

        :param setup_path: Function that returns the path to a setup.
        :param server: Server object name
        :param index: Mailbox new GlobalCount value
        """

        # Step 2. Search Server object
        filter = "(&(objectClass=server)(cn=%s))" % (server)
        res = self.search("", scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=[])
        assert(len(res) == 1)

        # Step 3. Update Server object
        server_dn = res[0].dn

        newGlobalCount = """
dn: %s
changetype: modify
replace: GlobalCount
GlobalCount: 0x%x
""" % (server_dn, GlobalCount)

        self.transaction_start()
        try:
            self.modify_ldif(newGlobalCount)
        finally:
            self.transaction_commit()

    def add_mailbox_user(self, setup_path, names, username):
        """Add a user record in openchange database.

        :param setup_path: Function that returns the path to a setup.
        :param names: Provision names context
        :param server: Server object name
        :param username: Username object
        """

        # Step 1. Add user object
        mailboxGUID = str(uuid.uuid4())
        replicaID = str(1)
        replicaGUID = str(uuid.uuid4())

        print "[+] Adding '%s' record" % (username)
        setup_add_ldif(self, setup_path("openchangedb/oc_provision_openchange_mailbox_user.ldif"), {
                "USERNAME": username,
                "FIRSTORGDN": names.ocfirstorgdn,
                "MAILBOXGUID": mailboxGUID,
                "REPLICAID": replicaID,
                "REPLICAGUID": replicaGUID
                })

    def add_mailbox_root_folder(self, setup_path, names, username, 
                                foldername, GlobalCount, ReplicaID,
                                SystemIdx):
        """Add a root folder to the user mailbox

        :param setup_path: Function that returns the path to a setup.
        :param names: Provision names context
        :param username: Username object
        :param foldername: Folder name
        :param GlobalCount: current global counter for message database
        :param ReplicaID: replica identifier for message database
        :param SystemIdx: System Index for root folders
        """

        names.ocuserdn = "CN=%s,%s" % (username, names.ocfirstorgdn)

        # Step 1. Add root folder to user subtree
        FID = gen_mailbox_folder_fid(GlobalCount, ReplicaID)
        print "[+] Adding SystemRoot folder '%s' (%s) to %s" % (FID, foldername, username)
        setup_add_ldif(self, setup_path("openchangedb/oc_provision_openchange_mailbox_folder.ldif"), {
                "USERDN": names.ocuserdn,
                "FOLDER_IDX": FID,
                "NAME": foldername,
                "SYSTEMIDX": "%s" % (SystemIdx)
                })


def gen_mailbox_folder_fid(GlobalCount, ReplicaID):
    """Generates a Folder ID from index.

    :param GlobalCount: Message database global counter
    :param ReplicaID: Message database replica identifier
    """

    folder = "0x%.12x%.4x" % (GlobalCount, ReplicaID)
#    folder = "0x%s" % tmp[::-1]

    return folder



