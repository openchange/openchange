#!/usr/bin/python

# OpenChange provisioning
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008-2009
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2009
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

from base64 import b64encode
import os
import samba
from openchange import mailbox
from samba import Ldb
from samba.samdb import SamDB
from samba.auth import system_session
from samba.provision import setup_add_ldif, setup_modify_ldif
import ldb

__docformat__ = 'restructuredText'

DEFAULTSITE = "Default-First-Site-Name"
FIRST_ORGANIZATION = "First Organization"
FIRST_ORGANIZATION_UNIT = "First Organization Unit"


class ProvisionNames:
    def __init__(self):
        self.rootdn = None
        self.domaindn = None
        self.configdn = None
        self.schemadn = None
        self.dnsdomain = None
        self.netbiosname = None
        self.domain = None
        self.hostname = None
        self.firstorg = None
        self.firstou = None
        self.firstorgdn = None
        # OpenChange dispatcher database specific
        self.ocfirstorgdn = None
        self.ocserverdn = None
        self.ocuserdn = None

def guess_names_from_smbconf(lp, firstorg=None, firstou=None):
    """Guess configuration settings to use from smb.conf.
    
    :param lp: Loadparm context.
    :param firstorg: First Organization
    :param firstou: First Organization Unit
    """

    netbiosname = lp.get("netbios name")
    hostname = netbiosname.lower()

    dnsdomain = lp.get("realm")
    dnsdomain = dnsdomain.lower()

    serverrole = lp.get("server role")
    if serverrole == "domain controller":
        domain = lp.get("workgroup")
        domaindn = "DC=" + dnsdomain.replace(".", ",DC=")
    else:
        domain = netbiosname
        domaindn = "CN=" + netbiosname

    rootdn = domaindn
    configdn = "CN=Configuration," + rootdn
    schemadn = "CN=Schema," + configdn
    sitename = DEFAULTSITE

    names = ProvisionNames()
    names.rootdn = rootdn
    names.domaindn = domaindn
    names.configdn = configdn
    names.schemadn = schemadn
    names.dnsdomain = dnsdomain
    names.domain = domain
    names.netbiosname = netbiosname
    names.hostname = hostname
    names.sitename = sitename

    if firstorg is None:
        firstorg = FIRST_ORGANIZATION;

    if firstou is None:
        firstou = FIRST_ORGANIZATION_UNIT;

    names.firstorg = firstorg
    names.firstou = firstou
    names.firstorgdn = "CN=%s,CN=Microsoft Exchange,CN=Services,%s" % (firstorg, configdn)
    names.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)

    # OpenChange dispatcher DB names
    names.ocserverdn = "CN=%s,%s" % (names.netbiosname, names.domaindn)
    names.ocfirstorgdn = "CN=%s,CN=%s,%s" % (firstou, firstorg, names.ocserverdn)

    return names


def install_schemas(setup_path, names, lp, creds):
    """Install the OpenChange-specific schemas in the SAM LDAP database. 
    
    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    """

    # Step 1. Extending the prefixmap attribute of the schema DN record
    samdb = SamDB(url=lp.get("sam database"), session_info=system_session(),
                  credentials=creds, lp=lp)

    prefixmap = open(setup_path("AD/prefixMap.txt"), 'r').read()

    samdb.transaction_start()

    try:
        print "[+] Step 1: Register Exchange OIDs"
        setup_modify_ldif(samdb,
                          setup_path("AD/provision_schema_basedn_modify.ldif"), {
                "SCHEMADN": names.schemadn,
                "NETBIOSNAME": names.netbiosname,
                "DEFAULTSITE": names.sitename,
                "CONFIGDN": names.configdn,
                "SERVERDN": names.serverdn,
                "PREFIXMAP_B64": b64encode(prefixmap)
                })
    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()


    # Step 2. Add new Exchange classes to the schema
    samdb = SamDB(url=lp.get("sam database"), session_info=system_session(), 
                  credentials=creds, lp=lp)

    samdb.transaction_start()

    try:
        print "[+] Step 2: Add new Exchange classes and attributes to Samba schema"
        setup_add_ldif(samdb, setup_path("AD/oc_provision_schema.ldif"), { 
                "SCHEMADN": names.schemadn
                })
    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()

    # Step 3. Add ADSC classes to the schema
    samdb = SamDB(url=lp.get("sam database"), session_info=system_session(),
                  credentials=creds, lp=lp)

    samdb.transaction_start()

    try:
        print "[+] Step 3: Add missing ADSC classes to Samba schema"
        setup_add_ldif(samdb, setup_path("AD/oc_provision_schema_ADSC.ldif"), {
                "SCHEMADN": names.schemadn
                })
    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()


    # Step 4. Extend existing classes and attributes
    samdb = SamDB(url=lp.get("sam database"), session_info=system_session(),
                  credentials=creds, lp=lp)

    samdb.transaction_start()

    try:
        print "[+] Step 4: Extend existing Samba classes and attributes"
        setup_modify_ldif(samdb, setup_path("AD/oc_provision_schema_modify.ldif"), {
                "SCHEMADN": names.schemadn
                })
    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()


    # Step 4. Add configuration objects
    samdb = SamDB(url=lp.get("sam database"), session_info=system_session(),
                  credentials=creds, lp=lp)

    samdb.transaction_start()

    try:
        print "[+] Step 5: Exchange Samba with Exchange configuration objects"
        setup_add_ldif(samdb, setup_path("AD/oc_provision_configuration.ldif"), {
                "FIRSTORG": names.firstorg,
                "FIRSTORGDN": names.firstorgdn,
                "CONFIGDN": names.configdn,
                "SCHEMADN": names.schemadn,
                "DOMAINDN": names.domaindn,
                "DOMAIN": names.domain,
                "DNSDOMAIN": names.dnsdomain,
                "NETBIOSNAME": names.netbiosname,
                "HOSTNAME": names.hostname
                })
    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()


def newmailbox(lp, username=None, firstorg=None, firstou=None):
 
    names = guess_names_from_smbconf(lp, firstorg, firstou)

    db = mailbox.OpenChangeDB()

    # Step 1. Retrieve current FID index
    GlobalCount = db.get_message_GlobalCount(names.netbiosname)
    ReplicaID = db.get_message_ReplicaID(names.netbiosname)
    print "username is %s" % (username)
    print "GlobalCount: 0x%x" % GlobalCount
    print "ReplicaID: 0x%x" % ReplicaID

    # Step 2. Check if the user already exists
    assert db.user_exists(names.netbiosname, username)

    # Step 3. Create the user object
    db.add_mailbox_user(names.ocfirstorgdn, username=username)

    # Step 4. Create system mailbox folders for this user
    system_folders = [
        "Non IPM Subtree",
        "Deferred Actions",
        "Spooler Queue",
        "Top Information Store",
        "Inbox",
        "Outbox",
        "Sent Items",
        "Deleted Items",
        "Common Views",
        "Schedule",
        "Search",
        "Views",
        "Shortcuts"
        ];

    SystemIdx = 1
    for i in system_folders:
        db.add_mailbox_root_folder(names, 
            username=username, foldername=i, 
            GlobalCount=GlobalCount, ReplicaID=ReplicaID,
            SystemIdx=SystemIdx)
        GlobalCount += 1
        SystemIdx += 1

    # Step 5. Update FolderIndex
    db.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)
        
    GlobalCount = db.get_message_GlobalCount(names.netbiosname)
    print "GlobalCount: 0x%x" % GlobalCount


def newuser(lp, creds, username=None):
    """extend user record with OpenChange settings.
    
    :param lp: Loadparm context
    :param creds: Credentials context
    :param username: Name of user to extend
    """

    names = guess_names_from_smbconf(lp, None, None)

    samdb = Ldb(url="users.ldb", session_info=system_session(),
                  credentials=creds, lp=lp)

    samdb.transaction_start()
    try:
        user_dn = "CN=%s,CN=Users,%s" % (username, names.domaindn)

        extended_user = """
dn: %s
changetype: modify
add: displayName
add: auxiliaryClass
add: mailNickName
add: homeMDB
add: legacyExchangeDN
add: proxyAddresses
replace: msExchUserAccountControl
displayName: %s
auxiliaryClass: msExchBaseClass
mailNickname: %s
homeMDB: CN=Mailbox Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=%s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%s
legacyExchangeDN: /o=%s/ou=First Administrative Group/cn=Recipients/cn=%s
proxyAddresses: smtp:postmaster@%s
proxyAddresses: X400:c=US;a= ;p=First Organizati;o=Exchange;s=%s
proxyAddresses: SMTP:%s@%s
msExchUserAccountControl: 0
""" % (user_dn, username, username, names.netbiosname, names.netbiosname, names.firstorg, names.domaindn, names.firstorg, username, names.dnsdomain, username, username, names.dnsdomain)

        samdb.modify_ldif(extended_user)
    except:
        samdb.transaction_cancel()
        raise
    samdb.transaction_commit()

    print "[+] User %s extended and enabled" % username


def accountcontrol(lp, creds, username=None, value=0):
    """enable/disable an OpenChange user account.

    :param lp: Loadparm context
    :param creds: Credentials context
    :param username: Name of user to disable
    :param value: the control value
    """

    names = guess_names_from_smbconf(lp, None, None)

    samdb = Ldb(url="users.ldb", session_info=system_session(),
                  credentials=creds, lp=lp)

    samdb.transaction_start()
    try:
        user_dn = "CN=%s,CN=Users,%s" % (username, names.domaindn)
        extended_user = """
dn: %s
changetype: modify
replace: msExchUserAccountControl
msExchUserAccountControl: %d
""" % (user_dn, value)
        samdb.modify_ldif(extended_user)
    except:
        samdb.transaction_cancel()
        raise
    samdb.transaction_commit()
    if value == 2:
        print "[+] Account %s disabled" % username
    else:
        print "[+] Account %s enabled" % username


def provision(setup_path, lp, creds, firstorg=None, firstou=None):
    """Extend Samba4 with OpenChange data.
    
    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param firstorg: First Organization
    :param firstou: First Organization Unit
    """

    names = guess_names_from_smbconf(lp, firstorg, firstou)

    print "NOTE: This operation can take several minutes"

    # Install OpenChange-specific schemas
    install_schemas(setup_path, names, lp, creds)


def setup_openchangedb(path, setup_path, credentials, names, lp):
    """Setup the OpenChange database.

    :param path: Path to the database.
    :param setup_path: Function that returns the path to a setup.
    :param session_info: Session info
    :param credentials: Credentials
    :param lp: Loadparm context

    :note: This will wipe the OpenChange Database!
    """

    session_info = system_session()

    openchange_ldb = Ldb(path, session_info=session_info,
                           credentials=credentials, lp=lp)

    # Wipes the database
    try:
        openchange_ldb.erase()
    except:
        os.unlink(path)

    openchange_ldb.load_ldif_file_add(setup_path("openchangedb/oc_provision_openchange_init.ldif"))

    openchange_ldb = Ldb(path, session_info=session_info,
                           credentials=credentials, lp=lp)

    # Add a server object
    # It is responsible for holding the GlobalCount identifier (48 bytes)
    # and the Replica identifier
    openchange_ldb.add({"dn": names.ocserverdn,
                        "objectClass": ["top", "server"],
                        "cn": names.netbiosname,
                        "GlobalCount": "0x1",
                        "ReplicaID": "0x1"})

    openchange_ldb.add({"dn": "CN=%s,%s" % (names.firstorg, names.ocserverdn),
                        "objectClass": ["top", "org"],
                        "cn": names.firstorg})

    openchange_ldb.add({"dn": "CN=%s,CN=%s,%s" (names.firstou, names.firstorg, names.ocserverdn),
                        "objectClass": ["top", "ou"],
                        "cn": "${FIRSTOU}"})


def openchangedb_provision(setup_path, lp, creds, firstorg=None, firstou=None):
    """Create the OpenChange database.

    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param firstorg: First Organization
    :param firstou: First Organization Unit
    """

    private_dir = lp.get("private dir")
    path = os.path.join(private_dir, "openchange.ldb")

    names = guess_names_from_smbconf(lp, firstorg, firstou)
    
    print "Setting up openchange db"
    setup_openchangedb(path, setup_path, creds, names, lp)

