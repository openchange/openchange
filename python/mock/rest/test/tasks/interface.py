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
import test

class TasksInterfaceTestCase(test.MockApiBaseTestCase):
    """Basic tests for Tasks module interface"""

    def _create_test_att(self, name='Attachment', parent_id=1):
        data = {
            'parent_id': parent_id,
            'PidTagDisplayName': name
        }
        return self.post_req('/attachments/', data)

    def _create_test_msg(self, parent_id=1, subject='Message ', body='Message body'):
        data = {
            'parent_id': parent_id,
            'PidTagSubject': subject,
            'PidTagBody': body
        }
        return self.post_req('/tasks/', data)

    def test_create(self):
        status, text, headers = self._create_test_msg()
        self.assertEqual(status, 200)
        self.assertIn('id', self._to_json_ret(text))

    def test_get(self):
        # create some test item to play with
        status, text, headers = self._create_test_msg(subject='tasks tget', body='tasks tget body')
        item = self._to_json_ret(text)
        # fetch the message
        path = '/tasks/%d/?properties=id,type,PidTagSubject,PidTagBody' % item['id']
        status, text, headers = self.get_req(path)
        self.assertEqual(status, 200)
        res = self._to_json_ret(text)
        self.assertEqual(res['id'], item['id'])
        self.assertEqual(res['collection'], 'tasks')
        self.assertEqual(res['PidTagSubject'], 'tasks tget')
        self.assertEqual(res['PidTagBody'], 'tasks tget body')

    def test_update(self):
        # create some test item to play with
        status, text, headers = self._create_test_msg()
        item = self._to_json_ret(text)
        # update the event
        data = {
            'PidTagBody': 'Updated tasks body'
        }
        path = '/tasks/%d/' % item['id']
        status, text, headers = self.put_req(path, data)
        self.assertEqual(status, 201)
        self.assertEqual(text, "")

    def test_head(self):
        # create some test item to play with
        status, text, headers = self._create_test_msg()
        item = self._to_json_ret(text)
        # check message exists
        path = '/tasks/%d/' % item['id']
        status, text, headers = self.head_req(path)
        self.assertEqual(status, 200)

    def test_delete(self):
        # create some test item to play with
        status, text, headers = self._create_test_msg()
        item = self._to_json_ret(text)
        # check message exists
        path = '/tasks/%d/' % item['id']
        status, text, headers = self.delete_req(path)
        self.assertEqual(status, 204)
        self.assertEqual(text, "")

    def test_attachment_get(self):
        # create some test items to play with
        status, text, headers = self._create_test_msg()
        msg_item = self._to_json_ret(text)
        status, text, headers = self._create_test_att(name='my attachment',
                parent_id=msg_item['id'])
        att_item = self._to_json_ret(text)
        # fetch the message
        path = '/tasks/%d/attachments?properties=PidTagDisplayName,id' % msg_item['id']
        status, text, headers = self.get_req(path)
        self.assertEqual(status, 200)
        res = self._to_json_ret(text)
        self.assertEqual(len(res), 1)
        att_props = res[0]
        self.assertEqual(len(att_props), 2)
        self.assertEqual(att_props['PidTagDisplayName'], 'my attachment')
        self.assertEqual(att_props['id'], att_item['id'])

if __name__ == '__main__':
    unittest.main()
