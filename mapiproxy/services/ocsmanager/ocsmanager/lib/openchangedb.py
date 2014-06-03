from pylons import config
import MySQLdb
from samba import Ldb
from abc import ABCMeta, abstractmethod
import os

class OpenchangeDB:
    __metaclass__ = ABCMeta

    @abstractmethod
    def get_calendar_uri(self, usercn, email):
        pass


class OpenchangeDBWithMySQL(OpenchangeDB):
    def __init__(self, uri):
        self.db = self._connect_to_mysql(uri)

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
        cur = self.db.cursor()
        try:
            cur.execute(sql, params)
            rows = cur.fetchall()
            cur.close()
            return rows
        except MySQLdb.ProgrammingError as e:
            print "Error executing %s with %r: %r" % (sql, params, e)
            raise e

    def get_calendar_uri(self, usercn, email):
        mailbox_name = email.split('@')[0]
        return self._select("""
            SELECT MAPIStoreURI FROM folders f 
            JOIN folders_properties p ON p.folder_id = f.id 
                AND p.name = 'PidTagContainerClass' 
                AND p.value ='IPF.Appointment' 
            JOIN mailboxes m ON m.id = f.mailbox_id 
                AND m.name = %s""", mailbox_name)


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
