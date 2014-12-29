#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChangeDB migration framework
# Copyright (C) Enrique J. Hernández Blasco <ejhernandez@zentyal.com> 2014
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
Schema migration management for OpenChange DB with SQL based backends
"""
import MySQLdb


__docformat__ = 'restructuredText'


class MigrationManager(object):

    migrations = {}

    @classmethod
    def add_migration(cls, version, migration):
        """Add a new migration class

        :param int version: the version of this migration
        :param Migration migration: the migration class which performs this action
        :raises ValueError: if the version is already available
        """
        if version in cls.migrations:
            raise ValueError("Already added a migration for %d" % version)
        cls.migrations[version] = migration

    def __init__(self, db, db_name, table_name='migrations'):
        """
        :param db: the database connection
        :param str db_name: the database name
        :param str table_name: the migrations' table name
        """
        self.db = db
        self.db_name = db_name
        self.table_name = MySQLdb.escape_string(table_name)
        self.db.select_db(self.db_name)
        self._version = None
        self._setup()

    def _setup(self):
        """Create the migrations table if not exists
        """
        cur = self.db.cursor()
        cur.execute("""SHOW TABLES LIKE %s""", self.table_name)
        row = cur.fetchone()
        if row is None or row[0] is None:
            cur.execute("""CREATE TABLE IF NOT EXISTS `{0}` (
                           version INT UNSIGNED,
                           applied TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                           PRIMARY KEY(version))
            """.format(self.table_name))
            self.db.commit()

    @property
    def version(self):
        if self._version is None:
            cur = self.db.cursor()
            cur.execute('SELECT MAX(version) FROM {}'.format(self.table_name))
            row = cur.fetchone()
            if row and row[0]:
                self._version = int(row[0])

        return self._version

    def apply_version(self, version):
        """Apply a version

        :param int version: the new migration version
        """
        cur = self.db.cursor()
        cur.execute('INSERT INTO {} (version) VALUES (%s)'.format(self.table_name),
                    version)
        self.db.commit()
        self._version = None  # To reload the value

    def unapply_version(self, version):
        """Unapply or rollback a version

        :param int version: the unapplied migration version
        """
        cur = self.db.cursor()
        cur.execute('DELETE FROM {} WHERE version = %s'.format(self.table_name),
                    version)
        self.db.commit()
        self._version = None  # To reload the value

    def migrate(self, target_version=None):
        """
        Apply/unapply missing migrations and update version information.

        The method is idempotent and you can call as many times as you want.
        :param int target_version: the target version to apply/unapply migrations.
        :rtype bool:
        :returns: True if any migration was applied/unapplied
        """
        if target_version is None:
            target_version = max(self.migrations.keys())

        go_forward = target_version >= self.version

        # 1) Get migrations sorted
        # 2) Keep only the migrations to apply/unapply
        # 3) Call these migrations objects with the cursor as parameter
        if go_forward:
            migrations = sorted(self.migrations.items())
            migrations = [m for m in migrations if m[0] <= target_version and m[0] > self.version]
        else:
            migrations = sorted(self.migrations.items(), reverse=True)
            migrations = [m for m in migrations if m[0] > target_version and m[0] <= self.version]

        for version, migration in migrations:
            # Migrate inside a transaction
            cursor = self.db.cursor()
            if go_forward:
                migration.apply(cursor)
            else:
                migration.unapply(cursor)
            self.db.commit()
            cursor.close()
            if go_forward:
                self.apply_version(version)
            else:
                self.unapply_version(version)

        return len(migrations) > 0

    def applied_migration(self, version):
        """Give back the timestamp for a given version

        :rtype datetime:
        :returns: None if the version does not have been applied
        """
        cursor = self.db.cursor()
        cursor.execute("""SELECT applied FROM {}
                          WHERE version = %s""".format(self.table_name),
                       version)
        row = cursor.fetchone()
        if row and row[0]:
            return row[0]

        return None

    def list_migrations(self):
        """List migrations

        :rtype dict:
        :returns: the registered migrations indexed by version and having
                  as value a dict with applied and class as keys.
        """
        migrations = {version: {'class': cls, 'applied': None} for version, cls in self.migrations.iteritems()}
        cursor = self.db.cursor()
        cursor.execute("""SELECT version, applied FROM {0}""".format(self.table_name))
        rows = cursor.fetchall()
        for row in rows:
            if row[0] in migrations:
                migrations[row[0]]['applied'] = row[1]

        return migrations


def migration(version):
    """
    Decorator to register a migration class that will receive a MySQL cursor.

    The version must be an integer, the migration will be executed in order
    using this value as key.

    :param int version: the version to add to migration manager
    :raises ValueError: if this version is already registered
    """
    def register_migration(cls):
        MigrationManager.add_migration(version, cls)
        return cls
    return register_migration


class Migration(object):
    """Base migration class to implement the migration interface called by
       migration manager
    """
    description = 'migration'

    @classmethod
    def apply(cls, cur):
        """Apply this migration

        :param cur: the DB cursor
        """
        pass

    @classmethod
    def unapply(cls, cur):
        """Unapply this migration

        :param cur: the DB cursor
        """
        pass


@migration(1)
class MultitenancyMigration(Migration):

    description = 'multitenancy'

    @classmethod
    def apply(cls, cur):
        try:
            cur.execute("SELECT count(*) FROM company")
        except:
            # Table does not exist, we don't need migration
            return
        # Migrate schema
        cur.execute("ALTER TABLE organizational_units DROP FOREIGN KEY fk_organizational_units_company_id;")
        cur.execute("ALTER TABLE organizational_units DROP COLUMN company_id")
        cur.execute("ALTER TABLE servers DROP FOREIGN KEY fk_servers_company_id")
        cur.execute("ALTER TABLE servers DROP COLUMN company_id")
        cur.execute("DROP TABLE company")
        cur.execute("ALTER TABLE servers ADD COLUMN ou_id INT NOT NULL")
        cur.execute("UPDATE servers SET ou_id = (SELECT id FROM organizational_units)")
        cur.execute("ALTER TABLE servers ADD CONSTRAINT `fk_servers_ou_id` FOREIGN KEY (`ou_id`) REFERENCES `organizational_units` (`id`) ON DELETE NO ACTION ON UPDATE NO ACTION")
        # Migrate data
        cur.execute("UPDATE messages m JOIN mailboxes mb ON mb.id = m.mailbox_id set m.ou_id = mb.ou_id WHERE m.ou_id IS NULL")
        cur.execute("UPDATE messages m JOIN folders f ON f.id = m.folder_id set m.ou_id = f.ou_id WHERE m.ou_id IS NULL")

    @classmethod
    def unapply(cls, cur):
        raise ValueError('We cannot unapply this migration')


@migration(2)
class MAPIStoreIndexingIndexesMigration(Migration):

    description = "mapistore indexes"

    @classmethod
    def apply(cls, cur):
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
