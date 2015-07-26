#!/usr/bin/env python2.7

# OpenChange provisioning
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

from openchange.mailbox import NoSuchServer, OpenChangeDB, gen_mailbox_folder_fid
from openchange.provision import ProvisionNames
import os
import unittest


class OpenChangeDBTests(unittest.TestCase):
    """Tests for OpenChangeDB."""

    def setUp(self):
        if os.path.exists("openchange.ldb"):
            os.unlink("openchange.ldb")
        self.db = OpenChangeDB("openchange.ldb")
        self.db.setup()
        self.names = ProvisionNames()
        self.names.firstorg = 'firstorg'
        self.names.firstou = 'firstou'
        self.names.ocserverdn = 'dc=myserver'
        self.names.netbiosname = 'myserver'

    def test_user_exists_no_server(self):
        self.assertRaises(NoSuchServer, self.db.lookup_mailbox_user,
                          "someserver", "foo")

    def test_server_lookup_doesnt_exist(self):
        self.assertRaises(NoSuchServer, self.db.lookup_server,
                          "nonexistantserver")

    def test_server_lookup(self):
        self.db.add_server(self.names)
        self.assertEquals(self.names.ocserverdn,
                          str(self.db.lookup_server(self.names.netbiosname)['dn']))

    def test_msg_globalcount_initial(self):
        self.db.add_server(self.names)
        self.assertEquals(1, self.db.get_message_GlobalCount(self.names.netbiosname))

    def test_set_msg_globalcount(self):
        self.db.add_server(self.names)
        self.db.set_message_GlobalCount(self.names.netbiosname, 42)
        self.assertEquals(42, self.db.get_message_GlobalCount(self.names.netbiosname))

    def test_msg_replicaid_initial(self):
        self.db.add_server(self.names)
        self.assertEquals(1, self.db.get_message_ReplicaID(self.names.netbiosname))


class MailboxFIDTests(unittest.TestCase):

    def test_simple(self):
        self.assertEquals(0x7816000000012345, int(gen_mailbox_folder_fid(0x1678, 0x12345)))
