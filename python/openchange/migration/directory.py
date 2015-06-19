#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChangeDB migration for directory schema and contents
# Copyright (C) Javier Amor Garcia <jamor@zentyal.com> 2015
#               Enrique J. Hernández <ejhernandez@zentyal.com> 2015
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
import sys


@migration('directory', 1)
class AddRecipientAttributes(Migration):
    description = 'Add missing recipient attributes'

    @classmethod
    def apply(cls, cur, **kwargs):
        from openchange.provision import force_schemas_update, get_local_samdb

        lp = kwargs['lp']
        creds = kwargs['creds']
        names = kwargs['names']
        setup_path = kwargs['setup_path']

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
            sys.stderr.write("Error adding msExchRecipientTypeDetails and msExchRecipientDisplayType atributes to schema: %s\n" % str(ex))

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
            sys.stderr.write("Error adding msExchRecipientTypeDetails and msExchRecipientDisplayType to Mail-Recipient schema: %s\n" % str(ex))

        force_schemas_update(db, setup_path)

        # Add missing attributes for users
        user_add_details_ldif_template = """
dn: %(user_dn)s
changetype: modify
add: msExchRecipientTypeDetails
msExchRecipientTypeDetails: 6
"""
        user_add_display_ldif_template = """
dn: %(user_dn)s
changetype: modify
add: msExchRecipientDisplayType
msExchRecipientDisplayType: 0
"""

        base_dn = "CN=Users,%s" % names.domaindn
        ldb_filter = "(&(objectClass=user)(proxyAddresses=*))"
        res = db.search(base=base_dn, scope=ldb.SCOPE_SUBTREE, expression=ldb_filter)
        for element in res:
            # check if intrnal user
            if 'showInAdvancedViewOnly' in element:
                continue

            upgrade_ldif_templates = []
            if not 'msExchRecipientTypeDetails' in element:
                upgrade_ldif_templates.append(user_add_details_ldif_template)
            if not 'msExchRecipientDisplayType' in element:
                upgrade_ldif_templates.append(user_add_display_ldif_template)
            for template in upgrade_ldif_templates:
                dn = element.dn.get_linearized()
                ldif = template % {'user_dn': dn}
                try:
                    db.modify_ldif(ldif)
                except Exception, ex:
                    sys.stderr.write("Error migrating user %s: %s. Skipping user\n" % (dn, str(ex)))

    @classmethod
    def unapply(cls, cur, **kwargs):
        # The schema migration cannot be rolled back because AD cannot
        # remove schema entries to avoid replication problems
        sys.stderr.write("Schema migration cannot be rolled back\n")


@migration('directory', 2)
class SetPostmansterSecondaryProxyAddressesEntry(Migration):
    description = 'Set postmaster as secondary SMTP ProxyAddresses entry'

    @classmethod
    def apply(cls, cur, **kwargs):
        from openchange.provision import get_local_samdb

        names = kwargs['names']
        db = get_local_samdb(names, kwargs['lp'], kwargs['creds'])

        # Set postmaster proxyAddress attribute as secondary entry
        modify_postmaster_ldif_template = """
dn: {user_dn}
changetype: modify
delete: proxyAddresses
proxyAddresses: SMTP:postmaster@{mail_domain}
-
add: proxyAddresses
proxyAddresses: smtp:postmaster@{mail_domain}
"""

        ldb_filter = "(proxyAddresses=*)"
        res = db.search(base=names.domaindn, scope=ldb.SCOPE_SUBTREE, expression=ldb_filter)
        for element in res:
            if 'proxyAddresses' in element:
                for entry in element['proxyAddresses']:
                    if entry.startswith('SMTP:postmaster@'):
                        mail_domain = entry[len('SMTP:postmaster@'):]
                        dn = element.dn.get_linearized()
                        ldif = modify_postmaster_ldif_template.format(user_dn=dn,
                                                                      mail_domain=mail_domain)
                        try:
                            db.modify_ldif(ldif)
                        except Exception, ex:
                            sys.stderr.write("Error migrating user %s: %s\n" % (dn, ex))

    @classmethod
    def unapply(cls, cur, **kwargs):
        # Do nothing by now
        from openchange.provision import get_local_samdb

        names = kwargs['names']
        db = get_local_samdb(names, kwargs['lp'], kwargs['creds'])

        # Set postmaster proxyAddress attribute as secondary entry
        modify_postmaster_ldif_template = """
dn: {user_dn}
changetype: modify
delete: proxyAddresses
proxyAddresses: smtp:postmaster@{mail_domain}
-
add: proxyAddresses
proxyAddresses: SMTP:postmaster@{mail_domain}
"""

        ldb_filter = "(proxyAddresses=*)"
        res = db.search(base=names.domaindn, scope=ldb.SCOPE_SUBTREE, expression=ldb_filter)
        for element in res:
            if 'proxyAddresses' in element:
                for entry in element['proxyAddresses']:
                    if entry.startswith('smtp:postmaster@'):
                        mail_domain = entry[len('smtp:postmaster@'):]
                        dn = element.dn.get_linearized()
                        ldif = modify_postmaster_ldif_template.format(user_dn=dn,
                                                                      mail_domain=mail_domain)
                        try:
                            db.modify_ldif(ldif)
                        except Exception, ex:
                            sys.stderr.write("Error unmigrating user %s: %s\n" % (dn, ex))


@migration('directory', 3)
class AddActiveSyncAttributes(Migration):
    description = 'Add missing ActiveSync attributes'

    @classmethod
    def apply(cls, cur, **kwargs):
        from openchange.provision import get_local_samdb
        lp = kwargs['lp']
        creds = kwargs['creds']
        names = kwargs['names']
        db = get_local_samdb(names, lp, creds)
        add_activesync_template = """
dn: {dn}
changetype: modify
add: msExchOmaAdminWirelessEnable
msExchOmaAdminWirelessEnable: 0
"""
        recipients = cls._get_all_enabled_users(db, names.domaindn)
        for element in recipients:
            if 'msExchOmaAdminWirelessEnable' in element:
                continue
            dn = element.dn.get_linearized()
            ldif = add_activesync_template.format(dn=dn)
            try:
                db.modify_ldif(ldif)
            except Exception, ex:
                msg = "Error migrating recipient %s: %s\n" % (dn, ex)
                sys.stderr.write(msg)

    @classmethod
    def unapply(cls, cur, **kwargs):
        from openchange.provision import get_local_samdb
        lp = kwargs['lp']
        creds = kwargs['creds']
        names = kwargs['names']
        db = get_local_samdb(names, lp, creds)
        delete_activesync_template = """
dn: {dn}
changetype: delete
delete: msExchOmaAdminWirelessEnable
"""
        recipients = cls._get_all_enabled_users(db, names.domaindn)
        for element in recipients:
            if not 'msExchOmaAdminWirelessEnable' in element:
                continue
            dn = element.dn.get_linearized()
            ldif = delete_activesync_template.format(dn=dn)
            try:
                db.modify_ldif(ldif)
            except Exception, ex:
                msg = "Error reverting migration in recipient %s: %s\n" % (dn, ex)
                sys.stderr.write(msg)

    @classmethod
    def _get_all_enabled_users(cls, db, base_dn):
        ldb_filter = "(&(objectClass=user)(proxyAddresses=*))"
        return db.search(base=base_dn,
                         scope=ldb.SCOPE_SUBTREE,
                         expression=ldb_filter)
