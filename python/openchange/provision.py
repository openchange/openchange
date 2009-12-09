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

def openchangedb_url(lp):
    return os.path.join(lp.get("private dir"), "openchange.ldb")

def openchangedb_mapistore_url(lp):
    return os.path.join(lp.get("private dir"), "mapistore");

class ProvisionNames(object):

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
        firstorg = FIRST_ORGANIZATION

    if firstou is None:
        firstou = FIRST_ORGANIZATION_UNIT

    names.firstorg = firstorg
    names.firstou = firstou
    names.firstorgdn = "CN=%s,CN=Microsoft Exchange,CN=Services,%s" % (firstorg, configdn)
    names.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)

    # OpenChange dispatcher DB names
    names.ocserverdn = "CN=%s,%s" % (names.netbiosname, names.domaindn)
    names.ocfirstorg = firstorg
    names.ocfirstorgdn = "CN=%s,CN=%s,%s" % (firstou, names.ocfirstorg, names.ocserverdn)

    return names


def install_schemas(setup_path, names, lp, creds):
    """Install the OpenChange-specific schemas in the SAM LDAP database. 
    
    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    """
    session_info = system_session()

    # Step 1. Extending the prefixmap attribute of the schema DN record
    db = SamDB(url=lp.get("sam database"), session_info=session_info,
                  credentials=creds, lp=lp)

    prefixmap = open(setup_path("AD/prefixMap.txt"), 'r').read()

    db.transaction_start()

    try:
        print "[+] Step 1: Register Exchange OIDs"
        setup_modify_ldif(db,
                          setup_path("AD/provision_schema_basedn_modify.ldif"), {
                "SCHEMADN": names.schemadn,
                "NETBIOSNAME": names.netbiosname,
                "DEFAULTSITE": names.sitename,
                "CONFIGDN": names.configdn,
                "SERVERDN": names.serverdn,
                "PREFIXMAP_B64": b64encode(prefixmap)
                })
    except:
        db.transaction_cancel()
        raise

    db.transaction_commit()


    # Step 2. Add new Exchange classes to the schema
    db = SamDB(url=lp.get("sam database"), session_info=session_info, 
                  credentials=creds, lp=lp)

    db.transaction_start()

    try:
        print "[+] Step 2: Add new Exchange classes and attributes to Samba schema"
        setup_add_ldif(db, setup_path("AD/oc_provision_schema.ldif"), { 
                "SCHEMADN": names.schemadn
                })
    except:
        db.transaction_cancel()
        raise

    db.transaction_commit()

    # Step 3. Extend existing classes and attributes
    db = SamDB(url=lp.get("sam database"), session_info=session_info, 
                  credentials=creds, lp=lp)

    db.transaction_start()

    try:
        print "[+] Step 3: Extend existing Samba classes and attributes"
        setup_modify_ldif(db, setup_path("AD/oc_provision_schema_modify.ldif"), {
                "SCHEMADN": names.schemadn
                })
    except:
        db.transaction_cancel()
        raise

    db.transaction_commit()


    # Step 4. Add configuration objects
    db = SamDB(url=lp.get("sam database"), session_info=session_info, 
                  credentials=creds, lp=lp)

    db.transaction_start()

    try:
        print "[+] Step 4: Exchange Samba with Exchange configuration objects"
        setup_add_ldif(db, setup_path("AD/oc_provision_configuration.ldif"), {
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
        db.transaction_cancel()
        raise

    db.transaction_commit()


def newmailbox(lp, username, firstorg, firstou):
    names = guess_names_from_smbconf(lp, firstorg, firstou)

    db = mailbox.OpenChangeDB(openchangedb_url(lp))

    # Step 1. Retrieve current FID index
    GlobalCount = db.get_message_GlobalCount(names.netbiosname)
    ReplicaID = db.get_message_ReplicaID(names.netbiosname)

    print "[+] Mailbox for '%s'" % (username)
    print "==================" + "=" * len(username)
    print "* GlobalCount (0x%x) and ReplicaID (0x%x)" % (GlobalCount, ReplicaID)

    # Step 2. Check if the user already exists
    assert not db.user_exists(names.netbiosname, username)

    # Step 3. Create a default mapistore content repository for this user
    db.add_storage_dir(mapistoreURL=openchangedb_mapistore_url(lp), username=username)
    print "* Mapistore content repository created: %s" % os.path.join(openchangedb_mapistore_url(lp), username)

    # Step 4. Create the user object
    retdn = db.add_mailbox_user(names.ocfirstorgdn, username=username)
    print "* User object created: %s" % (retdn)

    # Step 5. Create system mailbox folders for this user
    print "* Adding System Folders"

    system_folders = ({
        "Deferred Actions": ({}, 2),
        "Spooler Queue": ({}, 3),
        "IPM Subtree": ({
            "Inbox": ({}, 5),
            "Outbox": ({}, 6),
            "Sent Items": ({}, 7),
            "Deleted Items": ({}, 8),
        }, 4),
        "Common Views": ({}, 9),
        "Schedule": ({}, 10),
        "Search": ({}, 11),
        "Views": ({}, 12),
        "Shortcuts": ({}, 13),
        "Reminders": ({}, 14),
    }, 1)

    fids = {}
    def add_folder(parent_fid, path, children, SystemIdx):
        name = path[-1]

        GlobalCount = db.get_message_GlobalCount(names.netbiosname)
        ReplicaID = db.get_message_ReplicaID(names.netbiosname)

        fid = db.add_mailbox_root_folder(names.ocfirstorgdn, 
            username=username, foldername=name,
            parentfolder=parent_fid, GlobalCount=GlobalCount, 
            ReplicaID=ReplicaID, SystemIdx=SystemIdx, 
            mapistoreURL=openchangedb_mapistore_url(lp))

        GlobalCount += 1
        db.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)

        fids[path] = fid

        print "\t* %-40s: %s" % (name, fid)
        for name, grandchildren in children.iteritems():
            add_folder(fid, path + (name,), grandchildren[0], grandchildren[1])

    add_folder(0, ("Mailbox Root",), system_folders[0], system_folders[1])

    # Step 6. Add special folders
    print "* Adding Special Folders:"
    special_folders = [
        (("Mailbox Root", "IPM Subtree"), "Calendar",   "IPF.Appointment",  "PidTagIpmAppointmentEntryId"),
        (("Mailbox Root", "IPM Subtree"), "Contacts",   "IPF.Contact",      "PidTagIpmContactEntryId"),
        (("Mailbox Root", "IPM Subtree"), "Journal",    "IPF.Journal",      "PidTagIpmJournalEntryId"),
        (("Mailbox Root", "IPM Subtree"), "Notes",      "IPF.StickyNote",   "PidTagIpmNoteEntryId"),
        (("Mailbox Root", "IPM Subtree"), "Tasks",      "IPF.Task",         "PidTagIpmTaskEntryId"),
        (("Mailbox Root", "IPM Subtree"), "Drafts",     "IPF.Notes",        "PidTagIpmDraftsEntryId")
        ]

    fid_inbox = fids[("Mailbox Root", "IPM Subtree", "Inbox")]
    fid_reminders = fids[("Mailbox Root", "Reminders")]
    fid_mailbox = fids[("Mailbox Root",)]
    for path, foldername, containerclass, pidtag in special_folders:
        GlobalCount = db.get_message_GlobalCount(names.netbiosname)
        ReplicaID = db.get_message_ReplicaID(names.netbiosname)
        fid = db.add_mailbox_special_folder(username, fids[path], fid_inbox, foldername, 
                                            containerclass, GlobalCount, ReplicaID, 
                                            openchangedb_mapistore_url(lp))
        db.add_folder_property(fid_inbox, pidtag, fid)
        db.add_folder_property(fid_mailbox, pidtag, fid)
        GlobalCount += 1
        db.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)
        print "\t* %-40s: %s (%s)" % (foldername, fid, containerclass)

    # Step 7. Set default receive folders
    print "* Adding default Receive Folders:"
    receive_folders = [
        (("Mailbox Root", "IPM Subtree", "Inbox"), "All"),
        (("Mailbox Root", "IPM Subtree", "Inbox"), "IPM"),
        (("Mailbox Root", "IPM Subtree", "Inbox"), "Report.IPM"),
        (("Mailbox Root", "IPM Subtree",), "IPC")
        ]
    
    for path, messageclass in receive_folders:
        print "\t* %-40s Message Class added to %s" % (messageclass, fids[path])
        db.set_receive_folder(username, names.ocfirstorgdn, fids[path], 
                              messageclass)

    # Step 8. Set additional properties on Inbox
    print "* Adding additional default properties to Inbox"
    db.add_folder_property(fid_inbox, "PidTagContentCount", "0")
    db.add_folder_property(fid_inbox, "PidTagContentUnreadCount", "0")
    db.add_folder_property(fid_inbox, "PidTagSubFolders", "FALSE")
    db.add_folder_property(fid_inbox, "PidTagAccess", "63")

    print "* Adding additional default properties to Reminders"
    db.add_folder_property(fid_reminders, "PidTagContainerClass", "Outlook.Reminder");
    db.add_folder_property(fid_inbox, "PidTagRemindersOnlineEntryId", fid_reminders);
    db.add_folder_property(fid_mailbox, "PidTagRemindersOnlineEntryId", fid_reminders);

    GlobalCount = db.get_message_GlobalCount(names.netbiosname)
    print "* GlobalCount (0x%x)" % GlobalCount


def newuser(lp, creds, username=None):
    """extend user record with OpenChange settings.
    
    :param lp: Loadparm context
    :param creds: Credentials context
    :param username: Name of user to extend
    """

    names = guess_names_from_smbconf(lp, None, None)

    db = Ldb(url=os.path.join(lp.get("private dir"), lp.get("sam database")), 
             session_info=system_session(), credentials=creds, lp=lp)

    user_dn = "CN=%s,CN=Users,%s" % (username, names.domaindn)

    extended_user = """
dn: %s
changetype: modify
add: displayName
displayName: %s
add: auxiliaryClass
auxiliaryClass: msExchBaseClass
add: mailNickName
mailNickname: %s
add: homeMDB
homeMDB: CN=Mailbox Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=%s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%s
add: legacyExchangeDN
legacyExchangeDN: /o=%s/ou=First Administrative Group/cn=Recipients/cn=%s
add: proxyAddresses
proxyAddresses: smtp:postmaster@%s
proxyAddresses: X400:c=US;a= ;p=First Organizati;o=Exchange;s=%s
proxyAddresses: SMTP:%s@%s
replace: msExchUserAccountControl
msExchUserAccountControl: 0
""" % (user_dn, username, username, names.netbiosname, names.netbiosname, names.firstorg, names.domaindn, names.firstorg, username, names.dnsdomain, username, username, names.dnsdomain)
    db.modify_ldif(extended_user)

    print "[+] User %s extended and enabled" % username


def accountcontrol(lp, creds, username=None, value=0):
    """enable/disable an OpenChange user account.

    :param lp: Loadparm context
    :param creds: Credentials context
    :param username: Name of user to disable
    :param value: the control value
    """

    names = guess_names_from_smbconf(lp, None, None)

    db = Ldb(url=os.path.join(lp.get("private dir"), lp.get("sam database")), 
             session_info=system_session(), credentials=creds, lp=lp)

    user_dn = "CN=%s,CN=Users,%s" % (username, names.domaindn)
    extended_user = """
dn: %s
changetype: modify
replace: msExchUserAccountControl
msExchUserAccountControl: %d
""" % (user_dn, value)
    db.modify_ldif(extended_user)
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


def openchangedb_provision(lp, firstorg=None, firstou=None):
    """Create the OpenChange database.

    :param lp: Loadparm context
    :param firstorg: First Organization
    :param firstou: First Organization Unit
    """
    names = guess_names_from_smbconf(lp, firstorg, firstou)
    
    print "Setting up openchange db"
    openchange_ldb = mailbox.OpenChangeDB(openchangedb_url(lp))
    openchange_ldb.setup()

    # Add a server object
    # It is responsible for holding the GlobalCount identifier (48 bytes)
    # and the Replica identifier
    openchange_ldb.add_server(names.ocserverdn, names.netbiosname, 
        names.firstorg, names.firstou)


def find_setup_dir():
    """Find the setup directory used by provision."""
    dirname = os.path.dirname(__file__)
    if "/site-packages/" in dirname:
        prefix = dirname[:dirname.index("/site-packages/")]
        for suffix in ["share/openchange/setup", "share/setup", "share/samba/setup", "setup"]:
            ret = os.path.join(prefix, suffix)
            if os.path.isdir(ret):
                return ret
    # In source tree
    ret = os.path.join(dirname, "../../setup")
    if os.path.isdir(ret):
        return ret
    raise Exception("Unable to find setup directory.")
