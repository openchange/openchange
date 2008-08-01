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

from samba.provision import setup_add_ldif, setup_modify_ldif

import ldb

__docformat__ = 'restructuredText'

def install_schemas(samdb, basedn, netbiosname, hostname):
    """Install the OpenChange-specific schemas in the SAM LDAP database.
    
    :param samdb: Samba SamDB object.
    :param basedn: BaseDN.
    """
    # Add new Active Directory classes schemas
    setup_add_ldif(samdb, "oc_provision_schema.ldif")

    # Extend existing Active Directory schemas
    setup_modify_ldif(samdb, "oc_provision_schema_modify.ldif", 
            {"BASEDN": basedn})

    # Add Configuration objects
    setup_add_ldif(samdb, "oc_provision_configuration.ldif", {
        "BASEDN": basedn,
        "NETBIOSNAME": netbiosname,
        "HOSTNAME": hostname})


def newuser(samdb, username):
    """extend user record with OpenChange settings.
    
    :param samdb: Sam Database handle.
    :param username: Name of user to extend.
    """
    dnsdomain    = strlower(lp.get("realm"))
    hostname    = hostname()
    netbiosname = strupper(hostname)

    samdb.transaction_start()
    try:
        # find the DNs for the domain and the domain users group
        res = samdb.search("defaultNamingContext=*", "", ldb.SCOPE_BASE, 
                           attrs=["defaultNamingContext"])
        domain_dn = res[0]["defaultNamingContext"]
        dom_users = samdb.searchone("dn", domain_dn, "name=Domain Users")

        user_dn = "CN=%s,CN=Users,%s" % (username, domain_dn)

        extended_user = """
    dn: %s
    changetype: modify
    add: auxiliaryClass
    displayName: %s
    auxiliaryClass: msExchBaseClass
    mailNickname: %s
    homeMDB: CN=Mailbox Store (%s),CN=First Storage Group,CN=InformationStore,CN=%s,CN=Servers,CN=First Administrative Group,CN=Administrative Groups,CN=First Organization,CN=Microsoft Exchange,CN=Services,CN=Configuration,%s
    legacyExchangeDN: /o=First Organization/ou=First Administrative Group/cn=Recipients/cn=%s
    proxyAddresses: smtp:postmaster@%s
    proxyAddresses: X400:c=US;a= ;p=First Organizati;o=Exchange;s=%s
    proxyAddresses: SMTP:%s@%s
    """ % (user_dn, username, username, netbiosname, netbiosname, domain_dn, username, dnsdomain, username, username, dnsdomain)

        samdb.modify_ldif(extended_user)
    except:
        samdb.transaction_abort()
        raise
    samdb.transaction_commit()


