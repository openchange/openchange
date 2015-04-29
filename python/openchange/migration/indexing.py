#!/usr/bin/python
# -*- coding: utf-8 -*-

# Indexing DB schema and its migrations
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
Schema migration for Indexing "app" with SQL based backend
"""
from MySQLdb import ProgrammingError
from openchange.migration import migration, Migration


@migration('indexing', 1)
class InitialIndexingMigration(Migration):

    description = 'initial'

    @classmethod
    def apply(cls, cur, **kwargs):
        try:
            cur.execute('SELECT COUNT(*) FROM `mapistore_indexing`')
            return False
        except ProgrammingError as e:
            if e.args[0] != 1146:
                raise
            # Table does not exist, then migrate

        cur.execute("""CREATE TABLE IF NOT EXISTS `mapistore_indexing` (
                         `id` INT NOT NULL AUTO_INCREMENT,
                         `username` VARCHAR(1024) NOT NULL,
                         `fmid` VARCHAR(36) NOT NULL,
                         `url` VARCHAR(1024) NOT NULL,
                         `soft_deleted` VARCHAR(36) NOT NULL,
                         PRIMARY KEY (`id`))
                       ENGINE = InnoDB""")

        cur.execute("""CREATE TABLE IF NOT EXISTS `mapistore_indexes` (
                         `id` INT NOT NULL AUTO_INCREMENT,
                         `username` VARCHAR(1024) NOT NULL,
                         `next_fmid` VARCHAR(36) NOT NULL,
                         PRIMARY KEY (`id`))
                       ENGINE = InnoDB""")

    @classmethod
    def unapply(cls, cur):
        for query in ("DROP TABLE mapistore_indexes",
                      "DROP TABLE mapistore_indexing"):
            cur.execute(query)


@migration('indexing', 2)
class IndexesMigration(Migration):

    description = 'Indexes on indexing tables'

    @classmethod
    def apply(cls, cur, **kwargs):
        cur.execute("CREATE INDEX `mapistore_indexing_username_idx` ON `mapistore_indexing` (`username`(255) ASC)")
        cur.execute("CREATE INDEX `mapistore_indexing_username_url_idx` ON `mapistore_indexing` (`username`(255) ASC, `url`(255) ASC)")
        cur.execute("CREATE INDEX `mapistore_indexing_username_fmid_idx` ON `mapistore_indexing` (`username`(255) ASC, `fmid` ASC)")
        cur.execute("CREATE INDEX `mapistore_indexes_username_idx` ON `mapistore_indexes` (`username`(255) ASC)")

    @classmethod
    def unapply(cls, cur):
        cur.execute("DROP INDEX `mapistore_indexing_username_idx` ON `mapistore_indexing`")
        cur.execute("DROP INDEX `mapistore_indexing_username_url_idx` ON `mapistore_indexing`")
        cur.execute("DROP INDEX `mapistore_indexing_username_fmid_idx` ON `mapistore_indexing`")
        cur.execute("DROP INDEX `mapistore_indexes_username_idx` ON `mapistore_indexes`")
