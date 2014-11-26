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
Basic tests on service interface
"""

import unittest
from test import MockApiBaseTestCase

class AttachmentsInterfaceTestCase(MockApiBaseTestCase):
    """Basic tests for Attachments module interface"""

    def _create_test_att(self, name='Attachment', parent_id=51):
        data = {
            'PidTagDisplayName': name,
            'parent_id': parent_id
        }
        return self.post_req('/attachments/', data)

    def test_create(self):
        status, text, headers = self._create_test_att()
        self.assertEqual(status, 200)
        self.assertIn('id', self._to_json_ret(text))

    def test_get(self):
        # create some test item to play with
        status, text, headers = self._create_test_att(name='attachment')
        item = self._to_json_ret(text)
        # fetch the message
        path = '/attachments/%d/' % item['id']
        status, text, headers = self.get_req(path)
        self.assertEqual(status, 200)
        res = self._to_json_ret(text)
        self.assertEqual(res['id'], item['id'])
        self.assertEqual(res['PidTagDisplayName'], 'attachment')

    def test_update(self):
        # create some test item to play with
        status, text, headers = self._create_test_att()
        item = self._to_json_ret(text)
        # update the event
        data = {
            'PidTagDisplayName': 'updated attachment'
        }
        path = '/attachments/%d/' % item['id']
        status, text, headers = self.put_req(path, data)
        self.assertEqual(status, 201)
        self.assertEqual(text, "")
        # check changes took effect
        status, text, headers = self.get_req(path)
        self.assertEqual(status, 200)
        res = self._to_json_ret(text)
        self.assertEqual(res['PidTagDisplayName'], 'updated attachment')

if __name__ == '__main__':
    unittest.main()
