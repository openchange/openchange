#!/usr/bin/python
# -*- coding: utf-8 -*-

# Named properties DB schema and its migrations
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
Schema migration for Named Properties "app" with SQL based backend
"""
from MySQLdb import ProgrammingError
from openchange.migration import migration, Migration


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
