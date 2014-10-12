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
This module tests "/calendars/" module.
For reference see <>
"""

import unittest
import test

class CalendarInterfaceTestCase(test.MockApiBaseTestCase):
    """Basic tests for Calendars module interface"""

    def _create_test_item(self, parent_id=1, subject='Calendar 1', body='Cal text body'):
        data = {
            'parent_id': 1,
            'PidTagSubject': subject,
            'PidTagBody': body
        }
        return self.post_req('/calendars/', data)

    def test_create(self):
        status, text, headers = self._create_test_item()
        self.assertEqual(status, 200)
        self.assertIn('id', self._to_json_ret(text))

    def test_get(self):
        # create some test item to play with
        status, text, headers = self._create_test_item(subject='tget', body='tget body')
        item = self._to_json_ret(text)
        # fetch the message
        path = '/calendars/%d/?properties=id,type,PidTagSubject,PidTagBody' % item['id']
        status, text, headers = self.get_req(path)
        self.assertEqual(status, 200)
        res = self._to_json_ret(text)
        self.assertEqual(res['id'], item['id'])
        self.assertEqual(res['type'], 'calendar')
        self.assertEqual(res['PidTagSubject'], 'tget')
        self.assertEqual(res['PidTagBody'], 'tget body')

    def test_update(self):
        # create some test item to play with
        status, text, headers = self._create_test_item()
        item = self._to_json_ret(text)
        # update the event
        data = {
            'PidTagBody': 'Updated body'
        }
        path = '/calendars/%d/' % item['id']
        status, text, headers = self.put_req(path, data)
        self.assertEqual(status, 201)
        self.assertEqual(text, "")

    def test_head(self):
        # create some test item to play with
        status, text, headers = self._create_test_item()
        item = self._to_json_ret(text)
        # check message exists
        path = '/calendars/%d/' % item['id']
        status, text, headers = self.head_req(path)
        self.assertEqual(status, 200)

    def test_delete(self):
        # create some test item to play with
        status, text, headers = self._create_test_item()
        item = self._to_json_ret(text)
        # check message exists
        path = '/calendars/%d/' % item['id']
        status, text, headers = self.delete_req(path)
        self.assertEqual(status, 204)
        self.assertEqual(text, "")


if __name__ == '__main__':
    unittest.main()
