#!/usr/bin/python

# OpenChange tests
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2009
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


def test_suite():
    import unittest
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    from openchange.tests import (
        test_provision,
        test_mailbox,
        )
    suite.addTests(loader.loadTestsFromModule(test_provision))
    suite.addTests(loader.loadTestsFromModule(test_mailbox))
    return suite
