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
import struct
import samba
from openchange import mailbox
from samba import Ldb
from samba.samdb import SamDB
from samba.auth import system_session
from samba.provision import setup_add_ldif, setup_modify_ldif
from openchange.urlutils import openchangedb_url, openchangedb_mapistore_url, openchangedb_mapistore_dir, openchangedb_suffix_for_mapistore_url

__docformat__ = 'restructuredText'

DEFAULTSITE = "Default-First-Site-Name"
FIRST_ORGANIZATION = "First Organization"
FIRST_ORGANIZATION_UNIT = "First Administrative Group"

# This is a hack. Kind-of cute, but still a hack
def abstract():
    import inspect
    caller = inspect.getouterframes(inspect.currentframe())[1][3]
    raise NotImplementedError(caller + ' must be implemented in subclass')

# Define an abstraction for progress reporting from the provisioning
class AbstractProgressReporter(object):

    def __init__(self):
        self.currentStep = 0

    def reportNextStep(self, stepName):
        self.currentStep = self.currentStep + 1
        self.doReporting(stepName)

    def doReporting(self, stepName):
        abstract()

# A concrete example of a progress reporter - just provides text output for
# each new step.
class TextProgressReporter(AbstractProgressReporter):
    def doReporting(self, stepName):
        print "[+] Step %d: %s" % (self.currentStep, stepName)

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

def provision_schema(setup_path, names, lp, creds, reporter, ldif, msg):
    """Provision schema using LDIF specified file

    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    """

    session_info = system_session()

    db = SamDB(url=lp.samdb_url(), session_info=session_info, 
               credentials=creds, lp=lp)

    db.transaction_start()

    try:
        reporter.reportNextStep(msg)
        setup_add_ldif(db, setup_path(ldif), { 
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

def modify_schema(setup_path, names, lp, creds, reporter, ldif, msg):
    """Modify schema using LDIF specified file

    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    """

    session_info = system_session()

    db = SamDB(url=lp.samdb_url(), session_info=session_info, 
               credentials=creds, lp=lp)

    db.transaction_start()

    try:
        reporter.reportNextStep(msg)
        setup_modify_ldif(db, setup_path(ldif), { 
                "SCHEMADN": names.schemadn,
                "CONFIGDN": names.configdn
                })
    except:
        db.transaction_cancel()
        raise

    db.transaction_commit()

def install_schemas(setup_path, names, lp, creds, reporter):
    """Install the OpenChange-specific schemas in the SAM LDAP database. 
    
    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    """
    session_info = system_session()

    # Step 1. Extending the prefixmap attribute of the schema DN record
    db = SamDB(url=lp.samdb_url(), session_info=session_info,
                  credentials=creds, lp=lp)

    prefixmap = open(setup_path("AD/prefixMap.txt"), 'r').read()

    db.transaction_start()

    try:
        reporter.reportNextStep("Register Exchange OIDs")
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

    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_attributes.ldif", "Add Exchange attributes to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_auxiliary_class.ldif", "Add Exchange auxiliary classes to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_objectCategory.ldif", "Add Exchange objectCategory to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_container.ldif", "Add Exchange containers to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_subcontainer.ldif", "Add Exchange *sub* containers to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_sub_CfgProtocol.ldif", "Add Exchange CfgProtocol subcontainers to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_sub_mailGateway.ldif", "Add Exchange mailGateway subcontainers to Samba schema")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema.ldif", "Add Exchange classes to Samba schema")
    modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_possSuperior.ldif", "Add possSuperior attributes to Exchange classes")
    modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_modify.ldif", "Extend existing Samba classes and attributes")
    provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_configuration.ldif", "Exchange Samba with Exchange configuration objects")
    print "[SUCCESS] Done!"

def newmailbox(lp, username, firstorg, firstou, backend):
    def guid_to_binary(guid):
        # "31c32976-efda-4d3c-81fb-63dd2ab9f780"
	time_low = int(guid[0:8], 16)
        time_mid = int(guid[9:13], 16)
        time_hi_and_version = int(guid[14:18], 16)
        clock_seq = ""
        for x in xrange(2):
            idx = 19 + x * 2
            clock_seq = clock_seq + chr(int(guid[idx:idx+2], 16))
        node = ""
        for x in xrange(6):
            idx = 24 + x * 2
            node = node + chr(int(guid[idx:idx+2], 16))

        binguid = struct.pack("<LHH", time_low, time_mid, time_hi_and_version) + clock_seq + node

        return binguid

    def make_folder_entryid(provider_uid, folder_type, database_guid, counter):
        entryid = (4 * chr(0)  #flags
                   + provider_uid
                   + struct.pack("<H", folder_type)
                   + database_guid
                   + struct.pack(">LH", (counter >> 16) & 0xffffffff, (counter & 0xffff))
                   + 2 * chr(0)) # padding

        return entryid

    names = guess_names_from_smbconf(lp, firstorg, firstou)

    db = mailbox.OpenChangeDB(openchangedb_url(lp))

    # Step 1. Retrieve current FID index
    GlobalCount = db.get_message_GlobalCount(names.netbiosname)
    ChangeNumber = db.get_message_ChangeNumber(names.netbiosname)
    ReplicaID = db.get_message_ReplicaID(names.netbiosname)

    print "[+] Mailbox for '%s'" % (username)
    print "==================" + "=" * len(username)
    print "* GlobalCount (0x%x), ChangeNumber (0x%x) and ReplicaID (0x%x)" % (GlobalCount, ChangeNumber, ReplicaID)

    # Step 2. Check if the user already exists
    assert not db.user_exists(names.netbiosname, username)

    # Step 3. Create a default mapistore content repository for this user
    db.add_storage_dir(mapistoreURL=openchangedb_mapistore_dir(lp), username=username)
    print "* Mapistore content repository created: %s" % os.path.join(openchangedb_mapistore_dir(lp), username)

    # Step 4. Create the user object
    retdn = db.add_mailbox_user(names.ocfirstorgdn, username=username)
    print "* User object created: %s" % (retdn)

    # Step 5. Create system mailbox folders for this user
    print "* Adding System Folders"

    system_folders = ({
        "Deferred Actions": ({}, 2),
        "Spooler Queue": ({}, 3),
        "To-Do Search": ({}, 4),
        "IPM Subtree": ({
            "Inbox": ({}, 6),
            "Outbox": ({}, 7),
            "Sent Items": ({}, 8),
            "Deleted Items": ({}, 9),
        }, 5),
        "Common Views": ({}, 10),
        "Schedule": ({}, 11),
        "Search": ({}, 12),
        "Views": ({}, 13),
        "Shortcuts": ({}, 14),
        "Reminders": ({}, 15),
    }, 1)

    fids = {}
    def add_folder(parent_fid, path, children, SystemIdx):
        name = path[-1]

        GlobalCount = db.get_message_GlobalCount(names.netbiosname)
        ChangeNumber = db.get_message_ChangeNumber(names.netbiosname)
        ReplicaID = db.get_message_ReplicaID(names.netbiosname)
        url = openchangedb_mapistore_url(lp, backend)

        fid = db.add_mailbox_root_folder(names.ocfirstorgdn, 
                                         username=username, foldername=name,
                                         parentfolder=parent_fid,
                                         GlobalCount=GlobalCount, ChangeNumber=ChangeNumber,
                                         ReplicaID=ReplicaID, SystemIdx=SystemIdx, 
                                         mapistoreURL=url,
                                         mapistoreSuffix=openchangedb_suffix_for_mapistore_url(url))

        GlobalCount += 1
        db.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)
        ChangeNumber += 1
        db.set_message_ChangeNumber(names.netbiosname, ChangeNumber=ChangeNumber)

        fids[path] = fid

        print "\t* %-40s: 0x%.16x (%s)" % (name, int(fid, 10), fid)
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
        (("Mailbox Root", "IPM Subtree"), "Drafts",     "IPF.Note",         "PidTagIpmDraftsEntryId")
        ]

    fid_inbox = fids[("Mailbox Root", "IPM Subtree", "Inbox")]
    fid_reminders = fids[("Mailbox Root", "Reminders")]
    fid_mailbox = fids[("Mailbox Root",)]

    mailbox_guid = guid_to_binary(db.get_user_MailboxGUID(names.netbiosname, username)[0])
    replica_guid = guid_to_binary(db.get_user_ReplicaGUID(names.netbiosname, username)[0])

    for path, foldername, containerclass, pidtag in special_folders:
        GlobalCount = db.get_message_GlobalCount(names.netbiosname)
        ChangeNumber = db.get_message_ChangeNumber(names.netbiosname)
        ReplicaID = db.get_message_ReplicaID(names.netbiosname)
        url = openchangedb_mapistore_url(lp, backend)
        fid = db.add_mailbox_special_folder(username, fids[path], foldername, 
                                            containerclass, GlobalCount, ChangeNumber, ReplicaID, 
                                            url, openchangedb_suffix_for_mapistore_url(url), retdn)
        entryid = make_folder_entryid(mailbox_guid, 1, replica_guid, GlobalCount)
        db.add_folder_property(fid_inbox, pidtag, entryid.encode("base64").strip())
        db.add_folder_property(fid_mailbox, pidtag, entryid.encode("base64").strip())
        GlobalCount += 1
        db.set_message_GlobalCount(names.netbiosname, GlobalCount=GlobalCount)
        ChangeNumber += 1
        db.set_message_ChangeNumber(names.netbiosname, ChangeNumber=ChangeNumber)
        print "\t* %-40s: 0x%.16x (%s)" % ("%s (%s)" % (foldername, containerclass), int(fid, 10), fid)

    # Step 7. Set default receive folders
    print "* Adding default Receive Folders:"
    receive_folders = [
        (("Mailbox Root", "IPM Subtree", "Inbox"), "All"),
        (("Mailbox Root", "IPM Subtree", "Inbox"), "IPM"),
        (("Mailbox Root", "IPM Subtree", "Inbox"), "Report.IPM"),
        (("Mailbox Root", "IPM Subtree", "Inbox"), "IPM.Note"),
        (("Mailbox Root", "IPM Subtree",), "IPC")
        ]
    
    for path, messageclass in receive_folders:
        print "\t* Message Class '%s' added to 0x%.16x (%s)" % (messageclass, int(fids[path], 10), fids[path])
        db.set_receive_folder(username, names.ocfirstorgdn, fids[path], messageclass)

    # Step 8. Set additional properties on Inbox
    print "* Adding additional default properties to Inbox"
    db.add_folder_property(fid_inbox, "PidTagContentCount", "0")
    db.add_folder_property(fid_inbox, "PidTagContentUnreadCount", "0")
    db.add_folder_property(fid_inbox, "PidTagSubFolders", "FALSE")

    print "* Adding additional default properties to Reminders"
    db.add_folder_property(fid_reminders, "PidTagContainerClass", "Outlook.Reminder");

    print "* Adding freebusy entry to public folders"
    db.add_user_public_freebusy(username, names)

    rev_fid_reminders = mailbox.reverse_int64counter(int(fid_reminders, 10) >> 16) >> 16
    entryid = make_folder_entryid(mailbox_guid, 1, replica_guid, rev_fid_reminders)
    db.add_folder_property(fid_inbox, "PidTagRemindersOnlineEntryId", entryid.encode("base64").strip())
    db.add_folder_property(fid_mailbox, "PidTagRemindersOnlineEntryId", entryid.encode("base64").strip())
    GlobalCount = db.get_message_GlobalCount(names.netbiosname)
    print "* GlobalCount (0x%.12x)" % GlobalCount
    ChangeNumber = db.get_message_ChangeNumber(names.netbiosname)
    print "* ChangeNumber (0x%.12x)" % ChangeNumber


def newuser(lp, creds, username=None):
    """extend user record with OpenChange settings.
    
    :param lp: Loadparm context
    :param creds: Credentials context
    :param username: Name of user to extend
    """

    names = guess_names_from_smbconf(lp, None, None)

    db = Ldb(url=lp.samdb_url(), session_info=system_session(), 
             credentials=creds, lp=lp)

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
add: homeMTA
homeMTA: CN=Mailbox Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=%s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%s
add: legacyExchangeDN
legacyExchangeDN: /o=%s/ou=First Administrative Group/cn=Recipients/cn=%s
add: proxyAddresses
proxyAddresses: =EX:/o=%s/ou=First Administrative Group/cn=Recipients/cn=%s
proxyAddresses: smtp:postmaster@%s
proxyAddresses: X400:c=US;a= ;p=First Organizati;o=Exchange;s=%s
proxyAddresses: SMTP:%s@%s
replace: msExchUserAccountControl
msExchUserAccountControl: 0
""" % (user_dn, username, username, names.netbiosname, names.netbiosname, names.firstorg, names.domaindn, names.netbiosname, names.netbiosname, names.firstorg, names.domaindn, names.firstorg, username, names.firstorg, username, names.dnsdomain, username, username, names.dnsdomain)
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

    db = Ldb(url=os.path.join(lp.get("private dir"), lp.samdb_url()), 
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


def provision(setup_path, lp, creds, firstorg=None, firstou=None, reporter=None):
    """Extend Samba4 with OpenChange data.
    
    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param firstorg: First Organization
    :param firstou: First Organization Unit
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)

    If a progress reporter is not provided, a text output reporter is provided
    """
    names = guess_names_from_smbconf(lp, firstorg, firstou)

    print "NOTE: This operation can take several minutes"

    if reporter is None:
        reporter = TextProgressReporter()

    # Install OpenChange-specific schemas
    install_schemas(setup_path, names, lp, creds, reporter)


def openchangedb_provision(lp, firstorg=None, firstou=None, mapistore=None):
    """Create the OpenChange database.

    :param lp: Loadparm context
    :param firstorg: First Organization
    :param firstou: First Organization Unit
    :param mapistore: The public folder store type (fsocpf, sqlite, etc)
    """
    names = guess_names_from_smbconf(lp, firstorg, firstou)
    
    print "Setting up openchange db"
    openchange_ldb = mailbox.OpenChangeDB(openchangedb_url(lp))
    openchange_ldb.setup()

    print "Adding root DSE"
    openchange_ldb.add_rootDSE(names.ocserverdn, names.firstorg, names.firstou)

    # Add a server object
    # It is responsible for holding the GlobalCount identifier (48 bytes)
    # and the Replica identifier
    openchange_ldb.add_server(names.ocserverdn, names.netbiosname, names.firstorg, names.firstou)

    mapistoreURL = os.path.join( openchangedb_mapistore_url(lp, mapistore), "publicfolders")
    print "[+] Public Folders"
    print "==================="
    openchange_ldb.add_public_folders(names, mapistoreURL)

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
