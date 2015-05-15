#!/usr/bin/python
# -*- coding: utf-8 -*-

# Named properties DB schema and its migrations
# Copyright (C) Enrique J. Hernández Blasco <ejhernandez@zentyal.com> 2015
# Copyright (C) Jesús García Sáez <jgarcia@zentyal.com> 2015
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
Schema migration for Named Properties "app" with SQL based backend
"""
from __future__ import print_function
from MySQLdb import ProgrammingError
from openchange.migration import migration, Migration, mapistore_namedprops
import sys


@migration('named_properties', 1)
class InitialIndexingMigration(Migration):

    description = 'initial'

    @classmethod
    def apply(cls, cur, **kwargs):
        try:
            cur.execute('SELECT COUNT(*) FROM `named_properties`')
            return False
        except ProgrammingError as e:
            if e.args[0] != 1146:
                raise
            # Table does not exist, then migrate

        cur.execute("""CREATE TABLE IF NOT EXISTS `named_properties` (
                         `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
                         `type` TINYINT(1) NOT NULL,
                         `propType` INT(10) unsigned NOT NULL,
                         `oleguid` VARCHAR(255) NOT NULL,
                         `mappedId` INT(10) unsigned NOT NULL,
                         `propId` INT(10) unsigned DEFAULT NULL,
                         `propName` VARCHAR(255) DEFAULT NULL,
                         `oom` VARCHAR(255) DEFAULT NULL,
                         `canonical` VARCHAR(255) DEFAULT NULL,
                         PRIMARY KEY(`id`),
                         KEY `named_properties_nappedId` (`mappedId`),
                         KEY `named_properties_type_oleguid_propId` (`type`,`oleguid`,`propId`),
                         KEY `named_properties_type_oleguid_propName` (`type`,`oleguid`,`propName`)
                      ) ENGINE=InnoDB CHARSET=utf8""")

    @classmethod
    def unapply(cls, cur):
        cur.execute("DROP TABLE named_properties")


@migration('named_properties', 2)
class InitialDataMigration(Migration):

    description = 'import default named properties'

    @classmethod
    def apply(cls, cur, **kwargs):
        cur.execute("START TRANSACTION")
        try:
            for p in mapistore_namedprops.named_properties:
                cls._insert_named_property(cur, p)
            cur.execute("COMMIT")
        except Exception as e:
            print("Error inserting named properties, rollback", file=sys.stderr)
            cur.execute("ROLLBACK")
            raise

    @classmethod
    def _already_inserted(cls, cur, p):
        # Avoid duplicates in already running installations
        cur.execute("SELECT count(*) FROM named_properties "
                    "WHERE mappedId = %d AND propType = %d" %
                    (p['mappedId'], p['propType']))
        res = cur.fetchone()
        return int(res[0]) != 0

    @classmethod
    def _insert_named_property(cls, cur, p):
        fields = []

        # Mandatory fields
        mandatory_keys = ('type', 'propType', 'oleguid', 'mappedId')
        for mandatory_key in mandatory_keys:
            if mandatory_key not in p:
                return
            # All of them are integers but oleguid
            if mandatory_key == 'oleguid':
                fields.append("%s='%s'" % (mandatory_key, p[mandatory_key]))
            else:
                fields.append("%s=%d" % (mandatory_key, p[mandatory_key]))

        if cls._already_inserted(cur, p):
            # Property already on the database. This will happen in upgrades
            return

        # Optional fields
        optional_keys = ('propId', 'propName', 'oom', 'canonical')
        for optional_key in optional_keys:
            if optional_key not in p:
                continue
            # All of them are strings but propId
            if optional_key == 'propId':
                fields.append("%s=%d" % (optional_key, p[optional_key]))
            else:
                fields.append("%s='%s'" % (optional_key, p[optional_key]))

        sql = "INSERT named_properties SET %s" % ",".join(fields)
        cur.execute(sql)

    @classmethod
    def unapply(cls, cur):
        cur.execute("DELETE FROM named_properties")
