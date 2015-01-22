# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Kamen Mazdrashki <kamenim@openchange.org>
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
'/mail/' service tests
"""

import os
import unittest


def main():
    cdir = os.path.dirname(os.path.abspath(__file__))
    testLoader = unittest.TestLoader()
    all_tests = testLoader.discover(cdir, pattern='__init__.py')
    unittest.TextTestRunner(verbosity=3).run(all_tests)

if __name__ == '__main__':
    # unittest.main()
    main()
