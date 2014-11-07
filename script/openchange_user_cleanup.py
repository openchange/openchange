#!/usr/bin/env python
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


class SambaHelper(object):
    def __init__(self):
        self.samba_lp = LoadParm()
        self.samba_lp.set('debug level', '0')
        self.samdb = SamDB(url=self.samdb_path(), lp=self.samba_lp,
                           session_info=system_session())

    def mapistore_folder(self):
        return self.samba_lp.private_path("mapistore")

    def samdb_path(self):
        return self.samba_lp.private_path("sam.ldb")

    def openchange_ldb_path(self):
        return self.samba_lp.private_path("openchange.ldb")

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
        conn = ldb.Ldb(self.openchange_ldb_path())
        entries = conn.search(None, expression="(MailboxGUID=*)", attrs=["cn"],
                              scope=ldb.SCOPE_SUBTREE)
        return [entry["cn"][0] for entry in entries]


class ImapCleaner(object):
    def __init__(self, imap_host="127.0.0.1", imap_port=143, dry_run=False, samba_helper=SambaHelper()):
        self.imap_host = imap_host
        self.imap_port = imap_port
        self.dry_run = dry_run
        self.samba_helper = samba_helper

    def cleanup(self, username, password=""):
        print "===== IMAP cleanup ====="
        client = imaplib.IMAP4(self.imap_host, self.imap_port)
        try:
            email = self.samba_helper.find_email_of(username)
            code, data = client.login(email, password)
            if code != "OK":
                raise Exception("Login failure")

            print " [IMAP] Logged in as %s" % email

            patterns = ["*(1)*", "Sync Issues*", "Spam*", "Problemas de sincroni*"]
            for pattern in patterns:
                code, data = client.list("", pattern)
                if code != "OK":
                    raise Exception("ERROR IMAP listing folders with pattern %s" % pattern)
                if data[0] is None:
                    print " [IMAP] No folders with %s pattern" % pattern
                else:
                    for d in data:
                        folder_name = d.split("\"")[-2]
                        if not self.dry_run:
                            code, data = client.delete(folder_name)
                            if code != "OK":
                                raise Exception("ERROR IMAP deleting %s" % folder_name)
                        print " [IMAP] Deleted %s" % folder_name
        finally:
            client.logout()


class OpenchangeCleaner(object):
    def __init__(self, samba_helper=SambaHelper(), dry_run=False):
        self.samba_helper = samba_helper
        self.dry_run = dry_run

    def mapistore_indexing_cleanup(self, username):
        print "===== Mapistore indexing cleanup ====="

        mapistore_user_dir = os.path.join(self.samba_helper.mapistore_folder(), username)
        print " [Indexing] Deleting %s" % mapistore_user_dir
        if not self.dry_run:
            shutil.rmtree(mapistore_user_dir, ignore_errors=True)

    def openchangedb_cleanup(self, username):
        print "===== OpenchangeDB cleanup ====="

        conn = ldb.Ldb(self.samba_helper.openchange_ldb_path())
        entries = conn.search(None, expression="(cn=%s)" % (username),
                              scope=ldb.SCOPE_SUBTREE)
        if len(entries) != 1:
            raise Exception(" [OpenchangeDB LDB] cn = %s must return 1 entry "
                            "instead of %d" % (username, len(entries)))
        subentries = conn.search(entries[0].dn.extended_str(),
                                 expression="(distinguishedName=*)",
                                 scope=ldb.SCOPE_SUBTREE)
        for subentry in subentries:
            if not self.dry_run:
                conn.delete(subentry.dn)
            print " [OpenchangeDB LDB] Deleted %s" % (subentry.dn)

    def cleanup(self, username):
        self.openchangedb_cleanup(username)
        self.mapistore_indexing_cleanup(username)


class SOGoCleaner(object):
    def __init__(self, dry_run=False, samba_helper=SambaHelper()):
        self.dry_run = dry_run
        self.samba_helper = samba_helper
        self.system_defaults_file = "/etc/sogo/sogo.conf"
        self.user_defaults_file = os.path.expanduser("~sogo/GNUstep/Defaults/.GNUstepDefaults")

    def asCSSIdentifier(self, inputString):
        cssEscapingCharMap = {"_" : "_U_",
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
        newChars = []
        for c in inputString:
            if c in cssEscapingCharMap:
                newChars.append(cssEscapingCharMap[c])
            else:
                newChars.append(c)
        return "".join(newChars)

    def sogo_mysql_cleanup(self, dbhost, dbport, dbuser, dbpass, dbname, username):
        conn = MySQLdb.connect(host=dbhost, port=int(dbport), user=dbuser, passwd=dbpass, db=dbname)
        c = conn.cursor()
        tablename = "socfs_%s" % self.asCSSIdentifier(username)
        if not self.dry_run:
            c.execute("TRUNCATE TABLE %s" % tablename)
        print " [SOGo MySQL] Table %s deleted" % tablename

        """
        c.execute("SELECT c_location, c_quick_location, c_acl_location "
                  "FROM sogo_folder_info "
                  "WHERE c_path2 = '%s'" % username.replace("'", "\\'"))
        results = c.fetchall()
        for tables in results:
            for table in tables:
                t = table.split('/')[-1]
                if not self.dry_run:
                    c.execute("TRUNCATE TABLE %s" % t)
                print " [SOGo MySQL] Table %s deleted" % t
        """

    def _getOCSFolderInfoURL(self):
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
        OCSFolderInfoURL = self._getOCSFolderInfoURL()
        if OCSFolderInfoURL is None:
            raise Exception("ERROR Couldn't fetch OCSFolderInfoURL")

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

    samba_helper = SambaHelper()

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
