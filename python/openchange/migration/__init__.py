#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChange schema DB migration framework
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
Schema migration management for OpenChange "apps" with SQL based backends
"""
import MySQLdb


__docformat__ = 'restructuredText'


class MigrationManager(object):

    migrations = {}

    @classmethod
    def add_migration(cls, app, version, migration):
        """Add a new migration class for an "app"

        :param str app: the application to add the migration from
        :param int version: the version of this migration
        :param Migration migration: the migration class which performs this action
        :raises ValueError: if the version is already available
        """
        if app in cls.migrations and version in cls.migrations[app]:
            raise ValueError("Already added a migration for %s with %d" % (app, version))
        if app not in cls.migrations:
            cls.migrations[app] = {}
        cls.migrations[app][version] = migration

    def __init__(self, db, db_name, table_name='migrations'):
        """
        Set up migrations in this db connection and create the
        table_name if it is not existing in this db_name.

        :param db: the database connection
        :param str db_name: the database name
        :param str table_name: the migrations' table name
        """
        self.db = db
        self.db_name = db_name
        self.table_name = MySQLdb.escape_string(table_name)
        self.db.select_db(self.db_name)
        self._version = {app: None for app in self.migrations.keys()}
        self._setup()

    def _setup(self):
        """Create the table_name table if not exists
        """
        cur = self.db.cursor()
        cur.execute("""SHOW TABLES LIKE %s""", self.table_name)
        row = cur.fetchone()
        if row is None or row[0] is None:
            cur.execute("""CREATE TABLE IF NOT EXISTS `{0}` (
                           app VARCHAR(127),
                           version INT UNSIGNED,
                           applied TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                           PRIMARY KEY(app, version))
            """.format(self.table_name))
            self.db.commit()

    def version(self, app):
        """Get current version for the given "app"

        :param str app: the "application"
        """
        if app not in self._version:
            return None

        if self._version[app] is None:
            cur = self.db.cursor()
            cur.execute('SELECT MAX(version) FROM {0} WHERE app = %s'.format(self.table_name),
                        app)
            row = cur.fetchone()
            if row and row[0]:
                self._version[app] = int(row[0])

        return self._version[app]

    def apply_version(self, app, version):
        """Apply a version

        :param str app: the "application" to apply the version
        :param int version: the new migration version
        """
        cur = self.db.cursor()
        cur.execute('INSERT INTO {} (app, version) VALUES (%s, %s)'.format(self.table_name),
                    (app, version))
        self.db.commit()
        self._version[app] = None  # To reload the value

    def unapply_version(self, app, version):
        """Unapply or rollback a version

        :param str app: the "application" to unapply the version
        :param int version: the unapplied migration version
        """
        cur = self.db.cursor()
        cur.execute('DELETE FROM {} WHERE app = %s AND version = %s'.format(self.table_name),
                    (app, version))
        self.db.commit()
        self._version[app] = None  # To reload the value

    def migrate(self, app, target_version=None):
        """
        Apply/unapply missing migrations and update version information.

        The method is idempotent and you can call as many times as you want.

        :param str app: the "application" to migrate to the target version
        :param int target_version: the target version to apply/unapply migrations.
        :rtype bool:
        :returns: True if any migration was applied/unapplied
        """
        if target_version is None:
            target_version = max(self.migrations[app].keys())

        go_forward = target_version >= self.version(app)

        # 1) Get migrations sorted
        # 2) Keep only the migrations to apply/unapply
        # 3) Call these migrations objects with the cursor as parameter
        if go_forward:
            migrations = sorted(self.migrations[app].items())
            migrations = [m for m in migrations if m[0] <= target_version and m[0] > self.version(app)]
        else:
            migrations = sorted(self.migrations[app].items(), reverse=True)
            migrations = [m for m in migrations if m[0] > target_version and m[0] <= self.version(app)]

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
                self.apply_version(app, version)
            else:
                self.unapply_version(app, version)

        return len(migrations) > 0

    def fake_migration(self, app, target_version):
        """Fake a migration to a specific target version by
        simulating the apply/unapply migrations.

        :param str app: the "application" to fake migrations
        :param int target_version: version to go
        :rtype: bool
        :returns: True if any migration was faked
        """
        if app not in self.migrations:
            return False

        if target_version is None:
            target_version = 0

        current_version = self.version(app)
        if current_version is None:
            current_version = 0

        if current_version < target_version:
            ran = range(current_version + 1, target_version + 1)
            func = self.apply_version
        elif current_version > target_version:
            ran = range(target_version + 1, current_version + 1)
            func = self.unapply_version
        else:
            return False

        for ver in ran:
            if ver in self.migrations[app]:
                func(app, ver)

        return True

    def applied_migration(self, app, version):
        """Give back the timestamp for a given version from an app

        :param str app: the application
        :param int version: the version
        :rtype datetime:
        :returns: None if the version does not have been applied
        """
        cursor = self.db.cursor()
        cursor.execute("""SELECT applied FROM {0}
                          WHERE app = %s AND version = %s""".format(self.table_name),
                       (app, version))
        row = cursor.fetchone()
        if row and row[0]:
            return row[0]

        return None

    def list_migrations(self, app):
        """List migrations

        :param str app: the "application" to list the migrations from
        :rtype dict:
        :returns: the registered migrations indexed by version and having
                  as value a dict with applied and class as keys.
        """
        if app not in self.migrations:
            return {}
        migrations = {version: {'class': cls, 'applied': None} for version, cls in self.migrations[app].iteritems()}
        cursor = self.db.cursor()
        cursor.execute("""SELECT version, applied FROM {0}
                          WHERE app = %s""".format(self.table_name),
                       app)
        rows = cursor.fetchall()
        for row in rows:
            if row[0] in migrations:
                migrations[row[0]]['applied'] = row[1]

        return migrations

    @classmethod
    def apps(cls):
        """Return the list of registered "apps"

        :rtype list:
        """
        return sorted(cls.migrations.keys())


def migration(app, version):
    """
    Decorator to register a migration class that will receive a MySQL cursor.

    The version must be an integer, the migration will be executed in order
    using this value as key.

    :param str app: the "application" which this migration belongs to
    :param int version: the version to add to migration manager
    :raises ValueError: if this version is already registered
    """
    def register_migration(cls):
        MigrationManager.add_migration(app, version, cls)
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


# Import to get the migrations from different applications
from . import indexing, openchangedb
