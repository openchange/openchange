#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChange provisioning
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2009
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2009
# Copyright (C) Enrique J. Hernández <ejhernandez@zentyal.com> 2015
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

import os
import os.path
import samba
from samba import Ldb
import ldb
from urlparse import urlparse
import uuid
import time
import MySQLdb


from openchange.migration import MigrationManager


__docformat__ = 'restructuredText'


class NoSuchServer(Exception):
    """Raised when a server could not be found."""


def _public_folders_meta(names):
    return ({"IPM_SUBTREE": ({}, 2),
             "NON_IPM_SUBTREE": ({
                 "EFORMS REGISTRY": ({}, 4),
                 "Events Root": ({}, -1),
                 "OFFLINE ADDRESS BOOK": ({
                     "/o=%s/cn=addrlists/cn=oabs/cn=Default Offline Address Book" % (names.firstorg): ({}, 9),
                     }, 6),
                 "SCHEDULE+ FREE BUSY": ({
                     "EX:/o=%s/ou=%s" % (names.firstorg, names.firstou): ({}, 8),
                     }, 5),
                 }, 3),
             }, 1)


class OpenChangeDBWithLdbBackend(object):
    """The OpenChange database."""

    def __init__(self, url):
        self.url = url
        self.ldb = Ldb(self.url)
        self.nttime = samba.unix2nttime(int(time.time()))

    def reopen(self):
        self.ldb = Ldb(self.url)

    def remove(self):
        """Remove an existing OpenChangeDB file."""
        if os.path.exists(self.url):
            os.remove(self.url)
        self.reopen()

    def setup(self, names=None):
        self.ldb.add_ldif("""
dn: @OPTIONS
checkBaseOnSearch: TRUE

dn: @INDEXLIST
@IDXATTR: cn

dn: @ATTRIBUTES
cn: CASE_INSENSITIVE
dn: CASE_INSENSITIVE

""")
        self.reopen()
        if names:
            self.add_rootDSE(names.ocserverdn, names.firstorg, names.firstou)

    def add_rootDSE(self, ocserverdn, firstorg, firstou):
        self.ldb.add({"dn": "@ROOTDSE",
                      "defaultNamingContext": "CN=%s,CN=%s,%s" % (firstou, firstorg, ocserverdn),
                      "rootDomainNamingContext": ocserverdn,
                      "vendorName": "OpenChange Team (http://www.openchange.org)"})

    def add_server(self, names):
        self.ldb.add({"dn": names.ocserverdn,
                      "objectClass": ["top", "server"],
                      "cn": names.netbiosname,
                      "GlobalCount": "1",
                      "ChangeNumber": "1",
                      "ReplicaID": "1"})
        self.ldb.add({"dn": "CN=%s,%s" % (names.firstorg, names.ocserverdn),
                      "objectClass": ["top", "org"],
                      "cn": names.firstorg})
        self.ldb.add({"dn": "CN=%s,CN=%s,%s" % (names.firstou, names.firstorg, names.ocserverdn),
                      "objectClass": ["top", "ou"],
                      "cn": names.firstou})

    def add_root_public_folder(self, dn, fid, change_num, SystemIdx, childcount):
        self.ldb.add({"dn": dn,
                      "objectClass": ["publicfolder"],
                      "cn": fid,
                      "PidTagFolderId": fid,
                      "PidTagChangeNumber": change_num,
                      "PidTagDisplayName": "Public Folder Root",
                      "PidTagCreationTime": "%d" % self.nttime,
                      "PidTagLastModificationTime": "%d" % self.nttime,
                      "PidTagSubFolders": str(childcount != 0).upper(),
                      "PidTagFolderChildCount": str(childcount),
                      "SystemIdx": str(SystemIdx)})

    def add_sub_public_folder(self, dn, parentfid, fid, change_num, name, SystemIndex, childcount):
        self.ldb.add({"dn": dn,
                      "objectClass": ["publicfolder"],
                      "cn": fid,
                      "PidTagFolderId": fid,
                      "PidTagParentFolderId": parentfid,
                      "PidTagChangeNumber": change_num,
                      "PidTagDisplayName": name,
                      "PidTagCreationTime": "%d" % self.nttime,
                      "PidTagLastModificationTime": "%d" % self.nttime,
                      "PidTagAttributeHidden": str(0),
                      "PidTagAttributeReadOnly": str(0),
                      "PidTagAttributeSystem": str(0),
                      "PidTagContainerClass": "IPF.Note (check this)",
                      "PidTagSubFolders": str(childcount != 0).upper(),
                      "PidTagFolderChildCount": str(childcount),
                      "FolderType": str(1),
                      "SystemIdx": str(SystemIndex)})

    def add_one_public_folder(self, parent_fid, path, children, SystemIndex, names, dn_prefix = None):

        name = path[-1]
        GlobalCount = self.get_message_GlobalCount(names.netbiosname)
        ChangeNumber = self.get_message_ChangeNumber(names.netbiosname)
        ReplicaID = self.get_message_ReplicaID(names.netbiosname)
        if dn_prefix is None:
            dn_prefix = "CN=publicfolders,CN=%s,CN=%s,%s" % (names.firstou, names.firstorg, names.ocserverdn)
        fid = gen_mailbox_folder_fid(GlobalCount, ReplicaID)
        dn = "CN=%s,%s" % (fid, dn_prefix)
        change_num = gen_mailbox_folder_fid(ChangeNumber, ReplicaID)
        childcount = len(children)
        print "\t* %-40s: 0x%.16x (%s)" % (name, int(fid, 10), fid)
        if parent_fid == 0:
            self.add_root_public_folder(dn, fid, change_num, SystemIndex, childcount)
        else:
            self.add_sub_public_folder(dn, parent_fid, fid, change_num, name, SystemIndex, childcount)

        GlobalCount += 1
        self.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)
        ChangeNumber += 1
        self.set_message_ChangeNumber(names.netbiosname, ChangeNumber=ChangeNumber)

        for name, grandchildren in children.iteritems():
            self.add_one_public_folder(fid, path + (name,), grandchildren[0], grandchildren[1], names, dn)

    def add_public_folders(self, names):
        pfstoreGUID = str(uuid.uuid4())
        self.ldb.add({"dn": "CN=publicfolders,CN=%s,CN=%s,%s" % (names.firstou, names.firstorg, names.ocserverdn),
                      "objectClass": ["container"],
                      "cn": "publicfolders",
                      "StoreGUID": pfstoreGUID,
                      "ReplicaID": str(1)})
        public_folders = _public_folders_meta(names)
        self.add_one_public_folder(0, ("Public Folder Root",), public_folders[0], public_folders[1], names)

    def lookup_server(self, cn, attributes=[]):
        # Step 1. Search Server object
        filter = "(&(objectClass=server)(cn=%s))" % cn
        res = self.ldb.search("", scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=attributes)
        if len(res) != 1:
            raise NoSuchServer(cn)
        return res[0]

    def lookup_mailbox_user(self, server, username, attributes=[]):
        """Check if a user already exists in openchange database.

        :param server: Server object name
        :param username: Username object
        :return: LDB Object of the user
        """
        server_dn = self.lookup_server(server, []).dn

        # Step 2. Search User object
        filter = "(&(objectClass=mailbox)(cn=%s))" % (username)
        return self.ldb.search(server_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=filter, attrs=attributes)

    def lookup_public_folder(self, server, displayname, attributes=[]):
        """Retrieve the record for a public folder matching a specific display name

        :param server: Server Object Name
        :param displayname: Display Name of the Folder
        :param attributes: Requested Attributes
        :return: LDB Object of the Folder
        """
        server_dn = self.lookup_server(server, []).dn

        filter = "(&(objectClass=publicfolder)(PidTagDisplayName=%s))" % (displayname)
        return self.ldb.search(server_dn, scope=ldb.SCOPE_SUBTREE,
                               expression=filter, attrs=attributes)

    def get_message_attribute(self, server, attribute):
        """Retrieve attribute value from given message database (server).

        :param server: Server object name
        """
        return int(self.lookup_server(server, [attribute])[attribute][0], 10)

    def get_message_ReplicaID(self, server):
        """Retrieve current mailbox Replica ID for given message database (server).

        :param server: Server object name
        """
        return self.get_message_attribute(server, "ReplicaID")

    def get_message_GlobalCount(self, server):
        """Retrieve current mailbox Global Count for given message database (server).

        :param server: Server object name
        """
        return self.get_message_attribute(server, "GlobalCount")


    def set_message_GlobalCount(self, server, GlobalCount):
        """Update current mailbox GlobalCount for given message database (server).

        :param server: Server object name
        :param index: Mailbox new GlobalCount value
        """
        server_dn = self.lookup_server(server, []).dn

        newGlobalCount = """
dn: %s
changetype: modify
replace: GlobalCount
GlobalCount: %d
""" % (server_dn, GlobalCount)

        self.ldb.transaction_start()
        try:
            self.ldb.modify_ldif(newGlobalCount)
        finally:
            self.ldb.transaction_commit()

    def get_message_ChangeNumber(self, server):
        """Retrieve current mailbox Global Count for given message database (server).

        :param server: Server object name
        """
        return self.get_message_attribute(server, "ChangeNumber")


    def set_message_ChangeNumber(self, server, ChangeNumber):
        """Update current mailbox ChangeNumber for given message database (server).

        :param server: Server object name
        :param index: Mailbox new ChangeNumber value
        """
        server_dn = self.lookup_server(server, []).dn

        newChangeNumber = """
dn: %s
changetype: modify
replace: ChangeNumber
ChangeNumber: %d
""" % (server_dn, ChangeNumber)

        self.ldb.transaction_start()
        try:
            self.ldb.modify_ldif(newChangeNumber)
        finally:
            self.ldb.transaction_commit()


class MysqlBackendMixin(object):
    """Mixin for common methods for MySQL backends.

    It expects the following attributes when calling its methods:

    * url string for MySQL connection string.
    * migration_app string for migration-related methods."""

    def _connect_to_mysql(self):
        host, user, passwd, self.db_name, port = self._parse_mysql_url()
        self.db = MySQLdb.connect(host=host, user=user, passwd=passwd, port=port)

    def _parse_mysql_url(self):
        # self.url should be mysql://user[:passwd]@some_host[:port]/some_db_name

        conn_url = urlparse(self.url)

        exc_str = "Bad connection string for mysql: "
        if conn_url.scheme != 'mysql':
            exc_str += "invalid schema"
            raise ValueError(exc_str)

        if (conn_url.hostname is None or conn_url.username is None
           or conn_url.path is None):
            missing_str = None
            if '@' not in self.url:
                missing_str = '@'
            elif '/' not in self.url:
                missing_str = '/'
            exc_str += "expected format mysql://user[:passwd]@host/db_name"
            if missing_str:
                exc_str += ". Missing: " + missing_str
            raise ValueError(exc_str)

        passwd = conn_url.password
        if passwd is None:
            passwd = ""
        port = conn_url.port
        if port is None:
            port = 0

        return (conn_url.hostname, conn_url.username, passwd, conn_url.path.strip('/'), port)

    def migrate(self, version=None):
        """Migrate both mysql schema and data

        :param int version: indicating which version to migrate. None migrates
                            to the lastest version.
        :rtype bool:
        :returns: True if any migration has been performed
        """
        self.db.select_db(self.db_name)
        migration_manager = MigrationManager(self.db, self.db_name)
        return migration_manager.migrate(self.migration_app, version)

    def list_migrations(self):
        """List migrations metadata

        :rtype dict:
        :returns: list migrations indexed by version with a dict as
                  value with 'applied' and 'class' as keys
        """
        self.db.select_db(self.db_name)
        migration_manager = MigrationManager(self.db, self.db_name)
        return migration_manager.list_migrations(self.migration_app)

    def fake_migration(self, target_version):
        """Fake a migration to a target version by simulating
           the apply/unapply migrations.

        :param int target_version: the version to simulate the migration
        :rtype: bool
        :returns: True if the migration was successfully faked
        """
        self.db.select_db(self.db_name)
        migration_manager = MigrationManager(self.db, self.db_name)
        return migration_manager.fake_migration(self.migration_app, target_version)


class OpenChangeDBWithMysqlBackend(MysqlBackendMixin):
    """The OpenChange database."""

    def __init__(self, url, setup_dir=""):
        self.url = url
        self._connect_to_mysql()
        self.nttime = samba.unix2nttime(int(time.time()))
        self.ou_id = None  # initialized on add_server() method
        self.replica_id = 1
        self.global_count = 1  # folder_id for public folders
        self._change_number = None
        self.migration_app = 'openchangedb'

    def _create_database(self):
        if '`' in self.db_name:
            raise Exception("Database name must not have '`' on its name")
        self._execute("DROP DATABASE IF EXISTS `%s`" %
                      self.db.escape_string(self.db_name))
        self._execute("CREATE DATABASE `%s`" %
                      self.db.escape_string(self.db_name))
        self.db.select_db(self.db_name)

    def remove(self):
        """Remove an existing OpenChangeDB."""
        self._execute("DROP DATABASE `%s`" %
                      self.db.escape_string(self.db_name))

    def setup(self, names=None):
        self._create_database()
        # Migrate to latest version
        return self.migrate()

    def _execute(self, sql, params=()):
        cur = self.db.cursor()
        try:
            if isinstance(params, list):
                cur.executemany(sql, params)
            else:
                cur.execute(sql, params)
        except MySQLdb.ProgrammingError as e:
            print "Error executing %s with %r: %r" % (sql, params, e)
            raise e
        self.db.commit()
        cur.close()
        return cur

    def add_server(self, names):
        self.db.select_db(self.db_name)
        cur = self._execute("INSERT organizational_units VALUES (0, %s, %s)",
                            (names.firstorg, names.firstou))
        self.ou_id = int(cur.lastrowid)
        change_number = 1
        cur = self._execute("INSERT servers VALUES (0, %s, %s, %s)",
                            (self.ou_id, self.replica_id, change_number))
        self.server_id = int(cur.lastrowid)

    def _add_root_public_folder(self, fid, change_num, SystemIdx, childcount):
        display_name = "Public Folder Root"
        properties = [("PidTagFolderChildCount", str(childcount)),
                      ("PidTagChangeNumber", change_num,),
                      ("PidTagDisplayName", display_name),
                      ("PidTagCreationTime", str(self.nttime)),
                      ("PidTagLastModificationTime", str(self.nttime)),
                      ("PidTagSubFolders", str(childcount != 0).upper())]
        cur = self._execute(
            "INSERT folders VALUES (0, %s, %s, 'public', NULL, NULL, 1, %s, NULL)",
            (self.ou_id, fid, SystemIdx))
        folder_id = cur.lastrowid
        self._execute("INSERT folders_properties VALUES (%s, %s, %s)",
                      [(folder_id,) + p for p in properties])

    def _add_sub_public_folder(self, parentfid, fid, change_num, display_name,
                               SystemIdx, childcount):
        properties = [("PidTagFolderChildCount", str(childcount)),
                      ("PidTagChangeNumber", change_num,),
                      ("PidTagDisplayName", display_name),
                      ("PidTagCreationTime", str(self.nttime)),
                      ("PidTagLastModificationTime", str(self.nttime)),
                      ("PidTagSubFolders", str(childcount != 0).upper()),
                      ("PidTagAttributeHidden", str(0)),
                      ("PidTagAttributeReadOnly", str(0)),
                      ("PidTagAttributeSystem", str(0)),
                      ("PidTagContainerClass", "IPF.Note (check this)")]
        cur = self._execute(
            "INSERT folders VALUES (0, %s, %s, 'public', NULL, (SELECT id FROM "
            "(SELECT id FROM folders WHERE folder_id = %s AND ou_id = %s) as t),"
            " 1, %s, NULL)",
            (self.ou_id, fid, int(parentfid), self.ou_id, SystemIdx))
        folder_id = cur.lastrowid
        self._execute("INSERT folders_properties VALUES (%s, %s, %s)",
                      [(folder_id,) + p for p in properties])

    def _add_one_public_folder(self, parent_fid, path, children, system_index,
                               names):
        name = path[-1]
        fid = gen_mailbox_folder_fid(self.global_count, self.replica_id)
        change_num = gen_mailbox_folder_fid(self.change_number, self.replica_id)
        childcount = len(children)
        print "\t* %-40s: 0x%.16x (%s)" % (name, int(fid, 10), fid)
        if parent_fid == 0:
            self._add_root_public_folder(fid, change_num, system_index, childcount)
        else:
            self._add_sub_public_folder(parent_fid, fid, change_num, name,
                                        system_index, childcount)

        self.global_count += 1
        self.change_number += 1

        for name, grandchildren in children.iteritems():
            self._add_one_public_folder(fid, path + (name,), grandchildren[0],
                                        grandchildren[1], names)

    def add_public_folders(self, names):
        if self.ou_id is None:
            raise Exception("You have to add a server before calling add_public_folders method")

        store_guid = str(uuid.uuid4())
        self._execute("INSERT public_folders VALUES (%s, %s, %s)",
                      (self.ou_id, self.replica_id, store_guid))

        public_folders = _public_folders_meta(names)

        print "[+] Public Folders"
        print "==================="
        self._add_one_public_folder(0, ("Public Folder Root",),
                                    public_folders[0], public_folders[1], names)

    @property
    def change_number(self):
        if self._change_number is None:
            cur = self._execute("SELECT change_number FROM servers WHERE id = %s",
                                self.server_id)
            data = cur.fetchone()
            if data:
                self._change_number = data[0]
            else:
                self._change_number = 1
        return self._change_number

    @change_number.setter
    def change_number(self, value):
        self._execute("UPDATE servers SET change_number = %s WHERE id = %s",
                      (value, self.server_id))
        self._change_number = value


OpenChangeDB = OpenChangeDBWithLdbBackend

def reverse_int64counter(GlobalCount):
    rev_counter = 0
    for x in xrange(6):
        sh = x * 8
        unsh = (7-x) * 8
        rev_counter = rev_counter | (((GlobalCount >> sh) & 0xff) << unsh)

    return rev_counter

def gen_mailbox_folder_fid(GlobalCount, ReplicaID):
    """Generates a Folder ID from index.

    :param GlobalCount: Message database global counter
    :param ReplicaID: Message database replica identifier
    """

    return "%d" % (ReplicaID | reverse_int64counter(GlobalCount))


class IndexingWithMysqlBackend(MysqlBackendMixin):
    """The MAPIStore indexing database in MySQL backend."""

    def __init__(self, url):
        self.url = url
        self._connect_to_mysql()
        self.migration_app = 'indexing'


class NamedPropertiesWithMysqlBackend(MysqlBackendMixin):
    """The MAPIStore named properties database in MySQL backend."""

    def __init__(self, url):
        self.url = url
        self._connect_to_mysql()
        self.migration_app = 'named_properties'


class Directory(MysqlBackendMixin):
    """
    The directory itself does not use mysql but we have this class for ease the migrations
    """

    def __init__(self, url, setup="", ctx=""):
        self.url = url
        self._connect_to_mysql()
        self.migration_app = 'directory'

    def migrate(self, version=None, extra=None):
        """Migrate directory schema and data

        :param int version: indicating which version to migrate. None migrates
                            to the lastest version.
        :param dict extra: in this dictionary we should have the needed data for
                           the directory migration
        :rtype bool:
        :returns: True if any migration has been performed
        """
        self.db.select_db(self.db_name)
        migration_manager = MigrationManager(self.db, self.db_name)
        return migration_manager.migrate(self.migration_app, version, extra)
