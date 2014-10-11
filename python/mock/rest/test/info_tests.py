# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Kamen Mazdrashki <kamenim@openchange.org>
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
"""
Unit testing for Openchange REST API server
This module tests "/info/" service.
For reference see <http://openchange.org/documentation/api/mapistore-http/index.html>
"""

import unittest
import test

class InfoInterfaceTestCase(test.MockApiBaseTestCase):
    """Basic tests for INFO service"""

    def test_get_info(self):
        status, text, headers = self.get_req('/info/')
        self.assertEqual(200, status)
        info = self._to_json_ret(text)
        self.assertIn('name', info)
        self.assertIn('version', info)
        self.assertIn('capabilities', info)
        # check contexts returned
        contexts = info.get('contexts', None)
        self.assertIsNotNone(contexts)
        self.assertIsInstance(contexts, list)
        # we should have at least 'inbox', 'sent', 'calendar'
        self.assertTrue(len(contexts) > 1)
        for ctx in contexts:
            self.assertIn('url', ctx)
            self.assertIn('roles', ctx)
            self.assertIn('name', ctx)
            self.assertIn('main_folder', ctx)
        # todo: check for 'notes', 'tasks', 'contacts', 'notes', 'attachments'


if __name__ == '__main__':
    unittest.main()
