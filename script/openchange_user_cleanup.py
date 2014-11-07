#!/usr/bin/env python
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


class SambaOCHelper(object):
    def __init__(self):
        self.samba_lp = LoadParm()
        self.samba_lp.set('debug level', '0')
        self.samba_lp.load_default()
        self.samdb = SamDB(url=self.samba_lp.private_path("sam.ldb"),
                           lp=self.samba_lp,
                           session_info=system_session())
        self.conn = self._open_mysql_connection()

    def _open_mysql_connection(self):
        connection_string = self.samba_lp.get('mapiproxy:openchangedb')
        if not connection_string:
            raise Exception("Not found mapiproxy:openchangedb on samba configuration")
        # mysql://openchange:password@localhost/openchange
        m = re.search("(.+)://(.+):(.+)@(.+)/(.+)", connection_string)
        if not m:
            raise Exception("Unable to parse mapiproxy:openchangedb: %s" %
                            connection_string)
        if m.group(1) != 'mysql':
            raise Exception("mapiproxy:openchangedb should start with mysql:// (we got %s)")

        dbuser = m.group(2)
        dbpass = m.group(3)
        dbhost = m.group(4)
        dbname = m.group(5)

        return MySQLdb.connect(host=dbhost, user=dbuser, passwd=dbpass, db=dbname)

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
    def __init__(self, imap_host="127.0.0.1", imap_port=143, dry_run=False, samba_helper=SambaOCHelper()):
        self.imap_host = imap_host
        self.imap_port = imap_port
        self.dry_run = dry_run
        self.samba_helper = samba_helper

    def cleanup(self, username, password=""):
        print "===== IMAP cleanup ====="
        client = imaplib.IMAP4(self.imap_host, self.imap_port)
        if not password:
            master_file = self.samba_helper.samba_lp.private_path('mapistore/master.password')
            with open(master_file) as f:
                password = f.read()
        try:
            email = self.samba_helper.find_email_of(username)
            code, data = client.login(email, password)
            if code != "OK":
                raise Exception("Login failure")

            print " [IMAP] Logged in as %s" % email

            patterns = ["*(1)*", "Spam", "Sync Issues*", "Problemas de sincroni*",
                        "Problèmes de synchronisation*"]
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
        conn = MySQLdb.connect(host=dbhost, port=int(dbport), user=dbuser, passwd=dbpass, db=dbname)
        c = conn.cursor()
        tablename = "sogo_cache_folder_%s" % self._as_css_id(username)
        if not self.dry_run:
            c.execute("TRUNCATE TABLE %s" % tablename)
        print " [SOGo MySQL] Table %s deleted" % tablename

    def _get_OCSFolderInfoURL(self):
        OCSFolderInfoURL = ""
        # read defaults from defaults files
        # order is important, user defaults must have precedence
        for f in [self.system_defaults_file, self.user_defaults_file]:
          if os.path.exists(f):
            p1 = subprocess.Popen(["sogo-tool", "dump-defaults", "-f", f], stdout=subprocess.PIPE)
            p2 = subprocess.Popen(["awk", "-F\"", "/ OCSFolderInfoURL =/ {print $2}"], stdin=p1.stdout, stdout=subprocess.PIPE)
            p1.stdout.close()  # Allow p1 to receive a SIGPIPE if p2 exits.
            tmp = p2.communicate()[0]
            if tmp:
                OCSFolderInfoURL = tmp
        return OCSFolderInfoURL

    def cleanup(self, username):
        print "===== SOGo cleanup ====="
        OCSFolderInfoURL = self._get_OCSFolderInfoURL()
        if OCSFolderInfoURL is None:
            raise Exception("Couldn't fetch OCSFolderInfoURL")

        # mysql://sogo:sogo@127.0.0.1:5432/sogo/sogo_folder_info
        m = re.search("(.+)://(.+):(.+)@(.+):(\d+)/(.+)/(.+)", OCSFolderInfoURL)
        if not m:
            raise Exception("ERROR Unable to parse OCSFolderInfoURL: %s" %
                            OCSFolderInfoURL)

        dbuser = m.group(2)
        dbpass = m.group(3)
        dbhost = m.group(4)
        dbport = m.group(5)
        dbname = m.group(6)

        self.sogo_mysql_cleanup(dbhost, dbport, dbuser, dbpass, dbname, username)

# -----------------------------------------------------------------------------

if __name__ == "__main__":

    def cleanup(username, samba_helper, dry_run=False):
        for klass in (OpenchangeCleaner, SOGoCleaner, ImapCleaner):
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

    cleanup(username, samba_helper, opts.dry_run)
