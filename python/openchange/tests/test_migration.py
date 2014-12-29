#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) Enrique J. Hernández <ejhernandez@zentyal.com> 2014
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
OpenChangeDB migration framework functional tests.

It requires having a MySQL daemon running where smb.conf says
"""
from datetime import datetime
import MySQLdb
import unittest


from openchange.migration import migration, Migration, MigrationManager
from samba import param


class MigrationDBTests(unittest.TestCase):
    """Tests for OpenChangeDB migration framework"""

    @classmethod
    def setUpClass(cls):
        lp = param.LoadParm()
        lp.load_default()
        cls.db = MySQLdb.connect(host=lp.get('namedproperties:mysql_host'),
                                 user=lp.get('namedproperties:mysql_user'),
                                 passwd=lp.get('namedproperties:mysql_pass'))
        cur = cls.db.cursor()
        cur.execute('CREATE DATABASE IF NOT EXISTS openchange')
        cls.db.commit()
        cur.close()

    def tearDown(self):
        cur = self.db.cursor()
        cur.execute('DROP TABLE IF EXISTS test_migrations')

    def _register_do_nothing_migrations(self):
        # Register a do-nothing migration
        @migration(1)
        class DoNothingMigration(Migration):

            @classmethod
            def apply(cls, cur):
                cur.execute('SELECT 1 FROM test_migrations')

            @classmethod
            def unapply(cls, cur):
                cur.execute('SELECT 2 FROM test_migrations')

        MigrationManager.add_migration(2, DoNothingMigration)

    def test_setup(self):
        manager = MigrationManager(self.db, 'openchange', 'test_migrations')
        self.assertIsNotNone(manager)
        cur = self.db.cursor()
        cur.execute('DESCRIBE test_migrations')
        row = cur.fetchone()
        self.assertIsNotNone(row)

    def test_version(self):
        manager = MigrationManager(self.db, 'openchange', 'test_migrations')
        self.assertIsNone(manager.version)

        manager.apply_version(1)
        self.assertIsNotNone(manager.version)
        self.assertEquals(manager.version, 1)

        manager.apply_version(3)
        self.assertEquals(manager.version, 3)

        manager.unapply_version(3)
        self.assertEquals(manager.version, 1)

        manager.unapply_version(1)
        self.assertIsNone(manager.version)

    def test_go_forward_and_backwards(self):
        manager = MigrationManager(self.db, 'openchange', 'test_migrations')
        for v in manager.migrations.keys():
            del manager.migrations[v]

        self._register_do_nothing_migrations()

        migrated = manager.migrate(target_version=1)
        self.assertTrue(migrated)
        self.assertEquals(manager.version, 1)
        self.assertLessEqual(manager.applied_migration(1), datetime.now())

        migrated = manager.migrate()
        self.assertTrue(migrated)
        self.assertEquals(manager.version, 2)
        self.assertLessEqual(manager.applied_migration(2), datetime.now())

        migrated = manager.migrate()
        self.assertFalse(migrated)

        migrated = manager.migrate(target_version=1)
        self.assertTrue(migrated)
        self.assertIsNone(manager.applied_migration(2))

        migrated = manager.migrate()
        self.assertTrue(migrated)

        migrated = manager.migrate(target_version=0)
        self.assertTrue(migrated)
        self.assertIsNone(manager.version)
        self.assertIsNone(manager.applied_migration(1))
        self.assertIsNone(manager.applied_migration(2))

        migrated = manager.migrate(target_version=0)
        self.assertFalse(migrated)

    def test_list_migrations(self):
        manager = MigrationManager(self.db, 'openchange', 'test_migrations')
        for v in manager.migrations.keys():
            del manager.migrations[v]

        self.assertEquals(len(manager.list_migrations().keys()), 0)
        self._register_do_nothing_migrations()

        self.assertEquals(len(manager.list_migrations().keys()), 2)
        self.assertTrue(all([m['applied'] is None for m in manager.list_migrations().itervalues()]))
        self.assertTrue(all(['class' in m for m in manager.list_migrations().itervalues()]))

        manager.migrate()
        for m in manager.list_migrations().itervalues():
            self.assertTrue(m['applied'])
            self.assertTrue(len(m['class'].description))
