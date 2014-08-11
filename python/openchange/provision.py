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
import re
from openchange import mailbox
from samba import Ldb, dsdb
from samba.samdb import SamDB
import ldb
from ldb import LdbError, SCOPE_SUBTREE, SCOPE_BASE
from samba import read_and_sub_file
from samba.auth import system_session
from samba.provision import (setup_add_ldif, setup_modify_ldif)
from samba.net import Net
from samba.dcerpc import nbt
from openchange.urlutils import openchangedb_url

__docformat__ = 'restructuredText'

DEFAULTSITE = "Default-First-Site-Name"


class NotProvisionedError(Exception):
    """Raised when an action expects the server to be provisioned and it's not."""


class ServerInUseError(Exception):
    """Raised when a server is still in use when requested to be removed."""


# This is a hack. Kind-of cute, but still a hack
def abstract():
    import inspect
    caller = inspect.getouterframes(inspect.currentframe())[1][3]
    raise NotImplementedError(caller + ' must be implemented in subclass')


class AbstractProgressReporter(object):
    """Define an abstraction for progress reporting from the provisioning"""
    def __init__(self):
        self.currentStep = 0

    def reportNextStep(self, stepName):
        self.currentStep = self.currentStep + 1
        self.doReporting(stepName)

    def doReporting(self, stepName):
        abstract()


class TextProgressReporter(AbstractProgressReporter):
    """A concrete example of a progress reporter - just provides text output
    for each new step."""
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
        self.hostname = None
        self.serverrole = None
        self.firstorg = None
        self.firstou = None
        self.firstorgdn = None
        # OpenChange dispatcher database specific
        self.ocfirstorgdn = None
        self.ocserverdn = None

        self._domain = None

    @property
    def domain(self):
        if self._domain:
            return self._domain
        elif self.ocserverdn:
            serverdn_parts = self.ocserverdn.split(',')
            return serverdn_parts[-2] + "." + serverdn_parts[-1]

    @domain.setter
    def domain(self, value):
        self._domain = value


def guess_names_from_smbconf(lp, creds=None, firstorg=None, firstou=None):
    """Guess configuration settings to use from smb.conf.

    :param lp: Loadparm context.
    :param firstorg: OpenChange Organization Name
    :param firstou: OpenChange Administrative Group
    """

    netbiosname = lp.get("netbios name")
    hostname = netbiosname.lower()

    dnsdomain = lp.get("realm")
    dnsdomain = dnsdomain.lower()

    serverrole = lp.get("server role")
    # Note: "server role" can have many forms, even for the same function:
    # "member server", "domain controller", "active directory domain
    # controller"...
    if "domain controller" in serverrole or serverrole == "member server":
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
    names.serverrole = serverrole
    names.rootdn = rootdn
    names.domaindn = domaindn
    names.configdn = configdn
    names.schemadn = schemadn
    names.dnsdomain = dnsdomain
    names.domain = domain
    names.netbiosname = netbiosname
    names.hostname = hostname
    names.sitename = sitename

    db = Ldb(url=get_ldb_url(lp, creds, names), session_info=system_session(),
             credentials=creds, lp=lp)
    exchangedn = 'CN=Microsoft Exchange,CN=Services,%s' % configdn
    if not firstorg:
        firstorg = db.searchone(
            'name', exchangedn, '(objectclass=msExchOrganizationContainer)',
            ldb.SCOPE_SUBTREE)
    assert(firstorg)
    firstorgdn = "CN=%s,%s" % (firstorg, exchangedn)

    if not firstou:
        firstou = db.searchone(
            'name', firstorgdn,
            '(&(objectclass=msExchAdminGroup)(msExchDefaultAdminGroup=TRUE))',
            ldb.SCOPE_SUBTREE)
    assert(firstou)

    names.firstorg = firstorg
    names.firstou = firstou
    names.firstorgdn = firstorgdn
    names.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)

    # OpenChange dispatcher DB names
    names.ocserverdn = "CN=%s,%s" % (names.netbiosname, names.domaindn)
    names.ocfirstorg = firstorg
    names.ocfirstorgdn = "CN=%s,CN=%s,%s" % (firstou, names.ocfirstorg, names.ocserverdn)

    return names


def provision_schema(setup_path, names, lp, creds, reporter, ldif, msg, modify_mode=False):
    """Provision/modify schema using LDIF specified file
    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    :param modify_mode: whether entries are added or modified
    """

    session_info = system_session()
    db = SamDB(url=get_ldb_url(lp, creds, names), session_info=session_info,
               credentials=creds, lp=lp)

    db.transaction_start()

    try:
        reporter.reportNextStep(msg)
        if modify_mode:
            ldif_function = setup_modify_ldif
        else:
            ldif_function = setup_add_ldif
        ldif_function(db, setup_path(ldif), {"FIRSTORG": names.firstorg,
                                             "FIRSTORGDN": names.firstorgdn,
                                             "FIRSTOU": names.firstou,
                                             "CONFIGDN": names.configdn,
                                             "SCHEMADN": names.schemadn,
                                             "DOMAINDN": names.domaindn,
                                             "DOMAIN": names.domain,
                                             "DNSDOMAIN": names.dnsdomain,
                                             "NETBIOSNAME": names.netbiosname,
                                             "HOSTNAME": names.hostname})
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

    return provision_schema(setup_path, names, lp, creds, reporter, ldif, msg, True)


def deprovision_schema(setup_path, names, lp, creds, reporter, ldif, msg, modify_mode=False):
    """Deprovision/unmodify schema using LDIF specified file, by reverting the
    modifications contained therein.

    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    :param modify_mode: whether entries are added or modified
    """

    session_info = system_session()
    db = SamDB(url=get_ldb_url(lp, creds, names), session_info=session_info,
               credentials=creds, lp=lp)

    db.transaction_start()

    try:
        reporter.reportNextStep(msg)

        ldif_content = read_and_sub_file(setup_path(ldif),
                                         {"FIRSTORG": names.firstorg,
                                          "FIRSTORGDN": names.firstorgdn,
                                          "FIRSTOU": names.firstou,
                                          "CONFIGDN": names.configdn,
                                          "SCHEMADN": names.schemadn,
                                          "DOMAINDN": names.domaindn,
                                          "DOMAIN": names.domain,
                                          "DNSDOMAIN": names.dnsdomain,
                                          "NETBIOSNAME": names.netbiosname,
                                          "HOSTNAME": names.hostname
                                          })
        if modify_mode:
            lines = ldif_content.splitlines()
            keep_line = False
            entries = []
            current_entry = []
            entries.append(current_entry)
            for line in lines:
                skip_this_line = False
                if line.startswith("dn:") or line == "":
                    # current_entry.append("")
                    current_entry = []
                    entries.append(current_entry)
                    keep_line = True
                elif line.startswith("add:"):
                    keep_line = True
                    line = "delete:" + line[4:]
                elif line.startswith("replace:"):
                    keep_line = False
                elif line.startswith("#") or line.strip() == "":
                    skip_this_line = True

                if keep_line and not skip_this_line:
                    current_entry.append(line)

            entries.reverse()
            for entry in entries:
                ldif_content = "\n".join(entry)
                try:
                    db.modify_ldif(ldif_content)
                except Exception as err:
                    print ("[!] error: %s" % str(err))
        else:
            lines = ldif_content.splitlines()
            lines.reverse()
            for line in lines:
                if line.startswith("dn:"):
                    try:
                        dn = line[4:]
                        ret = db.search(dn, scope=ldb.SCOPE_BASE)
                        if len(ret) != 0:
                            db.delete(line[4:], ["tree_delete:0"])
                    except ldb.LdbError, (enum, estr):
                        if enum == ldb.ERR_NO_SUCH_OBJECT:
                            pass
                        else:
                            print "[!] error: %s" % estr

    except:
        db.transaction_cancel()
        raise

    db.transaction_commit()


def unmodify_schema(setup_path, names, lp, creds, reporter, ldif, msg):
    """Unmodify schema using LDIF specified file

    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    """

    return deprovision_schema(setup_path, names, lp, creds, reporter, ldif, msg, True)


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
    samdb = SamDB(url=get_ldb_url(lp, creds, names), session_info=session_info,
                  credentials=creds, lp=lp)

    reporter.reportNextStep("Register Exchange OIDs")

    try:
        schemadn = str(names.schemadn)
        current = samdb.search(expression="objectClass=classSchema", base=schemadn,
                               scope=SCOPE_BASE)

        schema_ldif = ""
        prefixmap_data = ""
        for ent in current:
            schema_ldif += samdb.write_ldif(ent, ldb.CHANGETYPE_NONE)

            prefixmap_data = open(setup_path("AD/prefixMap.txt"), 'r').read()
            prefixmap_data = b64encode(prefixmap_data)

            # We don't actually add this ldif, just parse it
            prefixmap_ldif = "dn: %s\nprefixMap:: %s\n\n" % (schemadn, prefixmap_data)
            prefixmap_ldif += "dn:\nchangetype: modify\nreplace: schemaupdatenow\nschemaupdatenow: 1\n\n"
            dsdb._dsdb_set_schema_from_ldif(samdb, prefixmap_ldif, schema_ldif, schemadn)
    except RuntimeError as err:
        print ("[!] error while provisioning the prefixMap: %s"
               % str(err))
    except LdbError as err:
        print ("[!] error while provisioning the prefixMap: %s"
               % str(err))

    try:
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_attributes.ldif", "Add Exchange attributes to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_auxiliary_class.ldif", "Add Exchange auxiliary classes to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_objectCategory.ldif", "Add Exchange objectCategory to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_container.ldif", "Add Exchange containers to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_subcontainer.ldif", "Add Exchange *sub* containers to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_sub_CfgProtocol.ldif", "Add Exchange CfgProtocol subcontainers to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_sub_mailGateway.ldif", "Add Exchange mailGateway subcontainers to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema.ldif", "Add Exchange classes to Samba schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_possSuperior.ldif", "Add possSuperior attributes to Exchange classes")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_modify.ldif", "Extend existing Samba classes and attributes")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_schema_update.ldif", "Update schema")
    except LdbError, ldb_error:
        print ("[!] error while provisioning the Exchange"
               " schema classes (%d): %s"
               % ldb_error.args)

    try:
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_configuration.ldif", "Generic Exchange configuration objects")
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_configuration_finalize.ldif", "Finalize generic Exchange configuration objects")
        print "[SUCCESS] Done!"
    except LdbError, ldb_error:
        print ("[!] error while provisioning the Exchange configuration"
               " objects (%d): %s" % ldb_error.args)


def get_ldb_url(lp, creds, names):
    if names.serverrole == "member server":
        net = Net(creds, lp)
        dc = net.finddc(domain=names.dnsdomain, flags=nbt.NBT_SERVER_LDAP)
        url = "ldap://" + dc.pdc_dns_name
    else:
        url = lp.samdb_url()

    return url


def get_user_dn(ldb, basedn, username):
    if not isinstance(ldb, Ldb):
        raise TypeError("'ldb' argument must be an Ldb intance")

    ldb_filter = "(&(objectClass=user)(sAMAccountName=%s))" % username
    res = ldb.search(base=basedn, scope=SCOPE_SUBTREE, expression=ldb_filter, attrs=["*"])
    user_dn = None
    if len(res) == 1:
        user_dn = res[0].dn.get_linearized()

    return user_dn


def newuser(names, lp, creds, username=None):
    """extend user record with OpenChange settings.

    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: provision names object.
    :param username: Name of user to extend
    """

    db = Ldb(url=get_ldb_url(lp, creds, names), session_info=system_session(),
             credentials=creds, lp=lp)
    user_dn = get_user_dn(db, "CN=Users,%s" % names.domaindn, username)
    if user_dn:
        extended_user = """
dn: %(user_dn)s
changetype: modify
add: mailNickName
mailNickname: %(username)s
add: homeMDB
homeMDB: CN=Mailbox Store (%(netbiosname)s),CN=First Storage Group,CN=InformationStore,CN=%(netbiosname)s,CN=Servers,CN=%(firstou)s,CN=Administrative Groups,CN=%(firstorg)s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%(domaindn)s
add: homeMTA
homeMTA: CN=Mailbox Store (%(netbiosname)s),CN=First Storage Group,CN=InformationStore,CN=%(netbiosname)s,CN=Servers,CN=%(firstou)s,CN=Administrative Groups,CN=%(firstorg)s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%(domaindn)s
add: legacyExchangeDN
legacyExchangeDN: /o=%(firstorg)s/ou=%(firstou)s/cn=Recipients/cn=%(username)s
add: proxyAddresses
proxyAddresses: =EX:/o=%(firstorg)s/ou=%(firstou)s/cn=Recipients/cn=%(username)s
proxyAddresses: smtp:postmaster@%(dnsdomain)s
proxyAddresses: X400:c=US;a= ;p=First Organizati;o=Exchange;s=%(username)s
proxyAddresses: SMTP:%(username)s@%(dnsdomain)s
replace: msExchUserAccountControl
msExchUserAccountControl: 0
"""
        ldif_value = extended_user % {"user_dn": user_dn,
                                      "username": username,
                                      "netbiosname": names.netbiosname,
                                      "firstorg": names.firstorg,
                                      "firstou": names.firstou,
                                      "domaindn": names.domaindn,
                                      "dnsdomain": names.dnsdomain}
        db.modify_ldif(ldif_value)

        res = db.search(base=user_dn, scope=SCOPE_BASE, attrs=["*"])
        if len(res) == 1:
            record = res[0]
        else:
            raise Exception("this should never happen as we just modified the record...")
        record_keys = map(lambda x: x.lower(), record.keys())

        if "displayname" not in record_keys:
            extended_user = "dn: %s\nadd: displayName\ndisplayName: %s\n" % (user_dn, username)
            db.modify_ldif(extended_user)

        if "mail" not in record_keys:
            extended_user = "dn: %s\nadd: mail\nmail: %s@%s\n" % (user_dn, username, names.dnsdomain)
            db.modify_ldif(extended_user)

        print "[+] User %s extended and enabled" % username
    else:
        print "[!] User '%s' not found" % username


def accountcontrol(names, lp, creds, username=None, value=0):
    """enable/disable an OpenChange user account.

    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: Provision Names object
    :param username: Name of user to disable
    :param value: the control value
    """

    db = Ldb(url=get_ldb_url(lp, creds, names), session_info=system_session(),
             credentials=creds, lp=lp)
    user_dn = get_user_dn(db, "CN=Users,%s" % names.domaindn, username)
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


def checkusage(names, lp, creds):
    """Checks whether this server is already provisioned and is being used.

    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    """

    session_info = system_session()

    samdb = SamDB(url=get_ldb_url(lp, creds, names), session_info=session_info,
                  credentials=creds, lp=lp)

    try:
        config_dn = samdb.get_config_basedn()
        mapi_servers = samdb.search(
            base=config_dn, scope=ldb.SCOPE_SUBTREE,
            expression="(&(objectClass=msExchExchangeServer)(cn=%s))" % names.netbiosname)
        server_uses = []

        if len(mapi_servers) == 0:
            # The server is not provisioned.
            raise NotProvisionedError

        if len(mapi_servers) > 1:
            # Check if we are the primary folder store server.
            our_siteFolderName = "CN=Public Folder Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=%s,CN=AdministrativeGroups,%s" % (names.netbiosname, names.netbiosname, names.firstou, names.firstorgdn)
            dn = "CN=%s,CN=Administrative Groups,%s" % (names.firstou,
                                                        names.firstorgdn)
            ret = samdb.search(base=dn, scope=ldb.SCOPE_BASE, attrs=['siteFolderServer'])
            assert len(ret) == 1
            siteFolderName = ret[0]["siteFolderServer"][0]
            if our_siteFolderName.lower() == siteFolderName.lower():
                server_uses.append("primary folder store server")

            # Check if we are the primary receipt update service
            our_addressListServiceLink = "CN=%s,CN=Servers,CN=%s,CN=Administrative Groups,%s" % (names.netbiosname, names.firstou, names.firstorgdn)
            dn = "CN=Recipient Update Service (%s),CN=Recipient Update Services,CN=Address Lists Container,%s" % (names.domain, names.firstorgdn)
            ret = samdb.search(base=dn, scope=ldb.SCOPE_BASE, attrs=['msExchAddressListServiceLink'])
            assert len(ret) == 1
            addressListServiceLink = ret[0]['msExchAddressListServiceLink'][0]
            if our_addressListServiceLink.lower() == addressListServiceLink.lower():
                server_uses.append("primary receipt update service server")

        # Check if we handle any mailbox.
        db = Ldb(
            url=get_ldb_url(lp, creds, names), session_info=system_session(),
            credentials=creds, lp=lp)

        our_mailbox_store = "CN=Mailbox Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=%s,CN=Administrative Groups,%s" % (names.netbiosname, names.netbiosname, names.firstou, names.firstorgdn)
        mailboxes = db.search(
            base=names.domaindn, scope=ldb.SCOPE_SUBTREE,
            expression="(homeMDB=*)")
        mailboxes_handled = 0
        for user_mailbox in mailboxes:
            if (user_mailbox['homeMDB'][0] == our_mailbox_store and
                user_mailbox['msExchUserAccountControl'][0] != '2'):
                mailboxes_handled += 1

        if mailboxes_handled > 0:
            server_uses.append(
                "handling %d mailboxes" % mailboxes_handled)

        return server_uses
    except LdbError, ldb_error:
        print >> sys.stderr, "[!] error while checking whether this server is being used (%d): %s" % ldb_error.args
        raise ldb_error
    except RuntimeError as err:
        print >> sys.stderr, "[!] error while checking whether this server is being used: %s" % err
        raise err


def provision(setup_path, names, lp, creds, reporter=None):
    """Extend Samba4 with OpenChange data.

    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: Provision Names object
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)

    If a progress reporter is not provided, a text output reporter is provided
    """

    print "NOTE: This operation can take several minutes"

    if reporter is None:
        reporter = TextProgressReporter()

    # Install OpenChange-specific schemas
    install_schemas(setup_path, names, lp, creds, reporter)


def deprovision(setup_path, names, lp, creds, reporter=None):
    """Remove all configuration entries added by the OpenChange
    installation.

    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)

    It is assumed that checkusage has been used before to assure that the server is ready for deprovision
    """

    if reporter is None:
        reporter = TextProgressReporter()

    session_info = system_session()

    lp.set("dsdb:schema update allowed", "yes")

    samdb = SamDB(url=get_ldb_url(lp, creds, names), session_info=session_info,
                  credentials=creds, lp=lp)

    try:
        # This is the unique server, remove full schema
        deprovision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_configuration.ldif", "Remove Exchange configuration objects")
        # NOTE: AD schema objects cannot be deleted (it's a feature!)
        # So we can't remove all object added on provision
    except LdbError, ldb_error:
        print ("[!] error while deprovisioning the Exchange configuration"
               " objects (%d): %s" % ldb_error.args)
        raise ldb_error
    except RuntimeError as err:
        print ("[!] error while deprovisioning the Exchange configuration"
               " objects: %s" % err)
        raise err

def register(setup_path, names, lp, creds, reporter=None):
    """Register an OpenChange server as a valid Exchange server.

    :param setup_path: Path to the setup directory
    :param names: Provision Names object
    :param lp: Loadparm context
    :param creds: Credentials context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)

    If a progress reporter is not provided, a text output reporter is provided
    """

    if reporter is None:
        reporter = TextProgressReporter()

    try:
        provision_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_configuration_new_server.ldif", "Exchange Samba registration")
        print "[SUCCESS] Done!"
    except LdbError, ldb_error:
        print ("[!] error while registering Openchange Samba configuration"
               " objects (%d): %s" % ldb_error.args)


def unregister(setup_path, names, lp, creds, reporter=None):
    """Unregisters an OpenChange server.

    :param setup_path: Path to the setup directory
    :param names: Provision Names object
    :param lp: Loadparm context
    :param creds: Credentials context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)

    :raise ServerInUseError: If the server being unregistered is still being
                             used. The error string gives you the list of
                             uses.
    :raise NotProvisionedError: If we try to unregister a server which is not
                                yet provisioned.

    If a progress reporter is not provided, a text output reporter is provided
    """

    if reporter is None:
        reporter = TextProgressReporter()

    server_uses = checkusage(names, lp, creds)
    if (len(server_uses) > 0):
        raise ServerInUseError(', '.join(server_uses))

    try:
        # Unregister the server
        deprovision_schema(setup_path, names, lp, creds, reporter,
                           "AD/oc_provision_configuration_new_server.ldif",
                           "Unregistering Openchange server")
    except LdbError, ldb_error:
        print ("[!] error while unregistering the Openchange configuration"
               " objects (%d): %s" % ldb_error.args)
        raise ldb_error
    except RuntimeError as err:
        print ("[!] error while deprovisioning the Openchange configuration"
               " objects: %s" % err)
        raise err


def registerasmain(setup_path, names, lp, creds, reporter=None):
    """Register an OpenChange server as the main Exchange server.

    :param setup_path: Path to the setup directory
    :param names: Provision Names object
    :param lp: Loadparm context
    :param creds: Credentials context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)

    If a progress reporter is not provided, a text output reporter is provided
    """

    if reporter is None:
        reporter = TextProgressReporter()

    try:
        modify_schema(setup_path, names, lp, creds, reporter, "AD/oc_provision_configuration_as_main.ldif", "Register Exchange Samba as the main server")
        print "[SUCCESS] Done!"
    except LdbError, ldb_error:
        print ("[!] error while registering Openchange Samba configuration"
               " objects (%d): %s" % ldb_error.args)


def openchangedb_deprovision(names, lp, mapistore=None):
    """Removed the OpenChange database.

    :param names: Provision names object
    :param lp: Loadparm context
    :param mapistore: The public folder store type (fsocpf, sqlite, etc)
    """

    print "Removing openchange db"
    uri = openchangedb_url(lp)
    if uri.startswith('mysql'):
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri)
    else:
        openchangedb = mailbox.OpenChangeDB(uri)
    openchangedb.remove()


def openchangedb_provision(names, lp, uri=None):
    """Create the OpenChange database.

    :param names: Provision names object
    :param lp: Loadparm context
    :param uri: Openchangedb destination, by default will be a ldb file inside
    private samba directory. You can specify a mysql connection string like
    mysql://user:passwd@host/db_name to use openchangedb with mysql backend
    """

    print "Setting up openchange db"
    if uri is None or len(uri) == 0 or uri.startswith('ldb'):  # LDB backend
        openchangedb = mailbox.OpenChangeDB(openchangedb_url(lp))
    elif uri.startswith('mysql'):  # MySQL backend
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri, find_setup_dir())
    else:
        print "[!] error provisioning openchangedb: Unknown uri `%s`" % uri
        return
    openchangedb.setup(names)
    openchangedb.add_server(names)
    openchangedb.add_public_folders(names)


def find_setup_dir():
    """Find the setup directory used by provision."""
    dirname = os.path.dirname(__file__)
    search_path = re.search(r'(/(site|dist)-packages/)', dirname)
    if search_path and search_path.group(0):
        prefix = dirname[:dirname.index(search_path.group(0))]
        for suffix in ["share/openchange/setup", "share/setup", "share/samba/setup", "setup"]:
            ret = os.path.join(prefix, "../..", suffix)
            if os.path.isdir(ret):
                return os.path.abspath(ret)
    # In source tree
    ret = os.path.join(dirname, "../../setup")
    if os.path.isdir(ret):
        return os.path.abspath(ret)
    raise Exception("Unable to find setup directory.")
