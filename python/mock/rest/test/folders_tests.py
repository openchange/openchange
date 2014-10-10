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
This module tests "/folders/" module.
For reference see <>
"""

import unittest
import test


class FoldersInterfaceTestCase(test.MockApiBaseTestCase):

    def test_create(self):
        """Folder creation test"""
        fid = self._create_test_folder(1, 'folder-create', 'create comment')
        self.assertIsNotNone(fid)

    def test_get(self):
        # create some test item to play with
        fid = self._create_test_folder(name='folder-get', comment='folder-get comment')
        # fetch the Folder
        path = '/folders/%d/?properties=id,type,PidTagDisplayName,PidTagComment' % fid
        status, text, headers = self.get_req(path)
        self.assertEqual(status, 200)
        res = self._to_json_ret(text)
        self.assertEqual(res['id'], fid)
        self.assertEqual(res['type'], 'folder')
        self.assertEqual(res['PidTagDisplayName'], 'folder-get')
        self.assertEqual(res['PidTagComment'], 'folder-get comment')

    def test_update(self):
        # create some test item to play with
        fid = self._create_test_folder(name='folder-update', comment='folder-update comment')
        # update the Folder
        data = {
            'PidTagComment': 'Updated body'
        }
        path = '/folders/%d/' % fid
        status, text, headers = self.put_req(path, data)
        self.assertEqual(status, 201)
        self.assertEqual(text, "")

    def test_existing_folder(self):
        status, text, headers = self.head_req('/folders/1/')
        self.assertEquals(status, 200, "Unexpected status code")
        self.assertEquals(text, '', "Empty response expected")

    def test_delete(self):
        # create some test item to play with
        fid = self._create_test_folder(name='folder-delete', comment='delete comment')
        # check message exists
        path = '/folders/%d/' % fid
        status, text, headers = self.delete_req(path)
        self.assertEqual(status, 204)
        self.assertEqual(text, "")


if __name__ == '__main__':
    unittest.main()
