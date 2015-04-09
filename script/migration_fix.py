#!/usr/bin/env python
# encoding: utf8
#
# Mailbox migration fix script
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

import os
import sys
import re
import MySQLdb
import optparse
from samba.param import LoadParm

class MigrationFix(object):
    def __init__(self, username=None, no_dry_run=False, mysql_string=None):
        self.lp = LoadParm()
        self.lp.set('debug level', '0')
        self.lp.load_default()
        self.username = username
        self.no_dry_run = no_dry_run
        self.conn = self._open_mysql_conn(mysql_string)

    def _open_mysql_conn(self, mysql_conn):
        if mysql_conn is None:
            conn_str = self.lp.get('mapiproxy:openchangedb')
        else:
            conn_str = mysql_conn
        if not conn_str:
            raise Exception("No mysql connection string specified and no mapiproxy:openchangedb param option found")
        # mysql://openchange:password@localhost/openchange
        m = re.search('(?P<scheme>.+)://(?P<user>.+):(?P<pass>.+)@'
                      '(?P<host>.+)/(?P<db>.+)',
                      conn_str)
        if not m:
            raise Exception("Unable to parse mysql connection string: %s" % conn_str)
        group_dict = m.groupdict()
        if group_dict['scheme'] != 'mysql':
            raise Exception("mysql connection string should start with mysql:// (got %s)", group_dict['scheme'])
        conn = MySQLdb.connect(host=group_dict['host'], user=group_dict['user'],
                               passwd=group_dict['pass'], db=group_dict['db'])
        conn.autocommit(True)
        return conn

    def fix(self, username, folder_id, uri, mailbox_id, ou_id):
        error = False
        c = self.conn.cursor()
        c.execute("SELECT fmid FROM mapistore_indexing WHERE username=%s AND url=%s", (username,uri))
	result = c.fetchone()
	if result is None:
		print '[KO]: %s: could not find fmid for %s' % (username, uri)
		return True

        fmid = result[0]
        if str(fmid) != str(folder_id):
            error = True
            if self.no_dry_run is True:
                c = self.conn.cursor()
                c.execute("UPDATE folders SET folder_id=%s WHERE MAPIStoreURI=%s AND mailbox_id=%s AND ou_id=%s", (fmid, uri, mailbox_id, ou_id))
                print '[FIX]: %s: folder_id for %s has been fixed and is now set to %s' % (username, uri, folder_id)
            else:
                print '[KO]: %s: Mismatch for %s: found %s, expected %s' % (username, uri, folder_id, fmid)
        return error

    def run_all(self):
        c = self.conn.cursor()
        c.execute("SELECT id,ou_id,name FROM mailboxes")
        rows = c.fetchall()

        for row in rows:
            mailbox_id = row[0]
            ou_id = row[1]
            username = row[2]
            # Retrieve all MAPIStoreURI from folders table for username
            c = self.conn.cursor()
            c.execute("SELECT folder_id,MAPIStoreURI FROM folders WHERE mailbox_id=%s AND ou_id=%s AND MAPIStoreURI!=\"\"", (mailbox_id,ou_id))
            frows = c.fetchall()

            # Now check in mapistore_indexing if fmid are matching for given URI
	    gl_error =  False 
            for frow in frows:
                folder_id = frow[0]
                uri = frow[1]
                error = self.fix(username, folder_id, uri, mailbox_id, ou_id)
		if error is True:
			gl_error = True 
            if gl_error is False:
                print '[OK]: %s is OK' % (username)

    def run(self):
        # Retrieve username id, ou_id
        c = self.conn.cursor()
        c.execute("SELECT id,ou_id FROM mailboxes WHERE name=%s", self.username)
        (mailbox_id,ou_id) = c.fetchone()

        # Retrieve all MAPIStoreURI from folders table for username
        c = self.conn.cursor()
        c.execute("SELECT folder_id,MAPIStoreURI FROM folders WHERE mailbox_id=%s AND ou_id=%s AND MAPIStoreURI!=\"\"", (mailbox_id,ou_id))
        rows = c.fetchall()

        # Now check in mapistore_indexing if fmid are matching for given URI
        for row in rows:
            folder_id = row[0]
            uri = row[1]
            self.fix(self.username, folder_id, uri, mailbox_id, ou_id)

if __name__ == "__main__":

    parser = optparse.OptionParser("%s [options] <username>" % os.path.basename(sys.argv[0]))
    parser.add_option("--no-dry-run", action="store_true", help="Perform migration fix action")
    parser.add_option("--run-all", action="store_true", help="Do the check for every user")
    parser.add_option("--mysql-string", action="store_true", help="Specify mysql connection string")

    opts,args = parser.parse_args()

    if len(args) != 1 and opts.run_all is False:
        parser.print_help()
        sys.exit(1)

    if opts.run_all is True:
        mfix = MigrationFix(None, opts.no_dry_run, opts.mysql_string)
        mfix.run_all()
    else:
	if len(args):
        	mfix = MigrationFix(args[0], opts.no_dry_run, opts.mysql_string)
        	mfix.run()
	else:
		parser.print_help()
    
