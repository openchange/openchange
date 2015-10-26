#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChange provisioning
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008-2009
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2009
# Copyright (C) Enrique J. Hernández <ejhernandez@zentyal.com> 2015
# Copyright (C) Javier Amor Garcia <jamor@zentyal.com> 2015
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
import sys
from openchange import mailbox
from samba import Ldb, dsdb
from samba.samdb import SamDB
import ldb
from ldb import LdbError, SCOPE_SUBTREE, SCOPE_BASE
from samba import read_and_sub_file
from samba.auth import system_session
from samba.provision import setup_modify_ldif
from samba.net import Net
from samba.dcerpc import nbt
from openchange.urlutils import indexing_url, named_properties_url, openchangedb_url


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

    db = get_local_samdb(names, lp, creds)
    exchangedn = 'CN=Microsoft Exchange,CN=Services,%s' % configdn
    if not firstorg:
        ret = db.search(base=exchangedn,
                        scope=ldb.SCOPE_SUBTREE,
                        expression='(objectclass=msExchOrganizationContainer)',
                        attrs=['name'])
        if len(ret):
            firstorg = ret[0]['name'][0]
        if not firstorg:
            raise Exception("Cannot find first exchange organization in %s", exchangedn)
    firstorgdn = "CN=%s,%s" % (firstorg, exchangedn)

    if not firstou:
        firstou = db.searchone(
            'name', firstorgdn,
            '(&(objectclass=msExchAdminGroup)(msExchDefaultAdminGroup=TRUE))',
            ldb.SCOPE_SUBTREE)
    if not firstou:
        raise Exception("Cannot find first exchange OU in %s", firstorgdn)

    names.firstorg = firstorg
    names.firstou = firstou
    names.firstorgdn = firstorgdn
    names.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)

    # OpenChange dispatcher DB names
    names.ocserverdn = "CN=%s,%s" % (names.netbiosname, names.domaindn)
    names.ocfirstorg = firstorg
    names.ocfirstorgdn = "CN=%s,CN=%s,%s" % (firstou, names.ocfirstorg, names.ocserverdn)

    return names


def provision_schema(sam_db, setup_path, names, reporter, ldif, msg,
                     modify_mode=False, ignore_already_exists=False):
    """Provision/modify schema using LDIF specified file
    :param sam_db: sam db where to provision the schema
    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    :param modify_mode: Whether we want to modify or insert
    :param ignore_already_exists: Whether to ignore already existent schemas or not
    """
    sam_db.transaction_start()

    try:
        reporter.reportNextStep(msg)
        ldif_params = {
            "FIRSTORG": names.firstorg,
            "FIRSTORGDN": names.firstorgdn,
            "FIRSTOU": names.firstou,
            "CONFIGDN": names.configdn,
            "SCHEMADN": names.schemadn,
            "DOMAINDN": names.domaindn,
            "DOMAIN": names.domain,
            "DNSDOMAIN": names.dnsdomain,
            "NETBIOSNAME": names.netbiosname,
            "HOSTNAME": names.hostname
        }
        if modify_mode:
            ldif_apply_method = sam_db.modify_ldif
        else:
            ldif_apply_method = sam_db.add_ldif

        full_path = setup_path(ldif)
        ldif_data = read_and_sub_file(full_path, ldif_params)
        # schemaIDGUID can raise error if not match what is expected by schema master,
        #  if not present it will be automatically filled by the schema master
        ldif_data = re.sub("^schemaIDGUID:", '#schemaIDGUID:', ldif_data, flags=re.M)

        # We add elements one by one only to control better the exact position
        # of the error
        ldif_elements = re.split("\n\n+", ldif_data)
        elements_to_add = []
        for element in ldif_elements:
            if not element:
                continue
            # this match is to assure we have a legit element
            match = re.search('^\s*dn:\s+(.*)$', element, flags=re.M)
            if match:
                dn = match.group(1)
                if ignore_already_exists and exists_dn(sam_db, dn):
                    # TODO: check that the existent element has correct values
                    continue
                elements_to_add.append(element)

        if not elements_to_add:
            print 'No elements to add found in ' + full_path

        for el in elements_to_add:
            try:
                ldif_apply_method(el, ['relax:0'])
            except Exception as ex:
                print 'Error: "' + str(ex) + '" when adding element:\n' + el + '\n'
                raise
    except:
        sam_db.transaction_cancel()
        raise

    sam_db.transaction_commit()
    force_schemas_update(sam_db, setup_path)


def force_schemas_update(sam_db, setup_path):
    sam_db.transaction_start()
    try:
        # update schema now
        setup_modify_ldif(sam_db, setup_path('AD/update_now.ldif'), None)
    except:
        sam_db.transaction_cancel()
        raise
    sam_db.transaction_commit()


def modify_schema(sam_db, setup_path, names, reporter, ldif, msg,
                  ignore_already_exists=False):
    """Modify schema using LDIF specified file
    :param sam_db: sam db where to provision the schema
    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ldif: path to the LDIF file
    :param msg: reporter message
    :param ignore_already_exists: Whether to ignore already existent schemas or not
    """

    return provision_schema(sam_db, setup_path, names, reporter, ldif, msg,
                            modify_mode=True,
                            ignore_already_exists=ignore_already_exists)


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

    db = get_schema_master_samdb(names, lp, creds)
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


def install_schemas(setup_path, names, lp, creds, reporter,
                    ignore_already_exists=False):
    """Install the OpenChange-specific schemas in the SAM LDAP database.

    :param setup_path: Path to the setup directory.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ignore_already_exists: Whether to ignore already existent schemas or not
    """
    lp.set("dsdb:schema update allowed", "yes")

    sam_db = get_schema_master_samdb(names, lp, creds)

    # Step 1. Extending the prefixMap attribute of the schema DN record

    reporter.reportNextStep("Register Exchange OIDs")

    try:
        schemadn = str(names.schemadn)
        current = sam_db.search(expression="objectClass=classSchema", base=schemadn,
                                scope=SCOPE_BASE)

        schema_ldif = ""
        prefixmap_data = ""
        for ent in current:
            schema_ldif += sam_db.write_ldif(ent, ldb.CHANGETYPE_NONE)

            prefixmap_data = open(setup_path("AD/prefixMap.txt"), 'r').read()
            prefixmap_data = b64encode(prefixmap_data)

            # We don't actually add this ldif, just parse it
            prefixmap_ldif = "dn: %s\nprefixMap:: %s\n\n" % (schemadn, prefixmap_data)
            prefixmap_ldif += "dn:\nchangetype: modify\nreplace: schemaupdatenow\nschemaupdatenow: 1\n\n"
            dsdb._dsdb_set_schema_from_ldif(sam_db, prefixmap_ldif, schema_ldif, schemadn)
    except RuntimeError as err:
        print ("[!] error while provisioning the prefixMap: %s" % err)
    except LdbError as err:
        print ("[!] error while provisioning the prefixMap: %s" % err)

    schemas = [{'path': 'AD/oc_provision_schema_attributes.ldif',
                'description': 'Add Exchange attributes to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema_auxiliary_class.ldif',
                'description': 'Add Exchange auxiliary classes to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema_objectCategory.ldif',
                'description': 'Add Exchange objectCategory to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema_container.ldif',
                'description': 'Add Exchange containers to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema_subcontainer.ldif',
                'description': 'Add Exchange *sub* containers to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema_sub_CfgProtocol.ldif',
                'description': 'Add Exchange CfgProtocol subcontainers to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema_sub_mailGateway.ldif',
                'description': 'Add Exchange mailGateway subcontainers to Samba schema',
                'modify_mode': False},
               {'path': 'AD/oc_provision_schema.ldif',
                'description': 'Add Exchange classes to Samba schema',
                'modify_mode': False},

               # modify schemas
               {'path': 'AD/oc_provision_schema_possSuperior.ldif',
                'description': 'Add possSuperior attributes to Exchange classes',
                'modify_mode': True},
               {'path': 'AD/oc_provision_schema_modify.ldif',
                'description': 'Extend existing Samba classes and attributes',
                'modify_mode': True}]

    for schema in schemas:
        try:
            provision_schema(sam_db, setup_path, names, reporter, schema['path'],
                             schema['description'], schema['modify_mode'],
                             ignore_already_exists=ignore_already_exists)
        except LdbError, ldb_error:
            print ("[!] error while provisioning the Exchange"
                   " schema classes (%d): %s"
                   % ldb_error.args)
            raise

    try:
        provision_schema(sam_db, setup_path, names, reporter,
                         "AD/oc_provision_configuration.ldif",
                         "Generic Exchange configuration objects",
                         ignore_already_exists=ignore_already_exists)
    except LdbError, ldb_error:
        print ("[!] error while provisioning the Exchange configuration"
               " objects (%d): %s" % ldb_error.args)
        raise


def provision_organization(setup_path, names, lp, creds, reporter=None,
                           ignore_already_exists=False):
    """Create exchange organization

    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: Provision Names object
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ignore_already_exists: Whether to ignore already existent schemas or not
    """
    if reporter is None:
        reporter = TextProgressReporter()

    sam_db = get_schema_master_samdb(names, lp, creds)
    try:
        provision_schema(sam_db, setup_path, names, reporter,
                         "AD/oc_provision_configuration_org.ldif",
                         "Exchange Organization objects",
                         ignore_already_exists=ignore_already_exists)
    except LdbError, ldb_error:
            print ("[!] error while provisioning the Exchange organization objects"
                   " objects (%d): %s" % ldb_error.args)
            return False

    try:
        modify_schema(sam_db, setup_path, names, reporter,
                      "AD/oc_provision_configuration_finalize.ldif",
                      "Update generic Exchange configuration objects",
                      ignore_already_exists=ignore_already_exists)
    except LdbError, ldb_error:
        print ("[!] error while provisioning the Exchange organization"
               " objects (%d): %s" % ldb_error.args)
        return False
    return True


def deprovision_organization(setup_path, names, lp, creds, reporter=None):
    """Remove an existing exchange organization

    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: Provision Names object
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    """
    if reporter is None:
        reporter = TextProgressReporter()

    try:
        deprovision_schema(setup_path, names, lp, creds, reporter,
                           "AD/oc_provision_configuration_org.ldif",
                           "Exchange Organization objects")
    except LdbError, ldb_error:
            print ("[!] error while deprovisioning the Exchange organization objects"
                   " objects (%d): %s" % ldb_error.args)
            return False

    try:
        deprovision_schema(setup_path, names, lp, creds, reporter,
                           "AD/oc_provision_configuration_finalize.ldif",
                           "Update generic Exchange configuration objects",
                           modify_mode=True)
    except LdbError, ldb_error:
        print ("[!] error while deprovisioning the Exchange organization"
               " objects (%d): %s" % ldb_error.args)
        return False
    return True


def get_ldb_url(lp, creds, names):
    if names.serverrole == "member server":
        net = Net(creds, lp)
        dc = net.finddc(domain=names.dnsdomain, flags=nbt.NBT_SERVER_LDAP)
        url = "ldap://" + dc.pdc_dns_name
    else:
        url = lp.samdb_url()

    return url


def get_user_dn(ldb, basedn, username):
    ldb_filter = "(&(objectClass=user)(sAMAccountName=%s))" % username
    return _get_element_dn(ldb, basedn, ldb_filter)


def get_group_dn(ldb, basedn, groupname):
    ldb_filter = "(&(objectClass=group)(sAMAccountName=%s))" % groupname
    return _get_element_dn(ldb, basedn, ldb_filter)


def _get_element_dn(ldb, basedn, ldb_filter):
    if not isinstance(ldb, Ldb):
        raise TypeError("'ldb' argument must be an Ldb intance")

    dn = None
    res = ldb.search(base=basedn, scope=SCOPE_SUBTREE, expression=ldb_filter, attrs=["*"])
    n_res = len(res)
    if n_res == 1:
        dn = res[0].dn.get_linearized()
    elif n_res > 1:
        raise "More than one result for search %s with base %s" % (ldb_filter, basedn)

    return dn


def exists_dn(sam_db, dn):
    try:
        ret = sam_db.search(base=dn,
                            scope=ldb.SCOPE_BASE,
                            attrs=['dn'])
        return len(ret) > 0
    except LdbError as ex:
        code, leftover = ex
        if code == ldb.ERR_NO_SUCH_OBJECT:
            return False
        else:
            raise ex


def get_schema_master(db):
    res = db.search(base="", scope=ldb.SCOPE_BASE, attrs=["schemaNamingContext"])
    schemaNamingContext = res[0]["schemaNamingContext"][0]
    res = db.search(base=schemaNamingContext, scope=ldb.SCOPE_BASE, attrs=["fSMORoleOwner"])
    if len(res) == 0:
        raise Exception("No schema master found")
    elif len(res) > 1:
        raise Exception("More than one schema master found")

    owner = res[0]['fSMORoleOwner'][0]
    # remove prefix, to get the server owner
    server_owner = owner.split(',', 1)[1]

    return server_owner


def get_dns_owner(db, ntds_owner):
    res = db.search(base=ntds_owner, scope=ldb.SCOPE_BASE, attrs=["dnsHostname"])
    dns_hostname = res[0]['dnsHostname'][0]
    return dns_hostname.lower()


def get_schema_master_samdb(names, lp, creds):
    local_db = get_local_samdb(names, lp, creds)
    ntds_owner = get_schema_master(local_db)
    dns_owner = get_dns_owner(local_db, ntds_owner)
    fqdn = names.hostname + '.' + names.dnsdomain
    fqdn = fqdn.lower()
    if dns_owner == fqdn:
        return local_db

    owner_samdb_url = 'ldap://' + dns_owner
    session_info = system_session()
    sam_db = SamDB(url=owner_samdb_url, session_info=session_info,
                   credentials=creds, lp=lp)
    return sam_db


def get_local_samdb(names, lp, creds):
    samdb_url = get_ldb_url(lp, creds, names)
    session_info = system_session()
    local_db = SamDB(samdb_url, session_info=session_info,
                     credentials=creds, lp=lp)
    return local_db


def newuser(names, lp, creds, username=None, mail=None):
    """extend user record with OpenChange settings.

    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: provision names object.
    :param username: Name of user to extend
    :param mail: The user email address. If not specified, it will be set
                 to <samAccountName>@<dnsdomain>
    """
    db = get_local_samdb(names, lp, creds)
    user_dn = get_user_dn(db, names.domaindn, username)
    if user_dn:
        smtp_user, mail_domain = _smtp_user_and_domain_by_dn(user_dn, username, mail, names)
        extended_user = """
dn: %(user_dn)s
changetype: modify
replace: mailNickName
mailNickname: %(username)s
replace: homeMDB
homeMDB: CN=Mailbox Store (%(netbiosname)s),CN=First Storage Group,CN=InformationStore,CN=%(netbiosname)s,CN=Servers,CN=%(firstou)s,CN=Administrative Groups,CN=%(firstorg)s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%(domaindn)s
replace: homeMTA
homeMTA: CN=Mailbox Store (%(netbiosname)s),CN=First Storage Group,CN=InformationStore,CN=%(netbiosname)s,CN=Servers,CN=%(firstou)s,CN=Administrative Groups,CN=%(firstorg)s,CN=Microsoft Exchange,CN=Services,CN=Configuration,%(domaindn)s
replace: legacyExchangeDN
legacyExchangeDN: /o=%(firstorg)s/ou=%(firstou)s/cn=Recipients/cn=%(username)s
replace: proxyAddresses
proxyAddresses: SMTP:%(smtp_user)s
proxyAddresses: =EX:/o=%(firstorg)s/ou=%(firstou)s/cn=Recipients/cn=%(username)s
proxyAddresses: X400:c=US;a= ;p=%(firstorg_x400)s;o=%(firstou_x400)s;s=%(username)s
proxyAddresses: smtp:postmaster@%(mail_domain)s
replace: msExchUserAccountControl
msExchUserAccountControl: 0
replace: msExchRecipientTypeDetails
msExchRecipientTypeDetails: 6
replace: msExchRecipientDisplayType
msExchRecipientDisplayType: 0
replace: msExchOmaAdminWirelessEnable
msExchOmaAdminWirelessEnable: 0
"""
        # According to [MS-OXCMAIL] Section 1.1, the primary
        # proxyAddresses entry is the SMTP one and the secondaries
        # proxyAddresses entries are the smtp ones

        ldif_value = extended_user % {"user_dn": user_dn,
                                      "username": username,
                                      "netbiosname": names.netbiosname,
                                      "firstorg": names.firstorg,
                                      "firstorg_x400": names.firstorg[:16],
                                      "firstou": names.firstou,
                                      "firstou_x400": names.firstou[:64],
                                      "domaindn": names.domaindn,
                                      "dnsdomain": names.dnsdomain,
                                      "smtp_user": smtp_user,
                                      "mail_domain": mail_domain}
        db.modify_ldif(ldif_value)

        res = db.search(base=user_dn, scope=SCOPE_BASE, attrs=["*"])
        if len(res) == 1:
            record = res[0]
        else:
            raise Exception("Added user %s not found" % (user_dn))
        record_keys = map(lambda x: x.lower(), record.keys())

        if "displayname" not in record_keys:
            extended_user = "dn: %s\nadd: displayName\ndisplayName: %s\n" % (user_dn, username)
            db.modify_ldif(extended_user)

        if "mail" not in record_keys:
            extended_user = "dn: %s\nadd: mail\nmail: %s\n" % (user_dn, smtp_user)
            db.modify_ldif(extended_user)

        print "[+] User %s extended and enabled" % username
    else:
        print "[!] User '%s' not found" % username


def delete_user(names, lp, creds, username=None):
    db = get_local_samdb(names, lp, creds)
    basedn = "CN=Users,%s" % names.domaindn
    user_dn = get_user_dn(db, basedn, username)
    if not user_dn:
        print "[!] User '%s' not found" % username
        return

    to_delete = ['mailNickName', 'homeMDB', 'homeMTA', 'legacyExchangeDN',
                 'proxyAddresses', 'msExchUserAccountControl',
                 'msExchOmaAdminWirelessEnable',
                 'msExchRecipientTypeDetails', 'msExchRecipientDisplayType']
    _delete_attrs_ignore_nonexistent(db, user_dn, to_delete)

    print "[+] Removed all OpenChange LDAP attributes from account %s " % username


def accountcontrol(names, lp, creds, username, value=0):
    """enable/disable an OpenChange user account.

    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: Provision Names object
    :param username: Name of user to disable
    :param value: the control value
    """
    db = Ldb(url=get_ldb_url(lp, creds, names), session_info=system_session(),
             credentials=creds, lp=lp)
    dn = get_user_dn(db, names.domaindn, username)
    ldif = """
dn: %s
changetype: modify
replace: msExchUserAccountControl
msExchUserAccountControl: %d
""" % (dn, value)
    db.modify_ldif(ldif)
    if value == 2:
        print "[+] Account %s disabled" % username
    else:
        print "[+] Account %s enabled" % username


def newgroup(names, lp, creds, groupname, mail=None):
    """extend groupr record with OpenChange settings.

    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials context
    :param groupname: Name of group to extend
    :param mail: The group email address. If not specified, it will
                 be set to <samAccountName>@<dnsdomain>
    """
    db = get_local_samdb(names, lp, creds)
    group_dn = get_group_dn(db, names.domaindn, groupname)
    if group_dn:
        _enable_group(db, names, group_dn, groupname, mail)
        print "[+] Group %s extended and enabled" % groupname
    else:
        print "[!] Group '%s' not found" % groupname


def enable_all_groups_in_organization(names, lp, creds, org_name):
    """enable all groups in the given organization

    Groups without mail atrtibute are ignored
    This method only works in schemas where the groups reside under "CN=Groups,CN=%s,CN=Users,%s" % (org_name, domain_dn)

    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials context
    :param org_name: Name of the organization where the groups reside
    """
    basedn = "CN=Groups,CN=%s,CN=Users,%s" % (org_name, names.domaindn)
    ldap_filter = "(&(objectClass=group)(mail=*))"
    db = get_local_samdb(names, lp, creds)
    res = db.search(base=basedn,
                    scope=ldb.SCOPE_SUBTREE,
                    expression=ldap_filter,
                    attrs=['cn', 'mail'])
    for entry in res:
        dn = entry.dn.get_linearized()
        cn = entry['cn'][0]
        mail = entry['mail'][0]
        _enable_group(db, names, dn, cn, mail)


def _enable_group(db, names, group_dn, groupname, mail=None):
    smtp_user, mail_domain = _smtp_user_and_domain_by_dn(group_dn, groupname, mail, names)
    (recipient_type_details, recipient_display_type) = _group_recipient_type_details(db, group_dn)
    extended_group = """
dn: %(group_dn)s
changetype: modify
replace: mailNickName
mailNickname: %(groupname)s
replace: legacyExchangeDN
legacyExchangeDN: /o=%(firstorg)s/ou=%(firstou)s/cn=Recipients/cn=%(groupname)s
replace: proxyAddresses
proxyAddresses: SMTP:%(smtp_user)s
proxyAddresses: =EX:/o=%(firstorg)s/ou=%(firstou)s/cn=Recipients/cn=%(groupname)s
proxyAddresses: smtp:postmaster@%(mail_domain)s
proxyAddresses: X400:c=US;a= ;p=%(firstorg_x400)s;o=%(firstou_x400)s;s=%(groupname)s
replace: msExchRecipientTypeDetails
msExchRecipientTypeDetails: %(recipient_type_details)s
replace: msExchRecipientDisplayType
msExchRecipientDisplayType: %(recipient_display_type)s
"""
    ldif_value = extended_group % {"group_dn": group_dn,
                                   "groupname": groupname,
                                   "firstorg": names.firstorg,
                                   "firstorg_x400": names.firstorg[:16],
                                   "firstou": names.firstou,
                                   "firstou_x400": names.firstou[:64],
                                   "smtp_user": smtp_user,
                                   "mail_domain": mail_domain,
                                   "recipient_type_details": recipient_type_details,
                                   "recipient_display_type": recipient_display_type}
    db.modify_ldif(ldif_value)

    res = db.search(base=group_dn, scope=SCOPE_BASE, attrs=["*"])
    if len(res) == 1:
        record = res[0]
    else:
        raise Exception("Added group %s not found" % (group_dn))
    record_keys = [x.lower() for x in record.keys()]

    if "displayname" not in record_keys:
        extended_group = "dn: %s\nadd: displayName\ndisplayName: %s\n" % (group_dn, groupname)
        db.modify_ldif(extended_group)

    if "mail" not in record_keys:
        extended_group = "dn: %s\nadd: mail\nmail: %s\n" % (group_dn, smtp_user)
        db.modify_ldif(extended_group)


def delete_group(names, lp, creds, groupname):
    """
    Remove openchange provision attributes from group
    """
    db = get_local_samdb(names, lp, creds)
    group_dn = get_group_dn(db, names.domaindn, groupname)
    if not group_dn:
        raise Exception("Group not found " + groupname)
    to_delete = ['mailNickname', 'legacyExchangeDN', 'proxyAddresses',
                 'msExchRecipientTypeDetails', 'msExchRecipientDisplayType']
    _delete_attrs_ignore_nonexistent(db, group_dn, to_delete)

    print "[+] Removed all OpenChange LDAP attributes from group account %s " % groupname


def _delete_attrs_ignore_nonexistent(db, dn, to_delete):
    ldif_template = """
dn: %(dn)s
changetype: modify
delete: %(attr)s
"""
    for attr in to_delete:
        ldif_value = ldif_template % {"dn": dn, "attr": attr}
        try:
            db.modify_ldif(ldif_value)
        except LdbError as err:
            (code, msg) = err
            if code == ldb.ERR_NO_SUCH_ATTRIBUTE:
                # no existent attribute. Ignore
                pass
            else:
                raise err


def update_group(names, lp, creds, groupname):
    """
    Update the attributes for the group, should be called when a group is switched from security to distribution.
    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials context
    :param groupname: Name of group to extend
    """
    db = get_local_samdb(names, lp, creds)
    group_dn = get_group_dn(db, names.domaindn, groupname)

    res = db.search(base=group_dn, scope=SCOPE_BASE, attrs=['legacyExchangeDN'])
    if len(res) == 0:
        raise Exception("Group " + groupname + " is not provisioned for OpenChange")

    (recipient_type_details, recipient_display_type) = _group_recipient_type_details(db, group_dn)
    ldif = """
dn: %(group_dn)s
changetype: modify
replace: msExchRecipientTypeDetails
msExchRecipientTypeDetails: %(recipient_type_details)s
replace: msExchRecipientDisplayType
msExchRecipientDisplayType: %(recipient_display_type)s
"""
    ldif = ldif % {"group_dn": group_dn,
                   "recipient_type_details": recipient_type_details,
                   "recipient_display_type": recipient_display_type}
    db.modify_ldif(ldif)


def _group_recipient_type_details(db, group_dn):
    """
    Returned tuple (recipient_type_details, recipient_display_type)
    Values from http://blogs.technet.com/b/benw/archive/2007/04/05/exchange-2007-and-recipient-type-details.aspx
    Display type for Universal Security Group does not work in outlook2010 so I am using the
    one for distribution group
    """
    res = db.search(base=group_dn, scope=SCOPE_BASE, attrs=['groupType'])
    if len(res) == 1:
        groupType = int(res[0]['groupType'][0])
        universal = groupType & 0x8
        security = groupType & 0x80000000
    else:
        raise Exception("Group not found " + group_dn)

    if security and universal:
        return (1024, 1)
    elif security and not universal:
        # no value for this combination, using the previous one
        return (1024, 1)
    elif not security and universal:
        return (256, 1)
    elif not security and not universal:
        return (512, 1)


def _smtp_user_and_domain_by_dn(dn, name, mail, names):
    """
    Return the smpt user and mail domain for the given DN
    Returned as tuple (smtp_user, mail_domain)
    """
    if mail:
        smtp_user = mail
    else:
        smtp_user = name
    if '@' not in smtp_user:
        smtp_user += '@%s' % names.dnsdomain
        mail_domain = names.dnsdomain
    else:
        mail_domain = smtp_user.split('@')[1]
    return smtp_user, mail_domain


def checkusage(names, lp, creds):
    """Checks whether this server is already provisioned and is being used.

    :param names: provision names object.
    :param lp: Loadparm context
    :param creds: Credentials Context
    """

    samdb = get_local_samdb(names, lp, creds)

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
            server_uses.append("handling %d mailboxes" % mailboxes_handled)

        return server_uses
    except LdbError, ldb_error:
        print >> sys.stderr, "[!] error while checking whether this server is being used (%d): %s" % ldb_error.args
        raise ldb_error
    except RuntimeError as err:
        print >> sys.stderr, "[!] error while checking whether this server is being used: %s" % err
        raise err


def provision(setup_path, names, lp, creds, reporter=None,
              ignore_already_exists=False):
    """Extend Samba4 with OpenChange data.

    :param setup_path: Path to the setup directory
    :param lp: Loadparm context
    :param creds: Credentials context
    :param names: Provision Names object
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ignore_already_exists: Whether to ignore already existent schemas or not

    If a progress reporter is not provided, a text output reporter is provided
    """
    print "NOTE: This operation can take several minutes"

    if reporter is None:
        reporter = TextProgressReporter()

    # check not already provisioned
    check_not_provisioned(names, lp, creds)

    # Install OpenChange-specific schemas
    install_schemas(setup_path, names, lp, creds, reporter,
                    ignore_already_exists=ignore_already_exists)

    # Provision first org
    provision_organization(setup_path, names, lp, creds, reporter,
                           ignore_already_exists=ignore_already_exists)

    print "[SUCCESS] Done!"


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

    lp.set("dsdb:schema update allowed", "yes")
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


def register(setup_path, names, lp, creds, reporter=None,
             ignore_already_exists=False):
    """Register an OpenChange server as a valid Exchange server.

    :param setup_path: Path to the setup directory
    :param names: Provision Names object
    :param lp: Loadparm context
    :param creds: Credentials context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ignore_already_exists: Whether to ignore already existent schemas or not

    If a progress reporter is not provided, a text output reporter is provided
    """

    if reporter is None:
        reporter = TextProgressReporter()

    try:
        sam_db = get_schema_master_samdb(names, lp, creds)
        provision_schema(sam_db, setup_path, names, reporter,
                         "AD/oc_provision_configuration_new_server.ldif",
                         "Exchange Samba registration",
                         ignore_already_exists=ignore_already_exists)
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


def registerasmain(setup_path, names, lp, creds, reporter=None,
                   ignore_already_exists=False):
    """Register an OpenChange server as the main Exchange server.

    :param setup_path: Path to the setup directory
    :param names: Provision Names object
    :param lp: Loadparm context
    :param creds: Credentials context
    :param reporter: A progress reporter instance (subclass of AbstractProgressReporter)
    :param ignore_already_exists: Whether to ignore already existent schemas or not

    If a progress reporter is not provided, a text output reporter is provided
    """

    if reporter is None:
        reporter = TextProgressReporter()

    try:
        sam_db = get_schema_master_samdb(names, lp, creds)
        modify_schema(sam_db, setup_path, names, reporter,
                      "AD/oc_provision_configuration_as_main.ldif",
                      "Register Exchange Samba as the main server",
                      ignore_already_exists=ignore_already_exists)
        print "[SUCCESS] Done!"
    except LdbError, ldb_error:
        print ("[!] error while registering Openchange Samba configuration"
               " objects (%d): %s" % ldb_error.args)


def openchangedb_deprovision(names, lp, mapistore=None):
    """Remove the OpenChange database.

    :param names: Provision names object
    :param lp: Loadparm context
    :param mapistore: The public folder store type (fsocpf, sqlite, etc)
    """

    print "Removing openchange db"
    uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri)
    elif uri.startswith('ldb:') or uri.startswith('/'):
        openchangedb = mailbox.OpenChangeDB(uri)
    else:
        raise Exception("unknown backend in openchangedb URI %s" % uri)
    openchangedb.remove()


# migration methods for openchange directory
def directory_migrate(uri=None, version=None, **kwargs):
    provisioned = False
    lp = kwargs['lp']
    try:
        check_not_provisioned(kwargs['names'], lp, kwargs['creds'])
    except:
        provisioned = True
    if not provisioned:
        raise Exception("OpenChange is not provisioned, provision it instead of upgrading it")

    if uri is None:
        uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        directory = mailbox.Directory(uri)
        if directory.migrate(version, **kwargs):
            print "Migration of openchange directory done"
        else:
            print "Nothing to migrate"
    else:
        print "We need MySQL to manage directory migrations"


def directory_list_migrations(lp, uri):
    if uri is None:
        uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchange_directory = mailbox.Directory(uri)
        migrations = openchange_directory.list_migrations()
        print_migrations(migrations)
    else:
        print "Only Openchange_Directory with MySQL as backend has migration capability"


def directory_fake_migration(lp, uri, target_version):
    if uri is None:
        uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchange_directory = mailbox.Directory(uri)
        if openchange_directory.fake_migration(target_version):
            print "%d is now the current version" % target_version
    else:
        print "Only OpenchangeDB with MySQL as backend has migration capability"


def openchangedb_migrate(lp, uri=None, version=None):
    if uri is None:
        uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri)
        if openchangedb.migrate(version, kwargs={'lp': lp}):
            print "Migration openchange db done"
        else:
            print "Nothing to migrate"
    else:
        print "Only OpenchangeDB with MySQL as backend has migration capability"


def openchangedb_list_migrations(lp, uri):
    if uri is None:
        uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri)
        migrations = openchangedb.list_migrations()
        print_migrations(migrations)
    else:
        print "Only OpenchangeDB with MySQL as backend has migration capability"


def openchangedb_fake_migration(lp, uri, target_version):
    if uri is None:
        uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri)
        if openchangedb.fake_migration(target_version):
            print "%d is now the current version" % target_version
    else:
        print "Only OpenchangeDB with MySQL as backend has migration capability"


def openchangedb_provision(names, lp, uri):
    """Create the OpenChange database.

    :param names: Provision names object
    :param lp: Loadparm context
    :param uri: Openchangedb destination. You can specify a mysql connection
        string like mysql://user:passwd@host/db_name to use openchangedb with
        mysql backend or ldb: for LDB.
    """
    print "Setting up openchange db"
    if uri.startswith('ldb:'):  # LDB backend
        openchangedb = mailbox.OpenChangeDB(openchangedb_url(lp))
    elif uri.startswith('mysql:'):  # MySQL backend
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri, find_setup_dir())
    else:
        print "[!] error provisioning openchangedb: Unknown uri `%s`" % uri
        return
    openchangedb.setup(names)
    openchangedb.add_server(names)
    openchangedb.add_public_folders(names)


def openchangedb_new_organization(names, lp):
    """Create organization, servers and public folders in openchange db

    :param names: Provision names object
    :param lp: Loadparm context
    """
    uri = openchangedb_url(lp)
    if uri.startswith('mysql:'):
        openchangedb = mailbox.OpenChangeDBWithMysqlBackend(uri, find_setup_dir())
    else:
        openchangedb = mailbox.OpenChangeDB(uri)

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


def check_not_provisioned(names, lp, creds):
    """
    Check if the server is not already provisioned.
    For now only checking there are not exchange organizations created.
    """
    sam_db = get_schema_master_samdb(names, lp, creds)
    basedn = 'CN=Services,' + sam_db.get_config_basedn().get_linearized()
    ret = sam_db.search(
        attrs=['name'],
        base=basedn,
        expression='(objectclass=msExchOrganizationContainer)',
        scope=ldb.SCOPE_SUBTREE)
    if len(ret) > 0:
        firstorg = ret[0]['name'][0]
        raise Exception(
            'There is already, at least, one provisioned organization: %(name)s' % {'name': firstorg})


# Indexing related procedures
def indexing_migrate(lp, uri=None, version=None):
    if uri is None:
        uri = indexing_url(lp)
    if uri.startswith('mysql:'):
        indexing = mailbox.IndexingWithMysqlBackend(uri)
        if indexing.migrate(version):
            print "Migration MAPIStore indexing backend done"
        else:
            print "Nothing to migrate"
    else:
        print "Only indexing backend with MySQL as backend has migration capability"


def indexing_list_migrations(lp, uri):
    if uri is None:
        uri = indexing_url(lp)
    if uri.startswith('mysql:'):
        indexing = mailbox.IndexingWithMysqlBackend(uri)
        migrations = indexing.list_migrations()
        print_migrations(migrations)
    else:
        print "Only indexing backend with MySQL as backend has migration capability"


def indexing_fake_migration(lp, uri, target_version):
    if uri is None:
        uri = indexing_url(lp)
    if uri.startswith('mysql:'):
        indexing = mailbox.IndexingWithMysqlBackend(uri)
        if indexing.fake_migration(target_version):
            print "%d is now the current version" % target_version
    else:
        print "Only indexing backend with MySQL as backend has migration capability"


# Named properties related procedures
def named_props_migrate(lp, uri=None, version=None):
    if uri is None:
        uri = named_properties_url(lp)
    if uri.startswith('mysql:'):
        named_properties = mailbox.NamedPropertiesWithMysqlBackend(uri)
        if named_properties.migrate(version):
            print "Migration MAPIStore named properties backend done"
        else:
            print "Nothing to migrate"
    else:
        print "Only named properties backend with MySQL as backend has migration capability"


def named_props_list_migrations(lp, uri):
    if uri is None:
        uri = named_properties_url(lp)
    if uri.startswith('mysql:'):
        named_properties = mailbox.NamedPropertiesWithMysqlBackend(uri)
        migrations = named_properties.list_migrations()
        print_migrations(migrations)
    else:
        print "Only named properties backend with MySQL as backend has migration capability"


def named_props_fake_migration(lp, uri, target_version):
    if uri is None:
        uri = named_properties_url(lp)
    if uri.startswith('mysql:'):
        named_properties = mailbox.NamedPropertiesWithMysqlBackend(uri)
        if named_properties.fake_migration(target_version):
            print "%d is now the current version" % target_version
    else:
        print "Only named properties backend with MySQL as backend has migration capability"


# Helper procedures
def print_migrations(migrations):
    """Print formatted the migrations.

    :param dict migrations: indexed by version. It has a value a dict with these keys:
                            applied, class
    """
    for ver, migration in migrations.iteritems():
        print '[{applied}] {version} {name} {applied_date}'.format(
            applied='X' if migration['applied'] is not None else ' ',
            version=ver,
            name=migration['class'].description,
            applied_date="- %s" % migration['applied'] if migration['applied'] else "")
