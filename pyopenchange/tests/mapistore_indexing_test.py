#!/usr/bin/python
# OpenChange python sample backend
#
# Copyright (C) Kamen Mazdrashki <kamenim@openchange.org> 2014
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

from openchange.mapistore import Indexing

print "Initializing"
idx = Indexing('kamen')
assert idx.username == 'kamen'

print "clean up if any lefovers"
idx.del_fmid(1, Indexing.PERMANENT_DELETE)

print "Test ADDING"
assert idx.add_fmid(1, 'test/uri'), "Failed to add fmid/URI"
assert not idx.add_fmid(1, 'test/uri'), "Adding same FMID should fail"

print "Test SEARCHING"
assert 'test/uri' == idx.uri_for_fmid(1)
assert 1 == idx.fmid_for_uri('test/uri')

print "Test DELETE"
assert idx.del_fmid(2, Indexing.SOFT_DELETE), "Self-delete non-existent entry should succeeed"
assert idx.del_fmid(1, Indexing.SOFT_DELETE), "Failed to soft delete FMID"
assert idx.del_fmid(2, Indexing.PERMANENT_DELETE), "Hard-delete non existent FMID should succeeed"
assert idx.del_fmid(1, Indexing.PERMANENT_DELETE), "Failed to hard delete FMID"

print "Test deleting just deleted entry"
assert idx.uri_for_fmid(1) is None
assert idx.fmid_for_uri('test/uri') is None
