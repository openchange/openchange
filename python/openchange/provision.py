#!/usr/bin/python

# OpenChange provisioning
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008
#
# Based on the original in JavaScript:
# 
# Copyright (C) Julien Kerihuel <jkerihuel@openchange.org> 2005 
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
from samba.samdb import SamDB
from samba.auth import system_session
from samba.provision import setup_add_ldif, setup_modify_ldif
import ldb

__docformat__ = 'restructuredText'

DEFAULTSITE = "Default-First-Site-Name"

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
        self.firstorgdn = None

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
        firstorg = "First Organization"

    if firstou is None:
        firstou = "First Organization Unit"

    names.firstorg = firstorg
    names.firstorgdn = "CN=%s,CN=Microsoft Exchange,CN=Services,%s" % (firstorg, configdn)
    names.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)

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


def newuser(lp, creds, username=None):
    """extend user record with OpenChange settings.
    
    :param lp: Loadparm context
    :param creds: Credentials context
    :param username: Name of user to extend
    """

    names = guess_names_from_smbconf(lp, None, None)

    samdb = SamDB(url="users.ldb", session_info=system_session(),
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

    samdb = SamDB(url="users.ldb", session_info=system_session(),
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
