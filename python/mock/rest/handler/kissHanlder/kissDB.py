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
Helper module for kissHandler handler to provide some kind
of abstraction for data storage for Folder  Messages
"""
import os
import shelve
from cPickle import HIGHEST_PROTOCOL


class kissDB(object):
    """Wrapper arround simple database
       Main responsibilities are to:
        - open/close(flush) data
        - initial provisioning if needed
       Data format:
        - info - stores dictionary of per DB service data
        - folders - dictionary of folders
        - messages - dictionary of messages"""

    def __init__(self, user_id):
        self._user_id = user_id
        self._db = None

    def close(self):
        if self._db is not None:
            self._db.close()
        pass

    def get_folders(self):
        """@:return dict: Dictionary {folder_id -> data}"""
        return self._get_data('folders')

    def create_folder(self, parent_id, name, comment):
        """Create new folder and return a folder record
        :param parent_id:
        :param name:
        :param comment:
        :return dict: Create folder record
        """
        next_id = self._get_data('next_id')
        folder = self._folder_rec(next_id, name, comment, parent_id)
        folders = self._get_data('folders')
        folders[next_id] = folder
        self._set_data('next_id', next_id + 1)
        self._set_data('folders', folders, True)
        return folder

    def get_messages(self):
        """@:return dict: Dictionary {message_id -> data}"""
        return self._get_data('messages')

    def _get_data(self, top_key):
        if self._db is None:
            # load DB
            from flask import current_app
            filename = os.path.join(current_app.root_path, 'temp', self._user_id + '.kissdb')
            self._db = shelve.open(filename, protocol=HIGHEST_PROTOCOL, writeback=True)
            self._db.setdefault('kissdb', self._default_provisioning())

        data = self._db['kissdb']
        return data[top_key]

    def _set_data(self, top_key, value, flush=False):
        if self._db is None:
            # load DB if not loaded yet
            self._get_data('next_id')
        self._db['kissdb'][top_key] = value
        if flush:
            self._db.sync()
        pass

    @staticmethod
    def _default_provisioning():
        return {
            'next_id': 100,
            'folders': {
                1: kissDB._folder_rec(1, 'INBOX', 'Inbox folder', 0),
                2: kissDB._folder_rec(2, 'Outbox', 'Outbox folder', 0),
                3: kissDB._folder_rec(2, 'Sent', 'Sent items', 0)
            },
            'messages': {}
        }

    @staticmethod
    def _folder_rec(id, name, comment, parent_id):
        return {
            'id': id,
            'parent_id': parent_id,
            'name': name,
            'comment': comment,
            'type': 'folder'
        }
