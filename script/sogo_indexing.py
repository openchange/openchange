#!/usr/bin/python
#
# OpenChange debugging script
#
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2015
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
# encoding: utf8

import imaplib
import optparse
import MySQLdb
import ldb
import os
import re
import shutil
import subprocess
import sys
from samba.param import LoadParm
from samba.samdb import SamDB
from samba.auth import system_session
import time

class SambaOCHelper(object):
    def __init__(self):
        self.samba_lp = LoadParm()
        self.samba_lp.set('debug level', '0')
        self.samba_lp.load_default()
        self.next_fmid = None
        url = self.samba_lp.get('dcerpc_mapiproxy:samdb_url') or \
            self.samba_lp.private_path("sam.ldb")
        self.samdb = SamDB(url=url,
                           lp=self.samba_lp,
                           session_info=system_session())
        self.conn = self._open_mysql_connection()

    def _open_mysql_connection(self):
        connection_string = self.samba_lp.get('mapiproxy:openchangedb')
        if not connection_string:
            raise Exception("Not found mapiproxy:openchangedb on samba configuration")
        # mysql://openchange:password@localhost/openchange
        m = re.search(r'(?P<scheme>.+)://(?P<user>.+):(?P<pass>.+)@(?P<host>.+)/(?P<db>.+)',
                      connection_string)
        if not m:
            raise Exception("Unable to parse mapiproxy:openchangedb: %s" %
                            connection_string)
        group_dict = m.groupdict()
        if group_dict['scheme'] != 'mysql':
            raise Exception("mapiproxy:openchangedb should start with mysql:// (we got %s)",
                            group_dict['scheme'])

        conn = MySQLdb.connect(host=group_dict['host'], user=group_dict['user'],
                               passwd=group_dict['pass'], db=group_dict['db'])
        conn.autocommit(True)
        return conn

    def invalid_user(self, username):
        ret = self.samdb.search(base=self.samdb.domain_dn(),
                                scope=ldb.SCOPE_SUBTREE,
                                expression="(sAMAccountName=%s)" % ldb.binary_encode(username))
        return len(ret) != 1

    def find_email_of(self, username):
        ret = self.samdb.search(base=self.samdb.domain_dn(),
                                scope=ldb.SCOPE_SUBTREE, attrs=["mail"],
                                expression="(sAMAccountName=%s)" % ldb.binary_encode(username))
        return ret[0]["mail"][0]

    def active_openchange_users(self):
        c = self.conn.cursor()
        c.execute("SELECT name FROM mailboxes")
        return sorted([row[0] for row in c.fetchall()])

    def allocate_fmids(self, count, username):
        if self.next_fmid is None:
            c = self.conn.cursor()
            c.execute("SELECT next_fmid FROM mapistore_indexes WHERE username = '%s'" % username)
            self.next_fmid = c.fetchone()[0]
        if self.next_fmid is not None:
            self.next_fmid = int(self.next_fmid) + count

        return (int(self.next_fmid) - count, self.next_fmid)

    def create_indexes(self, username):
        c = self.conn.cursor()
        c.execute("INSERT INTO mapistore_indexes (username,next_fmid) VALUES('%s','1024')" % (username))
        return

    def commit_start(self):
        self.conn.autocommit(False)

    def insert_indexing(self, username, fmid, url):
        c = self.conn.cursor()
        
        c.execute("INSERT INTO mapistore_indexing (username,fmid,url,soft_deleted) VALUES('%s','%s','%s', '0')" % (username, str(fmid), url))

    def update_indexes(self, count, username):
        c = self.conn.cursor()
        updated_number = int(count) + int(self.next_fmid)
        print "Updating next_fmid to %s" % str(updated_number)
        c.execute("UPDATE mapistore_indexes SET next_fmid='%s' WHERE username='%s'" % (str(updated_number), username))

    def commit_end(self):
        c = self.conn.cursor()
        self.conn.commit()
        c.close()
        self.conn.autocommit(True)

class ImapWorker(object):
    def __init__(self, dry_run=False, samba_helper=SambaOCHelper(),
                 imap_host=None, imap_port=None, imap_ssl=False):
        self.dry_run = dry_run
        self.samba_helper = samba_helper
        self.imap_host = imap_host
        self.imap_port = imap_port
        self.imap_ssl = imap_ssl
        self.system_defaults_file = "/etc/sogo/sogo.conf"

    def _get_connection_url(self):
        connection_url = None
        # read defaults from defaults files
        if os.path.exists(self.system_defaults_file):
            p1 = subprocess.Popen(["sogo-tool", "dump-defaults", "-f",
                                   self.system_defaults_file],
                                  stdout=subprocess.PIPE)
            p2 = subprocess.Popen(["awk", "-F\"",
                                   "/ SOGoIMAPServer =/ {print $2}"],
                                  stdin=p1.stdout, stdout=subprocess.PIPE)
            p1.stdout.close()  # Allow p1 to receive a SIGPIPE if p2 exits.
            tmp = p2.communicate()[0]
            if tmp:
                connection_url = tmp

        return connection_url

    def _get_imap_from_sogo(self):
        connection_url = self._get_connection_url()

        # imap[s]://127.0.0.1:143
        m = re.search('(?P<scheme>.+)://(?P<host>.+):(?P<port>\d+)',
                      connection_url)
        if not m:
            raise Exception("ERROR Unable to parse IMAPServer: %s" %
                            connection_url)
        group_dict = m.groupdict()
        if group_dict['scheme'] not in ('imaps', 'imap'):
            raise Exception("IMAPServer should start with imap[s]:// "
                            "(we got %s)", group_dict['scheme'])

        self.imap_host = group_dict['host']
        self.imap_port = group_dict['port']
        self.imap_ssl = group_dict['scheme'] == 'imaps'

    def exchange_globcnt(self, globcnt2):
        globcnt = int(globcnt2)
        return (globcnt & 0x00000000000000ff) << 40 | (globcnt & 0x000000000000ff00) << 24 | (globcnt & 0x0000000000ff0000) << 8 | (globcnt & 0x00000000ff000000) >> 8 | (globcnt & 0x000000ff00000000) >> 24 | (globcnt & 0x0000ff0000000000)	>> 40



    def start_indexing(self, username, password=""):
        print "===== IMAP cleanup ====="

        if not self.imap_host:
            self._get_imap_from_sogo()

        if self.imap_ssl:
            client = imaplib.IMAP4_SSL(self.imap_host, self.imap_port)
        else:
            client = imaplib.IMAP4(self.imap_host, self.imap_port)

        if not password:
            master_file = self.samba_helper.samba_lp.private_path('mapistore/master.password')
            if os.path.exists(master_file):
                with open(master_file) as f:
                    password = f.read()
            else:
                password = 'unknown'

        try:
            email = self.samba_helper.find_email_of(username)
            code, data = client.login(email, password)
            count = 0
            if code != "OK":
                raise Exception("Login failure")

            print " [IMAP] Logged in as %s" % email

            code, data = client.list("")
            if code != "OK":
                raise Exception("ERROR IMAP listing folders")
            if data[0] is None:
                raise Exception(" [IMAP] No folders found")
            
            self.samba_helper.commit_start()
            (first_fmid,_) = self.samba_helper.allocate_fmids(0, username)
            for d in data:
                folder_name = None
                m = re.match(".*\".\" \"(.*)\"", d)
                if m:
                    folder_name = m.group(1)
                if not folder_name:
                    m = re.match(".*\".\" (.*)", d)
                    if m:
                        folder_name = m.group(1)
                if not folder_name:
                    raise Exception("Couldn't parse folder name on %r" % d)

                part = folder_name.split('/')
                for p in part:
                    o = p
                    p = p.replace(' ', '_SP_')
                    p = p.replace('_', '_U_')
                    p = "folder%s" % p
                    part[part.index(o)] = p
                sogo_folder_name = '/'.join(part)

                sogo_username = username.replace('@', '%40')
                sogo_url = "sogo://%s:%s@mail/%s" % (sogo_username, sogo_username, sogo_folder_name)
                print " [IMAP] %s" % sogo_url

                client.select(folder_name)
                result,data = client.search(None, "ALL")
                ids = data[0]
                id_list = ids.split()
                count = count + len(id_list)

                (start_range, end_range) =  self.samba_helper.allocate_fmids(len(id_list), username)
                fmid = start_range
                for email_id in id_list:
                  fmid_hex = self.exchange_globcnt(fmid) << 16 | 0x0001
                  uri = '%s/%s.eml' % (sogo_url, email_id)
                  self.samba_helper.insert_indexing(username, fmid_hex, uri)
                  fmid = fmid + 1
                    
            self.samba_helper.update_indexes(first_fmid, username)
            self.samba_helper.commit_end()

        finally:
            client.logout()

if __name__ == "__main__":
    parser = optparse.OptionParser("%s <username>" % os.path.basename(sys.argv[0]))
    opts,args = parser.parse_args()

    samba_helper = SambaOCHelper()

    username = args[0]
    if samba_helper.invalid_user(username):
        print "User %s doesn't exist on samba" % username
        sys.exit(2)
    if username not in samba_helper.active_openchange_users():
        print "User %s is not an active openchange user" % username
        samba_helper.create_indexes(username)

    worker = ImapWorker(dry_run=False, samba_helper=samba_helper)
    try:
        worker.start_indexing(username)
    except Exception as e:
        print "Error indexing %s mailbox: %s" % (username, str(e))

    print '[+] Restarting memcached to flush the cache'
    os.system("service memcached restart")
