#!/usr/bin/python
# OpenChange python sample backend
#
# Copyright (C) Kamen Mazdrashki <kamenim@openchange.org> 2014
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

import os,sys

__all__ = ["BackendObject",
           "ContextObject",
           "FolderObject",
           "TableObject"]

import sysconfig
sys.path.append(sysconfig.get_path('platlib'))

root = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.sep.join((root, '..', '..')))

import requests
import json
from openchange import mapistore


# few mapistore ERRORS until we expose them
MAPISTORE_SUCCESS = 0
MAPISTORE_ERR_NOT_IMPLEMENTED = 24


class _OxioConn(object):

    instance = None

    def __init__(self):
        self.so = requests.Session()
        (username, password) = self._read_config()
        payload = {
            'action': 'login',
            'client': 'open-xchange-appsuite',
            'language': 'en_US',
            'name': username,
            'password': password,
            'timeout': '10000',
            'version': '7.6.0-7'
        }
        r = self.so.post('https://www.ox.io/appsuite/api/login', params=payload)
        self.sess_info = r.json()
        self.sess_id = self.sess_info['session']

    @staticmethod
    def _read_config():
        """Try to read config.ini file. config.ini is under source
        control, so if you want to keep your secret w/o disclosing
        to the world, then config-dbg.ini is the plac to write it.
        config-dbg.ini is just a separate file, so it will be
        more difficult to be committed by accidence"""
        from ConfigParser import (ConfigParser, NoOptionError)
        base_dir = os.path.dirname(__file__)
        config = ConfigParser()
        config.read(os.path.join(base_dir, 'config.ini'))
        try:
            username = config.get('oxio', 'username')
            password = config.get('oxio', 'password')
        except NoOptionError:
            try:
                config.read(os.path.join(base_dir, 'config-dbg.ini'))
                username = config.get('oxio', 'username')
                password = config.get('oxio', 'password')
            except NoOptionError:
                raise Exception('No config.ini file or file is not valid')
        return (username, password)

    @classmethod
    def get_instance(cls):
        """
        :return _OxioConn:
        """
        if cls.instance is None:
            cls.instance = cls()
        assert cls.instance is not None, "Unable to connect to oxio"
        return cls.instance

    def getFolder(self, folder_id):
        """Fetch folder record from OX
        :param folder_id: oxid for the folder to fetch
        """
        payload = {'action': 'get',
                   'session': self.sess_id,
                   'id': folder_id}
        self._dump_request(payload)
        r = self.so.get('https://www.ox.io/appsuite/api/folders', params=payload)
        self._dump_response(r)
        return r.json()['data']

    def getSubFolders(self, folder_id, columns):
        """Fetch subfolders for folder_id
        :param folder_id: oxid for parent folder
        :param columns list: list of columns for fetch
        """
        payload = {"action": "list",
                   "all": "1",
                   "session": self.sess_id,
                   "columns": "1,20,300,301,302,304",
                   "parent": folder_id}
        self._dump_request(payload)
        r = self.so.get('https://www.ox.io/appsuite/api/folders', params=payload)
        self._dump_response(r)
        return r.json()['data']

    def getEmails(self, folder_id, columns):
        """Fetch subfolders for folder_id
        :param folder_id: oxid for parent folder
        :param columns list: list of columns for fetch
        """
        payload = {"action": "all",
                   "session": self.sess_id,
                   "folder": folder_id,
                   "columns": "600,601,603,604,605,607,610"}
        self._dump_request(payload)
        r = self.so.get('https://www.ox.io/appsuite/api/mail', params=payload)
        self._dump_response(r)
        return r.json()['data']

    def _dump_request(self, payload):
#         print json.dumps(payload, indent=4)
        pass

    def _dump_response(self, r):
#         print json.dumps(r.json(), indent=4)
        pass


class _Index(object):

    mapping = {}

    @classmethod
    def add_entry(cls, fid, uri):
        cls.mapping[fid] = uri

    @classmethod
    def add_uri(cls, uri):
        fid = cls.next_id()
        cls.mapping[fid] = uri
        return fid

    @classmethod
    def next_id(cls):
        next_id = max(cls.mapping.keys())
        return next_id + 1

    @classmethod
    def uri_by_id(cls, fid):
        return cls.mapping.get(fid)


class BackendObject(object):

    name = "oxio"
    description = "open-xchange backend"
    namespace = "oxio://"

    def __init__(self):
        print '[PYTHON]: [%s] backend class __init__' % self.name
        return

    def init(self):
        """ Initialize sample backend
        """
        print '[PYTHON]: [%s] backend.init: init()' % self.name
        return 0

    def list_contexts(self, username):
        """ List context capabilities of this backend.
        """
        print '[PYTHON]: [%s] backend.list_contexts(): username = %s' % (self.name, username)
        contexts = [{ "inbox", "default0/INBOX", "calendar", "CALENDAR" }]
        return (0, contexts)

    def create_context(self, uri):
        """ Create a context.
        """

        print '[PYTHON]: [%s] backend.create_context: uri = %s' % (self.name, uri)
        context = ContextObject(uri)

        return (0, context)


class ContextObject(object):
    """Context implementation with expected workflow:
     * Context get initialized with URI
     * next call must be get_root_folder() - this is where
       we map context.URI with and ID comming from mapistore
     * get_path() lookup directly into our local Indexing
     """

    def __init__(self, uri):
        self.uri = uri
        print '[PYTHON]: [%s] context class __init__' % self._log_marker()

    def get_path(self, fmid):
        print '[PYTHON]: [%s] context.get_path' % self._log_marker()
        uri = _Index.uri_by_id(fmid)
        print '[PYTHON]: [%s] get_path URI: %s' % (self._log_marker(), uri)
        return uri

    def get_root_folder(self, folderID):
        print '[PYTHON]: [%s] context.get_root_folder' % self._log_marker()
        # index our root uri, this is the first
        _Index.add_entry(folderID, self.uri)
        folder = FolderObject(self, self.uri, folderID, None)
        return (0, folder)

    def _log_marker(self):
        return "%s:%s" % (BackendObject.name, self.uri)


class FolderObject(object):
    """Folder object is responsible for mapping
    between MAPI folderID (64bit) and OXIO folderID (string).
    Mapping is done as:
     * self.folderID -> 64bit MAPI forlder ID
     * self.uri      -> string OXIO ID (basically a path to the object
    """

    def __init__(self, context, uri, folderID, parentFID):
        print '[PYTHON]: [%s] folder.__init__(uri=%s, fid=%s)' % (BackendObject.name, uri, hex(folderID))
        self.ctx = context
        self.uri = uri
        self.parentFID = parentFID;
        self.folderID = folderID;
        pass

    def open_folder(self, folderID):
        """Open childer by its id.
        Note: it must be indexed previously
        :param folderID int: child folder ID
        """
        print '[PYTHON]: [%s] folder.open_folder(%s)' % (BackendObject.name, hex(folderID))

        child_uri = _Index.uri_by_id(folderID)
        if child_uri is None:
            print '[PYTHON]:[ERR] child with id=%s not found' % hex(folderID)
            return None

        return FolderObject(self.ctx, child_uri, folderID, self.folderID)

    def create_folder(self, properties, folderID):
        print '[PYTHON]: [%s] folder.create_folder(%s)' % (BackendObject.name, folderID)
        print '[PYTHON]: %s ' % json.dumps(properties, indent=4)
        return (MAPISTORE_ERR_NOT_IMPLEMENTED, None)

    def delete(self):
        print '[PYTHON]: [%s] folder.delete(%s)' % (BackendObject.name, self.folderID)
        return MAPISTORE_ERR_NOT_IMPLEMENTED

    def open_table(self, table_type):
        print '[PYTHON]: [%s] folder.open_table' % (BackendObject.name)
        table = TableObject(self, table_type)
        return (table, self.get_child_count(table_type))

    def get_child_count(self, table_type):
        print '[PYTHON]: [%s] folder.get_child_count' % (BackendObject.name)
        counter = { 1: self._count_folders,
                    2: self._count_messages,
                    3: self._count_zero,
                    4: self._count_zero,
                    5: self._count_zero,
                    6: self._count_zero
                }
        return counter[table_type]()

    def _count_folders(self):
        print '[PYTHON][INTERNAL]: [%s] folder._count_folders(%s)' % (BackendObject.name, hex(self.folderID))
        return 0

    def _count_messages(self):
        print '[PYTHON][INTERNAL]: [%s] folder._count_messages(%s)' % (BackendObject.name, hex(self.folderID))
        return 0

    def _count_zero(self):
        return 0


class TableObject(object):

    def __init__(self, folder, tableType):
        print '[PYTHON]: [%s] table.__init__()' % (BackendObject.name)
        self.folder = folder
        self.tableType = tableType
        self.properties = None

    def set_columns(self, properties):
        print '[PYTHON]: [%s] table.set_columns()' % (BackendObject.name)
        self.properties = properties
        print 'properties: [%s]\n' % ', '.join(map(str, properties))
        return 0

    def get_row(self, rowId, query_type):
        print '[PYTHON]: %s table.get_row()' % (BackendObject.name)

        # TODO: lazy load childs - messages or folders
        # TODO: create index

        return (self.properties, {})
