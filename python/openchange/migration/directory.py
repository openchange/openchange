#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChangeDB DB schema and its migrations
# Copyright (C) Enrique J. Hernández Blasco <ejhernandez@zentyal.com> 2015
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
"""
Migration for OpenChange directory schema and data
"""
from openchange.migration import migration, Migration
import ldb

from samba.samdb import SamDB
from samba.auth import system_session
from samba.provision import setup_modify_ldif
from samba.net import Net
from samba.dcerpc import nbt

# this helper functions are extracted from openchange.provision to avoid circular import
# it would be better a refactoring of provision.ppy so we can avoid repeating code here


def get_ldb_url(lp, creds, names):
    if names.serverrole == "member server":
        net = Net(creds, lp)
        dc = net.finddc(domain=names.dnsdomain, flags=nbt.NBT_SERVER_LDAP)
        url = "ldap://" + dc.pdc_dns_name
    else:
        url = lp.samdb_url()

    return url


def get_local_samdb(names, lp, creds):
    samdb_url = get_ldb_url(lp, creds, names)
    session_info = system_session()
    local_db = SamDB(samdb_url, session_info=session_info,
                     credentials=creds, lp=lp)
    return local_db


def force_schemas_update(sam_db, setup_path):
    sam_db.transaction_start()
    try:
        # update schema now
        setup_modify_ldif(sam_db, setup_path('AD/update_now.ldif'), None)
    except:
        sam_db.transaction_cancel()
        raise
    sam_db.transaction_commit()


# end of helper functions

@migration('directory', 1)
class AddRecipientAttributes(Migration):
    description = 'Add missing recipient attributes'

    @classmethod
    def apply(cls, cur, extra):
        lp = extra['lp']
        creds = extra['creds']
        names = extra['names']
        setup_path = extra['setup_path']

        db = get_local_samdb(names, lp, creds)
        schema_dn = "CN=Schema,CN=Configuration,%s" % names.domaindn

        # Add new schem atributes
        add_schema_attr_ldif = """
#
# ms-Exch-Recipient-Display-Type
#
dn: CN=ms-Exch-Recipient-Display-Type,%(schema_dn)s
objectClass: top
objectClass: attributeSchema
cn: ms-Exch-Recipient-Display-Type
distinguishedName: CN=ms-Exch-Recipient-Display-Type,%(schema_dn)s
instanceType: 4
uSNCreated: 43689
attributeID: 1.2.840.113556.1.4.7000.102.50730
attributeSyntax: 2.5.5.9
isSingleValued: TRUE
mAPIID: 14597
uSNChanged: 43689
showInAdvancedViewOnly: TRUE
adminDisplayName: ms-Exch-Recipient-Display-Type
adminDescription: ms-Exch-Recipient-Display-Type
oMSyntax: 2
searchFlags: 1
lDAPDisplayName: msExchRecipientDisplayType
name: ms-Exch-Recipient-Display-Type
schemaIDGUID:: sKuTuH52IE+RXzhXu8lq/g==
attributeSecurityGUID:: iYopH5jeuEe1zVcq1T0mfg==
isMemberOfPartialAttributeSet: TRUE
objectCategory: CN=Attribute-Schema,%(schema_dn)s

#
# ms-Exch-Recipient-Type-Details
#
dn: CN=ms-Exch-Recipient-Type-Details,%(schema_dn)s
objectClass: top
objectClass: attributeSchema
cn: ms-Exch-Recipient-Type-Details
distinguishedName: CN=ms-Exch-Recipient-Type-Details,%(schema_dn)s
instanceType: 4
whenCreated: 20140829141013.0Z
whenChanged: 20140829141013.0Z
uSNCreated: 43693
attributeID: 1.2.840.113556.1.4.7000.102.50855
attributeSyntax: 2.5.5.16
isSingleValued: TRUE
uSNChanged: 43693
showInAdvancedViewOnly: TRUE
adminDisplayName: ms-Exch-Recipient-Type-Details
adminDescription: ms-Exch-Recipient-Type-Details
oMSyntax: 65
searchFlags: 1
lDAPDisplayName: msExchRecipientTypeDetails
name: ms-Exch-Recipient-Type-Details
schemaIDGUID:: +KGbBgpUqUK/JqfdNUdTRg==
attributeSecurityGUID:: iYopH5jeuEe1zVcq1T0mfg==
isMemberOfPartialAttributeSet: TRUE
objectCategory: CN=Attribute-Schema,%(schema_dn)s
"""

        add_schema_attr_ldif = add_schema_attr_ldif % {'schema_dn': schema_dn}
        try:
            db.add_ldif(add_schema_attr_ldif, ['relax:0'])
        except Exception, ex:
            print "Error adding msExchRecipientTypeDetails and msExchRecipientDisplayType atributes to schema: %s" % str(ex)

        force_schemas_update(db, setup_path)

        upgrade_schema_ldif = """
dn: CN=Mail-Recipient,%(schema_dn)s
changetype: modify
add: mayContain
mayContain: msExchRecipientDisplayType
mayContain: msExchRecipientTypeDetails
"""
        upgrade_schema_ldif = upgrade_schema_ldif % {'schema_dn': schema_dn}
        try:
            db.modify_ldif(upgrade_schema_ldif)
        except Exception, ex:
            print "Error adding msExchRecipientTypeDetails and msExchRecipientDisplayType to Mail-Recipient schema: %s" % str(ex)

        force_schemas_update(db, setup_path)

        # Add missing attributes for users
        user_upgrade_ldif = ""
        user_upgrade_ldif_template = """
dn: %(user_dn)s
add: msExchRecipientTypeDetails
msExchRecipientTypeDetails: 6
add: msExchRecipientDisplayType
msExchRecipientDisplayType: 0
"""
        base_dn = "CN=Users,%s" % names.domaindn
        ldb_filter = "(objectClass=user)"
        res = db.search(base=base_dn, scope=ldb.SCOPE_SUBTREE, expression=ldb_filter)
        for element in res:
            dn = element.dn.get_linearized()
            ldif = user_upgrade_ldif_template % {'user_dn': dn}
            user_upgrade_ldif += ldif + "\n"
            try:
                db.modify_ldif(ldif)
            except Exception, ex:
                print "Error migrating user %s: %s. Skipping user" % (dn, str(ex))

    @classmethod
    def unapply(cls, cur):
        raise Exception("Cannot revert directory migrations")
