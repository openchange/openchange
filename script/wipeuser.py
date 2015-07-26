#!/usr/bin/env python2.7
# OpenChange DB user wipe script
#
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2014
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

import MySQLdb
from samba import Ldb
import optparse
import os,sys
import samba
import samba.getopt as options
import threading

class OpenchangeDBWithMySQL():
    def __init__(self, uri):
        self.uri = uri
        self.db = self._connect_to_mysql(uri)
        self.qlock = threading.RLock()

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
            return self.select(sql, params)
        except MySQLdb.ProgrammingError as e:
            print "Error executing %s with %r: %r" % (sql, params, e)
            raise e
        finally:
            self.qlock.release()

    def delete(self, sql, params=()):
        cur = self.qlock.acquire()
        cur = self.db.cursor()
        cur.execute(sql, params)
        self.db.commit()
        cur.close()
        return

parser = optparse.OptionParser("wipeuser [options]")

sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)

parser.add_option("--username", type="string", metavar="USERNAME", help="User to wipe")

opts,args = parser.parse_args()
if len(args) != 0:
    parser.print_help()
    sys.exit(1)

lp = sambaopts.get_loadparm()

username = opts.username
if username is None:
    parser.print_help()
    sys.exit(1)
if username.isalnum() is False:
    print "[ERROR] Username must be alpha numeric"
    sys.exit(1)

openchangedb_uri = lp.get("mapiproxy:openchangedb")
if openchangedb_uri is None:
    print "This script only supports MySQL backend for openchangedb"
    sys.exit(1)

c = OpenchangeDBWithMySQL(openchangedb_uri)
rows = c.select("""SELECT * from mailboxes WHERE name=%s """, username)
if len(rows) == 0:
    print "[ERROR] No such user %s" % username
    sys.exit(1)

# Delete entry from mailboxes and mailboxes_properties
rows = c.select("""SELECT id FROM mailboxes WHERE name=%s """, username)
mailbox_id = rows[0][0]

# Delete entry from mailboxes
c.delete("""DELETE FROM mailboxes WHERE name=%s """, username)

# Delete entries from mailboxes_properties
c.delete("""DELETE FROM mailboxes_properties WHERE mailbox_id=%s """, mailbox_id)

# Delete entries for user from folders and folders_properties
rows = c.select("""SELECT id FROM folders WHERE mailbox_id=%s """, mailbox_id)
for ids in rows:
    c.delete("""DELETE FROM folders_properties WHERE folder_id=%s """, ids[0])
    c.delete("""DELETE FROM folders WHERE id=%s """, ids[0])

# Delete from mapistore_indexes and mapistore_indexing
c.delete("""DELETE FROM mapistore_indexes WHERE username=%s """, username)
c.delete("""DELETE FROM mapistore_indexing WHERE username=%s """, username)

# Delete message and properties from messages and messages_properties
rows = c.select("""SELECT message_id FROM messages WHERE mailbox_id=%s """, mailbox_id)
for ids in rows:
    c.delete("""DELETE FROM messages_properties WHERE message_id=%s """, ids[0])

print "[DONE]"
