# openchangedb.py -- OpenChange DB library
# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Jesús García Saez <jgarcia@zentyal.com>
#                     Samuel Cabrero Alamán <scabrero@zentyal.com>
#                     Enrique J. Hernández <ejhernandez@zentyal.com>
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
from pylons import config
import MySQLdb
from samba import Ldb
from abc import ABCMeta, abstractmethod
import ldb
import os
import threading


class OpenchangeDB:
    __metaclass__ = ABCMeta

    @abstractmethod
    def get_calendar_uri(self, usercn, email):
        """
        :param str usercn: the user common name
        :param str email: the email address

        :returns: the calendar URIs for this user
        :rtype: list of str
        """
        pass


class OpenchangeDBWithMySQL(OpenchangeDB):
    def __init__(self, uri):
        self.uri = uri
        self.db = self._connect_to_mysql(uri)
        self.qlock = threading.RLock()

    def _connect_to_mysql(self, uri):
        # uri should be mysql://user[:passwd]@some_host/some_db_name
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

    def _select(self, sql, params=()):
        try:
            self.qlock.acquire()
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
            return self._select(sql, params)
        except MySQLdb.ProgrammingError as e:
            print "Error executing %s with %r: %r" % (sql, params, e)
            raise e
        finally:
            self.qlock.release()

    def get_calendar_uri(self, usercn, email):
        mailbox_name = email.split('@')[0]
        return [r[0] for r in self._select("""
            SELECT MAPIStoreURI FROM folders f
            JOIN folders_properties p ON p.folder_id = f.id
                AND p.name = 'PidTagContainerClass'
                AND p.value ='IPF.Appointment'
            JOIN mailboxes m ON m.id = f.mailbox_id
                AND m.name = %s""", mailbox_name)]


class OpenchangeDBWithLDB(OpenchangeDB):
    def __init__(self, uri):
        self.ldb = Ldb(uri)

    def get_calendar_uri(self, usercn, email):
        base_dn = "CN=%s,%s" % (usercn, config["samba"]["oc_user_basedn"])
        ldb_filter = "(&(objectClass=systemfolder)(PidTagContainerClass=IPF.Appointment)(MAPIStoreURI=*))"
        res = self.ldb.search(base=base_dn, scope=ldb.SCOPE_SUBTREE,
                              expression=ldb_filter, attrs=["MAPIStoreURI"])
        return [str(res[x]["MAPIStoreURI"][0]) for x in xrange(len(res))]


def get_openchangedb(lp):
    openchangedb_uri = lp.get("mapiproxy:openchangedb")
    if openchangedb_uri:
        return OpenchangeDBWithMySQL(openchangedb_uri)
    else:
        openchage_ldb = os.path.join(lp.get("private dir"), "openchange.ldb")
        return OpenchangeDBWithLDB(openchage_ldb)
