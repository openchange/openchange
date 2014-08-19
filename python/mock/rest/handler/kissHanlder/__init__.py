# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Kamen Mazdrashki <kmazdrashki@zentyal.com>
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
Simple 'keep it simple' type of handler implementation.
No fancy logic or caching - just store Folders and Messages
as dictionaries and traverse them with every request.
"""
from handler.kissHanlder.kissDB import kissDB

__author__ = 'kamenim'


class ApiHandler(object):
    """Simple store implementation.
        It just store/load Message/Folder objects.
        No sessions, no user etc."""

    NAME = 'OC rest api mock'
    VERSION = '1.0'

    def __init__(self, user_id):
        """Simply load what we have and handle request"""
        self._user_id = user_id
        self._db = kissDB(user_id)
        pass

    def close_context(self):
        if self._db is not None:
            self._db.close()

    def info_get(self):
        """get static description for this handler implementation"""
        return {
            'name': ApiHandler.NAME,
            'version': ApiHandler.VERSION,
            'capabilities': {}
        }

    def folders_id_exists(self, folder_id):
        fold_dict = self._db.get_folders()
        return folder_id in fold_dict

    def folders_get_folder(self, folder_id):
        fold_dict = self._db.get_folders()
        if folder_id not in fold_dict:
            raise KeyError('No folder with id = %d' % folder_id)
        return fold_dict[folder_id]

    def folders_dir(self, parent_folder_id=0):
        """List child folders for a given folder ID
        :param int parent_folder_id: Folder to enumerate
        :return list: List of folder records
        """
        fold_dict = self._db.get_folders()
        if parent_folder_id != 0 and not (parent_folder_id in fold_dict):
            raise KeyError('No folder with id = %d' % parent_folder_id)

        return [self._folder_rec(f, fold_dict)
                for f in fold_dict.values() if f['parent_id'] == parent_folder_id]

    def folders_create(self, parent_id, name, comment):
        folders = self._db.get_folders()
        # check parent ID
        if parent_id not in folders:
            raise KeyError('No folder with id = %d' % parent_id)
        # crate new folder
        new_folder = self._db.create_folder(parent_id, name, comment)
        return new_folder

    @staticmethod
    def _folder_rec(fval, fold_dict):
        """Prepare a folder record suitable for jsonify
        :param dict fval: Folder record from DB
        :param dict fold_dict: All folders in DB
        :return: Dictionary
        """
        folder_id = fval['id']
        child_count = 0
        for f in fold_dict.values():
            if f['parent_id'] == folder_id:
                child_count += 1
        return {
            'id': fval['id'],
            'name': fval['name'],
            'item_count': child_count
        }
