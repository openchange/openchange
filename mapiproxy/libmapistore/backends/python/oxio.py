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

import datetime
import requests
import json

from openchange import mapistore

import logging


def _initialize_logger():
    # create logger with 'spam_application'
    logger = logging.getLogger('OXIO')
    logger.setLevel(logging.DEBUG)
    # create file handler which logs even debug messages
    fh = logging.FileHandler('/tmp/oxio.log')
    fh.setLevel(logging.DEBUG)
    # create console handler with a higher log level
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    # create formatter and add it to the handlers
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)
    # add the handlers to the logger
    logger.addHandler(fh)
    logger.addHandler(ch)
    return logger


# create global logger isntance
logger = _initialize_logger()


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
                    # id, folder_id, title, module, type, subfolders?, total
                   "columns": "1,20,300,301,302,304,309",
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
                   "columns": "600,601,603,604,605,607,609,610"}
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


class _Indexing(object):
    """Implements OXIO specific interface to indexing"""

    def __init__(self, username):
        self.ictx = mapistore.Indexing(username)

    def add_uri(self, uri):
        """
        Register new url with indexing service
        :param uri: Url to register in OXIO format usually
        :return: fmid for registered URL or 0 on error
        """
        # convert to mapistore URI if needed
        mstore_uri = self.uri_oxio_to_mstore(uri)
        # check if exists already
        fid = self.id_for_uri(mstore_uri)
        if fid is not None:
            return fid
        # add new entry
        fid = self.ictx.allocate_fmid()
        if fid != 0:
            self.ictx.add_fmid(fid, mstore_uri)
        return fid

    def uri_by_id(self, fid):
        mstore_uri = self.ictx.uri_for_fmid(fid)
        if mstore_uri is None:
            return None
        return self.uri_mstore_to_oxio(mstore_uri)

    def id_for_uri(self, uri):
        mstore_uri = self.uri_oxio_to_mstore(uri)
        return self.ictx.fmid_for_uri(mstore_uri)

    @staticmethod
    def uri_mstore_to_oxio(uri):
        return uri.replace(BackendObject.namespace, '').rstrip('/')

    @staticmethod
    def uri_oxio_to_mstore(uri):
        return "%s%s/" % (BackendObject.namespace, _Indexing.uri_mstore_to_oxio(uri))

    @staticmethod
    def _to_exchange_fmid(fmid):
        return (((fmid & 0x00000000000000ffL)    << 40
                | (fmid & 0x000000000000ff00L)    << 24
                | (fmid & 0x0000000000ff0000L)    << 8
                | (fmid & 0x00000000ff000000L)    >> 8
                | (fmid & 0x000000ff00000000L)    >> 24
                | (fmid & 0x0000ff0000000000L)    >> 40) | 0x0001)


class BackendObject(object):

    name = "oxio"
    description = "open-xchange backend"
    namespace = "oxio://"

    # FIXME: Very hacky - store last username we've seen here
    hack_username = None

    def __init__(self):
        logger.info('[PYTHON]: [%s] backend class __init__' % self.name)
        return

    def init(self):
        """ Initialize sample backend
        """
        logger.info('[PYTHON]: [%s] backend.init: init()' % self.name)
        return mapistore.errors.MAPISTORE_SUCCESS

    def list_contexts(self, username):
        """ List context capabilities of this backend.
        """
        logger.info('[PYTHON]: [%s] backend.list_contexts(): username = %s' % (self.name, username))
        BackendObject.hack_username = username
        inbox = {'name': 'INBOX',
                 'url': _Indexing.uri_oxio_to_mstore('default0/INBOX'),
                 'main_folder': True,
                 'role': mapistore.ROLE_MAIL,
                 'tag': 'tag',
                 }
        return [inbox,]

    def create_context(self, uri):
        """
        Create a context for given URL.
        Note: Username is deduced from Backend.list_contexts() call
        """
        logger.info('[PYTHON]: [%s] backend.create_context: uri = %s' % (self.name, uri))
        if BackendObject.hack_username is None:
            logger.error('[PYTHON]: [%s] create_context() called but we have no username!' % (self.name,))
            return (mapistore.errors.MAPISTORE_ERR_INVALID_CONTEXT, None)

        context = ContextObject(BackendObject.hack_username, uri)
        return (mapistore.errors.MAPISTORE_SUCCESS, context)


class ContextObject(object):
    """Context implementation with expected workflow:
     * Context get initialized with URI
     * next call must be get_root_folder() - this is where
       we map context.URI with and ID comming from mapistore
     * get_path() lookup directly into our local Indexing
     """

    def __init__(self, username, uri):
        self.username = username
        self.indexing = _Indexing(username)
        self.uri = self.indexing.uri_mstore_to_oxio(uri)
        self.fmid = self.indexing.id_for_uri(self.uri)
        logger.info('[PYTHON]: [%s] context class __init__(%s)' % (self._log_marker(), uri))

    def get_path(self, fmid):
        logger.info('[PYTHON]: [%s] context.get_path(%d/%x)' % (self._log_marker(), fmid, fmid))
        uri = self.indexing.uri_by_id(fmid)
        logger.info('[PYTHON]: [%s] get_path URI: %s' % (self._log_marker(), uri))
        return uri

    def get_root_folder(self, folderID):
        logger.info('[PYTHON]: [%s] context.get_root_folder(%s)' % (self._log_marker(), folderID))
        if self.fmid != folderID:
            logger.error('[PYTHON]: [%s] get_root_folder called with not Root FMID(%s)' % (self._log_marker(), folderID))
            return (mapistore.errors.MAPISTORE_ERR_INVALID_PARAMETER, None)

        folder = FolderObject(self, self.uri, folderID, None)
        return (mapistore.errors.MAPISTORE_SUCCESS, folder)

    def _log_marker(self):
        return "%s:%s" % (BackendObject.name, self.uri)


class PidTagAccessFlag(object):
    """Define possible flags for PidTagAccess property"""

    Modify              = 0x00000001
    Read                = 0x00000002
    Delete              = 0x00000004
    HierarchyTable      = 0x00000008
    ContentsTable       = 0x00000010
    AssocContentsTable  = 0x00000020


class FolderObject(object):
    """Folder object is responsible for mapping
    between MAPI folderID (64bit) and OXIO folderID (string).
    Mapping is done as:
     * self.folderID   -> 64bit MAPI forlder ID
     * self.uri        -> string OXIO ID (basically a path to the object
     * self.parentFID  ->  64bit MAPI folder ID for parent Folder
    """

    def __init__(self, context, uri, folderID, parentFID):
        logger.info('[PYTHON]: [%s] folder.__init__(uri=%s, fid=%s, parent=%s)' % (BackendObject.name, uri, folderID, parentFID))
        self.ctx = context
        self.uri = uri
        self.parentFID = long(parentFID) if parentFID is not None else None
        self.folderID = long(folderID)
        # ox folder record
        oxioConn = _OxioConn.get_instance()
        self.oxio_folder = oxioConn.getFolder(self.uri)
        self.parent_uri = self.oxio_folder['folder_id']
        self.has_subfolders = self.oxio_folder['subfolders']
        self.message_count = self.oxio_folder['total']
        #print json.dumps(self.oxio_folder, indent=4)
        # ox folder children folder records
        self.subfolders = []
        oxio_subfolders = []
        if self.has_subfolders:
            oxio_subfolders = oxioConn.getSubFolders(self.uri, [])
        # ox records for messages
        self.messages = []
        oxio_messages = []
        if self.message_count > 0:
            oxio_messages = oxioConn.getEmails(self.uri, [])
        # preload subfolders and messages
        self._index_subfolders(oxio_subfolders)
        self._index_messages(oxio_messages)
        pass

    def _index_subfolders(self, oxio_subfolders):
        #print json.dumps(oxio_subfolders, indent=4)
        for fr in oxio_subfolders:
            folder = {'uri': fr[0],
                      'parent_uri': self.uri,
                      'PidTagDisplayName': 'OXIO-' + fr[2],
                      'PidTagParentFolderId': self.folderID,
                      'PidTagFolderType': 1, # GENERIC FOLDER
                      'PidTagAccess': PidTagAccessFlag.Read |
                                      PidTagAccessFlag.HierarchyTable |
                                      PidTagAccessFlag.ContentsTable,
                      'PidTagContainerClass': 'IPF.Note',
                      'PidTagDefaultPostMessageClass': 'IPM.Note',
                      'PidTagSubfolders': fr[5],
                      'PidTagContentCount': fr[6]
                      }
            folder['PidTagFolderId'] = self.ctx.indexing.add_uri(folder['uri'])
            self.subfolders.append(folder)
        pass

    def _index_messages(self, oxio_messages):
        #print json.dumps(oxio_messages, indent=4)
        for msg in oxio_messages:
            self.messages.append(MessageObject(self, msg, None))
        pass

    def open_folder(self, folderID):
        """Open childer by its id.
        Note: it must be indexed previously
        :param folderID int: child folder ID
        """
        logger.info('[PYTHON]: [%s] folder.open_folder(%s)' % (BackendObject.name, folderID))

        child_uri = self.ctx.indexing.uri_by_id(folderID)
        if child_uri is None:
            logger.error('[PYTHON]:[ERR] child with id=%s not found' % folderID)
            return None

        return FolderObject(self.ctx, child_uri, folderID, self.folderID)

    def create_folder(self, properties, folderID):
        logger.info('[PYTHON]: [%s] folder.create_folder(%s)' % (BackendObject.name, folderID))
        logger.info('[PYTHON]: %s ' % json.dumps(properties, indent=4))
        return (mapistore.errors.MAPISTORE_ERR_NOT_IMPLEMENTED, None)

    def delete(self):
        logger.info('[PYTHON]: [%s] folder.delete(%s)' % (BackendObject.name, self.folderID))
        return mapistore.errors.MAPISTORE_ERR_NOT_IMPLEMENTED

    def get_properties(self, properties):
        logger.info('[PYTHON][%s/%s]: folder.get_properties(%s)' % (BackendObject.name, self.uri, properties))
        return {'PidTagFolderId': self.folderID,
                'PidTagDisplayName': 'OXIO-' + self.oxio_folder['title'],
                'PidTagParentFolderId': self.parentFID,
                'PidTagFolderType': 1, # GENERIC FOLDER
                'PidTagAccess': PidTagAccessFlag.Read |
                                PidTagAccessFlag.HierarchyTable |
                                PidTagAccessFlag.ContentsTable,
                'PidTagContainerClass': 'IPF.Note',
                'PidTagDefaultPostMessageClass': 'IPM.Note',
                'PidTagSubfolders': bool(self.oxio_folder['subfolders']),
                'PidTagContentCount': int(self.oxio_folder['total'])
                }

    def set_properties(self, properties):
        logger.info('[PYTHON][%s/%s]: folder.set_properties(%s)' % (BackendObject.name, self.uri, properties))
        return mapistore.errors.MAPISTORE_SUCCESS

    def open_table(self, table_type):
        logger.info('[PYTHON]: [%s] folder.open_table(table_type=%s)' % (BackendObject.name, table_type))
        table = TableObject(self, table_type)
        return (table, self.get_child_count(table_type))

    def get_child_count(self, table_type):
        logger.info('[PYTHON]: [%s] folder.get_child_count. table_type = %d' % (BackendObject.name, table_type))
        counter = { 1: self._count_folders,
                    2: self._count_messages,
                    3: self._count_zero,
                    4: self._count_zero,
                    5: self._count_zero,
                    6: self._count_zero
                }
        return counter[table_type]()

    def _count_folders(self):
        logger.info('[PYTHON][INTERNAL]: [%s] folder._count_folders(%s) = %d' % (BackendObject.name, self.folderID, len(self.subfolders)))
        return len(self.subfolders)

    def _count_messages(self):
        logger.info('[PYTHON][INTERNAL]: [%s] folder._count_messages(%s) = %s' % (BackendObject.name, self.folderID, len(self.messages)))
        return len(self.messages)

    def _count_zero(self):
        return mapistore.errors.MAPISTORE_SUCCESS

    def open_message(self, mid, rw):
        logger.info('[PYTHON]: folder.open_message(mid=%s, rw=%s)' % (mid, rw))

        for msg in self.messages:
            if mid == msg.mid:
                msg.fetch()
                return msg;
        logger.warn('No message with id %d found. messages -> %s' % (mid, self.messages))
        return None

    def create_message(self, mid, associated):
        logger.info('[PYTHON]: %s folder.create_message()' % BackendObject.name)
        return MessageObject(folder=self, mid=mid)


class MessageObject(object):

    def __init__(self, folder, oxio_msg=None, mid=None):
        logger.info('[PYTHON]:[%s] message.__init__(%s)' % (BackendObject.name, oxio_msg))
        self.folder = folder
        self.mid = mid or long(oxio_msg[0])
        self.properties = {}
        if oxio_msg is not None:
            self.init_from_msg_list(oxio_msg)
        logger.info('[PYTHON]:[%s] message.__init__(%s)' % (BackendObject.name, self.properties))

    def init_from_msg_list(self, oxio_msg):
        # msg[0] - message ID
        # msg[1] - folder URL (ie folder_id)
        # msg[2] - FROM list
        # msg[3] - TO list
        # msg[4] - CC list
        # msg[5] - SUBJECT string
        # msg[6] - sent_date timestamp => use this for modification date
        # msg[7] - received_date timestamp
        logger.info(json.dumps(oxio_msg, indent=4))
        subject = str(oxio_msg[5])
        self.properties['PidTagFolderId'] = self.folder.folderID
        self.properties['PidTagMid'] = long(oxio_msg[0])
        self.properties['PidTagInstID'] = long(oxio_msg[0])
        self.properties['PidTagSubjectPrefix'] = ''
        self.properties['PidTagSubject'] = subject
        self.properties['PidTagNormalizedSubject'] = subject
        self.properties['PidTagConversationTopic'] = subject
        self.properties['PidTagMessageClass'] = 'IPM.Note'
        self.properties['PidTagDepth'] = 0
        self.properties['PidTagRowType'] = 1
        self.properties['PidTagInstanceNum'] = 0
        self.properties['PidTagBody'] = "This is the content of this sample email"
        self.properties['PidTagHtml'] = bytearray("<html><head></head><h1>" +  self.properties['PidTagBody'] + "</h1></body></html>")
        self.properties['PidTagLastModificationTime'] = float(datetime.datetime.fromtimestamp(float(oxio_msg[6] / 1000)).strftime("%s.%f"))
        self.properties['PidTagMessageDeliveryTime'] = float(datetime.datetime.fromtimestamp(float(oxio_msg[7] / 1000)).strftime("%s.%f"))
#         self.properties["PidTagBody"] = "This is the content of this sample email"
#         self.properties["PidTagImportance"] = 2
#         self.properties["PidTagHasAttachments"] = False
#         self.properties["PidTagInternetMessageId"] = "internet-message-id@openchange.org"

        # build recipients
        def _make_recipient(oxio_rcpt, recipient_type):
            rcpt = {
                    'PidTagRecipientType': recipient_type,
                    'PidTagRecipientDisplayName': oxio_rcpt[0] or oxio_rcpt[1],
                    'PidTagSmtpAddress': oxio_rcpt[1],
                    }
            return rcpt
        self.recipients = []
        # From
        for oxio_rcpt in oxio_msg[2]:
            self.recipients.append(_make_recipient(oxio_rcpt, 0x00000000))
        # To
        for oxio_rcpt in oxio_msg[3]:
            self.recipients.append(_make_recipient(oxio_rcpt, 0x00000001))
        # CC
        for oxio_rcpt in oxio_msg[4]:
            self.recipients.append(_make_recipient(oxio_rcpt, 0x00000002))

    def fetch(self):
        pass

    def get_message_data(self):
        logger.info('[PYTHON]: message.get_message_data(mid=%d)' % self.mid)
        logger.debug('recipients: %s', json.dumps(self.recipients, indent=4))
        log_props = self.properties.copy()
        del log_props['PidTagHtml']
        logger.debug('properties: %s', json.dumps(log_props, indent=4))
        return (self.recipients, self.properties)

    def get_properties(self, properties):
        logger.info('[PYTHON]:[%s] message.get_properties(%s)' % (BackendObject.name, self.properties))
        return self.properties

    def set_properties(self, properties):
        logger.info('[PYTHON]:[%s] message.set_properties(%s)' % (BackendObject.name, properties))

        self.properties.update(properties)

        logger.info('[PYTHON]:[%s] message.set_properties(%s)' % (BackendObject.name, self.properties))
        return mapistore.errors.MAPISTORE_SUCCESS

    def save(self):
        logger.info('[PYTHON]:[%s] message.save(%s)' % (BackendObject.name, self.properties))

        return mapistore.errors.MAPISTORE_ERR_NOT_IMPLEMENTED


class TableObject(object):

    def __init__(self, folder, table_type):
        logger.info('[PYTHON]:[%s] table.__init__(%d, type=%s)' % (BackendObject.name, folder.folderID, table_type))
        self.folder = folder
        self.table_type = table_type
        self.properties = None

    def set_columns(self, properties):
        logger.info('[PYTHON]:[%s] table.set_columns(%s)' % (BackendObject.name, properties))
        self.properties = properties
        return mapistore.errors.MAPISTORE_SUCCESS

    def get_row_count(self, query_type):
        logger.info('[PYTHON]:[%s] table.get_row_count()' % (BackendObject.name))
        return self.folder.get_child_count(self.table_type)

    def get_row(self, row_no, query_type):
        logger.info('[PYTHON]:[%s] table.get_row(%s)' % (BackendObject.name, row_no))
        if self.get_row_count(self.table_type) == 0:
            return (self.properties, {})

        getter = {1: self._get_row_folders,
                  2: self._get_row_messages,
                  3: self._get_row_not_impl,
                  4: self._get_row_not_impl,
                  5: self._get_row_not_impl,
                  6: self._get_row_not_impl
                  }
        return getter[self.table_type](row_no)

    def _get_row_folders(self, row_no):
        folder = self.folder.subfolders[row_no]
        logger.debug('*** _get_row_folders')
        logger.debug(json.dumps(self.properties, indent=4))
        logger.debug(json.dumps(folder, indent=4))
        row = {}
        for name in self.properties:
            if name in folder:
                row[name] = folder[name]
#         print json.dumps(row, indent=4)
        return (self.properties, row)

    def _get_row_messages(self, row_no):
        assert row_no < len(self.folder.messages), "Index out of bounds for messages row=%s" % row_no
        message = self.folder.messages[row_no]
        (recipients, properties) = message.get_message_data()
        return (self.properties, properties)

    def _get_row_not_impl(self, row_no):
        return (self.properties, {})
