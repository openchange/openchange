#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) Enrique J. Hernández <ejhernandez@zentyal.com> 2015
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
from warnings import filterwarnings


from openchange.migration import migration, Migration, MigrationManager
from samba import param


# Borrowed from testsuite/testsuite_common.h
OC_TESTSUITE_MYSQL = {'HOST': '127.0.0.1',
                      'USER': 'root',
                      'PASS': "",
                      'DB': 'openchange_test'}


class MigrationDBTests(unittest.TestCase):
    """Tests for OpenChangeDB migration framework"""

    def setUp(self):
        # Load MySQL configuration from default, if it does not work, try with Samba Conf
        try:
            self.db = MySQLdb.connect(host=OC_TESTSUITE_MYSQL['HOST'],
                                      user=OC_TESTSUITE_MYSQL['USER'],
                                      passwd=OC_TESTSUITE_MYSQL['PASS'])
            self.db_name = OC_TESTSUITE_MYSQL['DB']
        except:
            lp = param.LoadParm()
            lp.load_default()
            self.db = MySQLdb.connect(host=lp.get('namedproperties:mysql_host'),
                                      user=lp.get('namedproperties:mysql_user'),
                                      passwd=lp.get('namedproperties:mysql_pass'))
            self.db_name = lp.get('namedproperties:mysql_db')

        cur = self.db.cursor()
        # Ignore database exists warnings
        filterwarnings(
            action="ignore",
            category=MySQLdb.Warning,
            message="Can't create database '{}'; database exists".format(self.db_name))
        cur.execute('CREATE DATABASE IF NOT EXISTS {}'.format(self.db_name))
        self.db.commit()
        cur.close()
        self.app = 'test'

    def tearDown(self):
        cur = self.db.cursor()
        cur.execute('DROP TABLE IF EXISTS test_migrations')

    def _register_do_nothing_migrations(self, app):
        # Register a do-nothing migration into app
        @migration(app, 1)
        class DoNothingMigration(Migration):

            @classmethod
            def apply(cls, cur, **kwargs):
                cur.execute('SELECT 1 FROM test_migrations')

            @classmethod
            def unapply(cls, cur):
                cur.execute('SELECT 2 FROM test_migrations')

        MigrationManager.add_migration(app, 2, DoNothingMigration)

    def test_setup(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')
        self.assertIsNotNone(manager)
        cur = self.db.cursor()
        cur.execute('DESCRIBE test_migrations')
        row = cur.fetchone()
        self.assertIsNotNone(row)

    def test_version(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')
        self.assertIsNone(manager.version(self.app))

        manager.apply_version(self.app, 1)
        self.assertIsNotNone(manager.version(self.app))
        self.assertEquals(manager.version(self.app), 1)

        manager.apply_version(self.app, 3)
        self.assertEquals(manager.version(self.app), 3)

        manager.unapply_version(self.app, 3)
        self.assertEquals(manager.version(self.app), 1)

        manager.unapply_version(self.app, 1)
        self.assertIsNone(manager.version(self.app))

    def test_go_forward_and_backwards(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')

        self._register_do_nothing_migrations(self.app)

        migrated = manager.migrate(self.app, target_version=1)
        self.assertTrue(migrated)
        self.assertEquals(manager.version(self.app), 1)
        self.assertLessEqual(manager.applied_migration(self.app, 1), datetime.now())

        migrated = manager.migrate(self.app)
        self.assertTrue(migrated)
        self.assertEquals(manager.version(self.app), 2)
        self.assertLessEqual(manager.applied_migration(self.app, 2), datetime.now())

        migrated = manager.migrate(self.app)
        self.assertFalse(migrated)

        migrated = manager.migrate(self.app, target_version=1)
        self.assertTrue(migrated)
        self.assertIsNone(manager.applied_migration(self.app, 2))

        migrated = manager.migrate(self.app)
        self.assertTrue(migrated)

        migrated = manager.migrate(self.app, target_version=0)
        self.assertTrue(migrated)
        self.assertIsNone(manager.version(self.app))
        self.assertIsNone(manager.applied_migration(self.app, 1))
        self.assertIsNone(manager.applied_migration(self.app, 2))

        migrated = manager.migrate(self.app, target_version=0)
        self.assertFalse(migrated)

        for v in manager.migrations.keys():
            del manager.migrations[v]

    def test_fake_migrations(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')

        self.assertFalse(manager.fake_migration('fake app', None))

        self._register_do_nothing_migrations(self.app)
        self.assertFalse(manager.fake_migration(self.app, None))

        self.assertTrue(manager.fake_migration(self.app, 3))
        self.assertEquals(manager.version(self.app), 2)

        self.assertFalse(manager.fake_migration(self.app, 2))

        self.assertTrue(manager.fake_migration(self.app, 1))
        self.assertEquals(manager.version(self.app), 1)

        for v in manager.migrations.keys():
            del manager.migrations[v]

    def test_list_migrations(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')

        self.assertEquals(len(manager.list_migrations(self.app).keys()), 0)
        self._register_do_nothing_migrations(self.app)

        self.assertEquals(len(manager.list_migrations(self.app).keys()), 2)
        self.assertTrue(all([m['applied'] is None for m in manager.list_migrations(self.app).itervalues()]))
        self.assertTrue(all(['class' in m for m in manager.list_migrations(self.app).itervalues()]))

        manager.migrate(self.app)
        for m in manager.list_migrations(self.app).itervalues():
            self.assertTrue(m['applied'])
            self.assertTrue(len(m['class'].description))

        for v in manager.migrations.keys():
            del manager.migrations[v]

    def test_multiapp(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')

        new_app = 'wannadie'
        self._register_do_nothing_migrations(self.app)
        self._register_do_nothing_migrations(new_app)

        migrated = manager.migrate(self.app, target_version=1)
        self.assertTrue(migrated)

        migrated = manager.migrate(new_app)
        self.assertTrue(migrated)

        self.assertEquals(manager.version(self.app), 1)
        self.assertEquals(manager.version(new_app), 2)

        self.assertLessEqual(manager.applied_migration(self.app, 1), datetime.now())
        self.assertLessEqual(manager.applied_migration(new_app, 2), datetime.now())

        self.assertEquals(len(manager.list_migrations(self.app).keys()), 2)
        self.assertEquals(len(manager.list_migrations(new_app).keys()), 2)

        for v in manager.migrations.keys():
            del manager.migrations[v]

    def test_apps(self):
        manager = MigrationManager(self.db, self.db_name, 'test_migrations')
        for v in manager.migrations.keys():
            del manager.migrations[v]

        self.assertEquals(manager.apps(), [])
        self._register_do_nothing_migrations(self.app)
        self._register_do_nothing_migrations('la_cena')

        self.assertEquals(manager.apps(), ['la_cena', self.app])

        for v in manager.migrations.keys():
            del manager.migrations[v]
