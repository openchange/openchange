#!/usr/bin/python
# OpenChange python REST backend
#
# Copyright (C) Julien Kerihuel  <j.kerihuel@openchange.org> 2014
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
           "AttachmentObject",
           "TableObject"]

import sysconfig
sys.path.append(sysconfig.get_path('platlib'))

from datetime import datetime,timedelta
from pytz import timezone

import requests
import json
import uuid
import base64

from openchange import mapistore

import MySQLdb
import optparse
import samba.getopt as options
import logging

def _initialize_logger():
    # create logger with 'spam_application'
    logger = logging.getLogger('REST')
    logger.setLevel(logging.DEBUG)
    # create file handle
    fh = logging.FileHandler('/tmp/mstore-rest.log')
    fh.setLevel(logging.DEBUG)
    # create console handler with higher log level
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    # create formatter and add it to the handlers
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)
    # add handlers to the logger
    logger.addHandler(fh)
    logger.addHandler(ch)
    return logger

# create global logger instance
logger = _initialize_logger()


class _RESTConn(object):

    instance = None

    def __init__(self):
        (self.base_url) = self._read_config()
        self.so = requests.Session()

    @staticmethod
    def _read_config():
        """Try to read config.ini file. config.ini is under source control, so
        if you want to keep your secret without disclosing to the
        world, then use config-dbg.ini instead. This file not under
        version control and prevent from committing accidentally."""
        from ConfigParser import (ConfigParser, NoOptionError, NoSectionError)
        base_dir = os.path.dirname(__file__)
        config = ConfigParser()
        base_url = None
        config.read(os.path.join(base_dir, 'config.ini'))
        if config.has_section('rest'):
            try:
                base_url = config.get('rest', 'base_url')
            except NoOptionError:
                config.read(os.path.join(base_dir, 'config-dbg.ini'))
                base_url = config.get('rest', 'base_url')
        else:
            print os.path.join(base_dir, 'config-dbg.ini')
            config.read(os.path.join(base_dir, 'config-dbg.ini'))
            if config.has_section('rest'):
                try:
                    base_url = config.get('rest', 'base_url')
                except NoOptionError:
                    raise Exception('No config.ini file or file is not valid')
            else:
                raise Exception('No [rest] section found in config-dbg.ini')

        if base_url is None:
            raise Exception('No base_url option found')

        return (base_url)

    @classmethod
    def get_instance(cls):
        """
        : return instance of _RESTConn
        """
        if cls.instance is None:
            cls.instance = cls()
        assert cls.instance is not None, "Unable to connect to %s" % cls.base_url
        return cls.instance

    def get_info(self):
        """Fetch server capabilities and root contexts
        """
        r = self.so.get('%s/info/' % self.base_url)
        return r.json()

    def get_folder(self, folder_uri):
        """Fetch folder record
        :param folder_uri: path to the folder on remote
        """
        r = self.so.get('%s%s' % (self.base_url, folder_uri))
        return r.json()

    def get_folder_count(self, uri):
        r = self.so.head('%s%sfolders' % (self.base_url, uri))
        if 'x-mapistore-rowcount' in r.headers:
            return int(r.headers['x-mapistore-rowcount'])
        return 0

    def get_folders(self, uri, properties):
        r = self.so.get('%s%sfolders' % (self.base_url, uri))
        self._dump_response(r)
        folders = r.json()
        newlist = []
        for folder in folders:
            self._decode_object_b64(folder)
            folder['PidTagInstID'] = folder['id']
            folder['PidTagInstanceNum'] = 0
            folder['PidTagRowType'] = 1
            folder['PidTagDepth'] = 0
            newlist.append(folder)
        return newlist

    def get_message(self, msg_uri):
        """Fetch message record
        :param msg_uri: path to the message on remote
        """
        r = self.so.get('%s%s' % (self.base_url, msg_uri))
        msg = r.json()
        self._decode_object_b64(msg)
        if "recipients" in msg and len(msg["recipients"]) > 0:
            # We assume all the recipients have the same properties
            rcp_keys = msg["recipients"][0].keys()
            for key in rcp_keys:
                if mapistore.isPtypBinary(key):
                    for recipient in msg["recipients"]:
                        value = recipient[key]
                        recipient[key] = bytearray(base64.b64decode(str(value)))
        return msg

    def get_message_count(self, uri):
        r = self.so.head('%s%smessages' % (self.base_url, uri))
        if 'x-mapistore-rowcount' in r.headers:
            return int(r.headers['x-mapistore-rowcount'])
        return 0

    def get_messages(self, uri, properties):
        """Get all messages from one folder."""
        r = self.so.get('%s%smessages' % (self.base_url, uri))
        self._dump_response(r)
        msgs = r.json()
        newlist = []
        for msg in msgs:
            self._decode_object_b64(msg)
            msg['PidTagInstID'] = msg['id']
            msg['PidTagInstanceNum'] = 0
            msg['PidTagRowType'] = 1
            msg['PidTagDepth'] = 0
            if "recipients" in msg and len(msg["recipients"]) > 0:
                # We assume all the recipients have the same properties
                rcp_keys = msg["recipients"][0].keys()
                for key in rcp_keys:
                    if mapistore.isPtypBinary(key):
                        for recipient in msg["recipients"]:
                            value = recipient[key]
                            recipient[key] = bytearray(base64.b64decode(str(value)))
            newlist.append(msg)
        return newlist

    def get_attachment(self, att_uri):
        """Fetch attachment record
        :param att_uri: path to the attachment on remote
        """
        r = self.so.get('%s%s' % (self.base_url, att_uri))
        att = r.json()
        self._decode_object_b64(att)
        return att

    def get_attachment_count(self, uri):
        r = self.so.head('%s%sattachments' % (self.base_url, uri))
        if 'x-mapistore-rowcount' in r.headers:
            return int(r.headers['x-mapistore-rowcount'])
        return 0

    def get_attachments(self, uri, properties):
        """Get all attachments from one message.
        :param uri: path to the message on remote
        :param properties: list with the requested properties
        """
        req_uri = '%s%sattachments' % (self.base_url, uri)
        req_props = ','.join(properties)
        r = self.so.get(req_uri, params={'properties': req_props})
        atts = r.json()
        newlist = []
        for att in atts:
            self._decode_object_b64(att)
            newlist.append(att)
        return newlist

    def create_attachment(self, parent_id, props):
        att = props.copy()
        att['parent_id'] = parent_id
        # base64 PtypBinary properties
        self._encode_object_b64(att)
        headers = {'Content-Type': 'application/json'}
        r = self.so.post('%s/attachments/' % self.base_url, data=json.dumps(att), headers=headers)
        return r.json()['id']

    def delete_attachment(self, uri):
        """Delete an attachment given its ID
        :param uri: path to the attachment on remote
        """
        self.so.delete('%s%s' % (self.base_url, uri))
        return mapistore.errors.MAPISTORE_SUCCESS

    def create_message(self, collection, parent_id, props, recipients):
        msg = props.copy()
        msg["parent_id"] = parent_id
        # base64 PtypBinary properties
        self._encode_object_b64(msg)
        msg["PidTagSubject"] = msg["PidTagNormalizedSubject"]
        # Add recipients
        rcps = [rcp.copy() for rcp in recipients]
        if len(rcps) > 0:
            # We assume all the recipients have the same properties
            rcp_keys = rcps[0].keys()
            for key in rcp_keys:
                if mapistore.isPtypBinary(key):
                    for rcp in rcps:
                        value = rcp[key]
                        rcp[key] = base64.b64encode(value)
            msg["recipients"] = rcps
        headers = {'Content-Type': 'application/json'}
        r = self.so.post('%s/%s/' % (self.base_url, collection),
                         data=json.dumps(msg), headers=headers)
        return r.json()['id']

    def create_folder(self, parent_id, folder):
        folder['parent_id'] = parent_id
        headers = {'Content-Type': 'application/json'}
        r = self.so.post('%s/folders/' % self.base_url, data=json.dumps(folder), headers=headers)
        return r.json()

    def update_object(self, uri, props):
        obj = props.copy()
        headers = {'Content-Type': 'application/json'}
        self._encode_object_b64(obj)
        self.so.put('%s%s' % (self.base_url, uri), data=json.dumps(obj), headers=headers)
        return mapistore.errors.MAPISTORE_SUCCESS

    def update_message(self, uri, props, recipients):
        msg = props.copy()
        headers = {'Content-Type': 'application/json'}
        self._encode_object_b64(msg)
        rcps = [rcp.copy() for rcp in recipients]
        # Add recipients
        if len(rcps) > 0:
            # We assume all the recipients have the same properties
            rcp_keys = rcps[0].keys()
            for key in rcp_keys:
                if mapistore.isPtypBinary(key):
                    for rcp in rcps:
                        value = rcp[key]
                        rcp[key] = base64.b64encode(value)
            msg["recipients"] = rcps
        self.so.put('%s%s' % (self.base_url, uri), data=json.dumps(msg),
                    headers=headers)
        return mapistore.errors.MAPISTORE_SUCCESS

    def submit_message(self, props, recipients, serv_spool):
        msg = props.copy()
        rcps = [rcp.copy() for rcp in recipients]
        # base64 PtypBinary properties
        self._encode_object_b64(msg)
        msg["PidTagSubject"] = msg["PidTagNormalizedSubject"]
        # Add recipients
        rcps = [rcp.copy() for rcp in recipients]
        if len(rcps) > 0:
            # We assume all the recipients have the same properties
            rcp_keys = rcps[0].keys()
            for key in rcp_keys:
                if mapistore.isPtypBinary(key):
                    for rcp in rcps:
                        value = rcp[key]
                        rcp[key] = base64.b64encode(value)
        else:
            return mapistore.errors.MAPISTORE_ERR_INVALID_DATA
        msg['recipients'] = rcps
        headers = {'Content-Type': 'application/json'}
        submit_data = {'msg': msg, 'serv_spool': serv_spool}
        self.so.post('%s/mails/submit/' % self.base_url,
                     data=json.dumps(submit_data), headers=headers)
        return mapistore.errors.MAPISTORE_SUCCESS

    def _dump_request(self, payload):
        print json.dumps(payload, indent=4)

    def _dump_response(self, r):
        print json.dumps(r.json(), indent=4)

    def _encode_object_b64(self, obj):
        """ Encode the values of a dictionary object in base 64
        : param obj: Dictionary object
        """
        for key, value in obj.iteritems():
            if mapistore.isPtypBinary(key) or mapistore.isPtypServerId(key):
                obj[key] = base64.b64encode(value)

    def _decode_object_b64(self, obj):
        """ Decode the values of a dictionary object from base 64
        : param obj: Dictionary object
        """
        for key, value in obj.iteritems():
            if mapistore.isPtypBinary(key):
                obj[key] = bytearray(base64.b64decode(str(value)))


class _Indexing(object):
    """Implements REST specific interface to indexing"""

    def __init__(self, username):
        self.ictx = mapistore.Indexing(username)

    def add_uri_with_fmid(self, uri, fmid):
        mstore_uri = self.uri_rest_to_mstore(uri)
        # FIXME: check if uri already exists
        self.ictx.add_fmid(fmid, mstore_uri)
        return mapistore.errors.MAPISTORE_SUCCESS

    def add_uri(self, uri):
        """
        Register new url with indexing service
        :param uri: Url to register in REST format usually
        :return: fmid for registered URL or 0 on error
        """
        # convert to mapistore URI if needed
        mstore_uri = self.uri_rest_to_mstore(uri)
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
        return self.uri_mstore_to_rest(mstore_uri)

    def id_for_uri(self, uri):
        mstore_uri = uri
        return self.ictx.fmid_for_uri(mstore_uri)

    @staticmethod
    def uri_mstore_to_rest(uri):
        return uri.replace(BackendObject.namespace, '')

    @staticmethod
    def uri_rest_to_mstore(uri):
        return "%s%s" % (BackendObject.namespace, _Indexing.uri_mstore_to_rest(uri))

    @staticmethod
    def _to_exchange_fmid(fmid):
        return (((fmid & 0x00000000000000ffL)    << 40
                | (fmid & 0x000000000000ff00L)    << 24
                | (fmid & 0x0000000000ff0000L)    << 8
                | (fmid & 0x00000000ff000000L)    >> 8
                | (fmid & 0x000000ff00000000L)    >> 24
                | (fmid & 0x0000ff0000000000L)    >> 40) | 0x0001)


class BackendObject(object):

    name = "rest"
    description = "REST backend"
    namespace = "rest://"


    # Same hack than oxio backend for now


    def __init__(self):
        logger.info('[PYTHON]: [%s] backend class __init__' % self.name)

    def init(self):
        """ Initialize REST backend
        """
        logger.info('[PYTHON]: [%s] backend.init: init()' % self.name)
        return mapistore.errors.MAPISTORE_SUCCESS

    def list_contexts(self, username):
        """ List context capabilities of this backend
        """
        logger.info('[PYTHON]: [%s] backend.list_contexts(): username = %s' % (self.name, username))
        conn = _RESTConn.get_instance()
        info = conn.get_info()
        self.username = username
        print "list_contexts: %s" % username
        if not 'contexts' in info: raise KeyError('contexts not available')
        for i in range(len(info['contexts'])):
            if not 'name' in info['contexts'][i]: raise KeyError('name not available')
            if not 'url' in info['contexts'][i]: raise KeyError('url not available')
            if not 'role' in info['contexts'][i]: raise KeyError('role not available')
            if not 'main_folder' in info['contexts'][i]: raise KeyError('main_folder not available')

            info['contexts'][i]['url'] = _Indexing.uri_rest_to_mstore(info['contexts'][i]['url']).encode('utf8')
            info['contexts'][i]['name'] = info['contexts'][i]['name'].encode('utf8')
            info['contexts'][i]['tag'] = 'tag'

        return info['contexts']

    def create_root_folder(self, username, name, fid):
        """ Create a root folder
        """
        logger.info('[PYTHON]: [%s] backend.create_root_folder()' % (self.name))
        conn = _RESTConn.get_instance()
        props = {}
        props['PidTagDisplayName'] = name
        rid = conn.create_folder(1, props)
        base_url = '/folders/%s/' % rid['id']
        uri = '%s%s' % (BackendObject.namespace, base_url)

        indexing = _Indexing(username)
        indexing.add_uri_with_fmid(uri, fid)
        return (mapistore.errors.MAPISTORE_SUCCESS, uri)

    def create_context(self, uri, username):
        """ Create a REST context.
        """
        logger.info('[PYTHON]: [%s] backend.create_context(): uri = %s' % (self.name, uri))

        context = ContextObject(username, uri)
        return (mapistore.errors.MAPISTORE_SUCCESS, context)


class ContextObject(object):

    def __init__(self, username, uri):
        self.username = username
        self.indexing = _Indexing(username)
        self.uri = self.indexing.uri_mstore_to_rest(uri)
        self.rest_uri = self.indexing.uri_rest_to_mstore(uri)
        self.fmid = self.indexing.id_for_uri(self.rest_uri)
        logger.info('[PYTHON]: [%s] context class __init__(%s)' % (self._log_marker(), uri))

    def get_path(self, fmid):
        logger.info('[PYTHON]: [%s] context.get_path(%d/%x)' % (self._log_marker(), fmid, fmid))
        uri = self.indexing.uri_by_id(fmid)
        logger.info('[PYTHON]: [%s] get_path URI: %s' % (self._log_marker(), uri))
        return uri

    def get_root_folder(self, folderID):
        logger.info('[PYTHON]: [%s] context.get_root_folder(%s)' % (self._log_marker(), folderID))
#        if self.fmid != folderID:
#            logger.error('[PYTHON]: [%s] get_root_folder called with not Root FMID(%s)' % (self._log_marker(), folderID))
#            return (mapistore.errors.MAPISTORE_ERR_INVALID_PARAMETER, None)
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

    def __init__(self, context, uri, folderID, parentFID):
        logger.info('[PYTHON]: [%s] folder.__init__(uri=%s, fid=%s, parent=%s' % (BackendObject.name, uri, folderID, parentFID))
        self.ctx = context
        self.uri = uri
        self.parentFID = long(parentFID) if parentFID is not None else None
        self.folderID = long(folderID)
        # List of messages
        self.messages = []
        self.subfolders = []

        conn = _RESTConn.get_instance()
        self.properties = conn.get_folder(self.uri)
        self.count = {}
        self.count["folders"] = conn.get_folder_count(self.uri)
        self.count["messages"] = conn.get_message_count(self.uri)

    def open_folder(self, folderID):
        logger.info('[PYTHON]: [%s] folder.open(fid=%s)' % (BackendObject.name, folderID))
        uri = self.ctx.indexing.uri_by_id(folderID)
        return FolderObject(self.ctx, uri, folderID,
                mapistore.errors.MAPISTORE_SUCCESS)


    def create_folder(self, props, fid):
        logger.info('[PYTHON]: [%s] folder.create' % (BackendObject.name))
        conn = _RESTConn.get_instance()
        rid = conn.create_folder(self.properties['id'], props)
        base_url = '/folders/%s/' % rid['id']
        uri = '%s%s' % (BackendObject.namespace, base_url)
        self.ctx.indexing.add_uri_with_fmid(uri, fid)
        self.count["folders"] = self.count["folders"] + 1
        return (mapistore.errors.MAPISTORE_SUCCESS, FolderObject(self.ctx, base_url, fid, self.folderID))

    def _index_messages(self, messages):
        self.messages = []
        conn = _RESTConn.get_instance()
        for msg in messages:
            if 'id' in msg and 'collection' in msg:
                uri = '%s/%s/%s/' % (BackendObject.namespace, msg['collection'], msg['id'])
                msg[u'PidTagMid'] = self.ctx.indexing.add_uri(uri)
                msg[u'PidTagFolderId'] = self.folderID
                self.messages.append(MessageObject(self, msg, None))

    def _index_folders(self, folders):
        self.folders = []
        conn = _RESTConn.get_instance()
        for folder in folders:
            if 'id' in folder:
                uri = '/folders/%s/' % (folder['id'])
                fid = self.ctx.indexing.add_uri(uri)
                self.subfolders.append(FolderObject(self.ctx, uri, fid, self.folderID))


    def get_properties(self, properties):
        logger.info('[PYTHON]: [%s][%s]: folder.get_properties(%s)' %(BackendObject.name, self.uri, properties))

        conn = _RESTConn.get_instance()

        role_properties = {}
        role_properties[mapistore.ROLE_MAIL] = { 'PidTagContainerClass': 'IPF.Note',
                                                 'PidTagMessageClass': 'IPM.Note',
                                                 'PidTagDefaultPostMessageClass': 'IPM.Note' }
        role_properties[mapistore.ROLE_OUTBOX] = role_properties[mapistore.ROLE_MAIL]
        role_properties[mapistore.ROLE_SENTITEMS] = role_properties[mapistore.ROLE_MAIL]
        role_properties[mapistore.ROLE_DELETEDITEMS] = role_properties[mapistore.ROLE_MAIL]
        role_properties[mapistore.ROLE_DRAFTS] = role_properties[mapistore.ROLE_MAIL]
        role_properties[mapistore.ROLE_CALENDAR] = { 'PidTagContainerClass': 'IPF.Appointment',
                                                     'PidTagMessageClass': 'IPM.Appointment',
                                                     'PidTagDefaultPostMessageClass': 'IPM.Appointment' }
        role_properties[mapistore.ROLE_CONTACTS] = { 'PidTagContainerClass': 'IPF.Contact',
                                                     'PidTagMessageClass': 'IPM.Contact',
                                                     'PidTagDefaultPostMessageClass': 'IPM.Contact'}
        role_properties[mapistore.ROLE_TASKS] = { 'PidTagContainerClass': 'IPF.Task',
                                                  'PidTagMessageClass': 'IPM.Task',
                                                  'PidTagDefaultPostMessageClass': 'IPM.Task'}
        role_properties[mapistore.ROLE_NOTES] = { 'PidTagContainerClass': 'IPF.StickyNote',
                                                  'PidTagMessageClass': 'IPM.StickyNote',
                                                  'PidTagDefaultPostMessageClass': 'IPM.StickyNote'}
        role_properties[mapistore.ROLE_JOURNAL] = { 'PidTagContainerClass': 'IPF.Journal',
                                                    'PidTagMessageClass': 'IPM.Journal',
                                                    'PidTagDefaultPostMessageClass': 'IPM.Journal'}
        role_properties[mapistore.ROLE_FALLBACK] = role_properties[mapistore.ROLE_MAIL]
        properties = self.properties.copy()
        properties['PidTagFolderId'] = self.folderID
        properties['PidTagDisplayName'] = str(self.properties['PidTagDisplayName'])
        properties['PidTagComment'] = str(self.properties['PidTagComment'])
        properties['PidTagFolderType'] = mapistore.FOLDER_GENERIC
        properties['PidTagAccess'] = (PidTagAccessFlag.Modify |
                                      PidTagAccessFlag.Read |
                                      PidTagAccessFlag.HierarchyTable |
                                      PidTagAccessFlag.ContentsTable |
                                      PidTagAccessFlag.AssocContentsTable)
        properties['PidTagCreationTime'] = float((datetime.now(tz=timezone('Europe/Madrid')) - timedelta(hours=1)).strftime("%s.%f"))
        properties['PidTagLastModificationTime'] = properties['PidTagCreationTime']
        # FIXME: Generate PidTagChangeKey based on PidTagChangeNumber
        properties["PidTagChangeKey"] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        properties['PidTagAccessLevel'] = 1
        properties['PidTagRights'] = 2043
        if not 'PidTagContainerClass' in properties and 'role' in self.properties:
            properties['PidTagContainerClass'] = str(role_properties[self.properties['role']]['PidTagContainerClass'],)
        properties['PidTagSubfolders'] = True if self.count["folders"] else False
        properties['PidTagContentCount'] = self.count["messages"]
        properties['PidTagContentUnreadCount'] = 0
        properties['PidTagAttributeHidden'] = False

        if self.parentFID is not None:
            properties['PidTagParentFolderId'] = self.parentFID

        return properties

    def set_properties(self, properties):
        logger.info('[PYTHON]: [%s][%s]: folder.set_properties' % (BackendObject.name, self.uri))
        conn = _RESTConn.get_instance()
        conn.update_object(self.uri, properties)
        self.properties.update(properties)
        return mapistore.errors.MAPISTORE_SUCCESS

    def open_table(self, table_type):
        logger.info('[PYTHON]: [%s] folder.open_table(table_type=%s)' % (BackendObject.name, table_type))
        factory = {mapistore.FOLDER_TABLE: self._open_table_folders,
                   mapistore.MESSAGE_TABLE: self._open_table_messages,
                   mapistore.FAI_TABLE: self._open_table_any,
                   mapistore.RULE_TABLE: self._open_table_any,
                   mapistore.ATTACHMENT_TABLE: self._open_table_any,
                   mapistore.PERMISSIONS_TABLE: self._open_table_any
                   }
        return factory[table_type](table_type)

    def _open_table_any(self, table_type):
        table = TableObject(self, table_type)
        return (table, self.get_child_count(table_type))

    def _open_table_folders(self, table_type):
        conn = _RESTConn.get_instance()
        folders = conn.get_folders(self.uri, [])
        self._index_folders(folders)
        return self._open_table_any(table_type)

    def _open_table_messages(self, table_type):
        conn = _RESTConn.get_instance()
        messages = conn.get_messages(self.uri, [])
        self._index_messages(messages)
        return self._open_table_any(table_type)

    def get_child_count(self, table_type):
        logger.info('[PYTHON]: [%s] folder.fet_child_count with table_type = %d' % (BackendObject.name, table_type))
        counter = {mapistore.FOLDER_TABLE: self._count_folders,
                   mapistore.MESSAGE_TABLE: self._count_messages,
                   mapistore.FAI_TABLE: self._count_zero,
                   mapistore.RULE_TABLE: self._count_zero,
                   mapistore.ATTACHMENT_TABLE: self._count_zero,
                   mapistore.PERMISSIONS_TABLE: self._count_zero}

        return counter[table_type]()

    def _count_folders(self):
        conn = _RESTConn.get_instance()
        logger.info('[PYTHON][INTERNAL]: [%s] folder._count_folders(%s)' % (BackendObject.name, self.folderID))
        return conn.get_folder_count(self.uri)

    def _count_messages(self):
        conn = _RESTConn.get_instance()
        logger.info('[PYTHON][INTERNAL]: [%s] folder._count_messages(%s)' % (BackendObject.name, self.folderID))
        return conn.get_message_count(self.uri)

    def _count_zero(self):
        return 0

    def open_message(self, mid, rw):
        logger.info('[PYTHON]: folder.open_message(mid=%s, rw=%s)' % (mid, rw))
        # Retrieve the message object from remote
        conn = _RESTConn.get_instance()
        msg_uri = self.ctx.indexing.uri_by_id(mid)
        msg_dict = conn.get_message(msg_uri)
        msg_rcps = msg_dict.pop("recipients", [])
        # Get the attachment ids
        att_list = conn.get_attachments(msg_uri, ['id'])
        att_ids = [att['id'] for att in att_list]
        return MessageObject(self, msg_dict, mid, msg_rcps, msg_uri, att_ids)

    def create_message(self, mid, associated):
        logger.info('[PYTHON]: folder.create_message(mid=%s' % mid)
        msg = {}
        msg['PidTagChangeKey'] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        return MessageObject(self, msg, mid)


class MessageObject(object):
    def __init__(self, folder, msg=None, mid=None, rcps=[], uri=None,
                 att_ids=[]):
        logger.info('[PYTHON]:[%s] message.__init__' % BackendObject.name)
        self.folder = folder
        self.mid = mid or long(msg['id'])
        self.uri = uri
        self.properties = msg
        self.recipients = rcps
        self.cached_attachments = []
        self.attachment_ids = att_ids
        self.next_aid = 0  # Provisional AID
        logger.info('[PYTHON]:[%s] message.__init__(%s)' % (BackendObject.name,
                    self.properties))

    def get_properties(self, properties):
        logger.info('[PYTHON]:[%s][%s] message.get_properties()' % (BackendObject.name, self.uri))
        return self.properties

    def set_properties(self, properties):
        logger.info('[PYTHON]:[%s][%s] message.set_properties()' % (BackendObject.name, self.uri))
        self.properties.update(properties)
        return mapistore.errors.MAPISTORE_SUCCESS

    def get_message_data(self):
        logger.info('[PYTHON][%s][%s]: message.get_message_data' % (BackendObject.name, self.uri))
        return (self.recipients, self.properties)

    def modify_recipients(self, recipients):
        logger.info('[PYTHON][%s][%s]: message.modify_recipients()' % (BackendObject.name, self.uri))
        self.recipients = recipients
        return mapistore.errors.MAPISTORE_SUCCESS

    def _collection_from_messageClass(self, messageclass):
        collections = {"IPM.Contact": "contacts",
                       "IPM.Appointment": "calendars"}
        if not messageclass in collections:
            return "mails"
        return collections[messageclass]

    def update(self, properties):
        logger.info('[PYTHON]:[%s] message.update()' % BackendObject.name)
        if self.uri is None:
            return mapistore.errors.MAPISTORE_ERR_NOT_FOUND
        self.properties.update(properties)
        conn = _RESTConn.get_instance()
        conn.update_object(self.uri, properties)
        return mapistore.errors.MAPISTORE_SUCCESS

    def save(self):
        logger.info('[PYTHON][%s][%s]: message.save' % (BackendObject.name, self.uri))
        conn = _RESTConn.get_instance()
        collection = self._collection_from_messageClass(self.properties['PidTagMessageClass'])
        # Check if the message already exists
        if self.uri is not None:
            # Update the message
            conn.update_message(self.uri, self.properties, self.recipients)
            msgid = self.uri
            msgid = int(msgid.replace('/%s/' % collection, '').rstrip('/'))
        else:
            # Create the message
            folder_id = self.folder.uri
            folder_id = int(folder_id.replace('/folders/', '').rstrip('/'))
            msgid = conn.create_message(collection, folder_id, self.properties,
                                        self.recipients)
            self.uri = '/%s/%d/' % (collection, msgid)
            # Index the record
            uri = '%s%s' % (BackendObject.namespace, self.uri)
            self.folder.ctx.indexing.add_uri_with_fmid(uri, self.mid)
            self.folder.messages.append(self)
            self.folder.count["messages"] = self.folder.count["messages"] + 1
        # If it has cached attachments, post them as well
        for att in self.cached_attachments:
            # Check if the attachment already exists
            if att.att_id in self.attachment_ids:
                att_uri = '/attachments/%d/' % att.att_id
                conn.update_object(att_uri, att.properties)
            else:
                # Create the attachment
                parent_id = msgid
                att.att_id = conn.create_attachment(parent_id, att.properties)
                self.attachment_ids.append(att.att_id)
        self.cached_attachments = []
        return mapistore.errors.MAPISTORE_SUCCESS

    def submit(self, serv_spool):
        logger.info('[PYTHON][%s][%s]: message.submit' % (BackendObject.name,
                    self.uri))
        # Only E-mails can be submitted
        collection = self._collection_from_messageClass(self.properties['PidTagMessageClass'])
        if collection != "mails":
            return mapistore.errors.MAPISTORE_ERR_INVALID_DATA
        # FIXME: REST API shouldn't care about these properties
        curr_time = float((datetime.now(tz=timezone('Europe/Madrid'))
                           - timedelta(hours=1)).strftime("%s.%f"))
        self.properties['PidTagClientSubmitTime'] = curr_time
        # Submit the message to the server
        conn = _RESTConn.get_instance()
        return conn.submit_message(self.properties, self.recipients,
                                   serv_spool)

    def create_attachment(self):
        logger.info('[PYTHON]: message.create_attachment()')
        attach_id = self.next_aid  # Provisional AID
        self.next_aid = attach_id + 1
        att = {}
        att['PidTagChangeKey'] = bytearray(uuid.uuid1().bytes + '\x00\x00\x00\x00\x00\x01')
        return (AttachmentObject(self, att), attach_id)

    def open_attachment(self, att_id):
        logger.info('[PYTHON][%s][%s]: message.open_attachment(aid=%d)' % (BackendObject.name, self.uri, att_id))
        if att_id not in self.attachment_ids:
            return mapistore.errors.MAPISTORE_ERR_NOT_FOUND
        conn = _RESTConn.get_instance()
        att_dict = conn.get_attachment('/attachments/%d/' % att_id)
        return AttachmentObject(self, att_dict, att_id)

    def delete_attachment(self, att_id):
        logger.info('[PYTHON][%s][%s]: message.delete_attachment(aid=%d)' % (BackendObject.name, self.uri, att_id))
        if att_id not in self.attachment_ids:
            return mapistore.errors.MAPISTORE_ERR_NOT_FOUND
        conn = _RESTConn.get_instance()
        conn.delete_attachment('/attachments/%d/' % att_id)
        self.attachment_ids.remove(att_id)
        return mapistore.errors.MAPISTORE_SUCCESS

    def get_child_count(self, table_type):
        logger.info('[PYTHON][%s][%s]: message.get_child_count' % (BackendObject.name, self.uri))
        if table_type != mapistore.ATTACHMENT_TABLE:
            return 0
        return len(self.attachment_ids)

    def get_attachment_ids(self):
        logger.info('[PYTHON][%s][%s]: message.get_attachment_ids' % (BackendObject.name, self.uri))
        return self.attachment_ids

    def get_attachment_table(self):
        table_type = mapistore.ATTACHMENT_TABLE
        table = TableObject(self, table_type)
        return (table, self.get_child_count(table_type))

class AttachmentObject(object):
    def __init__(self, message, att=None, aid=None):
        logger.info('[PYTHON]:[%s] attachment.__init__(%s)' % (BackendObject.name, att))
        self.message = message
        self.properties = att
        self.att_id = aid

    def save(self):
        if self.properties not in self.message.cached_attachments:
            self.message.cached_attachments.append(self)
        return mapistore.errors.MAPISTORE_SUCCESS

    def get_properties(self, properties):
        logger.info('[PYTHON][%s]: attachment.get_properties()' % BackendObject.name)
        return self.properties

    def set_properties(self, properties):
        logger.info('[PYTHON]:[%s] message.set_properties()' % BackendObject.name)
        self.properties.update(properties)
        return mapistore.errors.MAPISTORE_SUCCESS

class TableObject(object):
    def __init__(self, parent, table_type):
        logger.info('[PYTHON]:[%s] table.__init__(type=%s)' % (BackendObject.name, table_type))
        self.parent = parent
        self.table_type = table_type
        self.restrictions = None
        self.columns = None

    def set_columns(self, columns):
        logger.info('[PYTHON]:[%s] table.set_columns(%s)' % (BackendObject.name, columns))
        self.columns = columns
        return mapistore.errors.MAPISTORE_SUCCESS

    def set_restrictions(self, restrictions):
        logger.info('[PYTHON]:[%s] table.set_restrictions(%s)', BackendObject.name, restrictions)
        if restrictions is None:
            self.restrictions = {}
        else:
            self.restrictions = self._encode_restrictions(restrictions)

        print json.dumps(self.restrictions, indent=4)
        return mapistore.errors.MAPISTORE_SUCCESS

    def get_row_count(self, query_type):
        logger.info('[PYTHON]:[%s] table.get_row_count()' % (BackendObject.name))
        if self.table_type == mapistore.MESSAGE_TABLE:
            count = 0
            for message in self.parent.messages:
                if self._apply_restriction_message(self.restrictions, message) is True:
                    count = count + 1
            return count

        return self.parent.get_child_count(self.table_type)

    def get_row(self, row_no, query_type):
        logger.info('[PYTHON]:[%s] table.get_row(%s)' % (BackendObject.name, row_no))
        if self.get_row_count(self.table_type) == 0:
            return (self.columns, {})

        getter = {mapistore.FOLDER_TABLE: self._get_row_folders,
                  mapistore.MESSAGE_TABLE: self._get_row_messages,
                  mapistore.FAI_TABLE: self._get_row_not_impl,
                  mapistore.RULE_TABLE: self._get_row_not_impl,
                  mapistore.ATTACHMENT_TABLE: self._get_row_attachments,
                  mapistore.PERMISSIONS_TABLE: self._get_row_not_impl
                  }
        return getter[self.table_type](row_no)

    def _get_row_folders(self, row_no):
        logger.debug('*** _get_row_folders: row_no = %s', row_no)
        print self.parent.subfolders
        folder = self.parent.subfolders[row_no]
        row = {}
        for name in self.columns:
            if name == 'PidTagFolderId':
                row[name] = folder.folderID
            elif name == 'PidTagParentFolderId' and folder.parentFID is not None:
                row[name] = folder.parentFID
            elif name in folder.properties:
                row[name] = folder.properties[name]
        return (self.columns, row)

    def _encode_restrictions(self, restrictions):
        if restrictions is None:
            return None

        if 'type' not in restrictions:
            return None

        rst = restrictions.copy()

        if rst["type"] == "and":
            for i,condition in enumerate(rst["value"]):
                rst["value"][i] = self._encode_restrictions(condition)

        if rst["type"] == "or":
            for i,condition in enumerate(rst["value"]):
                rst["value"][i] = self._encode_restrictions(condition)

        if rst["type"] == "content":
            if mapistore.isPtypBinary(rst["property"]) or mapistore.isPtypServerId(rst["property"]):
                rst["value"] = base64.b64encode(str(rst["value"]))

        if rst["type"] == "property":
            if mapistore.isPtypBinary(rst["property"]) or mapistore.isPtypServerId(rst["property"]):
                rst["value"] = base64.b64encode(str(rst["value"]))

        return rst

    def _apply_restriction_message(self, restriction, message):
        if restriction is None:
            return True

        if (restriction["type"] == "and"):
            for condition in restriction["value"]:
                if self._apply_restriction_message(condition, message) is False:
                    return False
            return True

        if (restriction["type"] == "or"):
            conditions = []
            for condition in restriction["value"]:
                conditions.append(self._apply_restriction_message(condition, message))
            if True in conditions:
                return True
            return False

        if (restriction["type"] == "content"):
            if not restriction["property"] in message.properties:
                return False

            val1 = message.properties[restriction["property"]]
            val2 = restriction["value"]

            if "IGNORECASE" in restriction["fuzzyLevel"]:
                val1 = val1.lower()
                val2 = val2.lower()
            if "FULLSTRING" in restriction["fuzzyLevel"]:
                return val1 == val2
            elif "SUBSTRING" in restriction["fuzzyLevel"]:
                print '%s in %s' % (val2, val1)
                print val1.find(val2)
                print val1.find(val2) != -1
                return val1.find(val2) != -1
            elif "PREFIX" in restriction["fuzzyLevel"]:
                return val1.startswith(val2)
            return False

        if (restriction["type"] == "property"):
            print message
            if not restriction["property"] in message.properties:
                return False
            if restriction["operator"] == 4:
                if message.properties[restriction["property"]] == restriction["value"]:
                    return True
                return False

        return False

    def _get_row_messages(self, row_no):
        assert row_no < len(self.parent.messages), "Index out of bounds for messages row=%s" % row_no

        # Filter messages
        tmp_msg = []
        for message in self.parent.messages:
            if self._apply_restriction_message(self.restrictions, message) is True:
                tmp_msg.append(message)

        assert row_no < len(tmp_msg), "Index out of bounds of filtered data for message row=%s" % row_no
        message = tmp_msg[row_no]

        (recipients, properties) = message.get_message_data()
        return (self.columns, properties)

    def _get_row_attachments(self, row_no):
        logger.info('[PYTHON]:[%s] table.get_attachment_row(%d)' % (BackendObject.name, row_no))
        assert row_no < len(self.parent.attachment_ids), "Index out of bounds row=%s" % row_no
        # Fetch the entire attachment record
        conn = _RESTConn.get_instance()
        att_uri = '/attachments/%d/' % self.parent.attachment_ids[row_no]
        att = conn.get_attachment(att_uri)
        att['PidTagAttachNumber'] = att['id']
        att['PidTagMid'] = self.parent.mid
        # Filter the requested properties
        att_row = {}
        for name in self.columns:
            if name in att:
                att_row[name] = att[name]
        return (self.columns, att_row)

    def _get_row_not_impl(self, row_no):
        return (self.columns, {})


##################################################################################################################################
####### Provision Code ###########################################################################################################
##################################################################################################################################

class OpenchangeDBWithMySQL():
    def __init__(self, uri):
        self.uri = uri
        self.db = self._connect_to_mysql(uri)

    def _connect_to_mysql(self, uri):
        if not uri.startswith('mysql://'):
            raise ValueError("Bad connection string for mysql: invalid schema")

        if '@' not in uri:
            raise ValueError("Bad connection string for mysql: expected format "
                             "mysql://user[:passwd]@host/db_name")
        user_passwd, host_db = uri.split('@')

        user_passwd = user_passwd[len('mysql://'):]
        if ':' in user_passwd:
            user, passwd = user_passwd.split(':')
        else:
            user, passwd = user_passwd, ''

        if '/' not in host_db:
            raise ValueError("Bad connection string for mysql: expected format "
                             "mysql://user[:passwd]@host/db_name")
        host, db = host_db.split('/')

        return MySQLdb.connect(host=host, user=user, passwd=passwd, db=db)

    def select(self, sql, params=()):
        try:
            cur = self.db.cursor()
            cur.execute(sql, params)
            rows = cur.fetchall()
            cur.close()
            self.op_error = False
            return rows
        except MySQLdb.OperationalError as e:
            # FIXME: It may be leaded by another error
            # Reconnect and rerun this
            if self.op_error:
                raise e
            self.db = self._connect_to_mysql(self.uri)
            self.op_error = True
            return self.select(sql, params)
        except MySQLdb.ProgrammingError as e:
            print "Error executing %s with %r: %r" % (sql, params, e)
            raise e

    def insert(self, sql, params=()):
        self.delete(sql, params)

    def delete(self, sql, params=()):
        cur = self.db.cursor()
        cur.execute(sql, params)
        self.db.commit()
        cur.close()
        return


if __name__ == '__main__':
    parser = optparse.OptionParser("rest.py [options]")

    sambaopts = options.SambaOptions(parser)
    parser.add_option_group(sambaopts)

    parser.add_option("--status", action="store_true", help="Status of rest backend for specified user")
    parser.add_option("--provision", action="store_true", help="Provision rest backend for specified user")
    parser.add_option("--deprovision", action="store_true", help="Deprovision rest backend for specified user")
    parser.add_option("--username", type="string", help="User mailbox to update")

    opts,args = parser.parse_args()
    if len(args) != 0:
        parser.print_help()
        sys.exit(1)

    lp = sambaopts.get_loadparm()

    username = opts.username
    if username is None:
        print "[ERROR] No username specified"
        sys.exit(1)
    if username.isalnum() is False:
        print "[ERROR] Username must be alpha numeric"
        sys.exit(1)

    openchangedb_uri = lp.get("mapiproxy:openchangedb")
    if openchangedb_uri is None:
        print "This script only supports MySQL backend for openchangedb"

    if opts.provision and opts.deprovision:
        print "[ERROR] Incompatible options: provision and deprovision"
        sys.exit(1)


    c = OpenchangeDBWithMySQL(openchangedb_uri)

    # namespace like
    nslike = BackendObject.namespace + '%'

    # Retrieve mailbox_id, ou_id
    rows = c.select("SELECT id,ou_id FROM mailboxes WHERE name=%s", username)
    if len(rows) == 0:
        print "[ERROR] No such user %s" % username
        sys.exit(1)
    mailbox_id = rows[0][0]
    ou_id = rows[0][1]

    rows = c.select("SELECT * FROM folders WHERE MAPIStoreURI LIKE %s AND mailbox_id=%s AND ou_id=%s",
                    (nslike, mailbox_id, ou_id))

    if opts.status:
        if (rows):
            print "[INFO] REST backend is provisioned for user %s the following folders:" % username
            for row in rows:
                name = c.select("SELECT value FROM folders_properties WHERE name=\"PidTagDisplayName\" AND folder_id=%s", row[0])
                if name:
                    print "\t* %-40s (%s)" % (name[0][0], row[len(row) - 1])
                else:
                    print "[ERR] Backend provision incomplete!"
                    sys.exit(1)
        else:
            print "[INFO] REST backend is not provisioned for user %s" % username
        sys.exit(0)

    if opts.provision:
        if (rows):
            print "[WARN] Sample backend is already provisioned for user %s" % username
            sys.exit(0)

        rows = c.select("SELECT id,folder_id FROM folders WHERE SystemIdx=12 AND mailbox_id=%s AND ou_id=%s", (mailbox_id, ou_id))
        if len(rows) == 0:
            print "[ERROR] No such SystemIdx for mailbox_id=%s" % mailbox_id
        system_id = rows[0][0]
        parent_id = rows[0][1]

        ictx = _Indexing(username)
        backend = BackendObject()

        # Provision root folders (/folders/0/)
        conn = _RESTConn.get_instance()
        folders = conn.get_folders('/folders/0/', [])
        for folder in folders:
            url = '/folders/%d/' % int(folder['id'])
            furl = '%s%s' % (BackendObject.namespace, url)
            folder_id = ictx.add_uri(url)
            c.insert("INSERT INTO folders (ou_id,folder_id,folder_class,mailbox_id,"
                     "parent_folder_id,FolderType,SystemIdx,MAPIStoreURI) VALUES "
                     "(%s, %s, \"system\", %s, NULL, 1, %s, %s)",
                     (ou_id, folder_id, mailbox_id, folder['system_idx'], furl))

            rows = c.select("SELECT MAX(id) FROM folders")
            fid = rows[0][0]

            (ret,ctx) = backend.create_context(url, username)
            (ret,fld) = ctx.get_root_folder(folder_id)

            props = fld.get_properties(None)

            for prop in props:
                c.insert("INSERT INTO folders_properties (folder_id, name, value) VALUES (%s, %s, %s)", (fid, prop, props[prop]))
                c.insert("INSERT INTO folders_properties (folder_id, name, value) VALUES (%s, \"PidTagParentFolderId\", %s)", (fid, parent_id))

        # Provision folders under Top of Information Store (/folders/1/)
        contexts = backend.list_contexts(username)
        for context in contexts:
            folder_id = ictx.add_uri(context["url"])
            if context['role'] == mapistore.ROLE_FALLBACK:
                continue

            print "%d" % long(folder_id)
            c.insert("INSERT INTO folders (ou_id,folder_id,folder_class,mailbox_id,"
                     "parent_folder_id,FolderType,SystemIdx,MAPIStoreURI) VALUES "
                     "(%s, %s, \"system\", %s, %s, 1, %s, %s)",
                     (ou_id, folder_id, mailbox_id, system_id, context["system_idx"], context["url"]))

            rows = c.select("SELECT MAX(id) FROM folders")
            fid = rows[0][0]

            (ret,ctx) = backend.create_context(context["url"], username)
            (ret,fld) = ctx.get_root_folder(folder_id)

            props = fld.get_properties(None)

            for prop in props:
                c.insert("INSERT INTO folders_properties (folder_id, name, value) VALUES (%s, %s, %s)", (fid, prop, props[prop]))
            c.insert("INSERT INTO folders_properties (folder_id, name, value) VALUES (%s, \"PidTagParentFolderId\", %s)", (fid, parent_id))
        print "[DONE]"

    if opts.deprovision:
        if not rows:
            print "[WARN] Sample backend is not provisioned for user %s" % username
            sys.exit(0)

        for row in rows:
            c.delete("DELETE FROM folders WHERE id=%s", row[0])
            c.delete("DELETE FROM folders_properties WHERE folder_id=%s", row[0])
        c.delete("DELETE FROM mapistore_indexing WHERE url LIKE %s", nslike)
        print "[DONE]"
