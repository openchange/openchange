#!/usr/bin/python

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

from openchange.mailbox import NoSuchServer, OpenChangeDB

import os
import unittest

class OpenChangeDBTests(unittest.TestCase):
    """Tests for OpenChangeDB."""

    def setUp(self):
        if os.path.exists("openchange.ldb"):
            os.unlink("openchange.ldb")
        self.db = OpenChangeDB() 
        self.db.setup()

    def test_user_exists_no_server(self):
        self.assertRaises(NoSuchServer, self.db.user_exists, "foo", "someserver")

    def test_add_mailbox_user(self):
        self.db.add_server("dc=myserver", "myserver", "firstorg", "firstou")
        self.db.add_mailbox_user("dc=myserver", "someuser")
        import pdb; pdb.set_trace()
        self.assertTrue(self.db.user_exists("myserver", "someuser"))
