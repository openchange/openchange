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
import os,sys
import struct
import samba
from openchange import mailbox
from samba import param, Ldb, dsdb, substitute_var, read_and_sub_file
from samba.samdb import SamDB
import ldb
from ldb import (SCOPE_SUBTREE, SCOPE_BASE, FLAG_MOD_REPLACE, FLAG_MOD_ADD, FLAG_MOD_DELETE)
from samba.auth import system_session, admin_session
from samba.provision import (setup_add_ldif, setup_modify_ldif, setup_ldb,find_provision_key_parameters)
from samba.upgradehelpers import (get_paths, get_ldbs)
from openchange.urlutils import openchangedb_url

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
    """Modify schema using LDIF specified file                                                                                                                                                                          :param setup_path: Path to the setup directory.
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

    lp.set("dsdb:schema update allowed", "yes")

    # Step 1. Extending the prefixmap attribute of the schema DN record
    samdb = SamDB(url=lp.samdb_url(), session_info=session_info,
                  credentials=creds, lp=lp)

    schemadn = str(names.schemadn)
    current = samdb.search(expression="objectClass=*", base=schemadn, 
                           scope=SCOPE_SUBTREE)
    

    schema_ldif = ""
    prefixmap_data = ""
    for ent in current:
        schema_ldif += samdb.write_ldif(ent, ldb.CHANGETYPE_NONE)

    prefixmap_data = open(setup_path("AD/prefixMap.txt"), 'r').read()
    prefixmap_data = b64encode(prefixmap_data)

    # We don't actually add this ldif, just parse it
    prefixmap_ldif = "dn: %s\nprefixMap:: %s\n\n" % (schemadn, prefixmap_data)
    reporter.reportNextStep("Register Exchange OIDs")
    dsdb._dsdb_set_schema_from_ldif(samdb, prefixmap_ldif, schema_ldif, schemadn)

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

    print "[+] Public Folders"
    print "==================="
    openchange_ldb.add_public_folders(names)

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
