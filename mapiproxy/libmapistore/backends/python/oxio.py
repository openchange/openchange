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
        print '[PYTHON]: [%s] folder.__init__(uri=%s, fid=%s)' % (BackendObject.name, uri, folderID)
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
        print '[PYTHON]: [%s] folder.open_folder(%s)' % (BackendObject.name, folderID)

        child_uri = _Index.uri_by_id(folderID)
        if child_uri is None:
            print '[PYTHON]:[ERR] child with id=%s not found' % (folderID)
            return None

        return FolderObject(self.ctx, child_uri, folderID, self.folderID)

    def create_folder(self, properties, folderID):
        print '[PYTHON]: [%s] folder.create_folder(%s)' % (BackendObject.name, folderID)
        json = json.dumps(properties, indent=4)
        print '[PYTHON]: %s ' % json
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
        print '[PYTHON][INTERNAL]: [%s] folder._count_folders(%s)' % (BackendObject.name, self.folderID)
        return 0

    def _count_messages(self):
        print '[PYTHON][INTERNAL]: [%s] folder._count_messages(%s)' % (BackendObject.name, self.folderID)
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
