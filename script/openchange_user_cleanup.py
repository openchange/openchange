#!/usr/bin/env python
# encoding: utf8
import imaplib
import optparse
import MySQLdb
import memcache
import ldb
import os
import re
import subprocess
import sys
from samba.param import LoadParm
from samba.samdb import SamDB
from samba.auth import system_session


class SambaOCHelper(object):
    def __init__(self):
        self.samba_lp = LoadParm()
        self.samba_lp.set('debug level', '0')
        self.samba_lp.load_default()
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


class ImapCleaner(object):
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
        m = re.search('((?P<scheme>.+)://)?(?P<host>.+):(?P<port>\d+)',
                      connection_url)
        if not m:
            raise Exception("ERROR Unable to parse IMAPServer: %s" %
                            connection_url)
        group_dict = m.groupdict()
        if group_dict['scheme'] not in (None, 'imaps', 'imap'):
            raise Exception("IMAPServer should start with imap[s]:// "
                            "(we got %s)", group_dict['scheme'])

        self.imap_host = group_dict['host']
        self.imap_port = group_dict['port']
        self.imap_ssl = group_dict['scheme'] == 'imaps'

    def cleanup(self, username, password=""):
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
            if code != "OK":
                raise Exception("Login failure")

            print " [IMAP] Logged in as %s" % email

            patterns = ["*(1)*", "Spam", "Sync Issues*",
                        "Problemas de sincroni*",
                        "Problèmes de synchronisation*",
                        "Synchronisierungsprobleme*",
                        "Ошибки синхронизации*"]
            for pattern in patterns:
                code, data = client.list("", pattern)
                if code != "OK":
                    raise Exception("ERROR IMAP listing folders with pattern %s" % pattern)
                if data[0] is None:
                    print " [IMAP] No folders with %s pattern" % pattern
                else:
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
                        if not self.dry_run:
                            code, data = client.delete(folder_name)
                            if code != "OK":
                                raise Exception("ERROR IMAP deleting %s" % folder_name)
                        print " [IMAP] Deleted %s" % folder_name
        finally:
            client.logout()

class MemcachedCleaner(object):
    def __init__(self, samba_helper=SambaOCHelper(), dry_run=False):
        self.samba_helper = samba_helper
        self.dry_run = dry_run

    def cleanup(self, username):
        print "===== Memcached cleanup ====="

        mc = self._connect_to_memcached()
        if all(s.connect() == 0 for s in mc.servers):
            print " [Memcached] No memcached servers"
            return

        keys = self._get_all_keys(mc)
        if not keys:
            print " [Memcached] There are no keys to delete"
            return

        print "WARNING: All data from memcached will be deleted"
        if not self.dry_run:
            for key in keys:
                mc.delete(key)
        print " [Memcached] Deleted %d keys" % len(keys)

    def _connect_to_memcached(self):
        # FIXME read from openchange conf
        host = "127.0.0.1:11211"
        return memcache.Client([host])

    def _get_all_keys(self, mc):
        keys = []
        # FIXME support several memcached servers
        if len(mc.servers) > 1:
            print "WARNING: More than one server, you must restart them manually"
        server = mc.servers[0]
        slabs = mc.get_slabs()[0][1].keys()
        item_re = re.compile('^ITEM (?P<key>\S+) \[\d+ b; \d+ s\]$')
        for slab in slabs:
            server.send_cmd("stats cachedump %s 0" % slab)
            line = server.readline()
            eof = False
            while not eof:
                m = item_re.match(line)
                if m:
                    keys.append(m.groupdict()['key'])

                line = server.readline()
                eof = line == 'END'
        return keys


class OpenchangeCleaner(object):
    def __init__(self, samba_helper=SambaOCHelper(), dry_run=False):
        self.samba_helper = samba_helper
        self.dry_run = dry_run

    def mapistore_indexing_cleanup(self, username):
        print "===== Mapistore indexing cleanup ====="

        if not self.dry_run:
            c = self.samba_helper.conn.cursor()
            sql = ("DELETE FROM mapistore_indexes WHERE username = '%s'" %
                   MySQLdb.escape_string(username))
            c.execute(sql)
            sql = ("DELETE FROM mapistore_indexing WHERE username = '%s'" %
                   MySQLdb.escape_string(username))
            c.execute(sql)
            c.close()
        print " [OC-Indexing] Deleted indexing database for %s" % username

    def openchangedb_cleanup(self, username):
        print "===== OpenchangeDB cleanup ====="

        if not self.dry_run:
            c = self.samba_helper.conn.cursor()
            sql = ("DELETE FROM mailboxes WHERE name = '%s'" %
                   MySQLdb.escape_string(username))
            c.execute(sql)
            c.close()
        print " [OC-OpenchangeDB] Deleted openchangedb for %s" % username

    def cleanup(self, username):
        self.openchangedb_cleanup(username)
        self.mapistore_indexing_cleanup(username)


class SOGoCleaner(object):
    def __init__(self, dry_run=False, samba_helper=SambaOCHelper()):
        self.dry_run = dry_run
        self.samba_helper = samba_helper
        self.system_defaults_file = "/etc/sogo/sogo.conf"
        self.user_defaults_file = os.path.expanduser("~sogo/GNUstep/Defaults/.GNUstepDefaults")

    def _as_css_id(self, input_string):
        css_char_map = {"_" : "_U_",
                        "." : "_D_",
                        "#" : "_H_",
                        "@" : "_A_",
                        "*" : "_S_",
                        ":" : "_C_",
                        "," : "_CO_",
                        " " : "_SP_",
                        "'" : "_SQ_",
                        "&" : "_AM_",
                        "+" : "_P_"}
        new_chars = []
        for c in input_string:
            if c in css_char_map:
                new_chars.append(css_char_map[c])
            else:
                new_chars.append(c)
        return "".join(new_chars)

    def sogo_mysql_cleanup(self, dbhost, dbport, dbuser, dbpass, dbname, username):
        conn = MySQLdb.connect(host=dbhost, port=int(dbport), user=dbuser, passwd=dbpass,
                               db=dbname)
        c = conn.cursor()
        tablename = "sogo_cache_folder_%s" % self._as_css_id(username)
        if not self.dry_run:
            c.execute("DROP TABLE `%s`" % tablename)
        print " [SOGo MySQL] Table %s deleted" % tablename

    def _get_connection_url(self):
        connection_url = None
        # read defaults from defaults files
        # order is important, user defaults must have precedence
        for f in [self.system_defaults_file, self.user_defaults_file]:
          if os.path.exists(f):
            p1 = subprocess.Popen(["sogo-tool", "dump-defaults", "-f", f],
                                  stdout=subprocess.PIPE)
            p2 = subprocess.Popen(["awk", "-F\"", "/ OCSFolderInfoURL =/ {print $2}"],
                                  stdin=p1.stdout, stdout=subprocess.PIPE)
            p1.stdout.close()  # Allow p1 to receive a SIGPIPE if p2 exits.
            tmp = p2.communicate()[0]
            if tmp:
                connection_url = tmp
        return connection_url

    def cleanup(self, username):
        print "===== SOGo cleanup ====="
        connection_url = self._get_connection_url()
        if connection_url is None:
            raise Exception("Couldn't fetch OCSFolderInfoURL")

        # mysql://sogo:sogo@127.0.0.1:5432/sogo/sogo_folder_info
        m = re.search('(?P<scheme>.+)://(?P<user>.+):(?P<pass>.+)@'
                      '(?P<host>.+):(?P<port>\d+)/(?P<db>.+)/(?P<table>.+)',
                      connection_url)
        if not m:
            raise Exception("ERROR Unable to parse OCSFolderInfoURL: %s" %
                            connection_url)
        group_dict = m.groupdict()
        if group_dict['scheme'] != 'mysql':
            raise Exception("OCSFolderInfoURL should start with mysql:// "
                            "(we got %s)", group_dict['scheme'])

        self.sogo_mysql_cleanup(group_dict['host'], group_dict['port'],
                                group_dict['user'], group_dict['pass'],
                                group_dict['db'], username)

# -----------------------------------------------------------------------------

if __name__ == "__main__":

    def cleanup(username, samba_helper, ignore=[], dry_run=False):
        for klass in (OpenchangeCleaner, SOGoCleaner, ImapCleaner, MemcachedCleaner):
            if klass.__name__.split('Cleaner')[0].lower() in ignore:
                continue
            cleaner = klass(dry_run=dry_run, samba_helper=samba_helper)
            try:
                cleaner.cleanup(username)
            except Exception as e:
                print "Error cleaning up with %s: %s" % (str(klass), str(e))

    def list_users(samba_helper):
        for user in sorted(samba_helper.active_openchange_users()):
            print user

    parser = optparse.OptionParser("%s [options] <username>" % os.path.basename(sys.argv[0]))
    parser.add_option("--users", action="store_true", help="List active openchange users")
    parser.add_option("--dry-run", action="store_true", help="Do not perform any action")
    parser.add_option("--ignore", action="append", default=[], help=("Ignore to perform "
                      "some cleaner actions. The ones that exist are: openchange, sogo, "
                      "imap, memcached"))
    opts, args = parser.parse_args()

    samba_helper = SambaOCHelper()

    if opts.users:
        list_users(samba_helper)
        sys.exit(0)
    elif len(args) != 1:
        parser.print_help()
        sys.exit(1)

    username = args[0]
    if samba_helper.invalid_user(username):
        print "User %s doesn't exist on samba" % username
        sys.exit(2)
    if username not in samba_helper.active_openchange_users():
        print "User %s is not an active openchange user" % username
        sys.exit(2)

    cleanup(username, samba_helper, opts.ignore, opts.dry_run)
